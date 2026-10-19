/*
 * Tapecart SD Browser con capturas (v2.0)
 * Fork de Tapecart SD Browser v1.4-TAP, con capturas e indices por Christian Di Guardia (2026).
 *
 * Basado en Tapecart SD Browser v1.4-TAP
 *   (github.com/rainisto/tapecart, rama new_tap_with_1.4)
 * que deriva de Tapecart SD de Kim Jorgensen (github.com/KimJorgensen/tapecart),
 * tapecart (C) 2013-2017 Ingo Korb (github.com/ikorb/tapecart)
 * y Draco Browser V1.0C 8 Bit (27.12.2009), Copyright 2009 by Draco (Sascha Bader).
 *
 * The code can be used freely as long as you retain
 * a notice describing original source and author.
 *
 * THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL,
 * BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!
 *
 * Novedades:
 *  - Si una carpeta tiene INDEX.DB, muestra nombre completo, anio y genero.
 *  - Si ademas tiene SHOTS.DB, ESPACIO muestra la captura del juego.
 *  - Sin INDEX.DB funciona como el browser original.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <conio.h>
#include <cbm.h>
#include <c64.h>
#include <6502.h>
#include <joystick.h>
#include "tapecartif.h"
#include "loader.h"
#include "lang.h"

/* nombre del autor del fork */
#ifndef AUTHOR
#define AUTHOR "Christian Di Guardia"
#endif
#ifdef USE_TK
#include "tk.h"
#endif

/* ---------------------------------------------------------------- tipos */

#define FILE_NONE    0x00
#define FILE_DIR     0x01
#define FILE_PRG     0x02
#define FILE_TCRT    0x03
#define FILE_TAP     0x04
#define FILE_SID     0x05
#define FILE_UNKNOWN 0xFF

#define TITLE_LEN 30
#define PUB_LEN   24
#define REC_SIZE  80
#define LIST_Y    2
#define LIST_H    20
#define MAX_DIR   1000
/* Carpetas sin indice: se ordenan alfabeticamente en el C64 solo si tienen hasta
   SORT_MAX entradas (la raiz, utilidades, etc.: es instantaneo). Las mas grandes se
   muestran en el orden de la SD (FAT), porque ordenarlas en el C64 tarda varios
   segundos; para esas conviene ordenar la tabla de directorios de la SD en el PC
   (p.ej. DriveSort en Windows o FATSort en Linux) o generar su index.db.
   SORT_MAX 0 desactiva el orden por completo. */
#ifndef SORT_MAX
#define SORT_MAX  100
#endif
#if SORT_MAX > 255
#error SORT_MAX debe ser <= 255
#endif

typedef struct {
    uint8_t  type;
    char     name[FILENAME_LENGTH + 1];  /* nombre corto en la SD */
    char     title[TITLE_LEN + 1];       /* nombre para mostrar (PETSCII) */
    char     year[5];
    uint8_t  genre;
    uint8_t  rating;
    uint16_t shot;                       /* 0xFFFF = sin captura */
    uint32_t size;
    char     publisher[PUB_LEN + 1];
} Entry;

/* Registro en INDEX.DB (80 bytes, el encabezado tambien ocupa 80) */
typedef struct {
    uint8_t  type;
    char     name[16];
    char     title[TITLE_LEN];
    char     year[4];
    uint8_t  genre;
    uint8_t  rating;
    uint8_t  shot_lo;
    uint8_t  shot_hi;
    char     publisher[PUB_LEN];
    uint8_t  reserved;
} IdxRec;

/* Entrada del listado clasico (formato del firmware) */
typedef struct {
    char     name[FILENAME_LENGTH + 1];
    uint32_t size;
    uint8_t  type;
} DirElem;

static const char *genres[] = {
    S_GENRES
};
#define N_GENRES (sizeof(genres) / sizeof(genres[0]))

/* ---------------------------------------------------------------- estado */

static char     cur_dir[FILENAME_LENGTH + 1];
static char     sel_file[FILENAME_LENGTH + 1];   /* archivo seleccionado en el firmware */
static uint8_t  indexed;                         /* carpeta con INDEX.DB */
static uint8_t  has_shots;
static uint16_t count;
static uint16_t selected;
static char     folder_title[TITLE_LEN + 1];

static DirElem *dir;                             /* modo clasico */
static IdxRec   page_buf[LIST_H];                /* modo indexado: pagina en cache */
static int16_t  page_loaded = -1;

