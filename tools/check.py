"""Quick render checks on shot lists: every view drawn in one process, diffed against the accepted reference.

    python tools/check.py                 the quick group: the six bookmarks and the belt-sun view, TAA off, one frame
    python tools/check.py --changed       quick plus the groups whose paths the working tree touches against main
    python tools/check.py --group taa --group post
    python tools/check.py --all           every group
    python tools/check.py --accept        bless this run's captures as the reference for the shots it ran
    python tools/check.py --executable D:/other/build/orbital.exe --all --accept   a before-build as the reference
    python tools/check.py --validate      under the local Vulkan validation layer (core and sync); a finding fails

Each shot is compared with its reference capture pixel by pixel: the share of
pixels off by more than two display codes, the share over eight, and the largest
difference. A shot fails when the over-two share passes its tolerance (the
run-to-run floor is 0.007 percent with TAA off and 0.07 percent at 64 TAA
frames). A shot without a reference is reported as new and passes. The exposure
group runs the meter instead and checks each view's request against the bands
in check-exposure.py. Captures, the app's report and check.json land in
.scratch/check/run; the reference lives in .scratch/check/reference.
Requires the Windows build, numpy and Pillow.
"""
import argparse
import fnmatch
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
BOOKMARKS = ['earth', 'giant', 'moon', 'mars', 'dawn', 'belt']
FINDING = re.compile(r'Validation (?:Error|Warning)|SYNC-HAZARD-|VUID-|NoGraphicsAPI validation:', re.I)
STILL = 'taa=0 frames=1 time=0'  # converged in one frame; the signature settings


def bookmark_shots(suffix='', tokens=STILL):
    return [f'name={name}{suffix} bookmark={index} {tokens}' for index, name in enumerate(BOOKMARKS)]


