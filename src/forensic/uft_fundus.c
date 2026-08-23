/**
 * @file uft_fundus.c
 * @brief Anhaengender Aufnahme-Speicher mit Herkunft (MF-503).
 *
 * Zusicherungen, Begruendungen und Grenzen stehen im Header.
 */

#include "uft/forensic/uft_fundus.h"
#include "uft/core/uft_sha256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  include <direct.h>
#  define UFT_MKDIR(p) _mkdir(p)
#else
#  include <sys/stat.h>
#  include <sys/types.h>
#  define UFT_MKDIR(p) mkdir((p), 0755)
#endif

#define FUNDUS_MANIFEST_NAME  "manifest.jsonl"
#define FUNDUS_LINE_MAX       2048

/* ── Kleinkram ──────────────────────────────────────────────────────── */

static void hex32(const uint8_t in[32], char out[65])
{
    static const char *d = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        out[i * 2]     = d[in[i] >> 4];
        out[i * 2 + 1] = d[in[i] & 0x0F];
    }
    out[64] = '\0';
}

/**
 * Eine Zeichenkette JSON-gerecht anhaengen.
 *
 * Freitext enthaelt irgendwann ein Anfuehrungszeichen oder einen
 * Zeilenumbruch. Bricht das die Zeile, ist ab da das ganze Manifest
 * unlesbar — und ein Archiv mit unlesbarem Manifest ist ein Haufen
 * Dateien. Deshalb wird hier maskiert und nicht gehofft.
 *
 * @return false, wenn der Platz nicht reicht. Ein abgeschnittener
 *         Manifest-Eintrag waere schlimmer als gar keiner.
 */
static bool json_str(char *buf, size_t cap, size_t *at, const char *s)
{
    if (*at + 1 >= cap) return false;
    buf[(*at)++] = '"';
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        const char *esc = NULL;
        char tmp[8];
        switch (c) {
        case '"':  esc = "\\\""; break;
        case '\\': esc = "\\\\"; break;
        case '\n': esc = "\\n";  break;
        case '\r': esc = "\\r";  break;
        case '\t': esc = "\\t";  break;
        default:
            if (c < 0x20) {
                snprintf(tmp, sizeof(tmp), "\\u%04x", c);
                esc = tmp;
            }
            break;
        }
        if (esc) {
            size_t n = strlen(esc);
            if (*at + n + 1 >= cap) return false;
            memcpy(buf + *at, esc, n);
            *at += n;
        } else {
            if (*at + 2 >= cap) return false;
            buf[(*at)++] = (char)c;
        }
    }
    if (*at + 1 >= cap) return false;
    buf[(*at)++] = '"';
    return true;
}

/**
 * Ein Feld anhaengen — aber nur, wenn es einen Wert hat.
 *
 * Was der Aufrufer nicht mitteilt, steht nicht im Manifest. „Unbekannter
 * Bediener" ist eine andere Aussage als „Bediener: unbekannt", und ein
 * Archiv, das Luecken mit Standardwerten fuellt, erzeugt Herkunftsangaben,
 * die niemand gemacht hat.
 */
static bool json_field(char *buf, size_t cap, size_t *at,
                       const char *key, const char *val)
{
    if (!val || !*val) return true;
    if (*at + 2 >= cap) return false;
    buf[(*at)++] = ',';
    if (!json_str(buf, cap, at, key)) return false;
    if (*at + 1 >= cap) return false;
    buf[(*at)++] = ':';
    return json_str(buf, cap, at, val);
}

/**
 * Ein Zeichenketten-Feld aus einer Manifest-Zeile holen — entmaskiert.
 *
 * Ein winziger Leser statt eines JSON-Parsers: das Manifest wird von
 * DIESEM Code geschrieben, die Feldform ist bekannt, und eine
 * Fremdbibliothek dafuer waere eine Abhaengigkeit fuer eine Zeile.
 * Fremde Manifeste zu lesen ist nicht der Zweck.
 *
 * **Entmaskiert richtig.** Die erste Fassung uebersprang den Rueckstrich
 * und uebernahm das naechste Zeichen unveraendert — aus `\n` wurde damit
 * ein `n`. Solange nur der Dateiname gelesen wurde, fiel das nicht auf
 * (Dateinamen enthalten nichts Maskiertes); sobald Freitext
 * zurueckgelesen wird, waere es eine stille Verfaelschung: der Bediener
 * uebernaehme beim naechsten Mal einen Text, der nie so dastand.
 */
