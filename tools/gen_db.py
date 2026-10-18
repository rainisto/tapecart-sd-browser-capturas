"""Genera index.db y shots.db para cada carpeta indexada de la SD.

Uso: python -I gen_db.py <raiz_sd> <catalog.json> <carpeta_capturas_gb64> <salida> [carpeta ...]
Escribe en <salida>/<carpeta>/index.db y shots.db; despues se copian a la misma carpeta de la SD.
Formato de los archivos: docs/FORMATO.md
"""
import json, os, re, struct, sys
from multiprocessing import Pool

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from png2c64 import convert, SLOT

REC = 80
TITLE_LEN = 30
PUB_LEN = 24
TYPES = {'prg': 2, 'p00': 2, 'tcrt': 3, 'tap': 4}
GENRES = {'Arcade': 1, "Shoot'em Up": 2, 'Adventure': 3, 'Brain': 4, 'Educational': 5,
          'Sports': 6, 'Strategy': 7, 'Gambling': 8, 'Board Game': 9, 'Cards': 10,
          'Racing': 11, 'Simulation': 12, 'Miscellaneous': 13}


def petscii(s, n):
    """ASCII -> PETSCII (juego de minusculas), recortado/rellenado a n bytes."""
    out = bytearray()
    for ch in s:
        c = ord(ch)
        if 0x61 <= c <= 0x7A:
            c -= 0x20
        elif 0x41 <= c <= 0x5A:
            c += 0x80
        elif c in (0x5F, 0x60, 0x7B, 0x7C, 0x7D, 0x7E) or c > 0x7E or c < 0x20:
            c = 0x20 if c != 0x5F else 0x2D
        out.append(c)
    return bytes(out[:n]).ljust(n, b'\0')




def display_title(t):
    return t if len(t) <= TITLE_LEN else t[:TITLE_LEN - 1] + '~'


def folder_title(folder):
    """Titulo de la carpeta: "Juegos A-Z/W" -> "Juegos A-Z - W" (sin prefijos numericos "2 ")."""
    parts = [re.sub(r'^\d+ ', '', p) for p in folder.split('/')]
    return ' - '.join(parts[-2:])


def starts_ci(name, prefix):
    return name.lower().startswith(prefix.lower())


def include_in_index(name, is_dir):
    """Replica el filtro del browser clasico para que el indice sea valido."""
    low = name.lower()
    if low in ('index.db', 'shots.db'):
        return False
    if name.startswith('.'):
        return False
    if starts_ci(name, '$recycle'):
        return False
    if is_dir and starts_ci(name, 'system'):
        return False
    return True


if sys.platform == 'win32':
    import ctypes
    _get_short_path = ctypes.windll.kernel32.GetShortPathNameW

    def fat_shortname(path):
        """Alias FAT 8.3 real del archivo (p.ej. EXPLOS~2.PRG), leido del propio FAT.
        Es el mismo nombre que FatFs (y por lo tanto el firmware) ve para ese archivo."""
        buf = ctypes.create_unicode_buffer(260)
        n = _get_short_path(path, buf, 260)
        return os.path.basename(buf.value) if n else ''
else:
    def fat_shortname(path):
        return ''


def firmware_name(sd_root, folder, f):
    """Nombre que se guarda en index.db y que el firmware puede abrir con f_open().

    El firmware usa FatFs compilado con FF_MAX_LFN=16: para nombres largos (LFN de mas
    de 16 caracteres) el nombre largo no cabe en su buffer, asi que lista y abre el
    archivo por su alias FAT 8.3. Guardamos exactamente eso: el nombre real si cabe en
    16 caracteres, o el alias 8.3 en caso contrario. Asi el nombre coincide siempre con
    el que el firmware ve en su propio listado y f_open() lo encuentra."""
    if len(f) <= 16:
        return f
    sfn = fat_shortname(os.path.join(sd_root, folder.replace('/', os.sep), f))
    if sfn and len(sfn) <= 16:
        return sfn
    return f[:16]


