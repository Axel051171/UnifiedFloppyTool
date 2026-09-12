/**
 * @file test_scp_schreiber_gegen_samdisk.c
 * @brief Der SCP-Schreiber hatte keinen Aufrufer, keinen Test — und zwei
 *        Befunde (MF-1055)
 *
 * ── Warum dieser Test jetzt entsteht ────────────────────────────────────
 *
 * `P3-342` beschreibt den Weg zum FluxEngine-Schreibpfad in zwei
 * Schritten, und Schritt (a) lautet dort: *„den `FluxStream` in einen
 * SCP-Behaelter schreiben — der Schreiber liegt im Baum,
 * `src/formats/scp/uft_scp_writer.c`, es fehlt nur die Verdrahtung."*
 *
 * **Gemessen MF-1055 stimmt der erste Halbsatz und der zweite nicht.**
 * Der Schreiber liegt im Baum, aber ueber `git ls-files` gemessen hat er
 * **keinen Aufrufer und keinen Test** — und er hat zwei Befunde. Einen
 * ungeprueften Schreiber an einen Pfad zu haengen, der auf eine echte
 * Diskette schreibt, waere genau die Wette, die die EINFRIER-REGEL
 * verbietet.
 *
 * ── Die Referenz ────────────────────────────────────────────────────────
 *
 * `src/samdisk/scp.cpp` — SAMdisk, **MIT, im Baum vendoriert**, also
 * lesbar ohne Lizenzfrage. Sie legt beides ausdruecklich fest:
 *
 *     uint8_t heads;      // 0 = both heads, 1 = head 0 only,
 *                         // 2 = head 1 only                    (:32)
 *     uint32_t checksum;  // 32-bit checksum from after header
 *                         // to EOF                             (:34)
 *
 * und prueft die Pruefsumme auch wirklich nach:
 *
 *     auto checksum = std::accumulate(
 *         file.data().begin() + STANDARD_TDH_OFFSET,  // 0x10
 *         file.data().end(), uint32_t(0));            (:157)
 *
 * **Zweite Hand, ausgefuehrt:** `hxcfe` liest eine vom UFT-Schreiber
 * erzeugte Datei und meldet je Spur „Sanity Checker: Track n/Side 1 not
 * allocated ?!?" — es glaubt dem `heads = 0` und sucht eine zweite
 * Seite, die nicht da ist.
 *
 * ── Die zwei Befunde, beide gemessen ────────────────────────────────────
 *
 * **1. `heads` war fest auf 0 verdrahtet.** `scp_writer_create()` setzte
 * `w->header.heads = 0` mit dem Kommentar „Both sides" und liess es dort,
 * egal welche Seiten spaeter hinzukamen. Eine einseitige Aufnahme
 * behauptete damit, beidseitig zu sein.
 *
 * **2. Die Pruefsumme deckte die falsche Spanne.** Sie summierte die
 * Spurkoepfe, die Umdrehungskoepfe und den Fluss — aber **nicht** die
 * 672 Byte Spuroffset-Tafel, **nicht** die Herkunftszeichenketten und
 * **nicht** den 48-Byte-Footer. Gemessen an einer Datei von 400 822 Byte:
 * im Kopf stand `0x01FAA3EC`, die Regel ergibt `0x01FAB15A`.
 *
 * ── Was dieser Test bewusst NICHT tut ───────────────────────────────────
 *
 * Er liest die erzeugte Datei **byteweise** und nicht ueber UFTs eigenen
 * SCP-Leser. Ein Schreiber, den nur der eigene Leser abnimmt, ist ein
 * geschlossener Kreis — dieselbe Falle wie bei `apridisk` (MF-1009) und
 * `qrst` (MF-1028), wo Packer und Entpacker Spiegelbilder derselben
 * Erfindung waren.
 *
 * Und er sagt **nichts** darueber, ob eine so erzeugte Datei ein Laufwerk
 * richtig beschreiben wuerde. Das braucht Hardware (MF-310).
 */