static uint8_t  tmp_hdr[REC_SIZE];
static uint8_t  colbuf[1000];
static char     line[64];

/* nombres en ASCII (los literales de cc65 se convierten a PETSCII) */
static const char IDX_NAME[FILENAME_LENGTH]  = {0x69,0x6e,0x64,0x65,0x78,0x2e,0x64,0x62};  /* index.db */
static const char SHOT_NAME[FILENAME_LENGTH] = {0x73,0x68,0x6f,0x74,0x73,0x2e,0x64,0x62};  /* shots.db */

#define SLOT_SIZE    10240UL
#define BITMAP       ((uint8_t *)0xE000)
#define BMP_SCREEN   ((uint8_t *)0xCC00)

/* ---------------------------------------------------------------- utilidades */

static void select_name(const char *name)
{
    if (strncmp(sel_file, name, FILENAME_LENGTH) == 0) {
        return;
    }
    memset(sel_file, 0, sizeof(sel_file));
    strncpy(sel_file, name, FILENAME_LENGTH);
    tapecart_select_file(sel_file);
}

static void copy_fixed(char *dst, const char *src, uint8_t len)
{
    memcpy(dst, src, len);
    dst[len] = 0;
}

/* Lee la entrada i en e */
static void get_entry(uint16_t i, Entry *e)
{
    if (indexed) {
        int16_t page = i / LIST_H;
        IdxRec *r;
        if (page != page_loaded) {
            uint16_t first = page * LIST_H;
            uint16_t n = count - first;
            if (n > LIST_H) n = LIST_H;
            select_name(IDX_NAME);
            tapecart_read_flash_fast(REC_SIZE + (uint32_t)first * REC_SIZE, n * REC_SIZE, page_buf);
            page_loaded = page;
        }
        r = &page_buf[i % LIST_H];
        e->type = r->type;
        copy_fixed(e->name, r->name, 16);
        copy_fixed(e->title, r->title, TITLE_LEN);
        copy_fixed(e->year, r->year, 4);
        e->genre  = r->genre < N_GENRES ? r->genre : 0;
        e->rating = r->rating;
        e->shot   = r->shot_lo | (r->shot_hi << 8);
        e->size   = 0;
        copy_fixed(e->publisher, r->publisher, PUB_LEN);
    } else {
        DirElem *d = &dir[i];
        e->type = d->type;
        strcpy(e->name, d->name);
        strcpy(e->title, d->name);
        e->year[0] = 0;
        e->genre = 0;
        e->rating = 0;
        e->shot = 0xFFFF;
        e->size = d->size;
        e->publisher[0] = 0;
    }
}

/* ---------------------------------------------------------------- directorio */

/* compara sin distinguir mayusculas (los nombres vienen con el caso invertido) */
static uint8_t lc(uint8_t c)
{
    if ((c >= 0x41 && c <= 0x5A) || (c >= 0x61 && c <= 0x7A) || (c >= 0xC1 && c <= 0xDA)) {
        return (c & 0x1F) | 0x40;
    }
    return c;
}

static uint8_t starts_ci(const char *name, const char *prefix)
{
    while (*prefix) {
        if (lc(*name++) != lc(*prefix++)) return 0;
    }
    return 1;
}

/* Lee el listado del firmware. Devuelve 1 si la carpeta tiene index.db. */
static uint8_t read_classic(void)
{
    DirElem *cur;
    uint8_t *p;
    uint8_t i, found = 0;

    tapecart_sendbyte(CMD_SD_READ_DIR_FAST);
    tapecart_send_u16(MAX_DIR);
    SEI();
    while (count < MAX_DIR) {
        cur = &dir[count];
        cur->type = tapecart_getbyte_fast();
        p = (uint8_t *)&cur->size;
        *p++ = tapecart_getbyte_fast();
        *p++ = tapecart_getbyte_fast();
        *p++ = tapecart_getbyte_fast();
        *p   = 0;
        for (i = 0; i < FILENAME_LENGTH; ++i) {
            cur->name[i] = tapecart_getbyte_fast();
        }
        cur->name[FILENAME_LENGTH] = 0;
        if (cur->type == FILE_NONE) {
            break;
        }
        if (starts_ci(cur->name, "index.db")) {
            found = 1;
            continue;
        }
        /* ocultar indices y archivos de sistema */
        if ((cur->name[0] == '.' && cur->name[1] != '.') ||
            starts_ci(cur->name, "shots.db") ||
            (cur->type == FILE_DIR && starts_ci(cur->name, "system")) ||
            starts_ci(cur->name, "$recycle")) {
            continue;
        }
        ++count;
    }
    CLI();
    return found;
}

