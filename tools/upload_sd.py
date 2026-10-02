#!/usr/bin/env python3
"""Upload only this port's staged folder; publish the NRO after its assets."""
import argparse
from ftplib import FTP, error_perm
from pathlib import Path
import time
import uuid


def publish(ftp, temporary, name):
    try:
        ftp.rename(temporary, name)
        return
    except error_perm as error:
        # ftpd on Switch refuses to rename over an existing file. Keep the old
        # launcher recoverable until the verified replacement is in place.
        if not str(error).startswith('553') or ftp.size(name) is None:
            raise
    backup = name + '.previous-' + uuid.uuid4().hex[:12]
    ftp.rename(name, backup)
    try:
        ftp.rename(temporary, name)
    except Exception:
        ftp.rename(backup, name)
        raise
    ftp.delete(backup)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', required=True)
    parser.add_argument('--port', type=int, default=5000)
    parser.add_argument('--launcher-only', action='store_true', help='Upload the new NRO while preserving game data and settings')
    parser.add_argument('--source', type=Path,
                        default=Path(__file__).resolve().parents[1] / 'dist/sdmc/switch/boz-native')
    args = parser.parse_args()
    source = args.source.resolve(strict=True)
    files = sorted((p for p in source.rglob('*') if p.is_file()),
                   key=lambda p: (p.suffix == '.nro', p.relative_to(source).as_posix()))
    if not files or not (source / 'codboz.nro').is_file():
        parser.error('Source must be the prepared boz-native folder with its launcher')
    if args.launcher_only:
        files = [source / 'codboz.nro']
    ftp = FTP()
    ftp.connect(args.host, args.port, timeout=60)
    ftp.login()
    ftp.cwd('/sdmc:/switch')
    try:
        ftp.cwd('boz-native')
    except error_perm:
        ftp.mkd('boz-native')
        ftp.cwd('boz-native')
    root = ftp.pwd()
    ftp.voidcmd('TYPE I')
    total = sum(p.stat().st_size for p in files)
    uploaded = 0
    started = time.monotonic()
    last_report = started
    for local in files:
        rel = local.relative_to(source).as_posix()
        ftp.cwd(root)
        for part in Path(rel).parts[:-1]:
            try:
                ftp.cwd(part)
            except error_perm:
                ftp.mkd(part)
                ftp.cwd(part)
        name = local.name
        # Transfer under a temporary name so interrupted transfers are not used.
        temporary = name + '.upload'
        print(f'Uploading {rel} ({local.stat().st_size:,} bytes)', flush=True)

        def progress(block):
            nonlocal uploaded, last_report
            uploaded += len(block)
            now = time.monotonic()
            if now - last_report >= 20:
                print(f'Progress {uploaded:,}/{total:,} bytes; '
                      f'{uploaded / max(now - started, 0.001) / 1048576:.1f} MiB/s', flush=True)
                last_report = now

        with local.open('rb') as stream:
            ftp.storbinary('STOR ' + temporary, stream, blocksize=256 * 1024, callback=progress)
        assert ftp.size(temporary) == local.stat().st_size, f'Size mismatch: {rel}'
        publish(ftp, temporary, name)
    ftp.cwd(root)
    print(f'Uploaded and size-verified {len(files)} files to {root}; {total:,} bytes', flush=True)
    ftp.quit()


if __name__ == '__main__':
    main()
