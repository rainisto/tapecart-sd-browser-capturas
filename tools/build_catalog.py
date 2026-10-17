"""Arma catalog.json cruzando los juegos de una SD con la base de GameBase64.

Uso:
  python -I build_catalog.py <raiz_sd> <gb64_games.csv> <catalog.json> [carpeta ...]

- <gb64_games.csv> se obtiene con export_gb64.ps1 a partir de tu copia de GameBase64.
- Si no se indican carpetas, se recorren todas las que tengan juegos (PRG/TAP/TCRT).
- Opcional: un archivo "titulos.csv" en la raiz de la SD con lineas
  "carpeta/archivo;Titulo completo" para dar el titulo de archivos con nombre corto.
"""
import csv, json, os, re, sys

GAME_EXT = ('.prg', '.p00', '.tap', '.tcrt', '.d64', '.g64', '.t64')


def norm(name):
    n = re.sub(r'\s*[\(\[].*$', '', name)            # quita (anio)(editor)[flags] estilo TOSEC
    n = re.sub(r'-\[ex\]$', '', n)
    n = re.sub(r', (The|A|An)$', '', n, flags=re.I)
    n = re.sub(r'^(The|A|An) ', '', n, flags=re.I)
    n = n.replace('&', 'and')
    return re.sub(r'[^a-z0-9]', '', n.lower())


def clean_title(stem):
    t = re.split(r'\s*[\(\[]', re.sub(r'-\[ex\]$', '', stem))[0].strip()
    return t.replace('_', ' ') or stem


def main():
    sd, gbcsv, out = sys.argv[1:4]
    only = sys.argv[4:]
    gb = {}
    for r in csv.DictReader(open(gbcsv, encoding='utf-8-sig')):
        gb.setdefault(norm(r['Name']), []).append(r)

    titles = {}
    tpath = os.path.join(sd, 'titulos.csv')
    if os.path.exists(tpath):
        for line in open(tpath, encoding='utf-8'):
            if ';' in line:
                parts = line.rstrip('\n').split(';')
                if len(parts) >= 2:
                    k = parts[0].strip().replace('\\', '/').lower()
                    v = parts[1].strip()
                    titles[k] = v

    cat = []
    for dp, dn, fn in os.walk(sd):
        dn[:] = [d for d in dn if not d.startswith(('.', '$')) and d.lower() != 'system volume information']
        folder = os.path.relpath(dp, sd).replace('\\', '/')
        if folder == '.' or (only and folder not in only):
            continue
        for f in sorted(fn):
            if not f.lower().endswith(GAME_EXT):
                continue
            stem = os.path.splitext(f)[0]
            title = titles.get(f'{folder}/{f}'.lower()) or clean_title(stem)
            year = (re.search(r'\((19\d\d|20\d\d)\)', stem) or [None, ''])[1]
            cands = gb.get(norm(title), [])
            r = sorted(cands, key=lambda r: (r['Year'] != year, r['CloneOf'] not in ('', '0')))[0] if cands else {}
            cat.append(dict(folder=folder, short=f, title=title,
                            year=year or (r.get('Year', '') if r.get('Year', '').isdigit() else ''),
                            publisher=r.get('Publisher', ''), genre=r.get('ParentGenre', ''),
                            subgenre=r.get('Genre', ''), rating=r.get('Rating', ''),
                            shot=r.get('ScrnshotFilename', ''), gbid=r.get('GA_Id', ''), source=''))
    json.dump(cat, open(out, 'w'), indent=0)
    hits = sum(1 for c in cat if c['gbid'])
    print(f'{len(cat)} archivos, {hits} encontrados en GameBase64')


if __name__ == '__main__':
    main()