#if SORT_MAX
/* Orden: ".." primero, luego carpetas, luego archivos; alfabetico sin distinguir
   mayusculas. Para que sea rapido en el C64 se arma una sola vez una clave por entrada
   (clase + nombre en minusculas) y se ordena un arreglo de indices de 1 byte con
   Shell sort, comparando las claves con memcmp (en assembler). Al final el listado se
   reacomoda una sola vez. */
#define KEY_LEN (FILENAME_LENGTH + 1)
static char     sort_key[SORT_MAX][KEY_LEN];
static uint8_t  sort_idx[SORT_MAX];
static DirElem  sort_tmp[SORT_MAX];

static void sort_dir(uint8_t n)
{
    static const uint8_t gaps[] = { 40, 13, 4, 1 };
    uint8_t i, j, g, gap, t;
    char *k;
    const char *p;

    for (i = 0; i < n; ++i) {
        k = sort_key[i];
        p = dir[i].name;
        if (p[0] == '.' && p[1] == '.') {
            k[0] = 0;
        } else {
            k[0] = dir[i].type == FILE_DIR ? 1 : 2;
        }
        for (j = 0; j < FILENAME_LENGTH && p[j]; ++j) {
            k[j + 1] = lc(p[j]);
        }
        for (; j < FILENAME_LENGTH; ++j) {
            k[j + 1] = 0;
        }
        sort_idx[i] = i;
    }

    for (g = 0; g < sizeof(gaps); ++g) {
        gap = gaps[g];
        for (i = gap; i < n; ++i) {
            t = sort_idx[i];
            j = i;
            while (j >= gap && memcmp(sort_key[sort_idx[j - gap]], sort_key[t], KEY_LEN) > 0) {
                sort_idx[j] = sort_idx[j - gap];
                j -= gap;
            }
            sort_idx[j] = t;
        }
    }

    for (i = 0; i < n; ++i) {
        sort_tmp[i] = dir[sort_idx[i]];
    }
    memcpy(dir, sort_tmp, n * sizeof(DirElem));
}
#endif

static void open_dir(const char *path)
{
    char p[FILENAME_LENGTH + 1];

    memset(p, 0, sizeof(p));
    strncpy(p, path, FILENAME_LENGTH);

    gotoxy(0, 24);
    textcolor(COLOR_YELLOW);
    cputs(S_READING);

    tapecart_open_dir(p, cur_dir);
    cur_dir[FILENAME_LENGTH] = 0;

    count = 0;
    selected = 0;
    page_loaded = -1;
    indexed = 0;
    has_shots = 0;
    sel_file[0] = 0;   /* olvidar el archivo seleccionado: cada carpeta tiene su
                          propio index.db, hay que volver a seleccionarlo al entrar */
    copy_fixed(folder_title, cur_dir, FILENAME_LENGTH);

    /* Solo se selecciona index.db si aparece en el listado: si el firmware
       no encuentra un archivo, remonta la SD y vuelve a la raiz. */
    if (read_classic()) {
        uint16_t entries = count;
        if (count && dir[0].name[0] == '.' && dir[0].name[1] == '.') {
            --entries;
        }
        select_name(IDX_NAME);
        tapecart_read_flash_fast(0, REC_SIZE, tmp_hdr);
        /* "TCIX" v1, y la cantidad de entradas visibles coincide (si no, el indice
           esta desactualizado y se usa el listado normal) */
        if (tmp_hdr[0] == 0x54 && tmp_hdr[1] == 0x43 && tmp_hdr[2] == 0x49 && tmp_hdr[3] == 0x58 &&
            tmp_hdr[4] == 1 && (tmp_hdr[10] | (tmp_hdr[11] << 8)) == entries) {
            indexed = 1;
            count = tmp_hdr[6] | (tmp_hdr[7] << 8);
            has_shots = tmp_hdr[8];
            copy_fixed(folder_title, (char *)tmp_hdr + 16, TITLE_LEN);
        }
    }
#if SORT_MAX
    if (!indexed && count > 1 && count <= SORT_MAX) {
        sort_dir((uint8_t)count);
    }
#endif
}

