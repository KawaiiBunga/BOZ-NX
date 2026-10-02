"""Test the actual portable launcher setup with owner-supplied game inputs."""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import tempfile
from zipfile import ZipFile, ZIP_STORED, ZIP_DEFLATED


def index_entries(path):
    data = path.read_bytes()
    assert data[:4] == b'BOZI'
    version, archives, count = struct.unpack_from('<3I', data, 4)
    assert version == 1
    offset = 16; names = []
    for _ in range(archives):
        length, = struct.unpack_from('<H', data, offset); offset += 2
        names.append(data[offset:offset+length].decode().replace('\\', '/').split('/')[-1]); offset += length
    result = {}
    for _ in range(count):
        archive, start, size, length = struct.unpack_from('<3IH', data, offset); offset += 14
        name = data[offset:offset+length].decode(); offset += length
        result[name] = (names[archive], start, size)
    assert offset == len(data)
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--apk', type=Path, required=True)
    parser.add_argument('--etc', type=Path, required=True)
    parser.add_argument('--index', type=Path)
    args = parser.parse_args()
    binary = Path('test-artifacts/test_setup').resolve()
    def run(root, apk=args.apk, success=True, cancel=None):
        env = dict(os.environ)
        if cancel is not None: env['BOZ_SETUP_CANCEL_AT'] = str(cancel)
        result = subprocess.run([str(binary), str(root), str(apk)], env=env, text=True, capture_output=True)
        assert (result.returncode == 0) == success, (result.returncode, result.stderr)
        return result.stderr

    with tempfile.TemporaryDirectory(prefix='setup-', dir='test-artifacts') as temporary:
        root = Path(temporary)
        etc = root/'blackops_etc.dz'; etc.symlink_to(args.etc.resolve())
        (root/'config.txt').write_text('aim_sensitivity=173\n')
        (root/'save').mkdir(); (root/'save/profile.bin').write_bytes(b'KEEP SAVE')
        run(root)
        assert (root/'boz.s3e.unpacked').stat().st_size == 4550559
        actual = index_entries(root/'boz_files.idx')
        if args.index: assert actual == index_entries(args.index)
        with ZipFile(args.apk) as apk:
            for entry in apk.infolist():
                if entry.filename.startswith(('assets/blackops-music/', 'assets/deadops-music/')) and not entry.is_dir():
                    assert (root/entry.filename[7:]).read_bytes() == apk.read(entry)
        assert (root/'config.txt').read_text() == 'aim_sensitivity=173\n'
        assert (root/'save/profile.bin').read_bytes() == b'KEEP SAVE'
        # A completed install is idempotent and can repair individual files.
        original = (root/'boz.s3e.unpacked').read_bytes()
        run(root); assert (root/'boz.s3e.unpacked').read_bytes() == original
        music = next((root/'blackops-music').glob('*.mp3')); music.unlink()
        (root/'blackops_loader.dz').write_bytes(b'truncated')
        run(root); assert music.exists()
        etc.unlink()
        assert 'Graphics data is separate' in run(root, success=False)
        etc.write_bytes(b'DTRZ\0\0\0\0')
        assert 'Invalid or compressed DTRZ' in run(root, success=False)
        etc.unlink(); etc.symlink_to(args.etc.resolve())
        bad = root/'bad.apk'; bad.write_bytes(b'not a zip')
        assert 'Cannot open' in run(root, bad, success=False)
        # Bad LZMA and APK traversal names must not replace existing code or
        # write outside the selected music folders. Test both ZIP methods.
        with ZipFile(args.apk) as original_zip:
            packed = original_zip.read('assets/boz.s3e')
            loader = original_zip.read('assets/blackops_loader.dz')
        for method in (ZIP_STORED, ZIP_DEFLATED):
            fixture = root/f'fixture-{method}.apk'
            with ZipFile(fixture, 'w', compression=method) as z:
                z.writestr('assets/boz.s3e', packed)
                z.writestr('assets/blackops_loader.dz', loader)
                z.writestr('assets/blackops-music/../../escaped.mp3', b'NO')
            run(root, fixture)
            assert not (root.parent/'escaped.mp3').exists()
        with ZipFile(bad, 'w') as z:
            z.writestr('assets/boz.s3e', b'\xff'*32)
        assert 'compressed game code' in run(root, bad, success=False)
        assert (root/'boz.s3e.unpacked').read_bytes() == original
        interrupted = root/'interrupted'; interrupted.mkdir()
        (interrupted/'blackops_etc.dz').symlink_to(args.etc.resolve())
        run(interrupted, success=False, cancel=100)
        assert not list(interrupted.rglob('*.part'))
        run(interrupted)
    print('PASS: on-device setup code with real APK; stored/deflated ZIP, exact archive index, music, resume, missing/corrupt data, cancellation, traversal rejection, saves/settings preserved')


if __name__ == '__main__': main()
