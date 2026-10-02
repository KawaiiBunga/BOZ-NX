from pathlib import Path
import hashlib
r=Path(__file__).resolve().parents[1]
h=hashlib.sha256()
for p in sorted((r/'source').iterdir()):
    if p.suffix in ('.c','.cpp','.h','.S') and p.name!='boz_build_id.h':
        h.update(p.name.encode()); h.update(p.read_bytes().replace(b'\r\n', b'\n'))
text='#define BOZ_BUILD_ID "'+h.hexdigest()[:12]+'"\n'
p=r/'source/boz_build_id.h'
if not p.exists() or p.read_text()!=text: p.write_text(text,newline='\n')
