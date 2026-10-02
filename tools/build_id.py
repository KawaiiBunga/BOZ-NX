from pathlib import Path
import argparse
import hashlib
import re
r=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description='Generate version, source ID and build variant.')
parser.add_argument('--split-diagnostics', type=int, choices=(0, 1), default=0)
args=parser.parse_args()
version=(r/'VERSION').read_text().strip()
if not re.fullmatch(r'[0-9]+\.[0-9]+\.[0-9]+', version):
    parser.error('VERSION must contain a release version such as 0.5.0')
h=hashlib.sha256()
h.update(version.encode())
h.update(str(args.split_diagnostics).encode())
for p in sorted((r/'source').iterdir()):
    if p.suffix in ('.c','.cpp','.h','.S') and p.name!='boz_build_id.h':
        h.update(p.name.encode()); h.update(p.read_bytes().replace(b'\r\n', b'\n'))
text=('#define BOZ_VERSION "'+version+'"\n'
      '#define BOZ_SPLIT_DIAGNOSTICS '+str(args.split_diagnostics)+'\n'
      '#define BOZ_BUILD_ID "'+h.hexdigest()[:12]+'"\n')
p=r/'source/boz_build_id.h'
if not p.exists() or p.read_text()!=text: p.write_text(text,newline='\n')
