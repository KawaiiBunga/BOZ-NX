#!/usr/bin/env python3
"""Read the native port's hardware logs over anonymous FTP."""
import argparse
from datetime import datetime, timezone
from ftplib import FTP
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', required=True)
    parser.add_argument('--port', type=int, default=5000)
    parser.add_argument('--snapshot', action='store_true', help='Also retrieve the private actor sandbox (about 100 MB)')
    args = parser.parse_args()
    output = (Path(__file__).resolve().parents[1] / 'test-artifacts' / 'hardware' /
              datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
    ftp = FTP()
    ftp.connect(args.host, args.port, timeout=20)
    ftp.login()
    ftp.cwd('/sdmc:/switch/boz-native')
    available = {Path(p).name for p in ftp.nlst()}
    names = ['debug.log', 'crash.log', 'startup_stage.txt', 'title_id.txt', 'config.txt', 'split_capture.log']
    if args.snapshot:
        names.append('split_actor_snapshot.bin')
    for name in names:
        if name not in available:
            print(f'Not present: {name}')
            continue
        output.mkdir(parents=True, exist_ok=True)
        partial = output / (name + '.part')
        with partial.open('wb') as target:
            ftp.retrbinary('RETR ' + name, target.write, blocksize=65536)
        partial.replace(output / name)
        print(f'Saved {output / name}')
    ftp.quit()


if __name__ == '__main__':
    main()