static bool line_str_field(const char *line, const char *key,
                           char *out, size_t cap)
{
    if (!line || !key || !out || cap == 0) return false;
    out[0] = '\0';

    char pat[64];
    if ((size_t)snprintf(pat, sizeof(pat), "\"%s\":\"", key) >= sizeof(pat))
        return false;
    const char *k = strstr(line, pat);
    if (!k) return false;
    k += strlen(pat);

    size_t n = 0;
    while (*k && *k != '"' && n + 1 < cap) {
        if (*k == '\\' && k[1]) {
            k++;
            switch (*k) {
            case 'n': out[n++] = '\n'; break;
            case 'r': out[n++] = '\r'; break;
            case 't': out[n++] = '\t'; break;
            case 'u': {
                /* Nur die Steuerzeichen, die json_str() so schreibt. */
                unsigned v = 0;
                if (k[1] && k[2] && k[3] && k[4]) {
                    char hx[5] = { k[1], k[2], k[3], k[4], 0 };
                    v = (unsigned)strtoul(hx, NULL, 16);
                    k += 4;
                }
                out[n++] = (char)(v & 0xFF);
                break;
            }
            default: out[n++] = *k; break;   /* " und \\ */
            }
            k++;
        } else {
            out[n++] = *k++;
        }
    }
    out[n] = '\0';
    return true;
}

/** Eine Zahl aus einer Manifest-Zeile holen; 0 wenn nicht da. */
static unsigned line_uint_field(const char *line, const char *key)
{
    char pat[64];
    if ((size_t)snprintf(pat, sizeof(pat), "\"%s\":", key) >= sizeof(pat))
        return 0;
    const char *k = strstr(line, pat);
    if (!k) return 0;
    return (unsigned)strtoul(k + strlen(pat), NULL, 10);
}

/* ── Oeffnen ────────────────────────────────────────────────────────── */

bool uft_fundus_open(const char *dir, uft_fundus_t *out)
{
    if (!dir || !*dir || !out) return false;
    memset(out, 0, sizeof(*out));

    if (strlen(dir) + sizeof(FUNDUS_MANIFEST_NAME) + 2 >= UFT_FUNDUS_PATH_MAX)
        return false;

    /* Ein bestehendes Verzeichnis ist kein Fehler — der Fundus wird
     * fortgesetzt, nicht neu angelegt. */
    UFT_MKDIR(dir);

    snprintf(out->dir, sizeof(out->dir), "%s", dir);
    snprintf(out->manifest, sizeof(out->manifest), "%s/%s",
             dir, FUNDUS_MANIFEST_NAME);

    /* Die naechste Nummer kommt aus dem MANIFEST, nicht aus dem
     * Verzeichnis: wer von aussen eine Datei loescht, darf keine Nummer
     * freigeben — zwei verschiedene Aufnahmen unter einem Namen waeren
     * nicht mehr auseinanderzuhalten. */
    out->next_seq = 1;
    FILE *m = fopen(out->manifest, "rb");
    if (m) {
        char line[FUNDUS_LINE_MAX];
        while (fgets(line, sizeof(line), m)) {
            const char *k = strstr(line, "\"seq\":");
            if (!k) continue;
            unsigned seq = (unsigned)strtoul(k + 6, NULL, 10);
            if (seq >= out->next_seq) out->next_seq = seq + 1;
        }
        fclose(m);
    } else {
        /* Kein Manifest? Dann muss es anlegbar sein — sonst ist der
         * Fundus nicht benutzbar, und das soll jetzt auffallen und nicht
         * erst nach der ersten Aufnahme. */
        FILE *probe = fopen(out->manifest, "ab");
        if (!probe) return false;
        fclose(probe);
    }
    return true;
}

void uft_fundus_close(uft_fundus_t *f)
{
    if (!f) return;
    memset(f, 0, sizeof(*f));
}

/* ── Anhaengen ──────────────────────────────────────────────────────── */

