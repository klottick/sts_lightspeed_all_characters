#!/usr/bin/env python3
"""Run seven alternating trials against two already-built identical workloads."""
import argparse
import json
import statistics
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', required=True)
    parser.add_argument('--current', required=True)
    parser.add_argument('--repetitions', type=int, default=5000)
    args = parser.parse_args()
    runs = {'baseline': [], 'current': []}
    for trial in range(7):
        order = ('baseline', 'current') if trial % 2 == 0 else ('current', 'baseline')
        for name in order:
            result = subprocess.run([getattr(args, name), str(args.repetitions)], check=True, capture_output=True, text=True)
            runs[name].append(json.loads(result.stdout))
    state_equal = len({r['checksum'] for group in runs.values() for r in group}) == 1
    allocations_equal = len({r['allocations'] for group in runs.values() for r in group}) == 1
    medians = {name: statistics.median(r['seconds'] for r in group) for name, group in runs.items()}
    print(json.dumps({'runs': runs, 'medianSeconds': medians,
                      'medianSlowdownPercent': (medians['current'] / medians['baseline'] - 1) * 100,
                      'stateEqual': state_equal, 'allocationsEqual': allocations_equal}, indent=2))
    return 0 if state_equal and allocations_equal else 1


if __name__ == '__main__':
    raise SystemExit(main())
