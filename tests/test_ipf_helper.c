/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * test_ipf_helper.c — die Prozessgrenze fuer IPF, gemessen (MF-917).
 *
 * Vertrag: docs/specs/capsimg-helper/PROTOCOL.md (Fassung 1).
 *
 * ══════════════════════════════════════════════════════════════════════
 *  WAS HIER GEPRUEFT WIRD UND WARUM GERADE DAS
 * ══════════════════════════════════════════════════════════════════════
 *
 * Weg 3 aus docs/QUARANTINE_PROCESS.md §5 steht und faellt mit EINER
 * Eigenschaft: „Faellt der Helfer aus, degradiert die Faehigkeit
 * EHRLICH — nie still." Eine Prozessgrenze, deren Ausfall nach Erfolg
 * aussieht, ist schlimmer als gar keine, weil sie eine Zusage macht,
 * die niemand mehr nachprueft.
 *
 * Darum haben die ersten vier Pruefungen KEINEN Erfolgsfall: sie
 * messen, dass jeder der vier Ausfaelle einen BENANNTEN Grund traegt.
 * Der Erfolgsfall kommt danach.
 *
 * Der Helfer selbst wird hier NICHT gestartet. Er wird als Attrappe
 * eingesetzt — dasselbe Muster wie Fc5025Runner / FluxEngineRunner
 * (MF-256/257). Das ist kein Behelf: auf dieser Maschine liegt kein
 * capsimg (gemessen: `which capsimg` leer, keine CAPSImg.dll), und die
 * Bibliothek ist GPL-unvereinbar, darf hier also nie liegen. Was diese
 * Datei belegt, ist UFTS Seite der Grenze — vollstaendig. Was sie NICHT
 * belegt, steht in PROTOCOL.md §5 als Pruefvektoren H1..H6 fuer den,
 * der die andere Seite baut.
 *
 * Baut auch einzeln:
 *   gcc -I include -o t tests/test_ipf_helper.c \
 *       src/formats/ipf/uft_ipf_helper.c
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/formats/ipf/uft_ipf_helper.h"

/* ───────────────────────────── Werkzeug ─────────────────────────── */

static int  g_tests = 0;
static void ok(const char *name) { g_tests++; printf("  OK  %s\n", name); }

static void set_env(const char *key, const char *val)
{
#ifdef _WIN32
    char buf[512];
    snprintf(buf, sizeof buf, "%s=%s", key, val ? val : "");
    _putenv(buf);
#else
    if (val) setenv(key, val, 1);
    else     unsetenv(key);
#endif
}

/* Die Attrappe: sie ersetzt den PROZESSSTART, nicht das Protokoll.
 * Sie tut genau das, was ein echter Helfer taete — den Index in die
 * ihr genannte Datei schreiben — und gibt einen Rueckgabewert. */
typedef struct {
    int         rc;        /* was der "Helfer" zurueckgibt */
    const char *out;       /* was er in die Index-Datei schreibt */
    int         calls;
    char        seen_helper[256];
    char        seen_ipf[256];
    char        seen_index[256];
    char        seen_blob[256];
} attrappe_t;

static const char *INDEX_DATEI = "test_ipf_helper_index.txt";
static const char *BLOB_DATEI  = "test_ipf_helper_beilage.bin";

static int attrappe_run(void *ctx, const char *helper, const char *ipf,
                        const char *idx, const char *blob)
{
    attrappe_t *a = ctx;
    a->calls++;
    snprintf(a->seen_helper, sizeof a->seen_helper, "%s", helper ? helper : "");
    snprintf(a->seen_ipf,    sizeof a->seen_ipf,    "%s", ipf    ? ipf    : "");
    snprintf(a->seen_index,  sizeof a->seen_index,  "%s", idx    ? idx    : "");
    snprintf(a->seen_blob,   sizeof a->seen_blob,   "%s", blob   ? blob   : "");
    remove(idx);
    if (a->out) {
        FILE *f = fopen(idx, "wb");
        assert(f != NULL);
        fputs(a->out, f);
        fclose(f);
    }
    return a->rc;
}

