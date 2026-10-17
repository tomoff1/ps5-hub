"""PS5 Hub : import en masse, jaquettes et transfert de votre bibliothèque personnelle."""
import argparse
import csv
import difflib
import ftplib
import hashlib
import ipaddress
import json
import os
from pathlib import Path
import re
import shutil
import struct
import time
import unicodedata
import urllib.parse
import urllib.request
import urllib.error

CHUNK=1024*1024
MAX_ENTRIES=5000

class HTTPSRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,request,fp,code,message,headers,newurl):
        if urllib.parse.urlsplit(newurl).scheme!='https':raise ValueError('Redirection non HTTPS refusée.')
        return super().redirect_request(request,fp,code,message,headers,newurl)

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self,*args,**kwargs):raise ValueError('Redirection inattendue du service HyperDPI.')

def https_open(request,timeout):return urllib.request.build_opener(HTTPSRedirect()).open(request,timeout=timeout)

def atomic(path,data):
    path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
    temp=path.with_name(path.name+'.tmp');temp.write_bytes(data);os.replace(temp,path)

def digest_file(path):
    result=hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda:source.read(CHUNK),b''):result.update(block)
    return result.hexdigest()

def safe_url(url):
    value=urllib.parse.urlsplit(url)
    if value.scheme!='https' or not value.hostname or value.username or value.password or any(ord(c)<33 for c in url):
        raise ValueError('Un lien HTTPS direct sans identifiant est requis.')
    return url

def clean(value,limit):
    value=str(value or '').strip()
    if len(value.encode('utf-8'))>limit or any(ord(c)<32 or ord(c)==127 for c in value):raise ValueError('Champ trop long ou caractère de contrôle.')
    return value

def title_from_url(url):
    name=urllib.parse.unquote(Path(urllib.parse.urlsplit(url).path).name)
    name=re.sub(r'\.(pkg|zip)$','',name,flags=re.I)
    name=re.sub(r'[_+]+',' ',name)
    return clean(name or 'Contenu personnel',120)

def load_rows(source):
    source=Path(source)
    if source.stat().st_size>16*1024*1024:raise ValueError('Liste limitée à 16 Mio.')
    text=source.read_text(encoding='utf-8-sig')
    if not text.strip():return []
    if source.suffix.lower()=='.json':
        data=json.loads(text);rows=data.get('games',[]) if isinstance(data,dict) else data
        if not isinstance(rows,list):raise ValueError('Le JSON doit contenir une liste.')
        return rows
    if source.suffix.lower() in ('.csv','.tsv'):
        delimiter='\t' if source.suffix.lower()=='.tsv' else (';' if ';' in text.splitlines()[0] else ',')
        return list(csv.DictReader(text.splitlines(),delimiter=delimiter))
    return [{'url':line.strip()} for line in text.splitlines() if line.strip() and not line.lstrip().startswith('#')]

def normalize(row):
    if isinstance(row,str):row={'url':row}
    if not isinstance(row,dict):raise ValueError('Chaque fiche doit être un objet ou un lien.')
    aliases={'nom':'title','titre':'title','lien':'url','jaquette':'cover','empreinte':'sha256','type':'kind'}
    row={aliases.get(k.lower().strip(),k.lower().strip()):v for k,v in row.items() if k}
    url=safe_url(clean(row.get('url',''),2048))
    title=clean(row.get('title') or title_from_url(url),120)
    identity=hashlib.sha256(url.encode()).hexdigest()[:16]
    game_id=clean(row.get('id') or 'jeu-'+identity,64)
    if not re.fullmatch('[a-z0-9-]+',game_id):raise ValueError('Identifiant invalide.')
    checksum=clean(row.get('sha256',''),64).lower()
    if checksum and not re.fullmatch('[a-f0-9]{64}',checksum):raise ValueError('Empreinte SHA-256 invalide.')
    kind=clean(row.get('kind') or 'pkg',16)
    if kind not in ('pkg','homebrew','link','game'):raise ValueError('Type inconnu.')
    version=clean(row.get('version') or '00.000.000',10)
    if not re.fullmatch(r'\d{1,3}\.\d{1,3}\.\d{1,3}',version):raise ValueError('Version invalide.')
    return {'id':game_id,'title':title,'version':version,'kind':kind,'url':url,'sha256':checksum,
            'description':clean(row.get('description') or 'Bibliothèque personnelle · préparé sur PC',400),
            'cover':clean(row.get('cover',''),2048)}