bool uft_fundus_add(uft_fundus_t *f, const void *data, size_t size,
                    const char *suffix, const uft_fundus_meta_t *meta,
                    char *out_path, size_t out_path_size)
{
    if (!f || !f->dir[0] || !data || size == 0) return false;
    if (!suffix || !*suffix) suffix = "bin";

    char name[64];
    snprintf(name, sizeof(name), "cap_%04u.%s", f->next_seq, suffix);

    char path[UFT_FUNDUS_PATH_MAX];
    if ((size_t)snprintf(path, sizeof(path), "%s/%s", f->dir, name)
        >= sizeof(path))
        return false;

    /* Reihenfolge: Daten, Pruefsumme, Manifest. Ein Abbruch hinterlaesst
     * so hoechstens eine Datei ohne Eintrag — nie einen Eintrag ohne
     * Datei. Das erste ist Aufraeumarbeit, das zweite waere ein Manifest,
     * das etwas behauptet. */
    FILE *fp = fopen(path, "wb");
    if (!fp) return false;
    size_t wrote = fwrite(data, 1, size, fp);
    int cerr = fclose(fp);
    if (wrote != size || cerr != 0) { remove(path); return false; }

    uint8_t digest[32];
    char hex[65];
    uft_sha256(data, size, digest);
    hex32(digest, hex);

    char side[UFT_FUNDUS_PATH_MAX];
    if ((size_t)snprintf(side, sizeof(side), "%s.sha256", path)
        >= sizeof(side)) { remove(path); return false; }

    /* Format von `sha256sum`: Hex, zwei Zeichen Trenner, Dateiname OHNE
     * Verzeichnisanteil — sonst laesst sich die Datei nicht gemeinsam mit
     * ihrer Pruefsumme verschieben, und genau das tut man mit Archiven.
     * Der Stern ist der Binaermodus; Artefakte sind binaer. */
    fp = fopen(side, "wb");
    if (!fp) { remove(path); return false; }
    fprintf(fp, "%s *%s\n", hex, name);
    if (fclose(fp) != 0) { remove(side); remove(path); return false; }

    char line[FUNDUS_LINE_MAX];
    size_t at = 0;
    int n = snprintf(line, sizeof(line), "{\"seq\":%u", f->next_seq);
    if (n < 0 || (size_t)n >= sizeof(line)) { remove(side); remove(path); return false; }
    at = (size_t)n;

    bool okf = true;
    okf = okf && json_field(line, sizeof(line), &at, "file", name);
    okf = okf && json_field(line, sizeof(line), &at, "sha256", hex);
    {
        char szbuf[32];
        snprintf(szbuf, sizeof(szbuf), "%zu", size);
        if (at + 10 < sizeof(line)) {
            int k = snprintf(line + at, sizeof(line) - at, ",\"bytes\":%s",
                             szbuf);
            if (k < 0 || (size_t)k >= sizeof(line) - at) okf = false;
            else at += (size_t)k;
        } else okf = false;
    }
    if (meta) {
        okf = okf && json_field(line, sizeof(line), &at, "identifier",
                                meta->identifier);
        okf = okf && json_field(line, sizeof(line), &at, "description",
                                meta->description);
        okf = okf && json_field(line, sizeof(line), &at, "notes",
                                meta->notes);
        okf = okf && json_field(line, sizeof(line), &at, "operator",
                                meta->operator_id);
        okf = okf && json_field(line, sizeof(line), &at, "capture_protocol",
                                meta->capture_protocol);
        okf = okf && json_field(line, sizeof(line), &at, "tool", meta->tool);
        /* Der Verweis auf die Herkunftskette (MF-504). Er steht hier und
         * nicht in der Bruecke, weil der Fundus der einzige Schreiber des
         * Manifests bleibt — zwei Schreiber waeren zwei Formate. */
        okf = okf && json_field(line, sizeof(line), &at, "chain_hash",
                                meta->chain_hash);

        /* Der Zustand wird nur geschrieben, wenn jemand ihn behauptet
         * hat — UNSPECIFIED ist keine Aussage (MF-506). */
        if (meta->state == UFT_FUNDUS_STATE_COMPLETE)
            okf = okf && json_field(line, sizeof(line), &at, "state",
                                    "complete");
        else if (meta->state == UFT_FUNDUS_STATE_INTERRUPTED)
            okf = okf && json_field(line, sizeof(line), &at, "state",
                                    "interrupted");

        if (meta->continues_seq > 0 && at + 24 < sizeof(line)) {
            int k = snprintf(line + at, sizeof(line) - at,
                             ",\"continues\":%u", meta->continues_seq);
            if (k < 0 || (size_t)k >= sizeof(line) - at) okf = false;
            else at += (size_t)k;
        }
    }
    if (!okf || at + 3 >= sizeof(line)) { remove(side); remove(path); return false; }
    line[at++] = '}';
    line[at++] = '\n';
    line[at]   = '\0';

    /* "ab" — anhaengen. Das Manifest waechst nur am Ende; es wird nie neu
     * geschrieben, und deshalb ist es eine Zeile je Eintrag und kein
     * JSON-Array. */
    fp = fopen(f->manifest, "ab");
    if (!fp) { remove(side); remove(path); return false; }
    size_t mw = fwrite(line, 1, at, fp);
    cerr = fclose(fp);
    if (mw != at || cerr != 0) { remove(side); remove(path); return false; }

    f->next_seq++;
    if (out_path && out_path_size)
        snprintf(out_path, out_path_size, "%s", path);
    return true;
}