static uft_error_t frage(attrappe_t *a, uft_ipf_helper_reply_t *reply,
                         char *err, size_t errcap)
{
    uft_ipf_helper_runner_t r = { attrappe_run, a };
    return uft_ipf_helper_query(&r, "scheibe.ipf", INDEX_DATEI, BLOB_DATEI,
                                reply, err, errcap);
}

static void beilage_schreiben(const uint8_t *daten, size_t n)
{
    FILE *f = fopen(BLOB_DATEI, "wb");
    assert(f != NULL);
    assert(fwrite(daten, 1, n, f) == n);
    fclose(f);
}

/* ───────────────────────── 1. Kein Helfer ───────────────────────── */

static void t_kein_helfer(void)
{
    set_env(UFT_IPF_HELPER_ENV, NULL);
    assert(uft_ipf_helper_path() == NULL);

    attrappe_t a = { 0, "UFT-IPF-HELPER 1\nCYLS 1\nHEADS 1\nEND\n", 0,
                     {0}, {0}, {0}, {0} };
    uft_ipf_helper_reply_t reply;
    char err[UFT_IPF_HELPER_ERRLEN] = {0};

    uft_error_t e = frage(&a, &reply, err, sizeof err);

    /* Der Kern: nicht UFT_OK, und der Grund ist BENANNT. */
    assert(e == UFT_ERR_NOT_SUPPORTED);
    assert(strstr(err, UFT_IPF_HELPER_ENV) != NULL);
    assert(strstr(err, "nicht lesbar") != NULL);
    /* Und der Helfer wurde gar nicht erst gerufen. */
    assert(a.calls == 0);
    /* Und es steht keine halbe Antwort da. */
    assert(reply.track_count == 0);
    assert(reply.cylinders == 0);
    uft_ipf_helper_reply_free(&reply);
    ok("kein Helfer eingerichtet -> benannter Satz, kein Aufruf");
}

/* ───────────────────── 2. Helfer startet nicht ──────────────────── */

static void t_startet_nicht(void)
{
    set_env(UFT_IPF_HELPER_ENV, "/pfad/der/nicht/existiert");
    attrappe_t a = { -1, NULL, 0, {0}, {0}, {0}, {0} };
    uft_ipf_helper_reply_t reply;
    char err[UFT_IPF_HELPER_ERRLEN] = {0};

    assert(frage(&a, &reply, err, sizeof err) == UFT_ERROR_TOOL_FAILED);
    /* Der Pfad, der nicht startete, steht in der Meldung — sonst sucht
     * der Benutzer an der falschen Stelle. */
    assert(strstr(err, "/pfad/der/nicht/existiert") != NULL);
    assert(reply.track_count == 0);
    uft_ipf_helper_reply_free(&reply);
    ok("Helfer startet nicht -> TOOL_FAILED, Pfad benannt");
}

/* ────────────────────── 3. Helfer bricht ab ─────────────────────── */

static void t_bricht_ab(void)
{
    set_env(UFT_IPF_HELPER_ENV, "uft-ipf-helper");
    attrappe_t a = { 3, "ERROR capsimg lehnt die Datei ab\n", 0, {0}, {0}, {0}, {0} };
    uft_ipf_helper_reply_t reply;
    char err[UFT_IPF_HELPER_ERRLEN] = {0};

    assert(frage(&a, &reply, err, sizeof err) == UFT_ERROR_TOOL_FAILED);
    /* Rueckgabewert UND Ausgabe. Der Rueckgabewert allein sagt nichts.
     * Bei Abbruch liest UFT den Index trotzdem, um den Grund zu zeigen. */
    assert(strstr(err, "3") != NULL);
    assert(strstr(err, "capsimg lehnt die Datei ab") != NULL);
    uft_ipf_helper_reply_free(&reply);
    ok("Helfer bricht ab -> Rueckgabewert UND Ausgabe in der Meldung");
}

/* ─────────────────── 4. Antwort unverstaendlich ─────────────────── */

