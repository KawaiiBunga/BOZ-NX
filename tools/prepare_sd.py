"""Optional SD staging helper. The Switch performs extraction and indexing."""
from pathlib import Path
import argparse
import shutil


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apk', type=Path, required=True)
    parser.add_argument('--data', type=Path, required=True,
                        help='Your blackops_etc.dz file, or its containing folder')
    parser.add_argument('--output', type=Path, required=True,
                        help='SD root or a staging folder')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    etc = args.data / 'blackops_etc.dz' if args.data.is_dir() else args.data
    launcher = root / 'launcher/codboz.nro'
    for path in (args.apk, etc, launcher):
        if not path.is_file():
            parser.error(f'Missing file: {path}')
    dest = args.output.resolve() / 'switch/boz-native'
    dest.mkdir(parents=True, exist_ok=True)
    for source, name in ((args.apk, 'boz.apk'), (etc, 'blackops_etc.dz'),
                         (launcher, 'codboz.nro')):
        target = dest / name
        if source.resolve() != target.resolve():
            shutil.copy2(source, target)
    print(f'Staged codboz.nro, boz.apk and blackops_etc.dz in {dest}')
    print('Install a sphaira forwarder for codboz.nro, then launch its HOME icon.')
    print('The Switch extracts the APK and builds the file index on first launch.')


if __name__ == '__main__':
    main()
