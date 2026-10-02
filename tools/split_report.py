#!/usr/bin/env python3
"""Summarize complete split_capture.log snapshots; no game assets needed."""
import argparse
from collections import Counter
from pathlib import Path
import re


def summarize(text):
    captures, header, lines, begun = [], None, [], 0
    for line in text.splitlines():
        if line.startswith('BEGIN split-capture '):
            header, lines = line[len('BEGIN split-capture '):], []
            begun += 1
        elif line == 'END split-capture' and header is not None:
            captures.append((header, '\n'.join(lines)))
            header = None
        elif header is not None:
            lines.append(line)
    if not captures:
        raise ValueError('No complete captures found; enable Split-screen diagnostics and capture in the port menu.')
    for index, (header, body) in enumerate(captures, 1):
        print(f'Capture {index}: {header}')
        for line in body.splitlines():
            if line.startswith(('split-probe ', 'render-state=', 'pad ', 'count ', 'inventory ', 'network-trace ')):
                print('  ' + line)
            elif line.startswith('heap '):
                numbers = dict(re.findall(r'([\w-]+)=(\d+)', line))
                if int(numbers.get('high-water', 0)) > int(numbers.get('capacity', 0)):
                    numbers.pop('high-water', None)
                    print('  High-water unavailable: legacy capture recorded an address, not a byte count.')
                print('  Heap: ' + ', '.join(f'{key}={int(value)/1048576:.1f} MiB' for key,value in numbers.items()))
        transports, events, endpoints = Counter(), Counter(), set()
        for line in body.splitlines():
            if not line.startswith('net seq='):
                continue
            fields = dict(re.findall(r'(\w+)=([^ ]+)', line))
            transports[fields['transport']] += 1
            events[fields['op']] += 1
            if fields['peer'] != '0.0.0.0:0':
                endpoints.add(fields['peer'])
        print('  Retained network events:', ', '.join(f'{key}={count}' for key,count in transports.items()) or 'none')
        print('  Operations:', ', '.join(f'{key}={count}' for key,count in events.items()) or 'none')
        print('  Endpoints:', ', '.join(sorted(endpoints)) or 'none')
        print()
    if begun > len(captures):
        print('Incomplete captures were skipped. Check debug.log for capture write failures.')
    print('Controller detection/object counts do not establish two controlled players or working split screen.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    args = parser.parse_args()
    try:
        summarize(args.capture.read_text(encoding='utf-8', errors='replace'))
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
