# Tapecart SD Browser con capturas / with screenshots

**[Español](#español)** · **[English](#english)**

| Listado / List | Captura / Screenshot (SPACE) | Créditos / Credits (`.`) |
|---|---|---|
| ![listado](docs/img/listado.png) | ![captura](docs/img/captura.png) | ![creditos](docs/img/creditos.png) |

*(Imágenes tomadas en el emulador VICE / Captured in the VICE emulator.)*

---

## Español

Browser para el **Tapecart SD** de la Commodore 64 que muestra el **nombre completo, año, género y editor**
de cada juego y su **captura de pantalla**, directamente en la C64. Incluye además un
**[catálogo para el celular](https://diguardia.github.io/tapecart-sd-browser-capturas/)** para elegir los juegos.

Es un **fork de [Tapecart SD Browser v1.4-TAP](https://github.com/rainisto/tapecart/tree/new_tap_with_1.4)**
(Jonni Rainisto, con el soporte de TAP de Jarkko Sonninen), que a su vez deriva de [Tapecart SD](https://github.com/KimJorgensen/tapecart) de Kim Jørgensen,
de [tapecart](https://github.com/ikorb/tapecart) de Ingo Korb y de Draco Browser de Sascha Bader.
Ver [CREDITOS.md](CREDITOS.md).

Fork por **Christian Di Guardia** (Argentina), 2026.

### Qué agrega al browser original

- En carpetas con `index.db`: nombre completo (hasta 30 caracteres; el firmware solo muestra 16), año, género y editor.
- En carpetas con `shots.db`: **ESPACIO** muestra la captura del juego (bitmap hires o multicolor). RETURN desde la captura carga el juego.
- Carpetas sin índice: funciona como el original, pero **ordena alfabéticamente** (el original muestra el orden físico de la SD).
- Oculta carpetas de sistema (`System Volume Information`, `$RECYCLE.BIN`, archivos de macOS).
- Carga PRG, P00, TCRT y TAP igual que el original. **No hace falta cambiar el firmware.**

### Requisitos

- Tapecart SD con firmware **v1.4-TAP** ([rainisto/tapecart, rama `new_tap_with_1.4`](https://github.com/rainisto/tapecart/tree/new_tap_with_1.4)).
  Debería andar también con el firmware v1.4 de Kim Jørgensen, salvo la carga de TAP. *Sin probar.*
- Para compilar: [cc65](https://cc65.github.io).
- Para generar índices y capturas: Python 3, tu propia copia de [GameBase64](https://www.gb64.com) y,
  para exportar su base, Windows con el driver ODBC de Access e [innoextract](https://constexpr.org/innoextract/).

### Instalación

1. Hacé una copia de tu `browser.prg` original.
2. Copiá **`bin/browser_es.prg`** (español) o **`bin/browser_en.prg`** (inglés) a la raíz de la SD y
   renombralo a **`browser.prg`**: el firmware siempre carga `/browser.prg`.
3. Sin índices, ya funciona: listado ordenado alfabéticamente.

### Generar índices y capturas para tu colección

Los datos y capturas de GameBase64 son **solo para uso personal** (ver *Licencias*). Por eso este proyecto
no incluye ningún índice: cada usuario los genera con su propia copia.

1. Bajá de [gb64.com/downloads.php](https://www.gb64.com/downloads.php) la *GB64 Database* y las *GB64 Screenshots*
   (dos archivos `.7z`) y dejalos en una carpeta, por ejemplo `C:\Descargas\gb64`. No hace falta descomprimirlos.
2. Instalá [innoextract](https://constexpr.org/innoextract/) (o dejá `innoextract.exe` en la carpeta `tools`).
3. Con la SD conectada (por ejemplo en `E:\`), corré **un solo comando**:

   `python -I tools\generar_sd.py --sd E:\ --gb64 C:\Descargas\gb64`

   Descomprime lo necesario (sin ejecutar el instalador de GameBase64), busca cada juego de tu SD,
   genera `index.db` y `shots.db` en cada carpeta y los copia a la SD. La primera vez tarda unos minutos.
   Opciones: `--no-copiar` (solo genera, en `.\trabajo\out`) y `--catalogo catalogo-c64.json` (además arma
   el archivo para el catálogo del celular).

- Si tus archivos tienen nombres cortos, podés indicar el título real en `E:\titulos.csv`
  (líneas `carpeta/archivo;Título`).
- Requiere Windows con el driver ODBC de Microsoft Access (viene con Office o con *Access Database Engine*).
- Los pasos por separado (`export_gb64.ps1`, `build_catalog.py`, `gen_db.py`) siguen disponibles en `tools/`.

Consejos:
- Sin índice, el firmware sigue limitado a **16 caracteres** y muestra nombres largos en formato 8.3 (`LASTNI~1.TAP`).
- Con `index.db`, el browser puede mostrar nombres largos (hasta 30) para archivos y carpetas, aunque internamente siga entrando por el nombre corto/8.3.
- `generar_sd.py` también crea índices en carpetas padre (por ejemplo `c64games` y `c64games/a`) para que los listados intermedios muestren nombres largos.
- Al copiar a la SD, `generar_sd.py` compara el contenido y saltea `index.db`/`shots.db` sin cambios para reducir escrituras innecesarias.
- Si agregás juegos a una carpeta con índice, el browser lo detecta y muestra el listado normal hasta que regeneres el índice.
- El firmware **no carga D64/G64/T64**.

### Catálogo para el celular

**<https://diguardia.github.io/tapecart-sd-browser-capturas/>**: búsqueda, índice A-Z, géneros, favoritos,
captura ampliada y la ruta exacta de cada juego en la SD.

La página no trae datos: la primera vez elegís tu archivo `catalogo-c64.json` (por ejemplo, desde Google Drive)
y queda guardado solo en ese teléfono. El archivo se genera con:

`python -I tools\catalogo_html.py --pack catalog.json Screenshots catalogo-c64.json`

No publiques ese archivo: contiene capturas y datos de GameBase64, que son de uso personal.

*Alternativa sin internet:* `tools\catalogo_html.py catalog.json Screenshots catalogo` y después
`tools\servir_catalogo.py catalogo` lo sirve en la red de casa.

### ¿PAL, PAL-N o NTSC?

Muchos juegos europeos (PAL) fallan en máquinas NTSC o PAL-N (Drean, Argentina): parpadeos, gráficos rotos o cuelgues.
`bin/norma.prg` muestra la norma de video de tu máquina (fuente en `norma/norma.c`).

### Teclas

| Tecla | Acción |
|---|---|
| RETURN / disparo | jugar / abrir carpeta |
| ESPACIO | ver captura |
| DEL | carpeta anterior |
| HOME | carpeta raíz |
| Cursor ↑↓ | mover |
| Cursor ←→ | página anterior / siguiente |
| T / B | principio / final |
| `.` | créditos y ayuda |
| Q | salir |

### Compilar

```
build.bat      (Windows)
./build.sh     (Linux / macOS)
```

Genera `bin/browser_es.prg`, `bin/browser_en.prg` (con `-DLANG_EN`; los textos están en `browser/lang.h`) y `bin/norma.prg`.

El browser se puede probar en VICE con `browser/mock_tapecartif.c`, que simula el tapecart leyendo
`dirlist`, `index.db` y `shots.db` desde una carpeta del PC montada como unidad 8.

### Estado

- Probado en VICE (listado, capturas, orden y créditos) y en uso en una C128 en modo 64 con Tapecart SD.
- Formato de los archivos: [docs/FORMATO.md](docs/FORMATO.md).

### Licencias

- **Código derivado de Draco Browser:** se puede usar libremente conservando el aviso del origen y del autor.
- **Código derivado de tapecart (Ingo Korb):** licencia BSD de 2 cláusulas, conservada en cada archivo.
- **Cambios de este fork:** mismas condiciones, conservando estos avisos.
- **GameBase64:** sus datos y capturas son © GB64 Team, para **uso personal**. Este proyecto **no los incluye ni redistribuye**.
  Las herramientas leen la copia que cada usuario baja de [gb64.com](https://www.gb64.com).
- **Juegos:** este proyecto no incluye ni enlaza juegos.

SIN GARANTÍA. ÚSELO BAJO SU PROPIO RIESGO.

---

## English

A browser for the Commodore 64 **Tapecart SD** that shows each game's **full name, year, genre and publisher**
and its **screenshot**, right on the C64. It also comes with a
**[phone catalog](https://diguardia.github.io/tapecart-sd-browser-capturas/)** to pick games from.

It is a **fork of [Tapecart SD Browser v1.4-TAP](https://github.com/rainisto/tapecart/tree/new_tap_with_1.4)**
(Jonni Rainisto, with TAP support by Jarkko Sonninen), itself derived from [Tapecart SD](https://github.com/KimJorgensen/tapecart)
by Kim Jørgensen, [tapecart](https://github.com/ikorb/tapecart) by Ingo Korb and Draco Browser by Sascha Bader.
See [CREDITOS.md](CREDITOS.md).

Fork by **Christian Di Guardia** (Argentina), 2026. The browser comes in English and Spanish.

### What it adds

- Folders with an `index.db`: full name (up to 30 characters; the firmware only shows 16), year, genre and publisher.
- Folders with a `shots.db`: **SPACE** shows the game's screenshot (hires or multicolor bitmap); RETURN from there loads the game.
- Folders without an index work like the original, but are **sorted alphabetically** (the original shows the physical SD order).
- Hides system folders (`System Volume Information`, `$RECYCLE.BIN`, macOS files).
- Loads PRG, P00, TCRT and TAP like the original. **No firmware change needed.**

### Requirements

- Tapecart SD with firmware **v1.4-TAP** ([rainisto/tapecart, branch `new_tap_with_1.4`](https://github.com/rainisto/tapecart/tree/new_tap_with_1.4)).
  It should also work with Kim Jørgensen's v1.4 firmware, except TAP loading. *Untested.*
- To build: [cc65](https://cc65.github.io).
- To generate indexes and screenshots: Python 3, your own copy of [GameBase64](https://www.gb64.com) and,
  to export its database, Windows with the Access ODBC driver and [innoextract](https://constexpr.org/innoextract/).

### Install

1. Back up your original `browser.prg`.
2. Copy **`bin/browser_en.prg`** (English) or **`bin/browser_es.prg`** (Spanish) to the SD root and
   rename it to **`browser.prg`**: the firmware always loads `/browser.prg`.
3. Without indexes it already works, with an alphabetically sorted listing.

### Generating indexes and screenshots for your collection

GameBase64 data and screenshots are **for personal use only** (see *Licenses*), so this project ships no
index files: every user builds their own from their own copy.

1. Download the *GB64 Database* and *GB64 Screenshots* from [gb64.com/downloads.php](https://www.gb64.com/downloads.php)
   (two `.7z` files) into one folder, e.g. `C:\Downloads\gb64`. No need to extract them.
2. Install [innoextract](https://constexpr.org/innoextract/) (or drop `innoextract.exe` into the `tools` folder).
3. With the SD card connected (e.g. as `E:\`), run **a single command**:

   `python -I tools\generar_sd.py --sd E:\ --gb64 C:\Downloads\gb64`

   It extracts what it needs (without running the GameBase64 installer), matches every game on your SD,
   builds `index.db` and `shots.db` for each folder and copies them to the SD. The first run takes a few minutes.
   Options: `--no-copiar` (build only, into `.\trabajo\out`) and `--catalogo catalogo-c64.json` (also build
   the phone catalog file).

- For files with short names, put the real title in `E:\titulos.csv` (lines `folder/file;Title`).
- Requires Windows with the Microsoft Access ODBC driver (comes with Office or the *Access Database Engine*).
- The individual steps (`export_gb64.ps1`, `build_catalog.py`, `gen_db.py`) are still available in `tools/`.

Tips:
- Without an index, firmware is still limited to **16 characters** and shows long names as 8.3 (`LASTNI~1.TAP`).
- With `index.db`, the browser can show long names (up to 30) for files and folders, while still entering them through the short/8.3 name internally.
- `generar_sd.py` also creates indexes in parent folders (for example `c64games` and `c64games/a`) so intermediate listings can show long names.
- When copying to the SD, `generar_sd.py` compares file contents and skips unchanged `index.db`/`shots.db` files to reduce unnecessary writes.
- If you add games to an indexed folder, the browser notices and shows the plain listing until you regenerate the index.
- The firmware **does not load D64/G64/T64**.

### Phone catalog

**<https://diguardia.github.io/tapecart-sd-browser-capturas/>**: search, A-Z index, genres, favorites,
enlarged screenshots and the exact SD path of every game.

The page ships no data: the first time you pick your `catalogo-c64.json` file (e.g. from Google Drive),
and it is stored only on that phone. Build the file with:

`python -I tools\catalogo_html.py --pack catalog.json Screenshots catalogo-c64.json`

Don't publish that file: it contains GameBase64 screenshots and data, which are for personal use.

*Offline alternative:* `tools\catalogo_html.py catalog.json Screenshots catalogo`, then
`tools\servir_catalogo.py catalogo` serves it on your home network.

### PAL, PAL-N or NTSC?

Many European (PAL) games glitch on NTSC or PAL-N machines (Drean, Argentina): flicker, broken graphics or crashes.
`bin/norma.prg` shows your machine's video standard (source in `norma/norma.c`).

### Keys

| Key | Action |
|---|---|
| RETURN / fire | play / open folder |
| SPACE | show screenshot |
| DEL | parent folder |
| HOME | root folder |
| Cursor ↑↓ | move |
| Cursor ←→ | previous / next page |
| T / B | top / bottom |
| `.` | credits and help |
| Q | quit |

### Build

```
build.bat      (Windows)
./build.sh     (Linux / macOS)
```

It builds `bin/browser_es.prg`, `bin/browser_en.prg` (with `-DLANG_EN`; texts live in `browser/lang.h`) and `bin/norma.prg`.

The browser can be tested in VICE with `browser/mock_tapecartif.c`, which fakes the tapecart by reading
`dirlist`, `index.db` and `shots.db` from a PC folder mounted as drive 8.

### Status

- Tested in VICE (listing, screenshots, sorting and credits) and in use on a C128 in C64 mode with a Tapecart SD.
- File formats: [docs/FORMATO.md](docs/FORMATO.md) (Spanish).

### Licenses

- **Code derived from Draco Browser:** free to use as long as the notice of origin and author is kept.
- **Code derived from tapecart (Ingo Korb):** 2-clause BSD license, kept in each file.
- **Changes in this fork:** same terms, keeping these notices.
- **GameBase64:** data and screenshots © GB64 Team, **personal use only**. This project **does not include or redistribute them**.
  The tools read each user's own copy from [gb64.com](https://www.gb64.com).
- **Games:** this project neither includes nor links to any games.

NO WARRANTY. USE AT YOUR OWN RISK.