static void t_unverstaendlich(void)
{
    set_env(UFT_IPF_HELPER_ENV, "uft-ipf-helper");
    uft_ipf_helper_reply_t reply;
    char err[UFT_IPF_HELPER_ERRLEN];

    struct { const char *text; const char *muss_enthalten; const char *was; } f[] = {
        { "CYLS 80\nHEADS 2\nEND\n",
          "UFT-IPF-HELPER", "Antwort ohne Kennung" },
        { "UFT-IPF-HELPER 2\nCYLS 80\nHEADS 2\nEND\n",
          "Protokollfassung", "Fassung 2 (aufwaerts)" },
        { "UFT-IPF-HELPER 0\nCYLS 80\nHEADS 2\nEND\n",
          "Protokollfassung", "Fassung 0 (abwaerts)" },
        { "UFT-IPF-HELPER 1\nCYLS 80\nHEADS 2\n",
          "END", "abgeschnitten, kein END" },
        { "UFT-IPF-HELPER 1\nHEADS 2\nEND\n",
          "CYLS", "ohne CYLS" },
        { "UFT-IPF-HELPER 1\nCYLS 80\nEND\n",
          "HEADS", "ohne HEADS" },
        { "UFT-IPF-HELPER 1\nCYLS 80\nHEADS 2\nEND\nTRACK 0 0 1 0 0 0 0\n",
          "hinter", "Zeile hinter END" },
        { "UFT-IPF-HELPER 1\nCYLS 80\nHEADS 2\nTRACK 0 0 100\nEND\n",
          "7 Zahlen", "TRACK mit zu wenig Feldern" },
        { "UFT-IPF-HELPER 1\nCYLS 80\nHEADS 2\nTRACK 0 0 100 0 0 0 512\nEND\n",
          "BLOB", "Nutzdaten zugesagt, keine Beilage genannt" },
        { "UFT-IPF-HELPER 1\nCYLS 0\nHEADS 2\nEND\n",
          "CYLS", "CYLS 0" },
        { "UFT-IPF-HELPER 1\nCYLS 80\nHEADS 9\nEND\n",
          "HEADS", "HEADS 9" },
        { "UFT-IPF-HELPER 1\nCYLS 80\nHEADS 2\nWAS-IST-DAS 1\nEND\n",
          "unbekannt", "unbekanntes Schluesselwort" },
    };

    for (size_t i = 0; i < sizeof f / sizeof f[0]; i++) {
        memset(err, 0, sizeof err);
        attrappe_t a = { 0, f[i].text, 0, {0}, {0}, {0}, {0} };
        uft_error_t e = frage(&a, &reply, err, sizeof err);
        if (e != UFT_ERR_FORMAT_INVALID) {
            printf("  FEHLER: '%s' wurde AKZEPTIERT (rc=%d)\n", f[i].was, (int)e);
            assert(0);
        }
        if (!strstr(err, f[i].muss_enthalten)) {
            printf("  FEHLER: '%s' -> Meldung '%s' nennt '%s' nicht\n",
                   f[i].was, err, f[i].muss_enthalten);
            assert(0);
        }
        assert(reply.track_count == 0);
        uft_ipf_helper_reply_free(&reply);
    }
    ok("12 unverstaendliche Antworten -> je FORMAT_INVALID mit benanntem Grund");
}

/* ────────── 4b. Erfolg gemeldet, aber nichts geschrieben ────────── */

static void t_erfolg_ohne_index(void)
{
    set_env(UFT_IPF_HELPER_ENV, "uft-ipf-helper");
    /* out == NULL: die Attrappe schreibt KEINE Index-Datei und meldet
     * trotzdem 0. Genau der Fall, in dem ein nachlaessiger Helfer eine
     * stille Falschaussage erzeugen koennte. */
    attrappe_t a = { 0, NULL, 0, {0}, {0}, {0}, {0} };
    uft_ipf_helper_reply_t reply;
    char err[UFT_IPF_HELPER_ERRLEN] = {0};

    assert(frage(&a, &reply, err, sizeof err) == UFT_ERR_FORMAT_INVALID);
    assert(strstr(err, INDEX_DATEI) != NULL);
    assert(reply.track_count == 0);
    uft_ipf_helper_reply_free(&reply);
    ok("Erfolg gemeldet, kein Index geschrieben -> FORMAT_INVALID");
}