#include "uft/formats/uft_scp_writer.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define ZELLEN    2000u     /* je Spur; klein genug fuer einen Test */
#define TICK_NS     25u

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s — %s\n", was, detail ? detail : ""); }
}

static void pfad_bauen(char *p, size_t n, const char *name)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(p, n, "%s/uft_scpw_%s.scp", d, name);
}

static uint8_t *lies(const char *p, size_t *n)
{
    FILE *f = fopen(p, "rb");
    uint8_t *b;
    long len;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) { fclose(f); return NULL; }
    b = malloc((size_t)len);
    if (!b || fread(b, 1, (size_t)len, f) != (size_t)len) {
        free(b); fclose(f); return NULL;
    }
    fclose(f);
    *n = (size_t)len;
    return b;
}

/* Schreibt `spuren` Spuren auf die angegebenen Seiten. `seiten` ist eine
 * Bitmaske: 1 = Seite 0, 2 = Seite 1, 3 = beide. */
static int schreibe(const char *pfad, int spuren, int seiten)
{
    scp_writer_t *w = scp_writer_create(SCP_TYPE_PC_DD, 1);
    uint32_t *flux;
    int t, rc = 0;

    if (!w) return -1;
    flux = malloc(ZELLEN * sizeof(uint32_t));
    if (!flux) { scp_writer_free(w); return -1; }

    for (t = 0; t < spuren && rc == 0; t++) {
        int s;
        for (s = 0; s < 2 && rc == 0; s++) {
            uint32_t zelle, dauer = 0;
            unsigned i;
            if (!(seiten & (1 << s))) continue;
            zelle = 4000u + (uint32_t)t * 100u + (uint32_t)s * 50u;
            for (i = 0; i < ZELLEN; i++) { flux[i] = zelle; dauer += zelle; }
            rc = scp_writer_add_track(w, t, s, flux, ZELLEN, dauer, 0);
        }
    }
    if (rc == 0) rc = scp_writer_save(w, pfad);
    scp_writer_free(w);
    free(flux);
    return rc;
}

/* Die Regel aus samdisk/scp.cpp:157 — Summe ab Versatz 0x10 bis EOF. */
static uint32_t pruefsumme_nach_regel(const uint8_t *d, size_t n)
{
    uint32_t s = 0;
    size_t i;
    for (i = 0x10; i < n; i++) s += d[i];
    return s;
}

static uint32_t le32(const uint8_t *d) {
    return (uint32_t)d[0] | ((uint32_t)d[1] << 8)
         | ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24);
}

