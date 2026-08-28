/**
 * @file test_amigados_cycle.c
 * @brief Ein Ringschluss im AmigaDOS-Verzeichnis darf nicht haengen (MF-639)
 *
 * ── Der Fehler ───────────────────────────────────────────────────────────
 *
 * `src/fs/uft_amigados.c` folgt zwei Ketten, und beide brechen nur bei
 * einer SELBST-Schleife ab:
 *
 *   Hash-Kette      `:417`   if (e.hash_chain == head) break;
 *   Extension-Kette `:561`   if (next == cur_hdr)       break;
 *
 * Ein Ring ueber ZWEI Bloecke — A zeigt auf B, B zeigt zurueck auf A —
 * passiert beide Bedingungen nie. Die Hash-Schleife laeuft dann endlos,
 * und `dir_append()` (`:374-385`) verdoppelt bei jedem Durchgang die
 * Kapazitaet: unbegrenztes `realloc`, bis der Speicher ausgeht.
 *
 * Das ist kein theoretischer Fall. UFT oeffnet FREMDE Abbilder — genau
 * dafuer ist es gebaut. Eine beschaedigte oder absichtlich praeparierte
 * ADF-Datei laesst das Werkzeug damit haengen, statt einen Fehler zu
 * melden. Fuer ein forensisches Werkzeug ist „haengt" die schlechteste
 * aller Antworten: der Benutzer weiss nicht, ob es rechnet oder steht.
 *
 * ── Warum das aergerlich ist ─────────────────────────────────────────────
 *
 * Die Loesung liegt seit jeher im selben Baum. `uft_amigados_extended.c`
 * (`:82-129`) fuehrt eine `visited`-Bitmap ueber alle Bloecke plus einen
 * Tiefendeckel und zaehlt gefundene Ringe sogar als Befund (`cycles`).
 * Nur benutzt der registrierte Dateisystem-Treiber
 * (`src/fs/uft_fs_amigados_driver.c`) den anderen Walker — den ohne
 * Schutz. Achter Fall derselben Gestalt in dieser Sitzung: das Koennen
 * liegt im Baum, der Zugang fehlt.
 *
 * Aufmerksam geworden durch den Scout-Zyklus gegen `pbakota/amigadx`,
 * dessen Aenderungsprotokoll 2005 genau diese Feldbedingung nennt.
 *
 * ── Was dieser Test tut ──────────────────────────────────────────────────
 *
 * Er baut ein kleines, aber gueltiges Abbild mit einem Zwei-Block-Ring in
 * der Hash-Kette des Wurzelverzeichnisses und verlangt, dass
 * `uft_amiga_load_root()` **zurueckkehrt** und dabei eine plausible
 * Anzahl Eintraege liefert.
 *
 * Ohne die Korrektur kehrt er nicht zurueck — der Test haengt, statt
 * durchzufallen. Das ist beabsichtigt und beim Rotbeweis so beobachtet
 * worden (mit Zeitlimit abgebrochen). Eine Endlosschleife laesst sich
 * nicht hoeflich melden; genau deshalb ist sie es wert, gefangen zu
 * werden.
 */

#include "uft/fs/uft_amigados.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;

/* Aufbau wie in tests/test_d88_error_marks.c: die Zusicherung erhoeht den
 * Fehlerzaehler, und der Laeufer vergleicht ihn vorher/nachher. Damit kann
 * der Erfolgszaehler nicht bedingungslos hochgehen — die Form, die Tor 37
 * seit MF-596 verlangt und die dieser Baum durchgaengig benutzt.
 *
 * Meine erste Fassung fragte stattdessen den Rueckgabewert ab
 * (`if (test_##name() == 0)`). Sachlich genauso bedingt, aber Tor 37 sucht
 * das Verzweigungswort ZWISCHEN Aufruf und Zaehler und sah es dort nicht.
 * Ich habe kurz erwogen, das Tor zu erweitern, und es verworfen: ein
 * Waechter, den man fuer den eigenen Stil zurechtbiegt, ist ein Waechter
 * weniger. Der Baum hat eine Form — dies ist sie. */
#define RUN(name)  do { printf("  [TEST] %-42s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

#define BLK        512u
#define BLOCKS     256u                 /* 131072 Byte — klein und schnell */
#define ROOT_BLK   (BLOCKS / 2)         /* uft_amiga_detect: total/2 */

/* Versaetze aus src/fs/uft_amigados.c */
#define O_TYPE        0x000
#define O_HEADER_KEY  0x004
#define O_HTAB_SIZE   0x00C
#define O_HASHTABLE   0x018             /* 72 Langworte */
#define O_HASH_CHAIN  0x1F0
#define O_PARENT      0x1F4
#define O_SEC_TYPE    0x1FC
#define O_NAME        432

#define T_HEADER      2
#define ST_ROOT       1
#define ST_FILE       (-3)

static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

/** BCPL-Zeichenkette: Laengenbyte, dann die Zeichen. */
static void put_bcpl(uint8_t *p, const char *s)
{
    size_t n = strlen(s);
    if (n > 30) n = 30;
    p[0] = (uint8_t)n;
    memcpy(p + 1, s, n);
}

/**
 * Baut ein Abbild, dessen Wurzel-Hashkette einen Zwei-Block-Ring enthaelt.
 *
 * Wurzel (Block 128) -> Hash-Slot 0 -> Block 100
 *   Block 100 "RINGA", hash_chain -> 101
 *   Block 101 "RINGB", hash_chain -> 100      <- hier schliesst sich der Ring
 *
 * Keine der beiden Selbst-Schleifen-Bedingungen greift: 101 != 100 und
 * 100 != 101.
 */
