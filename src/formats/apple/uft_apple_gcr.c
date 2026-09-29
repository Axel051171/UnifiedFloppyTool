/**
 * @file uft_apple_gcr.c
 * @brief Apple-II-GCR dekodieren: 6-and-2 und 4-and-4 (MF-715)
 *
 * Referenzen und die Messung, die vor diesem Code stand: siehe
 * `include/uft/formats/apple/uft_apple_gcr.h`. Kurz: *Beneath Apple DOS*
 * Kapitel 3 fuer das Verfahren, und die 64 Diskettenbytes der Tabelle
 * gegen die Ausgabe des Oracles `to_woz2` geprueft (5488 Datenbytes,
 * 0 fremde Nibbles).
 */
#include "uft/formats/apple/uft_apple_gcr.h"

#include <string.h>

/* ── Die 6-and-2-Umsetzung ──────────────────────────────────────────────
 *
 * 64 zulaessige Diskettenbytes. Jedes hat das hohe Bit gesetzt und
 * mindestens zwei benachbarte gesetzte Bits — das ist die Bedingung,
 * die der Apple-Controller an einen lesbaren Strom stellt.
 * `Beneath Apple DOS`, Kapitel 3.
 *
 * BERICHTIGT MF-1126. Hier stand zusaetzlich „und nie mehr als eine
 * Null in Folge". Das ist falsch, und zwar ueber die eigene Tabelle
 * darunter: gemessen ist der laengste Nulllauf **2**, schon beim ersten
 * Wert `0x96` = `10010110`. Die TABELLE ist richtig — MF-715 hat ihre
 * 64 Werte gegen das Oracle `to_woz2` gehalten, 5488 Datenbytes, 0
 * fremde Nibbles —, falsch war die angegebene BEDINGUNG. Wer aus ihr
 * ableitet, erzeugt **33** Werte statt 64 (nachgerechnet).
 *
 * **BERICHTIGT MF-1515 — die hinreichende Regel IST feststellbar, und
 * hier stand, sie sei es nicht.** Der Satz lautete: „Mit ‚hohes Bit
 * gesetzt', ‚zwei Einsen in Folge' und ‚Nulllauf <= 2' kommen **74**
 * Werte heraus; welche weitere Bedingung die zehn ueberzaehligen
 * ausschliesst, ist ohne die gedruckte Quelle nicht feststellbar, und
 * sie zu erraten waere dieselbe Falschaussage in neuem Gewand."
 *
 * Die 74 sind richtig gezaehlt. Was fehlte, waren ZWEI Bedingungen, und
 * gefunden sind sie nicht durch Raten, sondern durch Aufzaehlen: alle
 * Kombinationen von sieben Bedingungsbausteinen wurden gegen DIESE
 * Tafel gerechnet, und genau eine liefert exakt die 64 Werte —
 *
 *     Bit 7 gesetzt · hoechstens EIN Paar benachbarter Nullen ·
 *     mindestens ein Einserpaar in Bit 6..0
 *
 * „hoechstens ein Nullenpaar" ist schaerfer als „Nulllauf <= 2": drei
 * Nullen enthalten zwei Paare, aber zwei GETRENNTE Paare haben auch nur
 * Laufweite 2 — und genau die zehn Werte mit zwei getrennten Paaren
 * (0x93, 0x99, 0x9C, 0xC9, 0xCA, 0xCC, 0xD2, 0xD4 u. a.) sind die
 * ueberzaehligen. Das Einserpaar zaehlt OHNE Bit 7, das ohnehin steht.
 *
 * Belegt mit einer Mutationsmatrix (3 von 3): jede der beiden
 * Praezisierungen einzeln gelockert macht die Zusagen rot. Die Regel
 * liegt seit MF-1509 in `include/uft/core/uft_gcr.h`, festgenagelt in
 * `tests/test_gcr_praedikat_trifft_die_tafel.c` — bijektiv, also auch
 * gegen die Woerter, die NICHT in der Tafel stehen. Das ist die Zusage,
 * die `tests/test_gcr_tafeln.c` (MF-1126) ausdruecklich offen liess.
 *
 * Die Tafel selbst ist deshalb entfallen: Index k ist das k-te gueltige
 * Byte in aufsteigender Reihenfolge — gemessen byteweise gegen die
 * Fassung, die hier stand. `uft_gcr_kodieren()` und
 * `uft_gcr_dekodieren()` leisten beides, die Rueckwaertstafel wird dort
 * aus dem Praedikat abgeleitet statt ein zweites Mal aufgebaut.
 */