/* ---------------------------------------------------------------- pantalla */

static void print_entry(uint16_t i)
{
    Entry e;
    uint8_t y = LIST_Y + (i % LIST_H);

    get_entry(i, &e);
    gotoxy(0, y);
    revers(i == selected);

    if (e.type == FILE_DIR) {
        textcolor(COLOR_YELLOW);
        sprintf(line, " %-32s DIR ", e.title);
    } else if (indexed) {
        textcolor(e.type == FILE_UNKNOWN ? COLOR_GRAY2 : COLOR_LIGHTGREEN);
        sprintf(line, " %-32s %-4s ", e.title, e.year);
    } else {
        uint32_t s = e.size;
        char u = 'b';
        if (s >= 1024) { s >>= 10; u = 'k'; }
        if (s >= 1024) { s >>= 10; u = 'm'; }
        textcolor(e.type == FILE_UNKNOWN ? COLOR_GRAY2 : COLOR_LIGHTGREEN);
        sprintf(line, " %-30s %4lu%c ", e.title, s, u);
    }
    line[39] = ' ';
    line[40] = 0;
    cputs(line);
    revers(0);
}

static void print_info(void)
{
    Entry e;

    gotoxy(0, 23);
    textcolor(COLOR_GRAY3);
    if (count == 0) {
        cclear(40);
        return;
    }
    get_entry(selected, &e);
    if (indexed && e.type != FILE_DIR) {
        sprintf(line, " %-12.12s %-19.19s%s", genres[e.genre], e.publisher,
                e.shot != 0xFFFF ? S_SHOT_TAG : "       ");
        line[40] = 0;
        cputs(line);
    } else {
        cclear(40);
    }
}

static void print_page(void)
{
    uint16_t first = (selected / LIST_H) * LIST_H;
    uint16_t i;

    for (i = first; i < first + LIST_H; ++i) {
        if (i < count) {
            print_entry(i);
        } else {
            gotoxy(0, LIST_Y + (i % LIST_H));
            cclear(40);
        }
    }
    print_info();
}

static void draw_frame(void)
{
    clrscr();
    bgcolor(COLOR_BLACK);
    bordercolor(COLOR_BLACK);

    revers(1);
    textcolor(COLOR_GREEN);
    gotoxy(0, 0);
    sprintf(line, " %-29s%4u/%-4u ", folder_title[0] ? folder_title : "/", count ? selected + 1 : 0, count);
    line[40] = 0;
    cputs(line);
    revers(0);

    textcolor(COLOR_GRAY1);
    gotoxy(0, 1);
    chline(40);
    gotoxy(0, 22);
    chline(40);

    gotoxy(0, 24);
    textcolor(COLOR_GRAY2);
    cputs(indexed && has_shots ? S_HELP_SHOTS : S_HELP_PLAIN);
}

static void update_counter(void)
{
    revers(1);
    textcolor(COLOR_GREEN);
    gotoxy(30, 0);
    sprintf(line, "%4u/%-4u", count ? selected + 1 : 0, count);
    line[9] = 0;
    cputs(line);
    revers(0);
}

static void redraw(void)
{
    draw_frame();
    print_page();
}

static void move_to(uint16_t n)
{
    uint16_t old = selected;
    if (count == 0) return;
    if (n >= count) n = count - 1;
    selected = n;
    if (old / LIST_H != n / LIST_H) {
        print_page();
    } else {
        print_entry(old);
        print_entry(n);
        print_info();
    }
    update_counter();
}

/* ---------------------------------------------------------------- captura */

