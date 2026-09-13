"""GPU regression for stable TAA output.

Uses isolated shader fixtures in .scratch; requires the Windows build, Slang,
numpy and Pillow. Scene/projection construction and the production TAA run intact.
"""
import json
import re
import shutil
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
RUNTIME = ROOT / '.scratch/taa-regression'


def compile_shader(source, name, stage='fragment'):
    path = RUNTIME / f'{name}-{stage}.slang'
    path.write_text(source, encoding='utf-8')
    subprocess.run([str(ROOT / '.tools/slang/bin/slangc.exe'), str(path),
                    '-target', 'spirv', '-profile', 'spirv_1_6',
                    '-matrix-layout-column-major', '-fvk-use-entrypoint-name',
                    '-entry', stage + 'Main', '-stage', stage, '-o',
                    str(RUNTIME / f'shaders/{name}.{stage}.spv')], check=True)


def capture(name, frames=120, taa=1, time=0, width=640, height=360):
    path = RUNTIME / f'{name}.png'
    result = subprocess.run([str(RUNTIME / 'orbital.exe'), '--time', str(time),
                             '--frames', str(frames), '--taa', str(taa), '--spatial', '0',
                             '--dust', '0', '--disc', '0', '--rocks', '1', '--no-hud',
                             '--vsync', '0', '--width', str(width), '--height', str(height),
                             '--capture', str(path)], capture_output=True, text=True,
                            timeout=60, creationflags=subprocess.CREATE_NO_WINDOW)
    (RUNTIME / f'{name}.log').write_text(result.stdout + result.stderr)
    result.check_returncode()
    with Image.open(path) as img:
        v = np.asarray(img, dtype=float)[..., 0] / 255
    return np.where(v <= .04045, v / 12.92, ((v + .055) / 1.055) ** 2.4)


def main():
    build = ROOT / 'build/release'
    RUNTIME.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build / 'orbital.exe', RUNTIME / 'orbital.exe')
    shutil.copytree(build / 'shaders', RUNTIME / 'shaders', dirs_exist_ok=True)
    for dll in build.glob('*.dll'):
        shutil.copy2(dll, RUNTIME / dll.name)
    if not (RUNTIME / 'assets').exists():
        def quote(p):
            return "'" + str(p).replace("'", "''") + "'"
        subprocess.run(['powershell', '-NoProfile', '-Command',
                        'New-Item -ItemType Junction -Path ' + quote(RUNTIME / 'assets') +
                        ' -Target ' + quote(build / 'assets') + ' | Out-Null'], check=True)
    bindings = '#include "' + (ROOT / 'shaders/scene/frame_bindings.slang').as_posix() + '"\n'
    # A band-limited stationary signal rendered with the real projection jitter.
    compile_shader(bindings + '''
    [shader("fragment")]
    float4 fragmentMain(float4 p : SV_Position) : SV_Target0 {
        float2 q = p.xy - root.frame.jitter.xy;
        float v = .4 + .2 * sin(q.x * 6.28318530718 / 32.0)
                     + .1 * sin(q.y * 6.28318530718 / 32.0);
        return float4(v, v, v, 1.0);
    }
    ''', 'background')
    # Remove geometry/stars/atmosphere from the fixture, leaving opaque depth clear.
    compile_shader('''
    [shader("vertex")]
    float4 vertexMain() : SV_Position { return float4(2.0, 2.0, 0.0, 1.0); }
    ''', 'surface', 'vertex')
    for name in ['stars', 'atmosphere']:
        compile_shader('''
        [shader("fragment")]
        float4 fragmentMain() : SV_Target0 { return float4(0.0); }
        ''', name)
    compile_shader(bindings + '''
    [shader("fragment")]
    float4 fragmentMain(float4 p : SV_Position) : SV_Target0 {
        return float4(textures[TEX_HISTORY_A].Load(int3(int2(p.xy), 0)).rgb, 1.0);
    }
    ''', 'composite')
    report = {}
    for taa in [0, 1]:
        phases = []
        for frame in range(120, 128):
            v = capture(f'taa-{taa}-{frame}', frames=frame, taa=taa)[64:192, 64:320]
            # Guard against an empty fixture accidentally passing the phase check.
            assert np.ptp(v.mean(axis=0)) > .3 and np.ptp(v.mean(axis=1)) > .15
            # Fourier phase measures translation independently of image contrast.
            x = np.arange(v.shape[1]) + 64.5
            y = np.arange(v.shape[0]) + 64.5
            phases.append([np.angle(np.sum(v.mean(axis=0) * np.exp(-2j*np.pi*x/32))),
                           np.angle(np.sum(v.mean(axis=1) * np.exp(-2j*np.pi*y/32)))])
        displacement = np.ptp(np.unwrap(phases, axis=0), axis=0) * 32 / (2*np.pi)
        report[f'taa_{taa}_displacement_px'] = displacement.tolist()
        assert max(displacement) < .04, report
    (RUNTIME / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