int main(void)
{
    char p0[600], p1[600], pb[600], det[240];
    uint8_t *d;
    size_t n;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("SCP-Schreiber gegen samdisk/scp.cpp (MIT, im Baum) — MF-1055\n");
    printf("============================================================\n");

    pfad_bauen(p0, sizeof p0, "s0");
    pfad_bauen(p1, sizeof p1, "s1");
    pfad_bauen(pb, sizeof pb, "beide");

    /* 1 — der Schreiber laeuft ueberhaupt durch. */
    pruefe("scp_writer_save meldet Erfolg", schreibe(p0, 4, 1) == 0, "rc != 0");

    d = lies(p0, &n);
    if (!d) {
        pruefe("die Datei ist lesbar", 0, "keine Datei");
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    /* 2 — Kopf: Kennung und Fassung. */
    snprintf(det, sizeof det, "%c%c%c rev=0x%02x", d[0], d[1], d[2], d[3]);
    pruefe("Kopf traegt \"SCP\" und die Fassung 0x19",
           n > 16 && d[0] == 'S' && d[1] == 'C' && d[2] == 'P'
           && d[3] == 0x19, det);

    /* 3 — heads sagt, WELCHE Seiten drin sind. Nur Seite 0 -> 1.
     *     DER KERN-ROTBEWEIS: der Vorzustand schrieb hier unbedingt 0. */
    snprintf(det, sizeof det, "heads=%u (0=beide, 1=nur Kopf 0, "
             "2=nur Kopf 1)", d[10]);
    pruefe("nur Seite 0 geschrieben -> heads == 1", d[10] == 1, det);

    /* 4 — die Pruefsumme deckt die Spanne, die samdisk/scp.cpp:157
     *     nachrechnet: ab Versatz 0x10 bis EOF. */
    {
        uint32_t gesp = le32(d + 12);
        uint32_t soll = pruefsumme_nach_regel(d, n);
        snprintf(det, sizeof det, "Kopf 0x%08X, Regel 0x%08X, "
                 "Datei %zu Byte", gesp, soll, n);
        pruefe("die Pruefsumme ist die Summe ab Versatz 0x10 bis EOF",
               gesp == soll, det);
    }

    /* 5 — Start- und Endspur in SCP-Zaehlung (spur*2 + seite). */
    snprintf(det, sizeof det, "start=%u end=%u", d[6], d[7]);
    pruefe("start_track 0 und end_track 6 bei vier Spuren auf Seite 0",
           d[6] == 0 && d[7] == 6, det);

    /* 6 — die Zelldauern stehen als 16-Bit BIG-ENDIAN in 25-ns-Ticks.
     *     Nachgerechnet an der ersten Spur, ohne UFTs eigenen Leser. */
    {
        uint32_t off = le32(d + 16);       /* Offset der SCP-Spur 0 */
        int ok = 0;
        snprintf(det, sizeof det, "Spuroffset %u", off);
        if (off + 4 + 12 + 2 <= n && d[off] == 'T' && d[off + 1] == 'R'
            && d[off + 2] == 'K') {
            /* Spurkopf 4 Byte, dann ein Umdrehungskopf zu 12 Byte. */
            uint32_t flux_off = le32(d + off + 4 + 8);
            uint32_t soll_ticks = 4000u / TICK_NS;   /* Spur 0, Seite 0 */
            if ((size_t)off + flux_off + 2 <= n) {
                uint32_t kam = ((uint32_t)d[off + flux_off] << 8)
                             | d[off + flux_off + 1];
                ok = (kam == soll_ticks);
                snprintf(det, sizeof det,
                         "erste Zelle %u Ticks, erwartet %u",
                         kam, soll_ticks);
            }
        }
        pruefe("die erste Zelle der Spur 0 steht als 160 Ticks "
               "(4000 ns / 25) in Big-Endian", ok, det);
    }
    free(d);

    /* 7 — nur Seite 1 -> heads == 2. */
    if (schreibe(p1, 2, 2) == 0 && (d = lies(p1, &n)) != NULL) {
        snprintf(det, sizeof det, "heads=%u start=%u end=%u",
                 d[10], d[6], d[7]);
        pruefe("nur Seite 1 geschrieben -> heads == 2, Spuren 1 und 3",
               d[10] == 2 && d[6] == 1 && d[7] == 3, det);
        free(d);
    } else {
        pruefe("nur Seite 1 geschrieben -> heads == 2", 0, "kein Ergebnis");
    }

    /* 8 — beide Seiten -> heads == 0, und DAS ist dann keine Behauptung
     *     mehr, sondern die Wahrheit. */
    if (schreibe(pb, 2, 3) == 0 && (d = lies(pb, &n)) != NULL) {
        uint32_t gesp = le32(d + 12);
        uint32_t soll = pruefsumme_nach_regel(d, n);
        snprintf(det, sizeof det, "heads=%u, Pruefsumme %s",
                 d[10], gesp == soll ? "stimmt" : "stimmt NICHT");
        pruefe("beide Seiten geschrieben -> heads == 0, Pruefsumme stimmt",
               d[10] == 0 && gesp == soll, det);
        free(d);
    } else {
        pruefe("beide Seiten geschrieben -> heads == 0", 0, "kein Ergebnis");
    }

    remove(p0);
    remove(p1);
    remove(pb);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
