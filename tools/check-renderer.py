"""Run renderer correctness checks with the pinned local Vulkan validation layer.

Requires a desktop GPU, the demo and render_validation_probe targets, and Pillow.
Core/synchronization validation is mandatory; --gpu-assisted adds shader checks.
Never use this test's timings as performance measurements.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from PIL import Image, ImageStat

ROOT = Path(__file__).resolve().parent.parent
FINDING = re.compile(r'Validation (?:Error|Warning)|SYNC-HAZARD-|VUID-|NoGraphicsAPI validation:', re.I)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT / 'build/release')
    parser.add_argument('--output', type=Path, default=ROOT / '.scratch/render-validation')
    parser.add_argument('--layers', type=Path, default=ROOT / '.tools/Vulkan-ValidationLayers/bin')
    parser.add_argument('--gpu-assisted', action='store_true')
    parser.add_argument('--skip-window', action='store_true')
    args = parser.parse_args()
    build, output, layers = args.build.resolve(), args.output.resolve(), args.layers.resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / 'report.json').unlink(missing_ok=True)
    suffix = '.exe' if os.name == 'nt' else ''
    demo, probe = build / ('orbital' + suffix), build / ('render_validation_probe' + suffix)
    for path in [demo, probe, layers / 'VkLayer_khronos_validation.json']:
        if not path.is_file():
            raise RuntimeError(f'Required validation dependency is missing: {path}')
    pins = json.loads((ROOT / 'tools/validation-dependencies.json').read_text())
    manifest = json.loads((layers.parent / 'manifest.json').read_text(encoding='utf-8-sig'))
    if manifest['revision'] != pins['revision'] or manifest['dependencies'] != pins['dependencies']:
        raise RuntimeError('Local validation layer does not match pinned dependencies; rebuild it')
    if os.name == 'nt' and hashlib.sha256((layers / 'VkLayer_khronos_validation.dll').read_bytes()).hexdigest() != manifest['dll_sha256']:
        raise RuntimeError('Validation DLL does not match its installation manifest')
    settings = '\n'.join([
        f'khronos_validation.validate_core = {str(not args.gpu_assisted).lower()}',
        'khronos_validation.validate_sync = true',
        'khronos_validation.syncval_submit_time_validation = true',
        'khronos_validation.debug_action = VK_DBG_LAYER_ACTION_LOG_MSG',
        'khronos_validation.report_flags = error,warn',
        'khronos_validation.log_filename = stdout',
        'khronos_validation.enable_message_limit = false',
        'khronos_validation.gpuav_validate_trace_ray = false',
        'khronos_validation.gpuav_mesh_shading = false',
        f'khronos_validation.gpuav_enable = {str(args.gpu_assisted).lower()}',
    ]) + '\n'
    (output / 'vk_layer_settings.txt').write_text(settings)
    env = os.environ.copy()
    env.update(VK_LAYER_PATH=str(layers), VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',
               VK_LAYER_SETTINGS_PATH=str(output), VK_LAYER_VALIDATE_SYNC='1')
    flags = subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0
    results = []
    layer_adjustments = {}

    def run(name, command, negative=False):
        result = subprocess.run([str(v) for v in command], cwd=output, env=env, capture_output=True,
                                text=True, errors='replace', timeout=180, creationflags=flags)
        log = result.stdout + result.stderr
        (output / f'{name}.log').write_text(log, encoding='utf-8')
        result.check_returncode()
        if negative:
            if 'SYNC-HAZARD-WRITE-AFTER-WRITE' not in log:
                raise RuntimeError('Negative control failed: synchronization validation is not reporting hazards')
        else:
            checked_log = log
            if args.gpu_assisted:
                # GPU-AV advertises/enables supported instrumentation features. Preserve
                # those documented setup notices, but fail all actual validation findings.
                pattern = r'Validation Warning: \[ WARNING-Setting-Limit-Adjusted \].*?(?=Validation (?:Error|Warning):|\Z)'
                layer_adjustments[name] = re.findall(pattern, log, re.S)
                checked_log = re.sub(pattern, '', log, flags=re.S)
            if FINDING.search(checked_log):
                raise RuntimeError(f'Validation finding in {name}; see {output / (name + ".log")}')
        results.append(name)
        print(f'Passed: {name}', flush=True)

    # Prove the layer is loaded, sync validation enabled and diagnostics captured.
    run('negative-control', [probe, '--missing-barrier'], negative=True)
    run('synchronized-control', [probe])
    cases = {
        'earth': ['--bookmark', '0'],
        'belt-taa': ['--bookmark', '5', '--taa', '1'],
        'belt-no-taa': ['--bookmark', '5', '--taa', '0'],
        'belt-stop': ['--bookmark', '5', '--pan', '.25', '--pan-stop-frame', '40'],
        'tour': ['--tour'],
        'ui': ['--bookmark', '5', '--ui'],
        'fullscreen': ['--bookmark', '5', '--fullscreen-at', '20'],
    }
    for name, options in cases.items():
        capture = output / f'{name}.png'
        capture.unlink(missing_ok=True)
        run(name, [demo, '--time', '0', '--frames', '80', '--width', '960', '--height', '540',
                   '--vsync', '0', '--no-hud', '--capture', capture, *options])
        with Image.open(capture) as image:
            image.load()
            if min(image.size) < 64 or max(ImageStat.Stat(image.convert('RGB')).stddev) < 1:
                raise RuntimeError(f'Empty or flat render: {capture}')
    if os.name == 'nt' and not args.skip_window and not args.gpu_assisted:
        run('window-lifecycle', ['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                                ROOT / 'tools/smoke-window.ps1', '-Executable', demo,
                                '-Capture', output / 'window.png', '-TimeoutSeconds', '120'])
    (output / 'report.json').write_text(json.dumps({'passed': results, 'gpu_assisted': args.gpu_assisted,
                                                  'build': str(build), 'layers': str(layers), 'layer_adjustments': layer_adjustments}, indent=2) + '\n')
    print(f'Render validation passed. Logs and captures: {output}')


if __name__ == '__main__':
    main()
