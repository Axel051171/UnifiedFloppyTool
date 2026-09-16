/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_bootstrap.c
 * @brief Umsetzung zu uft_bootstrap.h — P3-454.
 *
 * Die benannte Referenz, die drei Zustaende und der Grund fuer die
 * Wiederverwendung von `air_crc32_buffer()` stehen im Header. Hier steht
 * nur, was der Code tut.
 */

#include "uft/forensic/uft_bootstrap.h"
#include "uft/formats/uft_air_crc32.h"

#include <string.h>

/* ───────────────────────── Der Bestand ─────────────────────────────────── */

/**
 * Bekannte Bootstrap-Schluessel.
 *
 * **Leer, und das ist der Stand, kein Versehen.** Die Originaldatenbank
 * LIEGT VOR — `Resources/bootstrap.xml` im gitignorierten Fremdklon,
 * 60 251 Byte, **379** `<bootstrap>`-Schluessel und **490**
 * `<oemname>`-Namen, 223 davon `verified="true"` (gemessen MF-1189). Sie
 * ist **nicht uebernommen**, weil das zwei Entscheidungen braeuchte, die
 * nicht bei mir liegen: GPL-3.0 (MF-698) und das
 * EU-Datenbankherstellerrecht. Der Grund und die Fundstelle stehen im
 * Header; das ist S3, nicht Abwesenheit.
 *
 * Einen Eintrag zu erfinden, damit die Tafel nicht leer aussieht, waere
 * die Klasse FMT-2/3/10/11/12: ein Beleg, den niemand geprueft hat.
 * Einen abzuschreiben waere die Lizenzverletzung, die kein Rotbeweis
 * fangen kann (MF-695).
 *
 * Wenn die Entscheidung faellt, wird der Bestand hierher ERZEUGT (der
 * Wandler dafuer liegt in der Zulieferung als
 * `tools/bootstrap_xml_to_c.py`, und sein Kommentar warnt zu Recht:
 * „namehex UND name werden zu acht Rohbytes. Viele Eintraege sind KEIN
 * Text — wer sie als C-Zeichenkette ablegt, endet am ersten Nullbyte").
 */
typedef struct {
    uint32_t    crc32;
    const char *werkzeug;
} bs_eintrag_t;

static const bs_eintrag_t k_bestand[] = {
    { 0u, NULL }   /* Wachposten. Der Bestand hat NULL echte Eintraege. */
};

size_t uft_bs_bestand_groesse(void)
{
    /* Der Wachposten zaehlt nicht mit. */
    return (sizeof(k_bestand) / sizeof(k_bestand[0])) - 1u;
}

static const char *bestand_suche(uint32_t crc)
{
    size_t i;
    const size_t n = uft_bs_bestand_groesse();
    for (i = 0u; i < n; i++) {
        if (k_bestand[i].crc32 == crc) return k_bestand[i].werkzeug;
    }
    return NULL;   /* „dieser Bestand kennt ihn nicht" */
}

/* ───────────────────────── Bootstrap abgrenzen ─────────────────────────── */

bool uft_bs_bootstrap_bereich(const uint8_t *sektor, size_t sektor_len,
                              size_t *offset_aus, size_t *len_aus)
{
    size_t start, ende;

    if (offset_aus) *offset_aus = 0u;
    if (len_aus)    *len_aus    = 0u;
    if (!sektor || sektor_len < UFT_BS_SEKTOR_LEN) return false;

    /* Der Sprungbefehl sagt, wo der Code beginnt. Zwei Formen; alles
     * andere ist kein PC-Bootsektor. */
    if (sektor[0] == 0xEBu) {
        start = 2u + (size_t)sektor[1];
    } else if (sektor[0] == 0xE9u) {
        start = 3u + (size_t)((uint16_t)sektor[1]
                              | ((uint16_t)sektor[2] << 8));
    } else {
        return false;
    }
    if (start >= UFT_BS_SEKTOR_LEN - 2u) return false;

    /* Hinten die Kennung 0x55AA abschneiden, wenn sie da ist. */
    ende = UFT_BS_SEKTOR_LEN;
    if (sektor[510] == 0x55u && sektor[511] == 0xAAu) ende = 510u;
    if (ende <= start) return false;

    /* Nachlaufende Nullen gehoeren nicht zum Code — sonst haengt der
     * Schluessel daran, wie viel Fuellung der Formatierer geschrieben
     * hat. Der Test `die_polsterung_aendert_den_schluessel_nicht` haelt
     * genau das fest. */
    while (ende > start && sektor[ende - 1u] == 0x00u) ende--;
    if (ende == start) return false;   /* leerer Bootstrap = kein Code */

    if (offset_aus) *offset_aus = start;
    if (len_aus)    *len_aus    = ende - start;
    return true;
}

/* ───────────────────────── Win9x-Kennung ───────────────────────────────── */

bool uft_bs_oem_ist_win9x(const uint8_t *oem, size_t oem_len)
{
    if (!oem || oem_len < UFT_BS_OEM_LEN) return false;
    /* BootSector.vb:306 — „IHC" an den Positionen 5..7. */
    return oem[5] == 0x49u && oem[6] == 0x48u && oem[7] == 0x43u;
}

/* ───────────────────────── Traeger bestimmen ───────────────────────────── */

bool uft_bs_identifiziere(const uint8_t *sektor, size_t sektor_len,
                          uft_bs_traeger_id_t *aus)
{
    size_t offset = 0u, len = 0u;

    if (!aus) return false;

    /* In JEDEM Fall beschreiben, damit ein Aufrufer, der den
     * Rueckgabewert ignoriert, keine Altwerte liest. */
    memset(aus, 0, sizeof(*aus));
    aus->lage     = UFT_BS_TRAEGER_UNBEKANNT;
    aus->werkzeug = NULL;

    if (!sektor || sektor_len < UFT_BS_SEKTOR_LEN) return false;

    /* Der OEM-Name steht bei 0x03 und ist 8 Byte lang — er ist NICHT
     * nullterminiert, deshalb wird er hier kopiert und abgeschlossen. */
    memcpy(aus->oem, sektor + 0x03u, UFT_BS_OEM_LEN);
    aus->oem[UFT_BS_OEM_LEN] = '\0';
    aus->oem_ist_win9x = uft_bs_oem_ist_win9x(sektor + 0x03u,
                                              UFT_BS_OEM_LEN);

    if (!uft_bs_bootstrap_bereich(sektor, sektor_len, &offset, &len)) {
        /* Kein Schluessel. Der OEM-Name bleibt stehen, weil er gelesen
         * WURDE — das ist eine andere Aussage als „kein Bootsektor". */
        return false;
    }

    aus->code_offset = offset;
    aus->code_len    = len;
    /* EINE Rechnung: die CRC-32 des Baums, nicht eine eigene (MF-1177). */
    aus->crc32       = air_crc32_buffer(sektor, offset, len);

    aus->werkzeug = bestand_suche(aus->crc32);
    aus->lage     = (aus->werkzeug != NULL) ? UFT_BS_TRAEGER_ZUGEORDNET
                                            : UFT_BS_TRAEGER_GEMESSEN;
    return true;
}