# Each group: the source paths whose change makes it worth running (--changed),
# and its shots as shot-list lines; the tolerance is the over-two-codes share
# a shot may reach before it fails (measured floors: belt views up to 0.03 percent).
GROUPS = {
    'quick': {
        'paths': ['**'],
        'shots': bookmark_shots() + [f'name=belt-sun belt-sun-view {STILL}'],
    },
    'bodies': {
        'paths': ['shaders/planets/*', 'shaders/surface/*', 'shaders/scene/*', 'src/render/renderer_scene.cpp',
                  'src/render/frame_calculations.*', 'src/scene/*'],
        'shots': [f'name=earth-25 bookmark=0 back=25 {STILL}',
                  f'name=earth-55 bookmark=0 back=55 {STILL}',
                  f'name=earth-zoom bookmark=0 back=55 fov-div=5 {STILL}',
                  f'name=earth-far bookmark=0 back=205 fov-div=8 {STILL}',
                  f'name=giant-55 bookmark=1 back=55 {STILL}',
                  f'name=giant-zoom bookmark=1 back=205 fov-div=8 {STILL}',
                  f'name=giant-200 bookmark=1 back=200 {STILL}',  # mid-fade, about 45 px of radius
                  f'name=giant-400 bookmark=1 back=400 {STILL}',  # the smallest tier, about 22 px
                  f'name=giant-400-zoom bookmark=1 back=400 fov-div=5 {STILL}',  # the zoom brings the detail back
                  f'name=moon-25 bookmark=2 back=25 {STILL}',
                  f'name=moon-zoom bookmark=2 back=55 fov-div=5 {STILL}',
                  f'name=moon-far bookmark=2 back=205 fov-div=8 {STILL}',
                  f'name=mars-25 bookmark=3 back=25 {STILL}',
                  f'name=mars-zoom bookmark=3 back=55 fov-div=5 {STILL}',
                  f'name=minor-planet bookmark=8 {STILL}',
                  'name=minor-planet-close bookmark=9 taa=0 frames=40 time=0', # the near tier converges over frames
                  f'name=minor-planet-sphere bookmark=9 near-tier=0 {STILL}',
                  f'name=mars-far bookmark=3 back=205 fov-div=8 {STILL}'],
    },
    'belt': {
        'paths': ['shaders/belt/*', 'src/render/renderer_belt.cpp'],
        'shots': [f'name=belt-no-dust bookmark=5 dust=0 {STILL}',
                  f'name=belt-no-disc bookmark=5 disc=0 {STILL}',
                  f'name=belt-splat-0 bookmark=5 splat=0 {STILL}',
                  f'name=belt-high bookmark=5 high {STILL}',
                  f'name=dawn-high bookmark=4 high {STILL}',
                  f'name=dust-shadow bookmark=6 {STILL}',
                  f'name=dust-grazing bookmark=7 {STILL}'],
    },
    'taa': {
        'paths': ['shaders/aa/*', 'shaders/scene/motes.slang'],
        'tolerance': 0.5,
        'shots': ['name=belt-taa bookmark=5 taa=1 frames=64 time=0',
                  'name=belt-pan bookmark=5 taa=1 frames=64 time=0 pan=.25 pan-stop-frame=40',
                  'name=belt-fxaa bookmark=5 taa=1 spatial=1 frames=64 time=0',
                  'name=belt-smaa bookmark=5 taa=1 spatial=2 frames=64 time=0',
                  'name=earth-taa bookmark=0 taa=1 frames=64 time=0'],
    },
    'post': {
        'paths': ['shaders/post/*', 'src/render/renderer_post.cpp'],
        'shots': [f'name=earth-sun-corner bookmark=0 sun-at=0.7,-0.7 {STILL}',
                  f'name=dawn-sun-edge bookmark=4 sun-at=0.95,0 {STILL}',
                  f'name=belt-tone-0 bookmark=5 tone=0 {STILL}',
                  f'name=belt-tone-1 bookmark=5 tone=1 {STILL}',
                  f'name=belt-exposure-4 bookmark=5 exposure=4 {STILL}'],
    },
    'sky': {
        'paths': ['shaders/sky/*', 'src/render/renderer_assets.cpp'],
        # The meter leaves the sky-only view near black, so a fixed 16x shows the layers.
        'shots': [f'name=galaxy-splats galaxy-view=-90 galaxy=0 exposure=16 {STILL}',
                  f'name=galaxy-layers galaxy-view=-90 galaxy=1 exposure=16 {STILL}',
                  f'name=galaxy-full galaxy-view=-90 galaxy=2 exposure=16 {STILL}',
                  f'name=galaxy-centre galaxy-view=0 galaxy=1 exposure=16 {STILL}'],
    },
    'exposure': {
        'paths': ['shaders/post/meter_histogram.slang', 'src/render/renderer_post.cpp', 'src/render/frame_calculations.*'],
        'meter': True,  # the meter's request per view against the reviewed bands, not a diff
        'shots': bookmark_shots('-meter', 'frames=48 taa=1'),
    },
}


def load_exposure_bands():
    spec = importlib.util.spec_from_file_location('check_exposure', Path(__file__).with_name('check-exposure.py'))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.EXPECTED_STOPS, module.SLACK_STOPS


def changed_paths():
    diff = subprocess.check_output(['git', 'diff', '--name-only', 'main'], cwd=ROOT, text=True)
    untracked = subprocess.check_output(['git', 'ls-files', '--others', '--exclude-standard'], cwd=ROOT, text=True)
    return sorted(set((diff + untracked).split()))


def matches(path, patterns):
    return any(fnmatch.fnmatch(path, pattern) or fnmatch.fnmatch(Path(path).name, pattern) for pattern in patterns)


