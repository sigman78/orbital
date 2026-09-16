"""Fixed-scene render benchmarks and compatible-baseline comparisons (stdlib only)."""
import argparse
import csv
import datetime
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
SCENES = {name: ['--bookmark', str(i)] for i, name in
          enumerate(['earth', 'giant', 'moon', 'mars', 'dawn', 'belt'])}
SCENES['belt-sun'] = ['--belt-sun-view']
SETTINGS = ['--seed', '20260911', '--time', '0', '--taa', '1', '--spatial', '2',
            '--dust', '1', '--disc', '1', '--lod-scale', '1', '--splat', '2',
            '--tone', '2', '--galaxy', '0', '--exposure', '1', '--vsync', '0', '--no-hud']
METRICS = ['cpu_submit_and_wait_ms', 'gpu_ms', 'cpu_prepare_ms', 'gpu_cull_shadow_ms',
           'gpu_surface_ms', 'gpu_atmosphere_ms', 'gpu_post_ms']


def csv_metrics(fieldnames):
    # The group metrics above, then every other GPU scope the CSV carries (the
    # groups' children); older baselines without them still compare on the groups.
    return METRICS + [f for f in fieldnames if f.startswith('gpu_') and f.endswith('_ms') and f not in METRICS]


def sha(path):
    with path.open('rb') as f:
        return hashlib.file_digest(f, 'sha256').hexdigest()


def tree_hash(path, pattern='*'):
    digest = hashlib.sha256()
    files = sorted(p for p in path.rglob(pattern) if p.is_file())
    if not files:
        raise ValueError(f'No files to fingerprint: {path}')
    for file in files:
        digest.update(file.relative_to(path).as_posix().encode())
        digest.update(bytes.fromhex(sha(file)))
    return digest.hexdigest()


def command_output(command, cwd=ROOT):
    return subprocess.check_output(command, cwd=cwd, text=True).strip()


def summarize(rows, warmup, metrics=METRICS):
    rows = rows[warmup:]
    if len(rows) < 100:
        raise ValueError('At least 100 measured frames required after warmup')
    result = {}
    for key in metrics:
        values = sorted(float(r[key]) for r in rows)
        if any(not math.isfinite(v) or v < 0 for v in values):
            raise ValueError(f'Invalid timestamps in {key}')
        if key == 'gpu_ms' and values[0] <= 0:
            raise ValueError('Missing GPU timestamps')
        result[key] = {'median': statistics.median(values),
                       'p95': values[math.ceil(.95 * len(values)) - 1]}
    return result


def aggregate(runs):
    return {key: {'median': statistics.median(r[key]['median'] for r in runs),
                  'p95': statistics.median(r[key]['p95'] for r in runs),
                  'run_min': min(r[key]['median'] for r in runs),
                  'run_max': max(r[key]['median'] for r in runs)} for key in runs[0]}


def compare(current, baseline):
    # Executable and shader hashes should differ across revisions; assets should not.
    for key in ['suite_version', 'protocol', 'machine', 'assets_sha256']:
        if current[key] != baseline[key]:
            raise ValueError(f'Incompatible baseline {baseline["label"]}: {key} differs')
    if current['cases'].keys() != baseline['cases'].keys():
        raise ValueError('Incompatible baseline: scene/mode sets differ')
    changes = {}
    for case, now in current['cases'].items():
        old = baseline['cases'][case]
        if any(len(c['runs']) != r['protocol']['repeats'] for c, r in [(now, current), (old, baseline)]):
            raise ValueError(f'Incomplete benchmark rounds: {case}')
        if now['resolution'] != old['resolution']:
            raise ValueError(f'Incompatible baseline: {case} resolution differs')
        changes[case] = {}
        for metric in [m for m in now['metrics'] if m in old['metrics']]:
            a, b = now['metrics'][metric], old['metrics'][metric]
            delta = a['median'] - b['median']
            percent = 100 * delta / b['median'] if b['median'] else None
            large = delta > .2 and percent is not None and percent > 5
            status = ('regression' if a['run_min'] > b['run_max'] else 'noisy increase') if large else 'within threshold'
            changes[case][metric] = {'delta_ms': delta, 'delta_percent': percent, 'status': status}
    return changes


