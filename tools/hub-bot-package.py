"""Distribue le bot PC et ses sources, sans les listes ni les jeux personnels."""
from pathlib import Path
import hashlib
import json
import zipfile
root=Path(__file__).resolve().parent.parent
version=json.loads((root/'sce_sys/param.json').read_text())['contentVersion']
archive=root/'dist'/f'PS5-Hub-Bot-PC-{version}.zip';archive.parent.mkdir(exist_ok=True)
files=['Bibliotheque-PC.cmd','requirements-bot.txt','LICENSE','docs/BOT_BIBLIOTHEQUE_FR.md',
       'tools/hub-library-bot.py','tools/hub-library-gui.py','examples/library/mes-liens.csv','examples/library/mes-liens.txt']
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as output:
    for file in files:output.write(root/file,file)
digest=hashlib.sha256(archive.read_bytes()).hexdigest()
archive.with_suffix('.zip.sha256').write_text(f'{digest}  {archive.name}\n')
print(archive.name,digest)