#include "uft/core/uft_gcr.h"

/* Rueckwaerts: Diskettenbyte -> 6 Bit, `UFT_GCR_UNGUELTIG` (0xFF) heisst
 * „steht nicht in der Tabelle".
 *
 * Bis MF-1515 baute diese Datei die Umkehrung selbst auf — 256 Byte
 * `a2_read_tab` plus ein `ready`-Schalter, gefuellt aus `A2_WRITE_TAB`.
 * Genau dasselbe tut `uft_gcr_dekodieren()` seit MF-1512, nur aus dem
 * PRAEDIKAT statt aus einer Tafel, und fuer alle drei Codecs an einer
 * Stelle. Der Aufbau war also nicht falsch, sondern die zweite Ausgabe
 * derselben Rechnung (MF-1177). */

/* Die zwei niederwertigen Bits eines Nutzbytes liegen in den
 * Hilfsnibbles **vertauscht** — Bit 0 und Bit 1 sind gegenueber der
 * Nutzbyte-Reihenfolge gedreht. `Beneath Apple DOS`, Kapitel 3. */
static const uint8_t A2_SWAP2[4] = { 0x0, 0x2, 0x1, 0x3 };

/* ── Die 5-and-3-Umsetzung (13 Sektoren, DOS 3.2) ───────────────────────
 *
 * 32 zulaessige Diskettenbytes. GEMESSEN (MF-719), nicht erinnert: aus
 * einem `to_woz2`-WOZ, dessen Eingabe alle 256 Bytewerte enthielt, kamen
 * ueber 13 Datenfelder genau diese 32 heraus — und keinen mehr. Ein
 * erster Lauf mit einem schwaecheren Muster hatte nur 24 ausgeloest;
 * eine Teilmenge kann eine Tabelle bestaetigen, aber nicht
 * vervollstaendigen.
 *
 * Drittbestaetigt durch `mamedev/mame` `ap2_dsk.cpp` (`translate5`,
 * BSD-3-Clause) — eine Quelle, die die Messung nicht kannte.
 *
 * **Seit MF-1515 liefert das Register diese 32 Werte** — als Menge, die
 * ein Praedikat erzeugt (Bit 7 gesetzt · KEIN Paar benachbarter Nullen ·
 * ein Einserpaar in Bit 6..0), byteweise gegen die hier gestandene Tafel
 * gemessen, Reihenfolge eingeschlossen. Die Tafel selbst liegt als Zeuge
 * in `tests/oracles/gcr_apple_tafeln_f08d1a7a.c` — mit dieser Herkunft
 * woertlich, denn die Messung von MF-719 ist der Grund, warum die Zahlen
 * tragen, und ein Praedikat kann sie nicht ersetzen. Es kann nur
 * uebereinstimmen, und das tut es.
 */

bool uft_apple_gcr_denibblize_6_2(const uint8_t nib[UFT_A2_DATA_NIBBLES],
                                  uint8_t out[UFT_A2_SECTOR_SIZE])
{
    if (!nib || !out) return false;
    /* MF-1515: kein eigener Tafelaufbau mehr — `uft_gcr_dekodieren()`
     * baut seine Rueckrichtung beim ersten Zugriff aus dem Praedikat. */

    uint8_t aux[86];
    uint8_t pri[UFT_A2_SECTOR_SIZE];
    uint8_t chk = 0;

    /* 86 Hilfsbytes: je drei Bitpaare fuer drei spaetere Nutzbytes. */
    for (size_t i = 0; i < 86; i++) {
        uint8_t v = uft_gcr_dekodieren(UFT_GCR_APPLE_6_2, nib[i]);
        if (v == UFT_GCR_UNGUELTIG) return false;
        chk ^= v;
        aux[i] = chk;
    }
    /* 256 Hauptbytes: die oberen sechs Bit jedes Nutzbytes. */
    for (size_t i = 0; i < UFT_A2_SECTOR_SIZE; i++) {
        uint8_t v = uft_gcr_dekodieren(UFT_GCR_APPLE_6_2, nib[86 + i]);
        if (v == UFT_GCR_UNGUELTIG) return false;
        chk ^= v;
        pri[i] = chk;
    }
    /* Das 343. Byte ist die Pruefsumme: sie muss die laufende XOR-Summe
     * auf null bringen. Geht sie nicht auf, bleibt `out` unberuehrt —
     * ein halb dekodierter Sektor waere eine stille Veraenderung. */
    uint8_t last = uft_gcr_dekodieren(UFT_GCR_APPLE_6_2,
                                     nib[UFT_A2_DATA_NIBBLES - 1]);
    if (last == UFT_GCR_UNGUELTIG) return false;
    if ((uint8_t)(chk ^ last) != 0) return false;

    for (size_t i = 0; i < UFT_A2_SECTOR_SIZE; i++) {
        size_t j = i % 86;          /* welches Hilfsbyte      */
        size_t k = i / 86;          /* welches Bitpaar darin  */
        uint8_t low = A2_SWAP2[(aux[j] >> (2u * k)) & 3u];
        out[i] = (uint8_t)((pri[i] << 2) | low);
    }
    return true;
}

