"""Genera index.db y shots.db para toda la SD en un solo paso (Windows).
Builds index.db and shots.db for the whole SD card in one step (Windows).

Uso / Usage:
  python -I tools\\generar_sd.py --sd E:\\ --gb64 C:\\Descargas\\gb64

  --gb64        carpeta con lo bajado de gb64.com (los .7z tal cual, o ya descomprimidos)
                folder with the files downloaded from gb64.com (the .7z files as-is, or extracted)
  --sd          raiz de la SD / SD card root
  --no-copiar   genera en la carpeta de trabajo pero no copia a la SD / don't copy to the SD
  --catalogo F  ademas genera el archivo para el catalogo del celular / also build the phone catalog file
  --trabajo D   carpeta de trabajo / work folder (default: .\\trabajo)

Necesita / Needs: Python 3, innoextract (en el PATH o junto a este script), el driver ODBC de
Microsoft Access (viene con Office o con "Access Database Engine") y el tar de Windows 10/11.
"""
import argparse, glob, os, shutil, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
CHUNK = 1024 * 1024


def say(es, en):
    print(f'{es}  /  {en}', flush=True)


def run(cmd, **kw):
    r = subprocess.run(cmd, **kw)
    if r.returncode != 0:
        sys.exit(f'Fallo / Failed: {" ".join(map(str, cmd))}')


def find(root, pattern):
    hits = glob.glob(os.path.join(root, '**', pattern), recursive=True)
    return sorted(hits)[-1] if hits else None


def extract(archive, dest):
    os.makedirs(dest, exist_ok=True)
    run(['tar', '-xf', archive, '-C', dest])


def files_equal(src, dst):
    if not os.path.exists(dst):
        return False
    if os.path.getsize(src) != os.path.getsize(dst):
        return False
    with open(src, 'rb') as fs, open(dst, 'rb') as fd:
        while True:
            a = fs.read(CHUNK)
            b = fd.read(CHUNK)
            if a != b:
                return False
            if not a:
                return True


def find_innoextract():
    for c in (shutil.which('innoextract'), os.path.join(HERE, 'innoextract.exe')):
        if c and os.path.exists(c):
            return c
    sys.exit('No encuentro innoextract (https://constexpr.org/innoextract/). Ponelo en el PATH o junto a este script.\n'
             'innoextract not found. Put it in the PATH or next to this script.')


def get_mdb(gb64, work):
    mdb = find(gb64, 'GBC_*.mdb') or find(work, 'GBC_*.mdb')
    if mdb:
        return mdb
    exe = find(gb64, 'gb64v*.exe') or find(work, 'gb64v*.exe')
    if not exe:
        for arc in [a for a in glob.glob(os.path.join(gb64, 'gb64v*')) if a.endswith(('.7z', '.zip')) and not a.lower().endswith('s.7z')]:
            say(f'Descomprimiendo {os.path.basename(arc)}', f'Extracting {os.path.basename(arc)}')
            extract(arc, os.path.join(work, 'gb64'))
        for z in glob.glob(os.path.join(work, 'gb64', '*.zip')):
            extract(z, os.path.join(work, 'gb64'))
        exe = find(work, 'gb64v*.exe')
    if not exe:
        sys.exit('No encuentro la base de GameBase64 (GBC_*.mdb, gb64vXX.exe o gb64vXX.7z) en --gb64.\n'
                 'GameBase64 database not found in --gb64.')
    say('Extrayendo la base del instalador (sin ejecutarlo)', 'Extracting the database from the installer (without running it)')
    run([find_innoextract(), '-s', '-d', os.path.join(work, 'gb64'), exe])
    mdb = find(os.path.join(work, 'gb64'), 'GBC_*.mdb')
    if not mdb:
        sys.exit('El instalador no trajo GBC_*.mdb / No GBC_*.mdb in the installer.')
    return mdb


def get_screenshots(gb64, work):
    for root in (gb64, work):
        for d in glob.glob(os.path.join(root, '**', 'Screenshots'), recursive=True):
            if os.path.isdir(os.path.join(d, 'A')):
                return d
    arc = find(gb64, 'gb64v*s.7z')
    if not arc:
        sys.exit('No encuentro las capturas (carpeta Screenshots o gb64vXXs.7z) en --gb64.\n'
                 'Screenshots not found in --gb64.')
    say('Descomprimiendo las capturas (puede tardar unos minutos)', 'Extracting screenshots (may take a few minutes)')
    extract(arc, os.path.join(work, 'gb64'))
    return get_screenshots(gb64, work)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--sd', required=True)
    ap.add_argument('--gb64', required=True)
    ap.add_argument('--trabajo', default='trabajo')
    ap.add_argument('--no-copiar', action='store_true')
    ap.add_argument('--catalogo')
    a = ap.parse_args()
    work = os.path.abspath(a.trabajo)
    os.makedirs(work, exist_ok=True)
    py = [sys.executable, '-I']

    mdb = get_mdb(a.gb64, work)
    shots = get_screenshots(a.gb64, work)
    csv_path = os.path.join(work, 'games.csv')
    if not os.path.exists(csv_path):
        say('Exportando la base de GameBase64', 'Exporting the GameBase64 database')
        r = subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
                            os.path.join(HERE, 'export_gb64.ps1'), '-Mdb', mdb, '-Out', csv_path])
        # el driver de Access a veces hace caer a PowerShell al salir aunque la exportacion haya
        # terminado bien: se valida el CSV en vez del codigo de salida
        if not (os.path.exists(csv_path) and os.path.getsize(csv_path) > 100000):
            if os.path.exists(csv_path):
                os.remove(csv_path)
            sys.exit(f'Fallo la exportacion de la base / Database export failed (codigo {r.returncode})')

    catalog = os.path.join(work, 'catalog.json')
    say('Leyendo la SD y buscando cada juego', 'Reading the SD and matching games')
    run(py + [os.path.join(HERE, 'build_catalog.py'), a.sd, csv_path, catalog])

    out = os.path.join(work, 'out')
    if os.path.isdir(out):
        shutil.rmtree(out)
    say('Generando indices y capturas', 'Building indexes and screenshots')
    run(py + [os.path.join(HERE, 'gen_db.py'), a.sd, catalog, shots, out])

    if a.catalogo:
        run(py + [os.path.join(HERE, 'catalogo_html.py'), '--pack', catalog, shots, a.catalogo])

    copied = []
    skipped = 0
    for dp, dn, fn in os.walk(out):
        for f in fn:
            src = os.path.join(dp, f)
            rel = os.path.relpath(src, out)
            copied.append(rel)
            if not a.no_copiar:
                dst = os.path.join(a.sd, rel)
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                if files_equal(src, dst):
                    skipped += 1
                    continue
                shutil.copyfile(src, dst)
    if a.no_copiar:
        say(f'Listo. {len(copied)} archivos en {out} (no se copiaron a la SD)',
            f'Done. {len(copied)} files in {out} (not copied to the SD)')
    else:
        written = len(copied) - skipped
        say(f'Listo. Se copiaron {written} archivos y se saltearon {skipped} sin cambios',
            f'Done. Copied {written} files and skipped {skipped} unchanged files')


if __name__ == '__main__':
    main()