/* ── Nachrechnen ────────────────────────────────────────────────────── */

bool uft_fundus_verify(const uft_fundus_t *f, uft_fundus_verify_t *r)
{
    if (!f || !f->manifest[0] || !r) return false;
    memset(r, 0, sizeof(*r));

    FILE *m = fopen(f->manifest, "rb");
    if (!m) return true;              /* leerer Fundus ist kein Fehler */

    char line[FUNDUS_LINE_MAX];
    while (fgets(line, sizeof(line), m)) {
        char name[UFT_FUNDUS_PATH_MAX];
        if (!line_str_field(line, "file", name, sizeof(name)) || !name[0])
            continue;

        const char *k = strstr(line, "\"sha256\":\"");
        if (!k) continue;
        char want[65];
        snprintf(want, sizeof(want), "%.64s", k + 10);

        r->checked++;

        char path[UFT_FUNDUS_PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", f->dir, name);

        FILE *fp = fopen(path, "rb");
        if (!fp) {
            r->missing++;
            if (!r->first_bad[0])
                snprintf(r->first_bad, sizeof(r->first_bad), "%s", name);
            continue;
        }

        /* Stueckweise lesen: eine Aufnahme kann Dutzende Megabyte haben,
         * und ein Pruefwerkzeug, das dafuer Speicher braucht, versagt
         * ausgerechnet beim grossen Archiv. */
        uft_sha256_ctx_t ctx;
        uft_sha256_init(&ctx);
        uint8_t buf[64 * 1024];
        size_t got;
        while ((got = fread(buf, 1, sizeof(buf), fp)) > 0)
            uft_sha256_update(&ctx, buf, got);
        int rerr = ferror(fp);
        fclose(fp);

        uint8_t digest[32];
        char hex[65];
        uft_sha256_final(&ctx, digest);
        hex32(digest, hex);

        if (rerr || strncmp(hex, want, 64) != 0) {
            r->mismatched++;
            if (!r->first_bad[0])
                snprintf(r->first_bad, sizeof(r->first_bad), "%s", name);
        } else {
            r->ok++;
        }
    }
    fclose(m);
    return true;
}

/* ── Wiedererkennung (MF-506) ───────────────────────────────────────── */

bool uft_fundus_recall(const uft_fundus_t *f, const char *identifier,
                       uft_fundus_recall_t *out)
{
    if (!f || !f->manifest[0] || !identifier || !*identifier || !out)
        return false;
    memset(out, 0, sizeof(*out));

    FILE *m = fopen(f->manifest, "rb");
    if (!m) return true;            /* leerer Fundus ist kein Fehler */

    char line[FUNDUS_LINE_MAX];
    while (fgets(line, sizeof(line), m)) {
        char id[192];
        if (!line_str_field(line, "identifier", id, sizeof(id))) continue;
        /* GENAU vergleichen: "DISK-1" ist nicht "DISK-10". Ein Vergleich
         * nach blossem Vorkommen liesse eine Diskette die Notizen einer
         * anderen tragen. */
        if (strcmp(id, identifier) != 0) continue;

        /* Weiterlesen statt abbrechen: gesucht ist der JUENGSTE Eintrag,
         * und angehaengt wird am Ende. Wer die Beschreibung zwischendurch
         * praezisiert hat, will die praezisere zurueck. */
        out->found = true;
        out->seq           = line_uint_field(line, "seq");
        out->continues_seq = line_uint_field(line, "continues");
        line_str_field(line, "description", out->description,
                       sizeof(out->description));
        line_str_field(line, "notes", out->notes, sizeof(out->notes));
        line_str_field(line, "capture_protocol", out->capture_protocol,
                       sizeof(out->capture_protocol));
        line_str_field(line, "file", out->file, sizeof(out->file));

        char st[32];
        out->state = UFT_FUNDUS_STATE_UNSPECIFIED;
        if (line_str_field(line, "state", st, sizeof(st))) {
            if (strcmp(st, "complete") == 0)
                out->state = UFT_FUNDUS_STATE_COMPLETE;
            else if (strcmp(st, "interrupted") == 0)
                out->state = UFT_FUNDUS_STATE_INTERRUPTED;
        }
    }
    fclose(m);
    return true;
}