/* ── 5-and-3 zurueckwandeln ─────────────────────────────────────────────
 *
 * Die Aufteilung der Bits ist eine **Tatsache ueber das Format**, und
 * sie stammt aus `mamedev/mame` `src/lib/formats/ap2_dsk.cpp:356-372`
 * (`a2_13sect_format::save`, Lizenz **BSD-3-Clause**, Commit
 * `c0d3677674`). Uebernommen wurde, welches Bit wohin gehoert — die
 * Umkehrung unten ist eigenstaendig geschrieben, es wandert kein
 * Ausdruck ein.
 *
 * Der Kodierer schreibt in dieser Ordnung:
 *
 *     1 Byte    sdata[255] & 7
 *     153 Byte  k = 2..0, j = 0..50:
 *                 (sdata[j*5+k] & 7) << 2
 *               | ((sdata[j*5+3] >> (2-k)) & 1) << 1
 *               | ((sdata[j*5+4] >> (2-k)) & 1)
 *     255 Byte  k = 0..4, j = 50..0:  sdata[j*5+k] >> 3
 *     1 Byte    sdata[255] >> 3
 *     ─────────
 *     410 Byte  Nutzteil, danach 1 Pruefbyte  = 411 (MF-719 gemessen)
 *
 * Jedes Byte geht als `TAB5[nval ^ pval]` auf die Spur, `pval` ist der
 * VORIGE Nutzwert; das Pruefbyte ist `TAB5[pval]` ohne XOR.
 *
 * Die Aufteilung erklaert nebenbei, warum es 5-and-3 heisst: Indizes
 * `j*5+0..2` bekommen ihre unteren drei Bit am Stueck, die Indizes
 * `j*5+3` und `j*5+4` sammeln ihre drei Bit ueber die drei k-Durchlaeufe
 * einzeln ein.
 */
bool uft_apple_gcr_denibblize_5_3(const uint8_t nib[UFT_A2_DATA_NIBBLES_13],
                                  uint8_t out[UFT_A2_SECTOR_SIZE])
{
    if (!nib || !out) return false;
    /* MF-1515: siehe `denibblize_6_2` — das Register baut lazy auf. */

    /* Erst die laufende XOR aufloesen — 410 Nutzwerte, dann das
     * Pruefbyte. Ein Diskettenbyte ausserhalb der Tabelle beendet den
     * Versuch, BEVOR `out` angefasst wird. */
    uint8_t n[410];
    uint8_t pval = 0;
    for (size_t i = 0; i < 410; i++) {
        uint8_t v = uft_gcr_dekodieren(UFT_GCR_APPLE_5_3, nib[i]);
        if (v == 0xFF) return false;
        pval = (uint8_t)(v ^ pval);
        n[i] = pval;
    }
    uint8_t summe = uft_gcr_dekodieren(UFT_GCR_APPLE_5_3, nib[410]);
    if (summe == 0xFF) return false;
    if (summe != pval) return false;

    uint8_t buf[UFT_A2_SECTOR_SIZE];
    memset(buf, 0, sizeof(buf));

    /* 1 · die unteren drei Bit des letzten Bytes */
    buf[255] = (uint8_t)(n[0] & 7u);

    /* 2 · 153 Bytes mit den unteren drei Bit aller uebrigen */
    size_t idx = 1;
    for (int k = 2; k >= 0; k--) {
        for (int j = 0; j < 51; j++, idx++) {
            uint8_t v = n[idx];
            buf[j * 5 + k] = (uint8_t)((v >> 2) & 7u);
            buf[j * 5 + 3] |= (uint8_t)(((v >> 1) & 1u) << (2 - k));
            buf[j * 5 + 4] |= (uint8_t)((v & 1u) << (2 - k));
        }
    }

    /* 3 · 255 Bytes mit den oberen fuenf Bit */
    for (int k = 0; k < 5; k++)
        for (int j = 50; j >= 0; j--, idx++)
            buf[j * 5 + k] |= (uint8_t)(n[idx] << 3);

    /* 4 · und die oberen fuenf des letzten */
    buf[255] |= (uint8_t)(n[idx] << 3);

    memcpy(out, buf, sizeof(buf));
    return true;
}