static uint8_t show_shot(uint16_t slot)
{
    uint8_t key;
    uint8_t mode, bg;
    uint32_t base = (uint32_t)slot * SLOT_SIZE;

    gotoxy(0, 24);
    textcolor(COLOR_YELLOW);
    cputs(S_LOADING_SHOT);

    select_name(SHOT_NAME);
    tapecart_read_flash_fast(base, 16, tmp_hdr);
    mode = tmp_hdr[0];
    bg   = tmp_hdr[1];
    tapecart_read_flash_fast(base + 16, 8000, BITMAP);
    tapecart_read_flash_fast(base + 8016, 1000, BMP_SCREEN);
    tapecart_read_flash_fast(base + 9016, 1000, colbuf);

    /* pantalla negra mientras se cambia de modo */
    VIC.ctrl1 &= ~0x10;
    memcpy((uint8_t *)0xD800, colbuf, 1000);
    CIA2.pra = (CIA2.pra & 0xFC) | 0x00;     /* banco VIC 3: $C000-$FFFF */
    VIC.addr = 0x38;                         /* screen $CC00, bitmap $E000 */
    VIC.bgcolor0 = bg;
    VIC.bordercolor = mode ? bg : COLOR_BLACK;
    if (mode) {
        VIC.ctrl2 |= 0x10;
    }
    VIC.ctrl1 |= 0x30;                       /* bitmap + pantalla on */

    while (kbhit()) cgetc();
    for (;;) {
        if (kbhit()) { key = cgetc(); break; }
        if (JOY_BTN_1(joy_read(1))) { key = CH_ENTER; break; }
    }

    VIC.ctrl1 = (VIC.ctrl1 & ~0x20) & ~0x10;
    VIC.ctrl2 &= ~0x10;
    CIA2.pra = (CIA2.pra & 0xFC) | 0x03;     /* banco VIC 0 */
    VIC.addr = 0x17;                         /* screen $0400, juego de caracteres minusculas */
    VIC.ctrl1 |= 0x10;
    return key;
}

/* ---------------------------------------------------------------- ejecutar */

static void reset_and_load(void)
{
    clrscr();
    __asm__ ("jsr $fda3");
    __asm__ ("lda #$00");
    __asm__ ("tay");
    __asm__ ("cs2: sta $0002,y");
    __asm__ ("sta $0200,y");
    __asm__ ("sta $0300,y");
    __asm__ ("iny");
    __asm__ ("bne cs2");
    __asm__ ("ldx #$3c");
    __asm__ ("ldy #$03");
    __asm__ ("stx $b2");
    __asm__ ("sty $b3");
    __asm__ ("ldx #$00");
    __asm__ ("ldy #$a0");
    __asm__ ("stx $c1");
    __asm__ ("sty $c2");
    __asm__ ("jsr $fd8c");
    /* SHIFT + RUN/STOP */
    __asm__ ("lda #$83");
    __asm__ ("sta $0277");
    __asm__ ("lda #$01");
    __asm__ ("sta $c6");
    __asm__ ("jmp $fcf8");
}

static void execute(Entry *e)
{
    char name[FILENAME_LENGTH];

    memset(name, 0, sizeof(name));
    strncpy(name, e->name, FILENAME_LENGTH);
    tapecart_select_file(name);

    if (e->type == FILE_TAP) {
        reset_and_load();
    }
    tapecart_read_loader((uint8_t *)0x0351);
    load_and_run();
    while (1);
}

/* ---------------------------------------------------------------- bucle */

#ifdef TEST_KEYS
/* para pruebas en el emulador: teclas simuladas */
static const char test_keys[] = TEST_KEYS;
static uint8_t test_pos;
#endif

static uint8_t next_key(void)
{
#ifdef TEST_KEYS
    if (test_pos < sizeof(test_keys) - 1) {
        return test_keys[test_pos++];
    }
#endif
    return kbhit() ? cgetc() : 0;
}

static void about(void)
{
    clrscr();
    textcolor(COLOR_YELLOW);
    gotoxy(0, 0);  cputs(S_TITLE);
    textcolor(COLOR_WHITE);
    gotoxy(0, 1);  cputs(S_FORK_BY AUTHOR ", 2026");
    textcolor(COLOR_LIGHTGREEN);
    gotoxy(0, 3);  cputs(S_FORK_OF);
    gotoxy(0, 4);  cputs(" github.com/rainisto/tapecart");
    gotoxy(0, 5);  cputs("tapecart sd (c) kim jorgensen");
    gotoxy(0, 6);  cputs(" github.com/kimjorgensen/tapecart");
    gotoxy(0, 7);  cputs("tapecart (c) 2013-2017 ingo korb");
    gotoxy(0, 8);  cputs("draco browser v1.0c (c) 2009 draco");
    gotoxy(0, 10); cputs(S_SHOTS_DATA);
    gotoxy(0, 11); cputs(S_PERSONAL);
    textcolor(COLOR_GRAY2);
    gotoxy(0, 14); cputs(S_K_RET);
    gotoxy(0, 15); cputs(S_K_SPC);
    gotoxy(0, 16); cputs(S_K_DEL);
    gotoxy(0, 17); cputs(S_K_HOME);
    gotoxy(0, 18); cputs(S_K_CRSR);
    gotoxy(0, 19); cputs(S_K_TB);
    gotoxy(0, 20); cputs(S_K_Q);
    textcolor(COLOR_LIGHTRED);
    gotoxy(0, 23); cputs(S_NOWARRANTY);
    cgetc();
    redraw();
}

