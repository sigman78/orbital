"""Per-bookmark sanity check of the auto exposure: each view's metered request must stay in its band.

Runs the six bookmarks as one shot list in a single process, with the auto
exposure on (no time key, which would freeze it) and the window hidden, reads
the meter's request for each from the run's report and compares it, in stops,
against the band recorded here from the calibrated readings, so a meter change
that drives a view a stop away from where it was reviewed fails loudly. The
captures are kept next to the report for a look. Requires the Windows build.
"""
import argparse
import json
import math
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
# Stops of exposure the meter requests at each bookmark, as reviewed on 2026-09-15
# (histogram meter, key 0.15, highlight bias 0.1), with half a stop of slack each way.
EXPECTED_STOPS = {'earth': 0.0, 'giant': 0.15, 'moon': 1.3, 'mars': 2.35, 'dawn': -1.3, 'belt': -1.8}
SLACK_STOPS = .5


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, default=ROOT / 'build/release/orbital.exe')
    parser.add_argument('--output', type=Path, default=ROOT / '.scratch/exposure-check')
    parser.add_argument('--frames', type=int, default=80)  # five meter cycles of sixteen frames
    parser.add_argument('--width', type=int, default=960)
    parser.add_argument('--height', type=int, default=540)
    parser.add_argument('--show', action='store_true', help='keep the window visible while the shots run')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    shots = output / 'exposure.shots'
    shots.write_text(''.join(f'name={name} bookmark={index} capture={(output / name).as_posix()}.png\n'
                             for index, name in enumerate(EXPECTED_STOPS)), encoding='utf-8')
    report = output / 'report.json'
    command = [str(args.executable), '--shots', str(shots), '--report', str(report), '--frames', str(args.frames),
               '--width', str(args.width), '--height', str(args.height), '--no-hud', '--vsync', '0']
    if not args.show:
        command.append('--headless')
    result = subprocess.run(command, capture_output=True, text=True, timeout=300,
                            creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
    (output / 'run.log').write_text(result.stdout + result.stderr, encoding='utf-8')
    result.check_returncode()
    readings = {shot['name']: shot for shot in json.loads(report.read_text(encoding='utf-8'))['shots']}
    failures = []
    print(f'{"view":8s} {"metered":>9s} {"peak":>8s} {"request":>8s} {"stops":>7s} {"band":>14s}')
    for name, expected in EXPECTED_STOPS.items():
        shot = readings.get(name)
        if not shot or not shot['exposure']['ready'] or not shot['exposure']['automatic']:
            failures.append(f'{name}: no exposure reading in the report')
            continue
        exposure = shot['exposure']
        stops = math.log2(exposure['target'])
        low, high = expected - SLACK_STOPS, expected + SLACK_STOPS
        ok = low <= stops <= high
        print(f'{name:8s} {exposure["metered"]:9.4f} {exposure["peak"]:8.2f} {exposure["target"]:7.2f}x {stops:+7.2f} '
              f'{low:+6.2f} to {high:+5.2f}' + ('' if ok else '  OUT OF BAND'))
        if not ok:
            failures.append(f'{name}: requested {stops:+.2f} stops, expected {expected:+.2f} within {SLACK_STOPS}')
    if failures:
        print('\n'.join(failures), file=sys.stderr)
        sys.exit(1)
    print(f'All views within {SLACK_STOPS} stops of their reviewed exposure; captures and report in {output}')


if __name__ == '__main__':
    main()