uint8_t uft_apple_gcr_decode_4_4(uint8_t hi, uint8_t lo)
{
    /* „odd-even": das erste Byte traegt die ungeraden Bits (die geraden
     * sind auf 1 gesetzt), das zweite die geraden. */
    return (uint8_t)(((hi << 1) | 1u) & lo);
}

/* ── Bitstrom lesen ─────────────────────────────────────────────────────
 *
 * Ein Diskettenbyte beginnt immer mit einem gesetzten Bit; der Leser
 * synchronisiert darauf. Die Spur ist ein RING — `pos` laeuft modulo
 * `bit_count`, damit ein Feld ueber der Naht nicht verlorengeht.
 */
typedef struct {
    const uint8_t *bits;
    uint32_t       count;
    uint32_t       pos;
} a2_reader_t;

static uint8_t a2_bit(const a2_reader_t *r, uint32_t i)
{
    uint32_t k = i % r->count;
    return (uint8_t)((r->bits[k >> 3] >> (7u - (k & 7u))) & 1u);
}

/* Naechstes Diskettenbyte. `budget` begrenzt die Suche nach dem
 * Startbit, damit eine Spur ohne gesetzte Bits nicht endlos dreht. */
static bool a2_next_nibble(a2_reader_t *r, uint8_t *out, uint32_t budget)
{
    uint32_t spent = 0;
    while (!a2_bit(r, r->pos)) {
        r->pos++;
        if (++spent > budget) return false;
    }
    uint8_t b = 0;
    for (int k = 0; k < 8; k++, r->pos++)
        b = (uint8_t)((b << 1) | a2_bit(r, r->pos));
    *out = b;
    return true;
}

