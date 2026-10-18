# Formato de `index.db` y `shots.db`

Los dos archivos van dentro de cada carpeta de la SD que se quiera mostrar con datos y capturas.
Los nombres son exactamente `index.db` y `shots.db` (en minúsculas).

## `index.db`

Registros de 80 bytes. El primero es el encabezado, el resto un registro por entrada
(archivo o carpeta),
en el orden en que se muestran.

### Encabezado

| Offset | Bytes | Contenido |
|---|---|---|
| 0 | 4 | `TCIX` (ASCII) |
| 4 | 1 | versión = 1 |
| 6 | 2 | cantidad de registros (little endian) |
| 8 | 1 | 1 si hay `shots.db` |
| 10 | 2 | cantidad de entradas visibles al generar el índice (sin contar `index.db`/`shots.db`) |
| 16 | 30 | título de la carpeta (PETSCII, juego de minúsculas) |

Si la cantidad de archivos de la carpeta ya no coincide con el campo del offset 10
(por ejemplo, porque se agregó un juego), el browser ignora el índice y muestra el listado
normal hasta que se regenere.

### Registro

| Offset | Bytes | Contenido |
|---|---|---|
| 0 | 1 | tipo: 1 = carpeta, 2 = PRG/P00, 3 = TCRT, 4 = TAP, 255 = no cargable |
| 1 | 16 | nombre del archivo en la SD (ASCII, máximo 16 caracteres) |
| 17 | 30 | título para mostrar (PETSCII, juego de minúsculas) |
| 47 | 4 | año (PETSCII) |
| 51 | 1 | género (1 Arcade, 2 Shoot'em up, 3 Aventura, 4 Ingenio, 5 Educativo, 6 Deportes, 7 Estrategia, 8 Azar, 9 Tablero, 10 Cartas, 11 Carreras, 12 Simulador, 13 Varios) |
| 52 | 1 | puntaje (reservado) |
| 53 | 2 | número de captura en `shots.db`, 0xFFFF = sin captura |
| 55 | 24 | editor (PETSCII) |
| 79 | 1 | reservado |

## `shots.db`

Capturas de 10240 bytes cada una, una detrás de otra (captura *n* en el offset *n* × 10240):

| Offset | Bytes | Contenido |
|---|---|---|
| 0 | 1 | modo: 0 = hires, 1 = multicolor |
| 1 | 1 | color de fondo |
| 16 | 8000 | bitmap |
| 8016 | 1000 | memoria de pantalla (colores) |
| 9016 | 1000 | memoria de color (solo multicolor) |

## Por qué funciona con el firmware sin cambios

El browser usa comandos que el firmware Tapecart SD ya tiene:
`CMD_SD_SELECT_FILE` para abrir `index.db` o `shots.db` y `CMD_READ_FLASH_FAST` para leer
un rango de bytes del archivo abierto.

Detalle importante: si se selecciona un archivo que no existe, el firmware remonta la SD
en la siguiente selección y vuelve a la carpeta raíz. Por eso el browser solo abre
`index.db` después de verlo en el listado de la carpeta.