def validation_environment(layers, output):
    """The env of a child under the local validation layer, pinned as check-renderer.py pins it."""
    if not (layers / 'VkLayer_khronos_validation.json').is_file():
        raise RuntimeError(f'Validation layer missing: {layers}; build it with tools/build-validation.ps1')
    pins = json.loads((ROOT / 'tools/validation-dependencies.json').read_text())
    manifest = json.loads((layers.parent / 'manifest.json').read_text(encoding='utf-8-sig'))
    if manifest['revision'] != pins['revision'] or manifest['dependencies'] != pins['dependencies']:
        raise RuntimeError('Local validation layer does not match pinned dependencies; rebuild it')
    if os.name == 'nt' and hashlib.sha256((layers / 'VkLayer_khronos_validation.dll').read_bytes()).hexdigest() != manifest['dll_sha256']:
        raise RuntimeError('Validation DLL does not match its installation manifest')
    (output / 'vk_layer_settings.txt').write_text('\n'.join([
        'khronos_validation.validate_core = true',
        'khronos_validation.validate_sync = true',
        'khronos_validation.syncval_submit_time_validation = true',
        'khronos_validation.debug_action = VK_DBG_LAYER_ACTION_LOG_MSG',
        'khronos_validation.report_flags = error,warn',
        'khronos_validation.log_filename = stdout',
        'khronos_validation.enable_message_limit = false',
        'khronos_validation.gpuav_enable = false',
    ]) + '\n')
    env = os.environ.copy()
    env.update(VK_LAYER_PATH=str(layers), VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation',
               VK_LAYER_SETTINGS_PATH=str(output), VK_LAYER_VALIDATE_SYNC='1')
    return env


def diff_images(current, reference):
    with Image.open(current) as image:
        a = np.asarray(image.convert('RGB'), dtype=np.int16)
    with Image.open(reference) as image:
        b = np.asarray(image.convert('RGB'), dtype=np.int16)
    if a.shape != b.shape:
        return {'over_2': 100.0, 'over_8': 100.0, 'max': 255, 'note': f'size {a.shape[1]}x{a.shape[0]} vs {b.shape[1]}x{b.shape[0]}'}
    delta = np.abs(a - b).max(axis=2)
    return {'over_2': 100 * float((delta > 2).mean()), 'over_8': 100 * float((delta > 8).mean()), 'max': int(delta.max())}


