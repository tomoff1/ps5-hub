"""Valide le dossier natif, puis produit le ZIP et son empreinte."""
from pathlib import Path
import hashlib
import json
import shutil
import stat
import zipfile

ROOT = Path(__file__).resolve().parent.parent

def package():
    meta = json.loads((ROOT / 'sce_sys/param.json').read_text())
    title = meta['titleId']
    folder = ROOT / 'dist' / title
    for name in ['eboot.bin', 'sce_sys/param.json', 'sce_sys/icon0.png', 'sce_module/libc.prx', 'assets/catalog.tsv']:
        if not (folder / name).is_file():
            raise SystemExit(f'Fichier natif manquant : {name}. Lancez la compilation avant de préparer le ZIP.')
    if (folder / 'eboot.bin').read_bytes()[:4] != bytes.fromhex('4f153d1d'):
        raise SystemExit('eboot.bin ne possède pas la signature FSELF attendue.')
    notices=folder/'licenses'
    notices.mkdir(exist_ok=True)
    for name in ['LICENSE','THIRD_PARTY_NOTICES.md']:
        shutil.copy2(ROOT/name,notices/name)
    for source,name in [(ROOT/'.deps/pacbrew/v0.40.2/sysroot/user/homebrew/share/licenses','pacbrew'),
                        (ROOT/'.deps/ps5-opengl/ps5-opengl-sdk-1.0.2/LICENSES','opengl')]:
        if not source.is_dir():raise SystemExit(f'Notices des dépendances absentes : {source}')
        shutil.copytree(source,notices/name,dirs_exist_ok=True)
    shutil.copy2(ROOT/'docs/INSTALLATION_FR.md',folder/'INSTALLATION_FR.md')
    shutil.copy2(ROOT/'docs/COMPATIBILITE_13_60.md',folder/'COMPATIBILITE_13_60.md')
    archive = ROOT / 'dist' / f'PS5-Hub-{meta["contentVersion"]}-experimental.zip'
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        for path in sorted(folder.rglob('*')):
            if path.is_file():
                info=zipfile.ZipInfo.from_file(path,path.relative_to(folder.parent))
                info.create_system=3
                info.external_attr=(stat.S_IFREG|0o777)<<16
                info.compress_type=zipfile.ZIP_DEFLATED
                z.writestr(info,path.read_bytes())
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    archive.with_suffix('.zip.sha256').write_text(f'{digest}  {archive.name}\n')
    print(archive.name, digest)

if __name__ == '__main__': package()
