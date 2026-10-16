"""Installation FTP d'un dossier compilé, vérifiée et sans écrasement implicite."""
import argparse
import ftplib
import hashlib
import json
import os
from pathlib import Path
import re
import uuid

def inspect(folder):
    folder=Path(folder).resolve()
    meta=json.loads((folder/'sce_sys/param.json').read_text(encoding='utf-8'))
    title=meta['titleId']
    if not re.fullmatch(r'PPSA[0-9]{5}',title):raise ValueError('Title ID invalide')
    for name in ['eboot.bin','sce_sys/icon0.png','sce_module/libc.prx']:
        if not (folder/name).is_file():raise ValueError('Fichier requis absent : '+name)
    if (folder/'eboot.bin').read_bytes()[:4]!=bytes.fromhex('4f153d1d'):raise ValueError('FSELF invalide')
    for path in folder.rglob('*'):
        if path.is_symlink():raise ValueError('Les liens symboliques sont refusés')
    return folder,title

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder');parser.add_argument('--host',required=True)
    parser.add_argument('--port',type=int,default=2121)
    parser.add_argument('--user',default='anonymous')
    parser.add_argument('--yes',action='store_true',help='Confirmer explicitement cette installation')
    args=parser.parse_args()
    folder,title=inspect(args.folder)
    print(f'Destination : {args.host}:{args.port}/data/homebrew/{title}')
    if not args.yes and input('Installer ce dossier ? Tapez OUI : ')!='OUI':return
    with ftplib.FTP() as ftp:
        ftp.connect(args.host,args.port,timeout=30)
        ftp.login(args.user,os.environ.get('PS5_FTP_PASSWORD','anonymous@'))
        ftp.cwd('/data/homebrew')
        existing={name for name,_ in ftp.mlsd()}
        if title in existing:raise SystemExit('Ce titre existe déjà. Sauvegardez-le et utilisez un dossier libre ; aucun écrasement effectué.')
        # Pas de param.json visible pendant le transfert. La promotion termine l'installation.
        stage='.hub-'+uuid.uuid4().hex[:12]
        ftp.mkd(stage);ftp.cwd(stage)
        base=ftp.pwd()
        for path in sorted(folder.rglob('*')):
            rel=path.relative_to(folder).as_posix()
            if path.is_dir():ftp.mkd(base+'/'+rel)
            elif path.is_file():
                remote=rel+'.pending' if rel=='sce_sys/param.json' else rel
                with path.open('rb') as source:ftp.storbinary('STOR '+base+'/'+remote,source)
                expected=hashlib.sha256(path.read_bytes()).digest()
                received=hashlib.sha256();ftp.retrbinary('RETR '+base+'/'+remote,received.update)
                if received.digest()!=expected:raise RuntimeError('Vérification FTP échouée : '+rel)
                print('Vérifié :',rel)
        ftp.cwd('/data/homebrew')
        ftp.rename(stage,title)
        ftp.rename(title+'/sce_sys/param.json.pending',title+'/sce_sys/param.json')
    print('Dossier transféré et vérifié. Attendez son enregistrement par ShadowMountPlus. Aucun lancement automatique.')

if __name__=='__main__':main()
