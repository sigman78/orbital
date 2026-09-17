"""Report the host-visible GPU aperture (BAR1) and fail when too little is free (stdlib only).

The renderer's host-visible heaps come from BAR1: 256 MiB on a GTX 1080 Ti without
resizable BAR, shared by every process on the machine. When it is full the driver backs
new host-visible heaps with system memory silently, so start-up staging and every
per-frame read of the mapped heap cross PCIe; the Vulkan memory budget still reports
room, and only nvidia-smi shows the aperture. Run before benchmarking; the suite runs it.
"""
import argparse
import re
import subprocess
import sys

DEFAULT_MIN_MIB = 150  # the 64 MiB start-up staging plus the 4 MiB mapped heap, with room for other programs


def bar1_mib():
    """Total, used and free BAR1 in MiB, or None when nvidia-smi is unavailable."""
    try:
        text = subprocess.check_output(['nvidia-smi', '-q', '-d', 'MEMORY'], text=True, timeout=20)
    except (FileNotFoundError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
        return None
    section = text.split('BAR1 Memory Usage', 1)
    if len(section) < 2:
        return None
    values = {}
    for key in ('Total', 'Used', 'Free'):
        match = re.search(rf'^\s*{key}\s*:\s*(\d+)\s*MiB', section[1], re.MULTILINE)
        if not match:
            return None
        values[key.lower()] = int(match.group(1))
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--min', type=int, default=DEFAULT_MIN_MIB, metavar='MIB',
                        help=f'free MiB required (default {DEFAULT_MIN_MIB})')
    parser.add_argument('--strict', action='store_true', help='fail when nvidia-smi cannot report BAR1 (other vendors)')
    args = parser.parse_args()
    values = bar1_mib()
    if values is None:
        print('BAR1: not reported (no nvidia-smi or no BAR1 section)')
        return 1 if args.strict else 0
    print(f'BAR1: {values["free"]} MiB free of {values["total"]} ({values["used"]} used)')
    if values['free'] < args.min:
        print(f'Under the {args.min} MiB minimum: host-visible heaps will land in system memory and buffer '
              'reads cross PCIe. Close GPU programs or sign out and back in; the usage is not per process.',
              file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