static void enter(const char *path)
{
    open_dir(path);
    redraw();
}

int main(void)
{
    Entry e;
    uint8_t c, j, quit = 0;
    uint16_t joy_cnt = 0;
    uint8_t joy_down = 0;

    bgcolor(COLOR_BLACK);
    bordercolor(COLOR_BLACK);
    VIC.addr = 0x17;          /* minusculas */
    *(uint8_t *)0x0291 = 0x80; /* bloquear SHIFT+C= */
    clrscr();

    /* presentacion */
    textcolor(COLOR_YELLOW);
    gotoxy(1, 10); cputs(S_TITLE);
    textcolor(COLOR_WHITE);
    gotoxy(1, 12); cputs(S_BY AUTHOR);
    textcolor(COLOR_GRAY2);
    gotoxy(1, 14); cputs(S_FORK_OF);
    gotoxy(1, 15); cputs(S_FORK_AUTHORS);
    gotoxy(1, 17); cputs(S_PRESS_DOT);
    {
        uint8_t f;
        for (f = 0; f < 90 && !kbhit(); ++f) {   /* ~1,5 s o una tecla */
            waitvsync();
        }
        while (kbhit()) cgetc();
    }
    clrscr();

    if (joy_install(&joy_static_stddrv) != JOY_ERR_OK) {
        cputs(S_ERR_JOY);
        cgetc();
        return 0;
    }
    if (!tapecart_cmdmode()) {
        textcolor(COLOR_LIGHTRED);
        cputs(S_ERR_TC);
        cgetc();
        return 0;
    }

    dir = (DirElem *)malloc(sizeof(DirElem) * MAX_DIR);
    enter(".");

    while (!quit) {
        c = next_key();
        if (!c) {
            j = joy_read(1);
            c = JOY_UP(j) ? CH_CURS_UP :
                JOY_DOWN(j) ? CH_CURS_DOWN :
                JOY_LEFT(j) ? CH_CURS_LEFT :
                JOY_RIGHT(j) ? CH_CURS_RIGHT :
                JOY_BTN_1(j) ? CH_ENTER : 0;
            if (c) {
                if (joy_cnt) {
                    c = 0;
                } else {
                    joy_cnt = joy_down ? 100 : 1000;
                    joy_down = 1;
                }
            } else {
                joy_down = 0;
                joy_cnt = 0;
            }
        }
        if (joy_cnt) {
            --joy_cnt;
        }

        switch (c) {
        case CH_ENTER:
            if (!count) break;
            get_entry(selected, &e);
            if (e.type == FILE_DIR) {
                enter(e.name);
            } else if (e.type == FILE_PRG || e.type == FILE_TCRT || e.type == FILE_TAP) {
                execute(&e);
            }
            break;
        case ' ':
            if (!count || !has_shots) break;
            get_entry(selected, &e);
            if (e.shot != 0xFFFF) {
                c = show_shot(e.shot);
                redraw();
                if (c == CH_ENTER) {
                    get_entry(selected, &e);
                    execute(&e);
                }
            }
            break;
        case CH_HOME:
            enter("/");
            break;
        case CH_DEL:
            enter("..");
            break;
        case CH_CURS_DOWN:
            if (selected + 1 < count) move_to(selected + 1);
            break;
        case CH_CURS_UP:
            if (selected) move_to(selected - 1);
            break;
        case CH_CURS_RIGHT:
            move_to(selected + LIST_H);
            break;
        case CH_CURS_LEFT:
            move_to(selected > LIST_H ? selected - LIST_H : 0);
            break;
        case 't':
            move_to(0);
            break;
        case 'b':
            if (count) move_to(count - 1);
            break;
        case '.':
            about();
            break;
        case 'q':
            quit = 1;
            break;
        }
    }

    tapecart_streammode();
    __asm__ ("jmp $fce2");
    return 0;
}