static uint8_t *baue_ring_abbild(size_t *size_out)
{
    const size_t size = (size_t)BLOCKS * BLK;
    uint8_t *img = (uint8_t *)calloc(1, size);
    if (!img) return NULL;

    /* Bootblock: die Kennung, die uft_amiga_detect() verlangt. */
    memcpy(img, "DOS", 3);
    img[3] = 0x00;                      /* OFS */

    uint8_t *root = img + (size_t)ROOT_BLK * BLK;
    put_be32(root + O_TYPE, T_HEADER);
    put_be32(root + O_HEADER_KEY, 0);
    put_be32(root + O_HTAB_SIZE, 72);
    put_be32(root + O_SEC_TYPE, (uint32_t)ST_ROOT);
    put_bcpl(root + O_NAME, "RINGTEST");
    put_be32(root + O_HASHTABLE + 0 * 4, 100);   /* Slot 0 -> Block 100 */

    uint8_t *a = img + 100u * BLK;
    put_be32(a + O_TYPE, T_HEADER);
    put_be32(a + O_HEADER_KEY, 100);
    put_be32(a + O_SEC_TYPE, (uint32_t)ST_FILE);
    put_be32(a + O_PARENT, ROOT_BLK);
    put_be32(a + O_HASH_CHAIN, 101);
    put_bcpl(a + O_NAME, "RINGA");

    uint8_t *b = img + 101u * BLK;
    put_be32(b + O_TYPE, T_HEADER);
    put_be32(b + O_HEADER_KEY, 101);
    put_be32(b + O_SEC_TYPE, (uint32_t)ST_FILE);
    put_be32(b + O_PARENT, ROOT_BLK);
    put_be32(b + O_HASH_CHAIN, 100);             /* zurueck auf A */
    put_bcpl(b + O_NAME, "RINGB");

    *size_out = size;
    return img;
}

/**
 * Der eigentliche Beweis: das Lesen muss ZURUECKKEHREN.
 *
 * Geprueft wird zusaetzlich eine Schranke. Ein Verzeichnis mit zwei
 * angelegten Eintraegen kann nicht mehr als die 72 Hash-Slots hergeben;
 * kaeme eine groessere Zahl heraus, waere der Ring zwar abgebrochen, aber
 * mehrfach durchlaufen worden — auch das waere falsch.
 */
static void test_ringschluss_kehrt_zurueck(void)
{
    size_t size = 0;
    uint8_t *img = baue_ring_abbild(&size);
    ASSERT(img != NULL);

    uft_amiga_ctx_t *ctx = uft_amiga_create();
    ASSERT(ctx != NULL);
    ASSERT(uft_amiga_open_buffer(ctx, img, size, false, NULL) == 0);

    uft_amiga_dir_t dir;
    memset(&dir, 0, sizeof(dir));

    /* Ohne Korrektur kehrt dieser Aufruf nie zurueck. */
    int rc = uft_amiga_load_root(ctx, &dir);

    ASSERT(rc == 0 || rc == -2);        /* gelesen, oder sauber abgebrochen */
    ASSERT(dir.count <= 72);            /* nicht mehrfach im Ring gelaufen */

    /* Beide Eintraege sollen einmal auftauchen — der Ring darf gebrochen,
     * aber nichts unterschlagen werden. "Kein Bit verloren" gilt auch
     * hier: die Bloecke existieren, sie zeigen nur falsch aufeinander. */
    int sah_a = 0, sah_b = 0;
    for (size_t i = 0; i < dir.count; i++) {
        if (strcmp(dir.entries[i].name, "RINGA") == 0) sah_a++;
        if (strcmp(dir.entries[i].name, "RINGB") == 0) sah_b++;
    }
    ASSERT(sah_a == 1);
    ASSERT(sah_b == 1);

    uft_amiga_free_dir(&dir);
    uft_amiga_close(ctx);
    uft_amiga_destroy(ctx);
    free(img);
}

/**
 * Gegenprobe: eine gesunde Kette ohne Ring muss unveraendert lesen.
 *
 * Ohne diese Pruefung koennte die Korrektur einfach jede Kette nach dem
 * ersten Glied abbrechen und der Test oben waere trotzdem gruen.
 */
static void test_gesunde_kette_bleibt_vollstaendig(void)
{
    size_t size = 0;
    uint8_t *img = baue_ring_abbild(&size);
    ASSERT(img != NULL);

    /* Ring aufloesen: B endet, statt auf A zurueckzuzeigen. */
    put_be32(img + 101u * BLK + O_HASH_CHAIN, 0);

    uft_amiga_ctx_t *ctx = uft_amiga_create();
    ASSERT(ctx != NULL);
    ASSERT(uft_amiga_open_buffer(ctx, img, size, false, NULL) == 0);

    uft_amiga_dir_t dir;
    memset(&dir, 0, sizeof(dir));
    ASSERT(uft_amiga_load_root(ctx, &dir) == 0);
    ASSERT(dir.count == 2);

    uft_amiga_free_dir(&dir);
    uft_amiga_close(ctx);
    uft_amiga_destroy(ctx);
    free(img);
}

int main(void)
{
    printf("=== AmigaDOS: Ringschluss in der Hash-Kette (MF-639) ===\n");
    RUN(gesunde_kette_bleibt_vollstaendig);
    RUN(ringschluss_kehrt_zurueck);
    printf("\nErgebnis: %d bestanden, %d gescheitert\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
