"""Prépare un instantané traçable du catalogue GPL communautaire, sans télécharger de jeux.

Les liens ZIP restent ceux des développeurs. Aucun binaire n'est réhébergé.
La conversion des icônes se fait sur PC, jamais dans le processus PS5.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import struct
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parent.parent
PIN = 'ab6d9911ac82e56c73b8390131b93a58ce0545ed'
REPO = 'blackbearreloaded/ps5-homebrew-catalog'
SUMMARIES = {
    'PPSA69421': 'Courses de karts libres, mode histoire et courses locales. Portage alpha ; réseau désactivé.',
    'PPSA59322': 'Plateforme : portage en C de Celeste Classic, le jeu original PICO-8.',
    'PPSA28319': 'Ping-pong médiéval contre la console ou à deux en local.',
    'PPSA99006': 'Une collection de jeux de réflexion pour la manette.',
    'PPSA99666': 'DOOM natif avec son épisode shareware. Les autres épisodes demandent vos fichiers de jeu.',
    'PPSA99009': 'Client Lichess natif : jouer aux échecs en ligne à la manette.',
}


def fetch(url, limit=16*1024*1024):
    if not url.startswith('https://'):
        raise ValueError('HTTPS requis')
    spec = importlib.util.spec_from_file_location('library_bot', ROOT/'tools/hub-library-bot.py')
    bot = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(bot)
    req = urllib.request.Request(url, headers={'User-Agent': 'PS5-Hub-Catalog/0.3'})
    with bot.https_open(req, timeout=25) as response:
        data = response.read(limit+1)
    if len(data) > limit:
        raise ValueError('Réponse trop grande')
    return data


def version(value):
    match = re.match(r'^v?(\d{1,3})(?:\.(\d{1,3}))?(?:\.(\d{1,3}))?', value or '')
    return '.'.join(str(int(x or 0)) for x in match.groups()) if match else '0.0.0'


def clean(value, maximum):
    value = ' '.join(str(value or '').split())
    while len(value.encode()) > maximum:
        value = value[:-1]
    return value


def convert(record):
    from PIL import Image, ImageDraw, ImageOps
    identity = record['titleid'].lower()
    path = ROOT/'assets/covers'/f'{identity}.hubart'
    if path.is_file():
        return identity, 'cache'
    image = Image.new('RGB', (192, 288), '#132137')
    status = 'original'
    try:
        icon = Image.open(io.BytesIO(fetch(record['icon_url'], 8*1024*1024)))
        if icon.width*icon.height > 24_000_000:
            raise ValueError('Dimensions excessives')
        icon = ImageOps.contain(icon.convert('RGBA'), (192, 240))
        image.paste(icon, ((192-icon.width)//2, (240-icon.height)//2), icon)
        status = 'icône du développeur'
    except Exception:
        # Une panne d'icône ne transforme pas une fiche en faux téléchargement.
        draw = ImageDraw.Draw(image)
        seed = hashlib.sha256(identity.encode()).digest()
        for i in range(8):
            draw.line((i*35-70, 0, i*35+100, 240), fill=(45+seed[0]//4, 80+seed[1]//3, 100+seed[2]//3), width=12)
    draw = ImageDraw.Draw(image)
    draw.rectangle((0, 240, 192, 288), fill='#101b2d')
    draw.text((12, 258), record['titleid'], fill='#a6e5d7')
    rgba = image.convert('RGBA')
    path.write_bytes(b'HUBA'+struct.pack('<II', 192, 288)+rgba.tobytes())
    return identity, status


def build(commit):
    if not re.fullmatch('[a-f0-9]{40}', commit):
        raise ValueError('Un commit Git exact est requis')
    archive = zipfile.ZipFile(io.BytesIO(fetch(f'https://codeload.github.com/{REPO}/zip/{commit}')))
    records = []
    for name in archive.namelist():
        parts = name.split('/')
        if len(parts) == 3 and parts[1] == 'apps' and parts[2].endswith('.json'):
            record = json.loads(archive.read(name))
            if record.get('source_repo') and record.get('artifact_url'):
                records.append(record)
    if not records:
        raise ValueError('Catalogue vide, aucun fichier remplacé')
    priorities = ['PPSA69421', 'PPSA59322', 'PPSA28319', 'PPSA99006', 'PPSA99666', 'PPSA99009']
    records.sort(key=lambda r: (priorities.index(r['titleid']) if r['titleid'] in priorities else 100,
                               r['kind'] != 'game', r['name'].casefold()))
    lines = ['# PS5 Hub catalogue partagé v1', f'# Source GPL-3.0-or-later : {REPO} @ {commit}']
    for record in records:
        artifact = record['artifact_url']
        checksum = record.get('sha256') or ''
        downloadable = artifact.lower().endswith('.zip') and bool(re.fullmatch('[a-f0-9]{64}', checksum))
        kind = 'game' if record['kind'] == 'game' else ('homebrew' if downloadable else 'link')
        category = 'Jeu / portage PS5' if kind == 'game' else ('Outil PS5' if record['kind'] == 'tool' else 'Application PS5')
        summary = SUMMARIES.get(record['titleid'], category + '.')
        detail = f"{summary} {record['author']} · {record['license']}. Version auteur : {record['version']}. 13.60 non vérifié."
        if kind == 'game':
            detail += ' Données requises : voir le projet.'
        row = [record['titleid'].lower(), clean(record['name'], 120), version(record['version']), kind,
               artifact if downloadable else record['source_repo'], checksum if downloadable else '', clean(detail, 400)]
        lines.append('\t'.join(row))
    # Conserver les deux liens de développement et l'archive connue de l'alpha.
    old = (ROOT/'assets/catalog.tsv').read_text(encoding='utf-8')
    lines.extend(line for line in old.splitlines() if line.startswith(('ui-lab\t', 'native-base\t')))
    (ROOT/'assets/covers').mkdir(exist_ok=True)
    with ThreadPoolExecutor(max_workers=6) as pool:
        covers = dict(pool.map(convert, records))
    text = '\n'.join(lines)+'\n'
    (ROOT/'shared').mkdir(exist_ok=True)
    (ROOT/'assets/catalog.tsv').write_text(text, encoding='utf-8', newline='\n')
    (ROOT/'shared/catalog.tsv').write_text(text, encoding='utf-8', newline='\n')
    provenance = {'source': f'https://github.com/{REPO}', 'commit': commit,
                  'license': 'GPL-3.0-or-later', 'records': records, 'covers': covers}
    (ROOT/'assets/catalog-provenance.json').write_text(json.dumps(provenance, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(f"{len(records)} projets publiés, dont {sum(r['kind']=='game' for r in records)} jeux/portages.")
    print('Icônes originales de remplacement :', sum(v == 'original' for v in covers.values()))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--commit', default=PIN)
    build(parser.parse_args().commit)