def cover_art(source,destination,title):
    from PIL import Image,ImageDraw,ImageOps
    if source:
        if source.startswith('https://'):
            req=urllib.request.Request(safe_url(source),headers={'User-Agent':'PS5-Hub-Library/0.2'})
            with https_open(req,timeout=20) as response:
                if not response.geturl().startswith('https://'):raise ValueError('Redirection de jaquette non HTTPS.')
                data=response.read(8*1024*1024+1)
            if len(data)>8*1024*1024:raise ValueError('Jaquette limitée à 8 Mio.')
            import io
            image=Image.open(io.BytesIO(data))
        else:image=Image.open(source)
        if image.width*image.height>24_000_000:raise ValueError('Image trop grande.')
        image=ImageOps.fit(image.convert('RGB'),(192,288))
    else:
        # Visuel original de remplacement, clairement distinct d'une jaquette d'éditeur.
        seed=hashlib.sha256(title.encode()).digest();image=Image.new('RGB',(192,288),(10+seed[0]//5,20+seed[1]//5,35+seed[2]//4))
        draw=ImageDraw.Draw(image)
        for i in range(7):
            x=30+i*24;draw.line((x-80,0,x+140,288),fill=(50+seed[3]//4,100+seed[4]//3,140+seed[5]//3),width=10)
        draw.rounded_rectangle((35,96,157,174),radius=22,fill=(20,30,47),outline=(200,244,234),width=3)
        draw.line((57,134,91,134),fill='white',width=6);draw.line((74,117,74,151),fill='white',width=6)
        draw.ellipse((119,119,129,129),fill=(128,237,212));draw.ellipse((133,135,143,145),fill=(128,237,212))
    rgba=image.convert('RGBA');atomic(destination,b'HUBA'+struct.pack('<II',192,288)+rgba.tobytes())

def find_cover(title):
    """Suggestion depuis la boutique officielle Steam ; ne certifie pas l'édition PS5."""
    def words(text):
        value=unicodedata.normalize('NFKD',text).encode('ascii','ignore').decode().lower()
        return set(re.findall(r'[a-z0-9]+',value))-{'ps5','ps4','pkg','edition','version','enhanced','amelioree'}
    query=urllib.parse.urlencode({'term':title,'l':'french','cc':'FR'})
    req=urllib.request.Request('https://store.steampowered.com/api/storesearch/?'+query,headers={'User-Agent':'PS5-Hub-Library/0.2'})
    with urllib.request.urlopen(req,timeout=15) as response:
        data=json.loads(response.read(1024*1024))
    requested=words(title);best=None;score=0
    for item in data.get('items',[]):
        candidate=words(item.get('name',''))
        overlap=len(requested&candidate)/max(len(requested|candidate),1)
        if overlap>score:score=overlap;best=item
    if best is not None and score>=0.75:
        appid=int(best['id'])
        return f'https://shared.akamai.steamstatic.com/store_item_assets/steam/apps/{appid}/library_600x900.jpg',best.get('tiny_image','')
    return '',''

def import_list(source,output,auto_covers=False):
    source=Path(source).resolve();output=Path(output).resolve();rows=load_rows(source)
    if len(rows)>MAX_ENTRIES:raise ValueError('Maximum : 5000 éléments par liste.')
    entries=[];errors=[];seen=set()
    for number,row in enumerate(rows,1):
        try:
            entry=normalize(row)
            if entry['id'] in seen:continue
            seen.add(entry['id'])
            cover=entry['cover'];fallback=''
            if not cover and auto_covers:
                try:
                    cover,fallback=find_cover(entry['title']);entry['cover_source']='Suggestion Steam, édition à vérifier' if cover else ''
                    time.sleep(.3)
                except Exception:errors.append({'ligne':number,'erreur':'Recherche de jaquette indisponible'})
            if cover and not cover.startswith('https://'):cover=str((source.parent/cover).resolve())
            try:cover_art(cover,output/'bundle/covers'/f'{entry["id"]}.hubart',entry['title'])
            except Exception as error:
                errors.append({'ligne':number,'erreur':'Jaquette : '+type(error).__name__})
                try:cover_art(fallback,output/'bundle/covers'/f'{entry["id"]}.hubart',entry['title'])
                except Exception:cover_art('',output/'bundle/covers'/f'{entry["id"]}.hubart',entry['title'])
            entries.append(entry)
        except (ValueError,KeyError,TypeError) as error:errors.append({'ligne':number,'erreur':str(error)})
    if rows and not entries:raise ValueError('Aucun lien valide ; le catalogue précédent est conservé.')
    lines=['# PS5 Hub bibliothèque personnelle v1']
    for entry in entries:lines.append('\t'.join(entry[k] for k in ('id','title','version','kind','url','sha256','description')))
    atomic(output/'bundle/library.tsv',('\n'.join(lines)+'\n').encode())
    atomic(output/'library.json',json.dumps({'version':1,'games':entries},ensure_ascii=False,indent=2).encode())
    atomic(output/'import-report.json',json.dumps({'importes':len(entries),'erreurs':errors},ensure_ascii=False,indent=2).encode())
    print(f'{len(entries)} fiches préparées ; {len(errors)} avertissements. Rapport : {output / "import-report.json"}')
    return entries

def choose(library,identity):
    data=json.loads((Path(library)/'library.json').read_text(encoding='utf-8'))['games']
    matches=[g for g in data if g['id']==identity]
    if len(matches)!=1:raise ValueError('Choisissez un identifiant exact avec la commande list.')
    return matches[0]

def download(entry,library,progress=None):
    destination=Path(library)/'downloads'/f'{entry["id"]}.pkg';destination.parent.mkdir(parents=True,exist_ok=True)
    if destination.is_file() and entry['sha256'] and digest_file(destination)==entry['sha256']:return destination
    local_hash=destination.with_suffix('.pkg.sha256')
    if destination.is_file() and not entry['sha256'] and local_hash.is_file():
        recorded=local_hash.read_text().strip()
        if re.fullmatch('[a-f0-9]{64}',recorded) and digest_file(destination)==recorded:
            print('Cache PC contrôlé avec son empreinte locale ; aucune empreinte de référence fournie.')
            return destination
    part=destination.with_suffix('.pkg.part');checksum=hashlib.sha256();received=0
    request=urllib.request.Request(safe_url(entry['url']),headers={'User-Agent':'PS5-Hub-Library/0.2'})
    try:
        with https_open(request,timeout=60) as response,part.open('wb') as target:
            if not response.geturl().startswith('https://'):raise ValueError('Redirection non HTTPS refusée.')
            length=int(response.headers.get('Content-Length','0'))
            if length and length>shutil.disk_usage(destination.parent).free:raise ValueError('Espace disque PC insuffisant.')
            first=True;last_progress=0
            while True:
                block=response.read(CHUNK)
                if not block:break
                if first and block.lstrip().lower().startswith((b'<!doctype',b'<html',b'<head')):raise ValueError('Ce lien renvoie une page web, pas le fichier PKG direct.')
                first=False;target.write(block);checksum.update(block);received+=len(block)
                if progress and time.monotonic()-last_progress>.5:
                    progress(f'Réception PC : {received//CHUNK} Mio'+(f' / {length//CHUNK} Mio' if length else ''));last_progress=time.monotonic()
                if received>250*1024**3:raise ValueError('Limite de 250 Gio dépassée.')
            if not received or (length and length!=received):raise ValueError('Téléchargement incomplet.')
            target.flush();os.fsync(target.fileno())
        actual=checksum.hexdigest()
        if entry['sha256'] and actual!=entry['sha256']:raise ValueError('SHA-256 incorrect : fichier rejeté.')
        os.replace(part,destination)
        atomic(destination.with_suffix('.pkg.sha256'),(actual+'\n').encode())
        print('Téléchargement PC terminé.', 'SHA-256 comparé.' if entry['sha256'] else 'SHA-256 calculé localement, sans empreinte de référence.')
        return destination
    except BaseException:
        part.unlink(missing_ok=True);raise

def local_host(host):
    address=ipaddress.ip_address(host)
    if address.version!=4 or not (address.is_private or address.is_loopback) or address.is_unspecified:raise ValueError('Adresse IPv4 locale de la PS5 requise.')
    return str(address)

def connect_ftp(host,port=2121):
    ftp=ftplib.FTP();ftp.connect(local_host(host),port,timeout=60)
    ftp.login(os.environ.get('PS5_FTP_USER','anonymous'),os.environ.get('PS5_FTP_PASSWORD','anonymous@'))
    return ftp

def upload_verified(ftp,path,remote):
    temp=remote+'.hubpart'
    with Path(path).open('rb') as source:ftp.storbinary('STOR '+temp,source,blocksize=CHUNK)
    checksum=hashlib.sha256();ftp.retrbinary('RETR '+temp,checksum.update,blocksize=CHUNK)
    if checksum.hexdigest()!=digest_file(path):raise ValueError('Contrôle FTP incorrect : '+Path(path).name)
    # Si le serveur ne remplace pas une destination, on conserve les deux fichiers pour reprise manuelle.
    ftp.rename(temp,remote)

def ensure_ftp_dir(ftp,path):
    try:ftp.cwd(path)
    except ftplib.error_perm:ftp.mkd(path);ftp.cwd(path)

def sync_library(library,host,port):
    bundle=Path(library)/'bundle'
    if not (bundle/'library.tsv').is_file():raise ValueError('Importez une liste avant de synchroniser.')
    with connect_ftp(host,port) as ftp:
        ftp.cwd('/data/homebrew/PPSA99841/assets');base=ftp.pwd()
        ensure_ftp_dir(ftp,'covers')
        # Envoyer uniquement les jaquettes du catalogue actuel, jamais les résidus d'un ancien import.
        for entry in json.loads((Path(library)/'library.json').read_text(encoding='utf-8'))['games']:
            upload_verified(ftp,bundle/'covers'/f'{entry["id"]}.hubart',entry['id']+'.hubart')
        ftp.cwd(base);upload_verified(ftp,bundle/'library.tsv','library.tsv')
    print('Bibliothèque et jaquettes transférées. Dans PS5 Hub 0.2, appuyez sur Carré pour actualiser.')

def api_json(base,path,body=None):
    request=urllib.request.Request(base+path,data=body,headers={'Content-Type':'application/octet-stream'})
    try:
        with urllib.request.build_opener(urllib.request.ProxyHandler({}),NoRedirect()).open(request,timeout=180) as response:raw=response.read(64*1024+1)
    except urllib.error.HTTPError as error:
        raw=error.read(64*1024+1)
        try:detail=json.loads(raw).get('error','HTTP '+str(error.code))
        except (ValueError,AttributeError):detail='HTTP '+str(error.code)
        raise ValueError('HyperDPI a refusé la demande : '+str(detail)) from None
    if len(raw)>64*1024:raise ValueError('Réponse HyperDPI trop grande.')
    data=json.loads(raw)
    if not data.get('ok'):raise ValueError('HyperDPI a refusé la demande : '+str(data.get('error','inconnue')))
    return data

def hyperdpi_upload(file,host,port):
    file=Path(file);total=file.stat().st_size;base=f'http://{local_host(host)}:{port}'
    if total==0:raise ValueError('Le fichier PKG est vide.')
    status=api_json(base,'/ping')
    if status.get('name')!='pkg-server' or status.get('busy'):raise ValueError('HyperDPI absent, inconnu ou occupé ; transfert refusé.')
    name=urllib.parse.quote(file.name,safe='');offset=0
    with file.open('rb') as source:
        while offset<total:
            block=source.read(CHUNK if offset==0 else 8*CHUNK)
            if not block:raise ValueError('Le fichier a changé pendant le transfert.')
            query=f'/install?name={name}&offset={offset}&total={total}'
            if offset==0:query+='&head256='+hashlib.sha256(block).hexdigest()
            api_json(base,query,block);offset+=len(block)
            print(f'HyperDPI : {offset}/{total} octets transmis')
    result=api_json(base,f'/install?name={name}&offset={total}&total={total}&finalize=1',b'')
    print('Demande acceptée par HyperDPI. Confirmez le résultat dans son interface ou sur la console ; ce message ne prouve pas la fin de l’installation.')
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True)
    for name in ('import','watch'):
        p=sub.add_parser(name);p.add_argument('source');p.add_argument('--library',default='.local/library');p.add_argument('--auto-covers',action='store_true',help='Rechercher des suggestions de jaquettes Steam, à vérifier')
        if name=='watch':p.add_argument('--interval',type=int,default=5)
    p=sub.add_parser('list');p.add_argument('--library',default='.local/library')
    p=sub.add_parser('sync');p.add_argument('--library',default='.local/library');p.add_argument('--ps5',required=True);p.add_argument('--port',type=int,default=2121)
    p=sub.add_parser('transfer');p.add_argument('--library',default='.local/library');p.add_argument('--id',required=True);p.add_argument('--ps5',required=True);p.add_argument('--mode',choices=('ftp','hyperdpi'),default='ftp');p.add_argument('--port',type=int);p.add_argument('--yes',action='store_true')
    args=parser.parse_args()
    if args.command=='import':import_list(args.source,args.library,args.auto_covers)
    elif args.command=='watch':
        last=None
        print('Surveillance locale de la liste. Ctrl+C pour arrêter. Aucun téléchargement ni transfert automatique.')
        while True:
            stamp=Path(args.source).stat().st_mtime_ns
            if stamp!=last:
                try:import_list(args.source,args.library,args.auto_covers);last=stamp
                except Exception as error:print('Import refusé :',type(error).__name__)
            time.sleep(max(2,args.interval))
    elif args.command=='list':
        for entry in json.loads((Path(args.library)/'library.json').read_text(encoding='utf-8'))['games']:print(entry['id'],' ',entry['title'])
    elif args.command=='sync':sync_library(args.library,args.ps5,args.port)
    else:
        entry=choose(args.library,args.id);local_host(args.ps5)
        if entry['kind']!='pkg':raise ValueError('Le transfert de jeux attend une fiche de type pkg.')
        verb='transférer le fichier dans /data/ps5-hub/pkg' if args.mode=='ftp' else 'envoyer le fichier à HyperDPI et demander son installation'
        print(f'{entry["title"]} : {verb} sur {args.ps5}.')
        if not args.yes and input('Tapez OUI pour confirmer : ')!='OUI':return
        file=download(entry,args.library)
        if args.mode=='hyperdpi':hyperdpi_upload(file,args.ps5,args.port or 9090)
        else:
            with connect_ftp(args.ps5,args.port or 2121) as ftp:
                ftp.cwd('/data');ensure_ftp_dir(ftp,'ps5-hub');ensure_ftp_dir(ftp,'pkg')
                if file.name in {name for name,_ in ftp.mlsd()}:raise ValueError('Ce fichier existe déjà sur la console ; écrasement refusé.')
                upload_verified(ftp,file,file.name)
            print('PKG transféré et contrôlé. Transfert FTP uniquement : aucune installation demandée.')

if __name__=='__main__':
    try:main()
    except KeyboardInterrupt:print('\nArrêt demandé.')
    except Exception as error:raise SystemExit(str(error) if isinstance(error,ValueError) else 'Opération échouée ('+type(error).__name__+'). Vérifiez le réseau et la source ; aucun succès présumé.')