def build(folder, entries, shots_dir, out_dir, sd_root):
    names = sorted(os.listdir(os.path.join(sd_root, folder)))
    by_short = {e['short'].lower(): e for e in entries}
    rows = []
    for name in names:
        full = os.path.join(sd_root, folder, name)
        is_dir = os.path.isdir(full)
        is_file = os.path.isfile(full)
        if not (is_dir or is_file):
            continue
        if not include_in_index(name, is_dir):
            continue
        if is_dir:
            title = name
            e = {}
        else:
            e = by_short.get(name.lower(), {})
            title = e.get('title') or os.path.splitext(name)[0]
        rows.append((title.lower(), name.lower(), name, is_dir, e))
    rows.sort(key=lambda r: (r[0], r[1]))

    recs = bytearray()
    shot_jobs = []
    for _, _, name, is_dir, e in rows:
        if is_dir:
            typ = 1
        else:
            ext = name.rsplit('.', 1)[-1].lower() if '.' in name else 'prg'
            typ = TYPES.get(ext, 0xFF)
        name16 = firmware_name(sd_root, folder, name)
        slot = 0xFFFF
        if not is_dir:
            shot = e.get('shot', '')
            if shot:
                png = os.path.join(shots_dir, shot.replace('\\', '/'))
                if os.path.exists(png):
                    slot = len(shot_jobs)
                    shot_jobs.append(png)
        year = (e.get('year') or '')[:4] if not is_dir else ''
        display = e.get('title') or (name if is_dir else os.path.splitext(name)[0])
        rec = struct.pack('<B16s30s4sBBH24sB', typ,
                          name16.encode('ascii', 'replace')[:16].ljust(16, b'\0'),
                          petscii(display_title(display), TITLE_LEN),
                          petscii(year, 4), GENRES.get(e.get('genre', ''), 0), 0, slot,
                          petscii(e.get('publisher', ''), PUB_LEN), 0)
        assert len(rec) == REC
        recs += rec

    hdr = bytearray(REC)
    hdr[0:4] = b'TCIX'
    hdr[4] = 1
    struct.pack_into('<H', hdr, 6, len(rows))
    hdr[8] = 1 if shot_jobs else 0
    struct.pack_into('<H', hdr, 10, len(rows))
    hdr[16:16 + TITLE_LEN] = petscii(folder_title(folder), TITLE_LEN)

    od = os.path.join(out_dir, folder)
    os.makedirs(od, exist_ok=True)
    with open(os.path.join(od, 'index.db'), 'wb') as fh:
        fh.write(hdr + recs)
    return od, shot_jobs


def conv(png):
    try:
        return convert(png)
    except Exception as ex:  # imagen rara: slot negro
        print('ERR', png, ex, flush=True)
        return bytes(SLOT)


def main():
    sd_root, catalog, shots_dir, out_dir = sys.argv[1:5]
    only = sys.argv[5:]  # opcional: limitar a algunas carpetas
    cat = json.load(open(catalog))
    folders = {}
    for e in cat:
        folders.setdefault(e['folder'], []).append(e)
    targets = set(folders)
    for folder in list(folders):
        cur = folder
        while '/' in cur:
            cur = cur.rsplit('/', 1)[0]
            targets.add(cur)
    if only:
        only_norm = [p.replace('\\', '/').strip('/').lower() for p in only]
        filtered = set()
        for f in targets:
            low = f.lower()
            for o in only_norm:
                if low == o or low.startswith(o + '/'):
                    filtered.add(f)
                    break
        targets = filtered
    targets = [f for f in sorted(targets) if os.path.isdir(os.path.join(sd_root, f.replace('/', os.sep)))]
    with Pool() as pool:
        for folder in sorted(targets):
            od, jobs = build(folder, folders.get(folder, []), shots_dir, out_dir, sd_root)
            with open(os.path.join(od, 'shots.db'), 'wb') as fh:
                for slot in pool.imap(conv, jobs, chunksize=8):
                    fh.write(slot)
            if not jobs:
                os.remove(os.path.join(od, 'shots.db'))
            print(f'{folder}: {len(jobs)} capturas', flush=True)


if __name__ == '__main__':
    main()