def flat(capture):
    with Image.open(capture) as image:
        pixels = np.asarray(image.convert('RGB'), dtype=np.float32)
    return min(pixels.shape[:2]) < 64 or pixels.reshape(-1, 3).std(axis=0).max() < 1


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--executable', type=Path, default=ROOT / 'build/release/orbital.exe')
    parser.add_argument('--output', type=Path, default=ROOT / '.scratch/check')
    parser.add_argument('--group', action='append', default=[], choices=list(GROUPS), help='run this group as well')
    parser.add_argument('--changed', action='store_true', help='add the groups whose paths differ from main')
    parser.add_argument('--all', action='store_true', help='run every group')
    parser.add_argument('--accept', action='store_true', help='make this run the reference for the shots it ran')
    parser.add_argument('--validate', action='store_true', help='run under the local Vulkan validation layer')
    parser.add_argument('--layers', type=Path, default=ROOT / '.tools/Vulkan-ValidationLayers/bin')
    parser.add_argument('--width', type=int, default=960)
    parser.add_argument('--height', type=int, default=540)
    parser.add_argument('--show', action='store_true', help='keep the window visible while the shots run')
    args = parser.parse_args()
    output = args.output.resolve()
    run, reference = output / 'run', output / 'reference'
    run.mkdir(parents=True, exist_ok=True)
    reference.mkdir(parents=True, exist_ok=True)

    selected = ['quick'] + args.group
    if args.all:
        selected = list(GROUPS)
    elif args.changed:
        paths = changed_paths()
        selected += [name for name, group in GROUPS.items() if name not in selected and matches_any(paths, group['paths'])]
    selected = list(dict.fromkeys(selected))
    shots, lines = [], []
    for name in selected:
        group = GROUPS[name]
        for line in group['shots']:
            shot = dict(token.split('=', 1) if '=' in token else (token, '') for token in line.split())
            shot.update(group=name, tolerance=group.get('tolerance', 0.1), meter=group.get('meter', False))
            shots.append(shot)
            lines.append(f'{line} capture={(run / shot["name"]).as_posix()}.png')
    for shot in shots:
        (run / f'{shot["name"]}.png').unlink(missing_ok=True)
    (run / 'check.shots').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    report = run / 'report.json'
    report.unlink(missing_ok=True)
    command = [str(args.executable), '--shots', str(run / 'check.shots'), '--report', str(report), '--seed', '20260911',
               '--width', str(args.width), '--height', str(args.height), '--no-hud', '--vsync', '0']
    if not args.show:
        command.append('--headless')
    env = validation_environment(args.layers.resolve(), run) if args.validate else None
    print(f'Running {len(shots)} shots ({", ".join(selected)})' + (' under validation' if args.validate else ''), flush=True)
    result = subprocess.run(command, cwd=run, env=env, capture_output=True, text=True, errors='replace', timeout=900,
                            creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    log = result.stdout + result.stderr
    (run / 'run.log').write_text(log, encoding='utf-8')
    if result.returncode:
        print(log[-3000:], file=sys.stderr)
        raise SystemExit(f'orbital exited with {result.returncode}; see {run / "run.log"}')
    readings = {shot['name']: shot for shot in json.loads(report.read_text(encoding='utf-8'))['shots']}

    failures, results = [], {}
    if args.validate and FINDING.search(log):
        failures.append(f'validation finding; see {run / "run.log"}')
    bands, slack = load_exposure_bands() if any(shot['meter'] for shot in shots) else ({}, 0)
    print(f'{"shot":18s} {"over 2":>8s} {"over 8":>8s} {"max":>4s}  verdict')
    for shot in shots:
        name = shot['name']
        capture, accepted = run / f'{name}.png', reference / f'{name}.png'
        entry = results[name] = {'group': shot['group']}
        if name not in readings or not capture.is_file():
            entry['verdict'] = 'missing'
            failures.append(f'{name}: no capture in the run')
        elif flat(capture):
            entry['verdict'] = 'flat'
            failures.append(f'{name}: empty or flat render')
        elif shot['meter']:
            exposure = readings[name]['exposure']
            view = name.removesuffix('-meter')
            if not exposure['ready'] or not exposure['automatic'] or view not in bands:
                entry['verdict'] = 'no meter reading'
                failures.append(f'{name}: no exposure reading in the report')
            else:
                stops, expected = math.log2(exposure['target']), bands[view]
                in_band = abs(stops - expected) <= slack
                entry.update(stops=stops, expected=expected,
                             verdict=f'{"in band" if in_band else "OUT OF BAND"} ({stops:+.2f} stops, band {expected:+.2f})')
                if not in_band:
                    failures.append(f'{name}: requested {stops:+.2f} stops, expected {expected:+.2f} within {slack}')
        elif not accepted.is_file():
            entry['verdict'] = 'new'
        else:
            entry.update(diff_images(capture, accepted))
            entry['verdict'] = 'same' if entry['over_2'] <= shot['tolerance'] else 'CHANGED'
            if entry['verdict'] == 'CHANGED':
                failures.append(f'{name}: {entry["over_2"]:.3f}% of pixels over two codes (tolerance {shot["tolerance"]}%)')
        if readings.get(name, {}).get('frames', 0) >= 32:
            entry['gpu_ms'] = readings[name]['gpu']
        numbers = (f'{entry["over_2"]:7.3f}% {entry["over_8"]:7.3f}% {entry["max"]:4d}' if 'over_2' in entry
                   else f'{"-":>8s} {"-":>8s} {"-":>4s}')
        print(f'{name:18s} {numbers}  {entry["verdict"]}' + (f'  {entry["note"]}' if 'note' in entry else ''), flush=True)
        if args.accept and entry['verdict'] not in ('missing', 'flat') and not shot['meter']:
            shutil.copy2(capture, accepted)
    (run / 'check.json').write_text(json.dumps({'groups': selected, 'validate': args.validate, 'accepted': args.accept,
                                                'executable': str(args.executable), 'shots': results}, indent=2) + '\n')
    if failures:
        print('\n' + '\n'.join(failures), file=sys.stderr)
        sys.exit(1)
    print(('Reference accepted for ' if args.accept else 'Checks passed for ') + f'{len(shots)} shots; captures in {run}')


def matches_any(paths, patterns):
    return any(matches(path, patterns) for path in paths)


if __name__ == '__main__':
    main()