/* ─────────────────────── 5. Die gute Antwort ────────────────────── */

static void t_gute_antwort(void)
{
    set_env(UFT_IPF_HELPER_ENV, "uft-ipf-helper");

    char text[1024];
    snprintf(text, sizeof text,
             "# vom Helfer erzeugt\n"
             "UFT-IPF-HELPER 1\n"
             "BLOB %s\n"
             "CYLS 84\n"
             "HEADS 2\n"
             "PLATFORM 1\n"
             "TRACK 0 0 101376 2 0 0 8\n"
             "TRACK 0 1 101376 2 1 8 4\n"
             "TRACK 1 0 101376 5 0 12 0\n"
             "END\n", BLOB_DATEI);

    const uint8_t beilage[12] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04,
                                  0xAA, 0xBB, 0xCC, 0xDD };
    beilage_schreiben(beilage, sizeof beilage);

    attrappe_t a = { 0, text, 0, {0}, {0}, {0}, {0} };
    uft_ipf_helper_reply_t reply;
    char err[UFT_IPF_HELPER_ERRLEN] = {0};

    assert(frage(&a, &reply, err, sizeof err) == UFT_OK);

    /* Der Schnitt: UFT hat den PFAD gereicht, nicht die Daten. */
    assert(a.calls == 1);
    assert(strcmp(a.seen_helper, "uft-ipf-helper") == 0);
    assert(strcmp(a.seen_ipf,   "scheibe.ipf") == 0);
    assert(strcmp(a.seen_index, INDEX_DATEI)   == 0);
    assert(strcmp(a.seen_blob,  BLOB_DATEI)    == 0);

    assert(reply.cylinders == 84);
    assert(reply.heads     == 2);
    assert(reply.platform  == 1);
    assert(reply.track_count == 3);
    assert(strcmp(reply.blob_path, BLOB_DATEI) == 0);

    const uft_ipf_helper_track_t *t = uft_ipf_helper_find(&reply, 0, 1);
    assert(t != NULL);
    assert(t->bits == 101376);
    assert(t->density == 2);
    assert(t->flags == 1);
    assert(t->fuzzy == true);            /* Bit 0 -> unscharfe Bits */
    assert(t->blob_off == 8);
    assert(t->blob_len == 4);

    t = uft_ipf_helper_find(&reply, 0, 0);
    assert(t != NULL && t->fuzzy == false);

    /* Eine Spur, die der Helfer NICHT gemeldet hat, gibt es nicht —
     * das ist die Unterscheidung "Spur fehlt" vs. "Spur da, leer". */
    assert(uft_ipf_helper_find(&reply, 1, 1) == NULL);
    assert(uft_ipf_helper_find(&reply, 83, 0) == NULL);

    /* Nutzdaten kommen aus der Beilage, byteweise geprueft. */
    uint8_t *buf = NULL; size_t n = 0;
    assert(uft_ipf_helper_payload(&reply, uft_ipf_helper_find(&reply, 0, 1),
                                  &buf, &n, err, sizeof err) == UFT_OK);
    assert(n == 4);
    assert(memcmp(buf, beilage + 8, 4) == 0);
    free(buf);

    /* Spur ohne Nutzdaten: UFT_OK und NULL — kein leerer Puffer, der
     * wie Inhalt aussieht. */
    buf = (uint8_t *)0x1; n = 99;
    assert(uft_ipf_helper_payload(&reply, uft_ipf_helper_find(&reply, 1, 0),
                                  &buf, &n, err, sizeof err) == UFT_OK);
    assert(buf == NULL && n == 0);

    uft_ipf_helper_reply_free(&reply);
    ok("gute Antwort -> Geometrie, drei Spuren, Nutzdaten byteidentisch");
}

/* ──────────────── 6. Zusage groesser als die Beilage ────────────── */

