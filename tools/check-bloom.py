"""GPU bloom regression: source-pixel coverage and a smooth halo at multiple sizes.

Requires a built Windows demo, Slang, numpy and Pillow. Runs an isolated copy in
.scratch; production shaders/assets are never modified. The actual bloom shader
and renderer passes are exercised with a synthetic HDR input and bloom-only output.
"""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent


def main():
    if os.name != 'nt':
        raise SystemExit('This capture harness currently requires the Windows demo.')
    build = ROOT / 'build/release'
    runtime = ROOT / '.scratch/bloom-regression'
    runtime.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build / 'orbital.exe', runtime / 'orbital.exe')
    shutil.copytree(build / 'shaders', runtime / 'shaders', dirs_exist_ok=True)
    for dll in build.glob('*.dll'):
        shutil.copy2(dll, runtime / dll.name)
    # Directory junction avoids duplicating the material set. It is never removed recursively.
    assets = runtime / 'assets'
    if not assets.exists():
        def ps_quote(path):
            return "'" + str(path).replace("'", "''") + "'"
        subprocess.run(['powershell', '-NoProfile', '-Command',
                        f'New-Item -ItemType Junction -Path {ps_quote(assets)} -Target {ps_quote(build / "assets")} | Out-Null'], check=True)
    shader = (ROOT / 'shaders/post/bloom.slang').read_text(encoding='utf-8')
    shader = shader.replace('#include "hdr.slang"', (ROOT / 'shaders/post/hdr.slang').read_text(encoding='utf-8'))
    shader = re.sub(r'#include "([^"]+)"', lambda m: '#include "' +
                    (ROOT / 'shaders/post' / m[1]).resolve().as_posix() + '"', shader)
    old = 'float3 sampleHDR(float2 uv) { return textures[TEX_HISTORY_A].SampleLevel(samplers[SAMPLER_CLAMP], uv, 0.0).rgb; }'
    assert old in shader
    # Sixteen isolated source texels cover all 4x4 downsample phases in one frame.
    synthetic = '''float3 sampleHDR(float2 uv) {
        float2 pixel = uv * root.frame.options.xy;
        float energy = 0.0;
        for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
            float2 centre = float2(400 + 96 * x + x, 240 + 96 * y + y) + .5;
            float2 tent = max(1.0 - abs(pixel - centre), 0.0);
            energy += 16.0 * tent.x * tent.y;
        }
        return float3(energy, energy, energy);
    }'''
    shader = shader.replace(old, synthetic)
    source = runtime / 'bloom-test.slang'
    source.write_text(shader, encoding='utf-8')
    subprocess.run([str(ROOT / '.tools/slang/bin/slangc.exe'), str(source), '-target', 'spirv',
                    '-profile', 'spirv_1_6', '-matrix-layout-column-major', '-fvk-use-entrypoint-name',
                    '-entry', 'fragmentMain', '-stage', 'fragment', '-o',
                    str(runtime / 'shaders/bloom.fragment.spv')], check=True)
    # Display the bloom image directly; the real bloom passes and present shader run unchanged.
    def compile_fixture(text, name):
        path = runtime / f'{name}-fixture.slang'
        path.write_text(text, encoding='utf-8')
        subprocess.run([str(ROOT / '.tools/slang/bin/slangc.exe'), str(path), '-target', 'spirv',
                        '-profile', 'spirv_1_6', '-matrix-layout-column-major', '-fvk-use-entrypoint-name',
                        '-entry', 'fragmentMain', '-stage', 'fragment', '-o',
                        str(runtime / f'shaders/{name}.fragment.spv')], check=True)

    bindings = '#include "' + (ROOT / 'shaders/scene/frame_bindings.slang').as_posix() + '"\n'
    compile_fixture(bindings + """
    [shader("fragment")]
    float4 fragmentMain(float4 position : SV_Position) : SV_Target0 {
        return float4(textures[TEX_BLOOM_A].SampleLevel(samplers[SAMPLER_CLAMP], position.xy / root.frame.options.xy, 0.0).rgb, 1.0);
    }
    """, 'composite')
    reports = []
    for width, height, resize in [(1600, 900, False), (1603, 903, False), (1600, 900, True)]:
        name = 'fullscreen' if resize else f'{width}x{height}'
        capture = runtime / f'{name}.png'
        command = [str(runtime / 'orbital.exe'), '--time', '0', '--frames', '8', '--rocks', '1',
                   '--width', str(width), '--height', str(height), '--no-hud', '--spatial', '0',
                   '--vsync', '0', '--capture', str(capture)]
        if resize:
            command += ['--fullscreen-at', '3']
        result = subprocess.run(command, capture_output=True, text=True, timeout=60,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        (runtime / f'{name}.log').write_text(result.stdout + result.stderr, encoding='utf-8')
        result.check_returncode()
        encoded = np.asarray(Image.open(capture), dtype=np.float64)[..., 0] / 255
        linear = np.where(encoded <= .04045, encoded / 12.92, ((encoded + .055) / 1.055) ** 2.4)
        energies = []
        for y in range(4):
            for x in range(4):
                cx, cy = 400 + 97 * x, 240 + 97 * y
                patch = linear[cy-44:cy+45, cx-44:cx+45]
                energies.append(float(patch.sum()))
                # The marginal of a blurred isolated point has no secondary peaks
                # or holes. Ignore <1% ripples from 8-bit screenshot quantization.
                for profile in (patch.sum(axis=0), patch.sum(axis=1)):
                    peak = int(profile.argmax())
                    tolerance = profile.max() * .01
                    assert np.all(np.diff(profile[:peak+1]) >= -tolerance), (name, x, y, 'rising halo')
                    assert np.all(np.diff(profile[peak:]) <= tolerance), (name, x, y, 'falling halo')
        spread = (max(energies) - min(energies)) / np.mean(energies)
        assert min(energies) > 10, (name, 'missed source texel', energies)
        assert spread < .02, (name, 'phase-dependent bloom energy', spread)
        reports.append(dict(case=name, size=list(Image.open(capture).size),
                            min_energy=min(energies), max_energy=max(energies), relative_spread=spread))
        print(json.dumps(reports[-1]), flush=True)
    (runtime / 'report.json').write_text(json.dumps(reports, indent=2) + '\n', encoding='utf-8')
    # Exercise the production visibility helper with a synthetic opaque half-plane.
    # Move the disc in fifth-pixel steps; dense visibility filtering should not
    # produce the old 1/12 global flare jumps. Keep this fixture out of production.
    lens = (ROOT / 'shaders/post/sun_occlusion.slang').read_text(encoding='utf-8')
    lens = re.sub(r'#include "([^"]+)"', lambda m: '#include "' +
                  (ROOT / 'shaders/post' / m[1]).resolve().as_posix() + '"', lens)
    lens = lens.replace('depth.Load', 'fixtureDepth')
    fixture = '#include "' + (ROOT / 'shaders/scene/frame_bindings.slang').as_posix() + '"\n'
    fixture += """float4 fixtureDepth(int3 pixel) {
        float value = float(pixel.x) >= root.frame.options.x * .5 ? 1.0 : .5;
        return float4(value, value, value, value);
    }
    """ + lens + """
    [shader("fragment")]
    float4 fragmentMain(float4 position : SV_Position) : SV_Target0 {
        float2 sun = (float2(root.frame.camera_time.w, 0.0) - root.frame.jitter.xy) * 2.0 / root.frame.options.xy;
        float v = sunDepthVisibility(root.frame, textures[TEX_DEPTH], sun, float2(root.frame.options.x / root.frame.options.y, 1.0));
        return float4(v, 0.0, 0.0, 1.0);
    }
    """
    compile_fixture(fixture, 'sun_visibility')
    compile_fixture(bindings + """
    [shader("fragment")]
    float4 fragmentMain(float4 position : SV_Position) : SV_Target0 {
        float visibility = textures[TEX_SUN_VISIBILITY].Load(int3(0, 0, 0)).r;
        return float4(visibility, visibility, visibility, 1.0);
    }
    """, 'composite')
    visibility = []
    for offset in [0, .2, .4, .6, .8, 1.0]:
        capture = runtime / f'visibility-{offset}.png'
        result = subprocess.run([str(runtime / 'orbital.exe'), '--time', str(offset), '--frames', '2',
                                 '--rocks', '1', '--width', '1600', '--height', '900', '--no-hud',
                                 '--spatial', '0', '--vsync', '0', '--capture', str(capture)],
                                capture_output=True, text=True, timeout=60, creationflags=subprocess.CREATE_NO_WINDOW)
        result.check_returncode()
        with Image.open(capture) as img:
            code = img.getpixel((800, 450))[0] / 255
        visibility.append(code / 12.92 if code <= .04045 else ((code + .055) / 1.055) ** 2.4)
    assert abs(visibility[0] - .5) < .015, visibility
    assert min(np.diff(visibility)) >= 0 and max(np.diff(visibility)) < .05, visibility
    print('Half-plane visibility, 0..1 pixel: ' + str(visibility))
    (runtime / 'visibility.json').write_text(json.dumps(visibility) + '\n', encoding='utf-8')
    print('GPU bloom coverage, halo continuity and sun visibility passed.')


if __name__ == '__main__':
    main()
