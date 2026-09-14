"""Compare movement-to-rest captures against an otherwise identical no-streak shader.

Exercises the real camera, mote geometry, TAA, bloom and tone mapping in an isolated runtime.
Requires Windows, Slang, numpy and Pillow. The stationary frame must not retain motion cues.
"""
import argparse
import json
from pathlib import Path
import shutil
import re
import subprocess
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build/release')
    parser.add_argument('--output', type=Path, default=ROOT / '.scratch/motion-streaks')
    args = parser.parse_args()
    build, runtime = args.build.resolve(), args.output.resolve()
    runtime.mkdir(parents=True, exist_ok=True)
    shutil.copy2(build / 'orbital.exe', runtime / 'orbital.exe')
    shutil.copytree(build / 'shaders', runtime / 'shaders', dirs_exist_ok=True)
    if not (runtime / 'assets').exists():
        quote = lambda path: "'" + str(path).replace("'", "''") + "'"
        subprocess.run(['powershell', '-NoProfile', '-Command', 'New-Item -ItemType Junction -Path ' +
                        quote(runtime / 'assets') + ' -Target ' + quote(build / 'assets') + ' | Out-Null'], check=True)

    def compile_fixture(source, name, stage='fragment'):
        shader = runtime / f'{name}-{stage}.slang'
        shader.write_text(source)
        subprocess.run([str(ROOT / '.tools/slang/bin/slangc.exe'), str(shader), '-target', 'spirv',
                        '-profile', 'spirv_1_6', '-matrix-layout-column-major', '-fvk-use-entrypoint-name',
                        '-entry', stage + 'Main', '-stage', stage, '-o',
                        str(runtime / f'shaders/{name}.{stage}.spv')], check=True)

    # Remove atomic draw-order variation from the reference comparison. Keep the
    # real camera and mote geometry, with a uniform backdrop and known fractional
    # splat metadata (the history path on which the reported trails are strongest).
    compile_fixture('[shader("vertex")] float4 vertexMain() : SV_Position { return float4(2,2,0,1); }', 'surface', 'vertex')
    zero = '[shader("fragment")] float4 fragmentMain() : SV_Target0 { return float4(0.0); }'
    for name in ['stars', 'atmosphere']:
        compile_fixture(zero, name)
    compile_fixture('[shader("fragment")] float4 fragmentMain() : SV_Target0 { return float4(.02,.02,.02,1); }', 'background')
    temporal = (ROOT / 'shaders/aa/temporal.slang').read_text()
    temporal = re.sub(r'#include "([^\"]+)"', lambda m: '#include "' +
                      (ROOT / 'shaders/aa' / m[1]).resolve().as_posix() + '"', temporal)
    metadata = 'float4 metadata = textures[TEX_SPLAT_MASK].Load(int3(pixel, 0));'
    assert metadata in temporal
    compile_fixture(temporal.replace(metadata, 'float4 metadata = float4(20.0, 0.0, 0.0, .4);'), 'temporal')

    def capture(name, frames, taa):
        path = runtime / f'{name}.png'
        result = subprocess.run([str(runtime / 'orbital.exe'), '--bookmark', '5', '--time', '0',
                                 '--width', '960', '--height', '540', '--vsync', '0', '--no-hud',
                                 '--pan', '.7', '--pan-stop-frame', '60', '--frames', str(frames),
                                 '--taa', str(taa), '--dust', '0', '--disc', '0', '--rocks', '1', '--capture', str(path)], capture_output=True,
                                text=True, timeout=90, creationflags=subprocess.CREATE_NO_WINDOW)
        (runtime / f'{name}.log').write_text(result.stdout + result.stderr)
        result.check_returncode()
        with Image.open(path) as image:
            return np.asarray(image.convert('RGB'), dtype=np.int16)

    frames = [60, 61, 68]
    active = {(taa, frame): capture(f'active-{taa}-{frame}', frame, taa)
              for taa in [0, 1] for frame in frames}
    compile_fixture(zero, 'motes')
    report = {}
    for taa in [0, 1]:
        for frame in frames:
            delta = np.abs(active[taa, frame] - capture(f'no-motes-{taa}-{frame}', frame, taa))
            report[f'taa-{taa}-frame-{frame}'] = {'mean': float(delta.mean()), 'max': int(delta.max()),
                                                'pixels_over_8': int(np.any(delta > 8, axis=2).sum())}
    (runtime / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2), flush=True)
    for taa in [0, 1]:
        assert report[f'taa-{taa}-frame-60']['max'] > 8, 'Moving fixture did not show streaks'
        for frame in [61, 68]:
            result = report[f'taa-{taa}-frame-{frame}']
            assert result['max'] == 0, 'Streak history remains after stopping'


if __name__ == '__main__':
    main()