def write_report(report, baselines, output):
    comparisons = {b['label']: compare(report, b) for b in baselines}
    if len(comparisons) != len(baselines):
        raise ValueError('Baseline labels must be unique')
    report['comparisons'] = comparisons
    (output / 'summary.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    lines = [f'# Render performance: {report["label"]}', '',
             f'GPU: {report["machine"]["gpu"]}. Revision: `{report["revision"]}`. '
             f'Dirty source: {report["source_dirty"]}.', '',
             'Times are milliseconds, medians of repeated run medians; p95 is the median of run p95s.', '',
             '| Scene / mode | Resolution | CPU + wait | GPU | GPU p95 | Cull/maps | Surface | Atmosphere group | Post |',
             '| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |']
    with (output / 'summary.csv').open('w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(['case', 'width', 'height', 'metric', 'median_ms', 'p95_ms', 'run_min_ms', 'run_max_ms'])
        for name, case in report['cases'].items():
            m = case['metrics']
            for key in m:
                writer.writerow([name, *case['resolution'], key, *[m[key][v] for v in ['median', 'p95', 'run_min', 'run_max']]])
            vals = [m[k]['median'] for k in METRICS if k != 'cpu_prepare_ms']
            vals.insert(2, m['gpu_ms']['p95'])
            lines.append(f'| {name} | {case["resolution"][0]}×{case["resolution"][1]} | ' +
                         ' | '.join(f'{v:.3f}' for v in vals) + ' |')
    children = [(name, [k for k in case['metrics'] if k not in METRICS]) for name, case in report['cases'].items()]
    if any(keys for _, keys in children):
        lines += ['', '## Children', '', 'The scopes inside the groups: indicative, since a scope reads where its '
                  'commands were issued rather than an exact cost.', '',
                  '| Scene / mode | Scope | Median | p95 | Run range |', '| --- | --- | ---: | ---: | ---: |']
        for name, keys in children:
            for key in keys:
                m = report['cases'][name]['metrics'][key]
                lines.append(f'| {name} | {key} | {m["median"]:.3f} | {m["p95"]:.3f} | '
                             f'{m["run_min"]:.3f} to {m["run_max"]:.3f} |')
    for label, changes in comparisons.items():
        lines += ['', f'## Compared with {label}', '',
                  'Flags require both >5% and >0.2 ms increase. Overlapping run ranges are marked noisy; confirm flagged results with another run.', '',
                  '| Scene / mode | Metric | Change, ms | Change, % | Assessment |',
                  '| --- | --- | ---: | ---: | --- |']
        for case, metrics in changes.items():
            for metric, change in metrics.items():
                percent = change['delta_percent']
                pct = f'{percent:+.1f}' if percent is not None else 'n/a'
                lines.append(f'| {case} | {metric} | {change["delta_ms"]:+.3f} | {pct} | {change["status"]} |')
    (output / 'summary.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--label', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--executable', type=Path, default=ROOT / 'build/release/orbital.exe')
    parser.add_argument('--baseline', type=Path, action='append', default=[])
    parser.add_argument('--scenes', nargs='+', choices=list(SCENES), default=list(SCENES))
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--frames', type=int, default=360)
    parser.add_argument('--warmup', type=int, default=120)
    parser.add_argument('--width', type=int, default=1600)
    parser.add_argument('--height', type=int, default=900)
    # The window is hidden by default: not composed by the desktop, the GPU frame reads 7 to 12
    # percent lower and steadier. The protocol records it, so a hidden run never compares with
    # a visible baseline (the series before 2026-09-15) and a visible run never with a hidden one.
    parser.add_argument('--visible', action='store_true', help='run with the window shown: the protocol of the baselines before 2026-09-15')
    args = parser.parse_args()
    if args.repeats < 2 or args.warmup < 60 or args.frames - args.warmup < 100:
        parser.error('Use >=2 repeats, >=60 warmup frames and >=100 measured frames')
    if args.width <= 0 or args.height <= 0 or len(set(args.scenes)) != len(args.scenes):
        parser.error('Positive dimensions and unique scenes required')
    if os.name == 'nt':
        running = command_output(['tasklist', '/FI', 'IMAGENAME eq orbital.exe', '/FO', 'CSV', '/NH'])
        if '"orbital.exe"' in running.lower():
            raise RuntimeError('Close running Orbital instances before benchmarking')
    # A full host-visible aperture puts the mapped heaps in system memory: the numbers would be about the
    # machine, not the change (tools/check-bar1.py explains).
    if subprocess.run([sys.executable, str(ROOT / 'tools/check-bar1.py')]).returncode:
        raise RuntimeError('The host-visible aperture is too full to benchmark; free it first')
    baselines = [json.loads(p.read_text(encoding='utf-8')) for p in args.baseline]
    exe = args.executable.resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)  # Never overwrite a recorded run.
    artifacts = output / 'raw'
    artifacts.mkdir()
    nvidia = None
    try:
        nvidia = command_output(['nvidia-smi', '--query-gpu=name,driver_version', '--format=csv,noheader'])
    except (FileNotFoundError, subprocess.CalledProcessError):
        pass
    report = {'suite_version': 1, 'label': args.label,
              'recorded_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'revision': command_output(['git', 'rev-parse', 'HEAD']),
              'source_dirty': bool(command_output(['git', 'status', '--porcelain'])),
              'source_diff_sha256': hashlib.sha256(subprocess.check_output(['git', 'diff', 'HEAD'], cwd=ROOT)).hexdigest(),
              'executable_sha256': sha(exe), 'shaders_sha256': tree_hash(exe.parent / 'shaders', '*.spv'),
              'assets_sha256': tree_hash(exe.parent / 'assets'),
              'machine': {'os': platform.platform(), 'cpu': platform.processor(), 'gpu': None, 'driver_info': nvidia},
              'protocol': {'settings': SETTINGS, 'scenes': {s: SCENES[s] for s in args.scenes},
                           'quality': 'default (no --high)', 'window': [args.width, args.height],
                           'frames': args.frames, 'warmup': args.warmup, 'repeats': args.repeats,
                           'fullscreen_at': 30, **({} if args.visible else {'headless': True})}, 'cases': {}}
    runs = {}
    # Repeat complete rounds to expose clock/temperature drift across the suite.
    for repeat in range(args.repeats):
        for scene in args.scenes:
            for mode in ['window', 'fullscreen']:
                case = f'{scene}/{mode}'
                stem = f'{scene}-{mode}-{repeat+1}'
                csv_path = artifacts / f'{stem}.csv'
                command = [str(exe), *SETTINGS, *SCENES[scene], '--width', str(args.width),
                           '--height', str(args.height), '--frames', str(args.frames), '--benchmark', str(csv_path)]
                if mode == 'fullscreen':
                    command += ['--fullscreen-at', '30']
                if not args.visible:
                    command.append('--headless')
                process = subprocess.run(command, capture_output=True, text=True, timeout=120,
                                         creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                log = process.stdout + process.stderr
                (artifacts / f'{stem}.log').write_text(log, encoding='utf-8')
                process.check_returncode()
                gpu = re.search(r'^GPU: (.+)$', log, re.MULTILINE)
                sizes = re.findall(r'Resizing frame targets \d+x\d+ -> (\d+)x(\d+)', log)
                if not gpu or not sizes or (mode == 'fullscreen' and len(sizes) < 2):
                    raise ValueError(f'{case}: missing GPU/resolution or fullscreen transition')
                resolution = list(map(int, sizes[-1]))
                identity = gpu[1].strip()
                if report['machine']['gpu'] not in [None, identity]:
                    raise ValueError('GPU changed during suite')
                report['machine']['gpu'] = identity
                if case in report['cases'] and report['cases'][case]['resolution'] != resolution:
                    raise ValueError(f'{case}: resolution changed between repeats')
                with csv_path.open(newline='') as f:
                    reader = csv.DictReader(f)
                    rows = list(reader)
                if len(rows) != args.frames:
                    raise ValueError(f'{case}: incomplete frame capture')
                metrics = summarize(rows, args.warmup, csv_metrics(reader.fieldnames))
                runs.setdefault(case, []).append(metrics)
                report['cases'][case] = {'resolution': resolution, 'metrics': aggregate(runs[case]),
                                         'runs': runs[case]}
                (artifacts / f'{stem}.command.json').write_text(json.dumps(command, indent=2) + '\n')
                print(f'{repeat+1}/{args.repeats} {case}: GPU {metrics["gpu_ms"]["median"]:.3f} ms', flush=True)
        # A checkpoint is useful after failure, but is not a promoted summary.
        (output / 'checkpoint.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    if tree_hash(exe.parent / 'assets') != report['assets_sha256']:
        raise ValueError('Runtime assets changed during benchmarking; rerun after caches settle')
    if sha(exe) != report['executable_sha256'] or tree_hash(exe.parent / 'shaders', '*.spv') != report['shaders_sha256']:
        raise ValueError('Executable/shaders changed during benchmarking; rerun without rebuilding concurrently')
    write_report(report, baselines, output)
    print(f'Report: {output / "summary.md"}')


if __name__ == '__main__':
    main()
