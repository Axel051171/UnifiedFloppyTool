/**
 * @file test_ipf_sonde_beansprucht_nur_ipf.c
 * @brief Die IPF-Sonde darf keine Datei beanspruchen, die kein IPF ist
 *        (MF-1002).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * `ipf_plugin_probe()` hatte zwei Zweige. Der erste prueft den ASCII-
 * Vierer „CAPS" und ist richtig. Der zweite rief
 * `uft_caps_is_ipf()` aus `src/formats/ipf/uft_caps_ipf.c` und
 * beanspruchte bei Erfolg Konfidenz **90**.
 *
 * Gemessen am belegten Pfad (uebersetzt und ausgefuehrt, nicht gelesen):
 *
 *     uft_caps_is_ipf(echtes IPF, "CAPS")      = FALSE
 *     uft_caps_is_ipf(kein IPF, 00 00 00 01)   = TRUE
 *
 * Also genau verkehrt herum. Der Grund steht in derselben Datei:
 *
 *     #define IPF_BLOCK_CAPS 1        // interne Aufzaehlung, 1..10
 *     header->type = read32_be(data + pos);   // ROHE ASCII-Bytes
 *     return (type == IPF_BLOCK_CAPS);
 *
 * `read_block_header()` legt die vier ASCII-Bytes unuebersetzt als
 * BE-u32 ab. Fuer „CAPS" steht dort **0x43415053**, nie **1**. Damit
 * kann `switch (header.type)` in `uft_caps_load_image()` fuer KEINEN
 * der zehn Satztypen je greifen — die Datei ist strukturell ausserstande,
 * ein IPF zu lesen. Und `uft_caps_is_ipf()` meldet stattdessen jede
 * Datei als IPF, die mit `00 00 00 01` beginnt.
 *
 * Das ist die Klasse aus **MF-961**: dort probte `86f` auf `"86BX"`, ein
 * Magic, das in keiner echten Datei steht.
 *
 * ── Warum das schlimmer ist als toter Code ──────────────────────────
 *
 * Fuer ein echtes IPF ist der zweite Zweig unerreichbar — der erste hat
 * schon zugegriffen. Er kann also nur noch **falsch positiv** werden.
 * Nach der Konfidenzskala aus MF-729 bedeutet 80..100 „Merkmal
 * getroffen"; 90 fuer vier Nullbytes-mit-Eins ist genau die
 * Ueberzeichnung, die MF-729 abgeschafft hat.
 *
 * ── Was dieser Test bewusst NICHT prueft ────────────────────────────
 *
 * Ob UFT IPF richtig LIEST. Das tut `uft_ipf_air.c`, und das steht auf
 * einem eigenen Blatt (Quarantaene, GPL-3-Bindung). Hier geht es allein
 * um die Zusage der Sonde.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "uft/uft_format_common.h"
#include "uft/uft_format_plugin.h"

static int gruen = 0;
static int rot = 0;

static void pruefe(const char *name, int bedingung, const char *hinweis)
{
    if (bedingung) {
        printf("  [OK]   %s\n", name);
        gruen++;
    } else {
        printf("  [ROT]  %s%s%s\n", name, hinweis ? "  -- " : "",
               hinweis ? hinweis : "");
        rot++;
    }
}

static void be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

extern const uft_format_plugin_t uft_format_plugin_ipf;

int main(void)
{
    printf("=== Die IPF-Sonde beansprucht nur IPF (MF-1002) ===\n");

    const uft_format_plugin_t *p = &uft_format_plugin_ipf;
    if (!p->probe) {
        printf("  [ROT]  das IPF-Plugin hat keine Sonde\n");
        return 1;
    }

    /* (1) Ein echtes IPF: Satzkopf ist der ASCII-Vierer "CAPS", danach
     *     Laenge und CRC. So beginnt jede IPF-Datei. */
    uint8_t echt[64];
    memset(echt, 0, sizeof(echt));
    memcpy(echt, "CAPS", 4);
    be32(echt + 4, 12);
    be32(echt + 8, 0x12345678);

    int konf = -1;
    bool nimmt = p->probe(echt, sizeof(echt), sizeof(echt), &konf);
    pruefe("ein echtes IPF wird angenommen", nimmt, "„CAPS\" abgewiesen");
    pruefe("und zwar mit voller Konfidenz", nimmt && konf >= 80,
           "Merkmal getroffen heisst 80..100 (MF-729)");

    /* (2) Kein IPF, beginnt aber mit 00 00 00 01 — ein sehr haeufiger
     *     Anfang binaerer Formate (eine BE-Eins als Version, Anzahl
     *     oder Satzzahl). */
    uint8_t fremd[64];
    memset(fremd, 0xAA, sizeof(fremd));
    be32(fremd, 1);

    konf = -1;
    nimmt = p->probe(fremd, sizeof(fremd), sizeof(fremd), &konf);

    char hinweis[160];
    snprintf(hinweis, sizeof(hinweis),
             "vier Bytes 00 00 00 01 wurden als IPF beansprucht "
             "(Konfidenz %d) -- das ist keine IPF-Kennung", konf);
    pruefe("eine Nicht-IPF-Datei wird NICHT beansprucht", !nimmt, hinweis);

    /* (3) Gegenprobe, damit (2) nicht durch eine Sonde gruen wird, die
     *     ueberhaupt nichts mehr annimmt: reiner Zufall bleibt drau3en,
     *     „CAPS" kommt herein — beides muss gelten. */
    uint8_t rausch[64];
    for (size_t i = 0; i < sizeof(rausch); i++)
        rausch[i] = (uint8_t)(i * 37u + 11u);
    rausch[0] = 0x7F;                 /* sicher kein "CAPS", kein 00.. */
    konf = -1;
    pruefe("Rauschen bleibt draussen",
           !p->probe(rausch, sizeof(rausch), sizeof(rausch), &konf), NULL);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}