static void t_zusage_ueberschreitet_beilage(void)
{
    set_env(UFT_IPF_HELPER_ENV, "uft-ipf-helper");

    char text[1024];
    snprintf(text, sizeof text,
             "UFT-IPF-HELPER 1\nBLOB %s\nCYLS 1\nHEADS 1\n"
             "TRACK 0 0 100 0 0 0 4096\nEND\n", BLOB_DATEI);

    const uint8_t klein[4] = { 1, 2, 3, 4 };
    beilage_schreiben(klein, sizeof klein);

    attrappe_t a = { 0, text, 0, {0}, {0}, {0}, {0} };
    uft_ipf_helper_reply_t reply;
    char err[UFT_IPF_HELPER_ERRLEN] = {0};
    assert(frage(&a, &reply, err, sizeof err) == UFT_OK);

    uint8_t *buf = (uint8_t *)0x1; size_t n = 99;
    uft_error_t e = uft_ipf_helper_payload(&reply, uft_ipf_helper_find(&reply, 0, 0),
                                           &buf, &n, err, sizeof err);
    /* Der Punkt: KEIN Puffer voller Restspeicher, der wie 4096 Byte
     * Spurinhalt aussieht. Die Zusage wird gegen die Wirklichkeit
     * gehalten, BEVOR belegt wird.
     *
     * MF-917, aus dem Mutations-Gegenbeweis gelernt: hier stand
     * `strstr(err, "4096")`, und damit blieb die Pruefung GRUEN, wenn
     * man die Grenzpruefung ersatzlos entfernte. Grund: dann greift
     * das kurze fread(), liefert denselben Fehlercode UND eine
     * Meldung, in der "4096" ebenfalls vorkommt ("4 von 4096 Byte
     * gelesen"). Die Pruefung konnte die beiden Wege nicht
     * auseinanderhalten — genau die Klasse "Zusicherung, die nicht
     * feuern kann".
     *
     * Jetzt wird auf den Satzteil geprueft, den NUR die Grenzpruefung
     * erzeugt. Was dabei ehrlich zu sagen ist: gemessen wird die
     * MELDUNG. Der zweite Unterschied — ohne die Pruefung belegt UFT
     * bis zu 1 MiB je Spur, bevor es merkt, dass die Beilage vier Byte
     * hat — ist im Test nicht beobachtbar. */
    assert(e == UFT_ERR_FILE_READ);
    assert(buf == NULL && n == 0);
    assert(strstr(err, "die Beilage hat") != NULL);

    uft_ipf_helper_reply_free(&reply);
    ok("Zusage > Beilage -> FILE_READ, kein Puffer, Zahlen benannt");
}

/* ─────────────────────── 7. Beilage fehlt ───────────────────────── */

static void t_beilage_fehlt(void)
{
    set_env(UFT_IPF_HELPER_ENV, "uft-ipf-helper");
    const char *text =
        "UFT-IPF-HELPER 1\nBLOB gibt_es_nicht_xyz.bin\nCYLS 1\nHEADS 1\n"
        "TRACK 0 0 100 0 0 0 16\nEND\n";
    attrappe_t a = { 0, text, 0, {0}, {0}, {0}, {0} };
    uft_ipf_helper_reply_t reply;
    char err[UFT_IPF_HELPER_ERRLEN] = {0};
    assert(frage(&a, &reply, err, sizeof err) == UFT_OK);

    uint8_t *buf = NULL; size_t n = 0;
    assert(uft_ipf_helper_payload(&reply, uft_ipf_helper_find(&reply, 0, 0),
                                  &buf, &n, err, sizeof err) == UFT_ERR_FILE_OPEN);
    assert(strstr(err, "gibt_es_nicht_xyz.bin") != NULL);
    assert(buf == NULL);
    uft_ipf_helper_reply_free(&reply);
    ok("Beilage fehlt -> FILE_OPEN, Pfad benannt");
}

/* ─────────────────────────────────────────────────────────────────── */

int main(void)
{
    printf("test_ipf_helper — Prozessgrenze fuer IPF (MF-917)\n");
    t_kein_helfer();
    t_startet_nicht();
    t_bricht_ab();
    t_unverstaendlich();
    t_erfolg_ohne_index();
    t_gute_antwort();
    t_zusage_ueberschreitet_beilage();
    t_beilage_fehlt();
    remove(BLOB_DATEI);
    remove(INDEX_DATEI);
    printf("%d/%d Pruefungen gruen\n", g_tests, g_tests);
    return 0;
}
