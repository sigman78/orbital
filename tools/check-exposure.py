"""Per-bookmark sanity check of the auto exposure: each view's metered request must stay in its band.

Runs every bookmark with the auto exposure on (no --time, which would freeze it),
reads the meter's closing log line and compares the requested multiplier, in
stops, against the band recorded here from the calibrated readings, so a meter
change that drives a view a stop away from where it was reviewed fails loudly.
The captures are kept next to the report for a look. Requires the Windows build.
"""
import argparse
import math
import os
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
# Stops of exposure the meter requests at each bookmark, as reviewed on 2026-09-15
# (histogram meter, key 0.15, highlight bias 0.1), with half a stop of slack each way.
EXPECTED_STOPS = {'earth': 0.0, 'giant': 0.15, 'moon': 1.3, 'mars': 2.35, 'dawn': -1.3, 'belt': -1.8}
SLACK_STOPS = .5
LINE = re.compile(r'Exposure: metered ([\d.eE+-]+), peak ([\d.eE+-]+), target ([\d.eE+-]+)x, adapted ([\d.eE+-]+)x')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable', type=Path, default=ROOT / 'build/release/orbital.exe')
    parser.add_argument('--output', type=Path, default=ROOT / '.scratch/exposure-check')
    parser.add_argument('--frames', type=int, default=80)  # five meter cycles of sixteen frames
    parser.add_argument('--width', type=int, default=960)
    parser.add_argument('--height', type=int, default=540)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    failures = []
    print(f'{"view":8s} {"metered":>9s} {"peak":>8s} {"request":>8s} {"stops":>7s} {"band":>14s}')
    for index, (name, expected) in enumerate(EXPECTED_STOPS.items()):
        capture = args.output / f'{name}.png'
        command = [str(args.executable), '--bookmark', str(index), '--frames', str(args.frames),
                   '--width', str(args.width), '--height', str(args.height), '--no-hud', '--vsync', '0',
                   '--capture', str(capture)]
        result = subprocess.run(command, capture_output=True, text=True, timeout=120,
                                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        log = result.stdout + result.stderr
        (args.output / f'{name}.log').write_text(log, encoding='utf-8')
        result.check_returncode()
        match = LINE.search(log)
        if not match:
            failures.append(f'{name}: no exposure reading in the log')
            continue
        metered, peak, target, _adapted = map(float, match.groups())
        stops = math.log2(target)
        low, high = expected - SLACK_STOPS, expected + SLACK_STOPS
        ok = low <= stops <= high
        print(f'{name:8s} {metered:9.4f} {peak:8.2f} {target:7.2f}x {stops:+7.2f} {low:+6.2f} to {high:+5.2f}'
              + ('' if ok else '  OUT OF BAND'))
        if not ok:
            failures.append(f'{name}: requested {stops:+.2f} stops, expected {expected:+.2f} within {SLACK_STOPS}')
    if failures:
        print('\n'.join(failures), file=sys.stderr)
        sys.exit(1)
    print(f'All views within {SLACK_STOPS} stops of their reviewed exposure; captures in {args.output}')


if __name__ == '__main__':
    main()