int uft_apple_gcr_scan_track(const uint8_t *bits, uint32_t bit_count,
                             uft_a2_sector_t *out, size_t max)
{
    if (!bits || !out || max == 0 || bit_count < 8) return -1;

    a2_reader_t r = { bits, bit_count, 0 };
    size_t found = 0;

    /* GENAU eine Umdrehung. Ein Feld darf ueber die Naht hinausreichen —
     * `a2_bit()` liest den Ring —, aber ein Feld, das ERST hinter der
     * Naht beginnt, ist dasselbe, das am Anfang schon gelesen wurde.
     *
     * Gemessen (MF-715): mit `bit_count + 8*400` als Schranke lieferten
     * 35 Spuren **595** statt 560 Sektoren — genau 17 je Spur, weil der
     * erste doppelt kam. Alle 595 waren korrekt dekodiert; die Zahl war
     * trotzdem falsch, und eine Doppelung stillschweigend zu liefern
     * waere schlimmer als eine fehlende: sie sieht aus wie ein Fund. */
    uint8_t w0 = 0, w1 = 0, w2 = 0;
    /* where each of the three nibbles began (MF-1485) */
    uint32_t p0 = 0, p1 = 0, p2 = 0;
    uft_a2_sector_t cur;
    bool cur_valid = false;
    bool cur_5_3 = false;
    memset(&cur, 0, sizeof(cur));

    /* MF-1485 (P3-638): an address field read before the seam whose data
     * field begins AFTER it. The loop used to stop at `r.pos < bit_count`
     * and that sector vanished without a word — measured at the real
     * capture copy2plus_52, T4 P15 and T5 P7 (558 instead of 560). While
     * an address field is pending the loop now reads on past the seam,
     * and stops at the first ADDRESS prologue that begins after it: that
     * is the start of the track a second time, and recording it would be
     * the 595-instead-of-560 double of MF-715. At most one more
     * revolution, so a track without a data field cannot spin forever.
     * The same holds for a PROLOGUE that began before the seam and is not
     * complete yet (`D5` before it, `AA 96` after it): measured, 32 of 6328
     * rotations of a to_woz2 track lost a sector that way even with the
     * pending-field rule alone.
     * Guarded by tests/test_apple_gcr_naht_und_pruefsumme.c. */
    while (found < max
           && (r.pos < bit_count
               || ((uint64_t)r.pos < 2u * (uint64_t)bit_count
                   && (cur_valid
                       || (w2 == 0xD5 && p2 < bit_count)
                       || (w1 == 0xD5 && w2 == 0xAA && p1 < bit_count))))) {
        uint8_t n;
        if (!a2_next_nibble(&r, &n, bit_count)) break;
        w0 = w1; w1 = w2; w2 = n;
        p0 = p1; p1 = p2; p2 = r.pos - 8u;

        if (w0 != 0xD5 || w1 != 0xAA) continue;
        if ((w2 == 0x96 || w2 == 0xB5) && p0 >= bit_count) break;

        /* Der Adress-Vorspann sagt, welche Kodierung folgt — die Spur
         * weiss es, der Aufrufer muss es nicht mitgeben:
         *
         *     D5 AA 96  ->  16 Sektoren, 6-and-2, 343 Nibbles
         *     D5 AA B5  ->  13 Sektoren, 5-and-3, 411 Nibbles
         *
         * Gemessen (MF-719) an einer 13-Sektor-Spur aus `to_woz2`:
         * 13 x `D5 AA B5`, 13 x `D5 AA AD`, **null** `D5 AA 96`. Ein
         * Abtaster, der nur 0x96 kennt, findet dort nichts — und meldet
         * dabei nicht Fehler, sondern schweigend 0. */
        if (w2 == 0x96 || w2 == 0xB5) {
            cur_5_3 = (w2 == 0xB5);
            /* Adressfeld: 4 Werte zu je zwei 4-and-4-Bytes. */
            uint8_t b[8];
            bool ok = true;
            for (int i = 0; i < 8 && ok; i++)
                ok = a2_next_nibble(&r, &b[i], bit_count);
            if (!ok) break;

            memset(&cur, 0, sizeof(cur));
            cur.volume = uft_apple_gcr_decode_4_4(b[0], b[1]);
            cur.track  = uft_apple_gcr_decode_4_4(b[2], b[3]);
            cur.sector = uft_apple_gcr_decode_4_4(b[4], b[5]);
            uint8_t sum = uft_apple_gcr_decode_4_4(b[6], b[7]);
            cur.addr_checksum_ok =
                ((uint8_t)(cur.volume ^ cur.track ^ cur.sector) == sum);
            cur_valid = true;
            w0 = w1 = w2 = 0;
        } else if (w2 == 0xAD && cur_valid) {
            /* Datenfeld. Die Laenge haengt an der Kodierung, die das
             * Adressfeld angekuendigt hat: 343 bei 6-and-2, 411 bei
             * 5-and-3 (beide gemessen, MF-715 / MF-719). */
            const size_t n = cur_5_3 ? UFT_A2_DATA_NIBBLES_13
                                     : UFT_A2_DATA_NIBBLES;
            uint8_t nib[UFT_A2_DATA_NIBBLES_13];
            bool ok = true;
            for (size_t i = 0; i < n && ok; i++)
                ok = a2_next_nibble(&r, &nib[i], bit_count);
            if (!ok) break;

            cur.has_data = true;

            /* Der Bootsektor einer 13-Sektor-Diskette traegt eine
             * ANDERE 5-and-3-Variante (Begruendung und Messung am Feld
             * `alt_encoding` im Header). Die Pruefsumme trennt die
             * beiden nicht — wer hier dekodiert, gibt plausible falsche
             * Bytes zurueck. Also wird nicht dekodiert. */
            if (cur_5_3 && cur.track == 0 && cur.sector == 0) {
                cur.alt_encoding = true;
                cur.data_checksum_ok = false;
            } else {
                cur.data_checksum_ok = cur_5_3
                    ? uft_apple_gcr_denibblize_5_3(nib, cur.data)
                    : uft_apple_gcr_denibblize_6_2(nib, cur.data);
            }
            out[found++] = cur;
            cur_valid = false;
            w0 = w1 = w2 = 0;
        }
    }
    return (int)found;
}
