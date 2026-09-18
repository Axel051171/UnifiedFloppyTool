/*
 * @file test_formatfilter_kommt_aus_der_registry.c
 * @brief Rotbeweis zu MF-1245 — die Oberflaeche hielt SIEBEN
 *        handgeschriebene Listen derselben Tatsache, und sie waren
 *        alle verschieden.
 *
 * DER BEFUND, GEMESSEN
 * --------------------
 * Sieben Dateidialoge bieten „alle Abbilder" an, jeder mit einer
 * eigenen, von Hand gepflegten Endungsliste:
 *
 *     src/gui/uft_compare_dialog.cpp:141      16 Endungen
 *     src/workflowtab.cpp:325                 14
 *     src/mainwindow.cpp                      13
 *     src/explorertab.cpp:486                  8
 *     src/workflowtab.cpp:937                  8
 *     src/gui/uft_recovery_dialog.cpp:627      7
 *     src/forensictab.cpp:203/226/233          6
 *
 * Derselbe Benutzer sieht je nach Reiter andere Formate. Gegen die
 * Registry gehalten: die registrierten Plugins beanspruchen 105
 * eindeutige Endungen, und 85 davon kommen in KEINEM Dialog des
 * ganzen Baums vor — darunter `atr`, `dmk`, `d88`, `2mg`, `cas`,
 * `dc42`, `86f`, `cqm`, Formate auf Stufe T1b.
 *
 * Das ist die Aufzaehlung-statt-Messung-Klasse, die dieser Baum
 * vierzehnmal bezahlt hat, und sie ist hier besonders teuer: eine
 * fehlende Zeile im Filter macht ein vorhandenes, geprueftes Format
 * fuer den Bediener unerreichbar.
 *
 * WAS DIESER TEST FESTNAGELT
 * --------------------------
 * (1) `uft_ext_naechste()` — die Trennregel des `extensions`-Feldes,
 *     an EINER Stelle. Sie stand bisher nur in
 *     `plugin_claims_extension()`, das FRAGT („beansprucht dieses
 *     Plugin die Endung X?") statt AUFZUZAEHLEN. Ein zweiter Laeufer
 *     fuer den Filter waere „eine Groesse, zwei Rechnungen"
 *     (`CLAUDE.md` §MF-1177).
 *
 * (2) `uft_format_endungen_sammeln()` — die Endungsmenge der Registry
 *     als fertige Zeichenkette. Der Test prueft sie NICHT gegen eine
 *     eigene Liste, sondern laeuft die Registry ab und verlangt fuer
 *     JEDES Plugin und JEDE seiner Endungen einen Treffer. Eine
 *     Handliste im Test waere dieselbe Krankheit in der Abnahme.
 *
 * WARUM ER ROT WERDEN KANN: vor diesem Commit gibt es beide Symbole
 * nicht, der Test bindet nicht. Die Sperren darunter bleiben in beiden
 * Zustaenden gruen und messen, dass ueberhaupt etwas registriert ist —
 * ohne sie waere „jede Endung ist enthalten" auch bei LEERER Registry
 * wahr (Klasse MF-1014).
 */

#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int gruen = 0;
static int rot = 0;

#define ZUSAGE(bed, text)                                                   \
    do {                                                                    \
        if (bed) { gruen++; printf("   [ok ] %s\n", (text)); }              \
        else     { rot++;   printf("   [ROT] %s\n", (text)); }              \
    } while (0)

/* Sucht `*.<endung>` als GANZES Wort in der Sammelkette. Absichtlich
 * kein blankes `strstr` auf die Endung: `ad` steckt in `adf`, und
 * genau diese Falle hat `A-027` sechsmal behandelt. */
static int kette_hat(const char *kette, const char *endung, size_t len)
{
    char muster[64];
    if (len + 3 >= sizeof muster) return 0;
    muster[0] = '*';
    muster[1] = '.';
    for (size_t i = 0; i < len; i++)
        muster[2 + i] = (char)tolower((unsigned char)endung[i]);
    muster[2 + len] = '\0';

    const size_t mlen = len + 2;
    for (const char *p = strstr(kette, muster); p; p = strstr(p + 1, muster)) {
        const char danach = p[mlen];
        const int anfang_ok = (p == kette) || (p[-1] == ' ');
        if (anfang_ok && (danach == ' ' || danach == '\0'))
            return 1;
    }
    return 0;
}

int main(void)
{
    printf("=== MF-1245: der Formatfilter kommt aus der Registry ===\n");

    /* ---- 1) DIE TRENNREGEL, EINZELN ------------------------------- */
    printf("\n1) die Trennregel des `extensions`-Feldes\n");

    /* Gemessen im Baum: 29 Eintraege trennen mit `;`, 14 mit `,`, und
     * der Header sagt nur `";"-getrennt`. Beides muss durchgehen, sonst
     * saehe ein Verbraucher eine Endung namens `adf,adz`. */
    {
        const char *feld = "adf,adz; .dsk\tmsa";
        const char *erwartet[] = { "adf", "adz", "dsk", "msa" };
        size_t n = 0, len = 0;
        const char *s = feld;
        int alle_richtig = 1;

        for (const char *e = uft_ext_naechste(&s, &len); e;
             e = uft_ext_naechste(&s, &len)) {
            if (n >= 4 || len != strlen(erwartet[n])
                || strncmp(e, erwartet[n], len) != 0)
                alle_richtig = 0;
            n++;
        }
        ZUSAGE(n == 4, "vier Eintraege aus `adf,adz; .dsk<TAB>msa`");
        ZUSAGE(alle_richtig, "und zwar genau adf, adz, dsk, msa");
    }
    {
        /* GEGENPROBE: die Regel darf nicht alles durchlassen. */
        const char *leer = "  ;; , ";
        size_t len = 0;
        const char *s = leer;
        ZUSAGE(uft_ext_naechste(&s, &len) == NULL,
               "GEGENPROBE: eine Kette aus lauter Trennern ergibt nichts");
    }

    /* ---- 2) SPERREN: die Registry ist ueberhaupt gefuellt ---------- */
    printf("\n2) Sperren — ohne sie sagt Abschnitt 3 nichts\n");

    ZUSAGE(uft_register_all_formats() == UFT_OK,
           "SPERRE: uft_register_all_formats() gelingt");
    const size_t anzahl = uft_registered_format_plugin_count();
    printf("      registrierte Plugins: %zu\n", anzahl);
    ZUSAGE(anzahl > 100,
           "SPERRE: mehr als 100 Plugins registriert (MF-447)");

    /* ---- 3) JEDE ENDUNG JEDES PLUGINS STEHT IN DER KETTE ----------- */
    printf("\n3) die Sammelkette deckt die GANZE Registry\n");

    char kette[4096];
    size_t noetig = 0;
    const bool ok = uft_format_endungen_sammeln(kette, sizeof kette, &noetig);
    ZUSAGE(ok, "die Sammelkette entsteht");
    printf("      %zu Byte noetig, Kette beginnt: %.72s...\n",
           noetig, ok ? kette : "(keine)");

    size_t geprueft = 0, fehlend = 0;
    char erster_fehler[64] = {0};
    if (ok) {
        for (size_t i = 0; i < anzahl; i++) {
            const uft_format_plugin_t *p = uft_registered_format_plugin_at(i);
            if (!p || !p->extensions) continue;
            const char *s = p->extensions;
            size_t len = 0;
            for (const char *e = uft_ext_naechste(&s, &len); e;
                 e = uft_ext_naechste(&s, &len)) {
                geprueft++;
                if (!kette_hat(kette, e, len)) {
                    if (!erster_fehler[0] && len < sizeof erster_fehler - 1)
                        memcpy(erster_fehler, e, len);
                    fehlend++;
                }
            }
        }
    }
    printf("      %zu beanspruchte Endungen geprueft, %zu fehlen%s%s\n",
           geprueft, fehlend, fehlend ? " — erste: " : "",
           fehlend ? erster_fehler : "");
    ZUSAGE(geprueft > 100,
           "SPERRE: ueber 100 beanspruchte Endungen durchlaufen");
    ZUSAGE(fehlend == 0,
           "KEINE beanspruchte Endung fehlt in der Sammelkette");

    /* NACHGETRAGEN beim Mutationslauf: ohne diese Zusage faengt der
     * Test eine entfernte Dublettenpruefung NICHT. Gemessen tragen die
     * 137 Plugins zusammen 193 beanspruchte Endungen — viele davon
     * mehrfach (`dsk` allein von einem Dutzend). Waeren sie alle in der
     * Kette, blieben alle anderen Zusagen trotzdem gruen: jede Endung
     * ist ja enthalten, und die Groessenrechnung bleibt in sich
     * stimmig. Genau die Gestalt von MF-1014. */
    size_t doppelt = 0;
    if (ok) {
        for (const char *p = strstr(kette, "*."); p; p = strstr(p + 1, "*.")) {
            const char *e = p + 2;
            size_t l = 0;
            while (e[l] && e[l] != ' ') l++;
            /* Kommt dasselbe Wort noch einmal? */
            for (const char *q = strstr(p + 1, "*."); q;
                 q = strstr(q + 1, "*.")) {
                const char *f = q + 2;
                size_t m = 0;
                while (f[m] && f[m] != ' ') m++;
                if (m == l && strncmp(e, f, l) == 0) { doppelt++; break; }
            }
        }
    }
    printf("      Dubletten in der Kette: %zu\n", doppelt);
    ZUSAGE(doppelt == 0, "und KEINE steht doppelt darin");

    /* Die Stichproben sind die, die heute in KEINEM Dialog stehen —
     * gemessen. Sie stehen hier zusaetzlich zur Schleife oben, damit
     * der Befund im Testprotokoll namentlich sichtbar ist. */
    printf("\n4) namentlich: was heute in keinem Dialog steht\n");
    if (ok) {
        const char *stichprobe[] = { "atr", "dmk", "d88", "2mg", "cas",
                                     "dc42", "86f", "cqm" };
        for (size_t i = 0; i < sizeof stichprobe / sizeof stichprobe[0]; i++) {
            char text[64];
            snprintf(text, sizeof text, "`%s` ist jetzt im Filter",
                     stichprobe[i]);
            ZUSAGE(kette_hat(kette, stichprobe[i], strlen(stichprobe[i])),
                   text);
        }
        /* GEGENPROBE: was kein Plugin beansprucht, steht auch nicht
         * drin — sonst waere „enthalten" eine Zusage ohne Inhalt. */
        ZUSAGE(!kette_hat(kette, "xyzzy", 5),
               "GEGENPROBE: `xyzzy` steht NICHT darin");
    }

    /* ---- 5) D5: melden, nicht kappen ------------------------------- */
    printf("\n5) zu kleiner Puffer wird gemeldet, nicht gekappt\n");
    {
        char klein[16];
        size_t braucht = 0;
        ZUSAGE(!uft_format_endungen_sammeln(klein, sizeof klein, &braucht),
               "zu kleiner Puffer: false");
        ZUSAGE(klein[0] == '\0',
               "und der Puffer ist LEER, nicht halb gefuellt");
        ZUSAGE(braucht == noetig,
               "und `needed` nennt dieselbe Zahl wie der gelungene Lauf");

        char *passend = malloc(braucht ? braucht : 1);
        ZUSAGE(passend != NULL, "SPERRE: Speicher fuer die Gegenprobe da");
        if (passend) {
            ZUSAGE(uft_format_endungen_sammeln(passend, braucht, NULL),
                   "GEGENPROBE: mit genau dieser Groesse gelingt es");
            ZUSAGE(strlen(passend) + 1 == braucht,
                   "und die genannte Zahl war exakt, nicht grosszuegig");
            free(passend);
        }

        size_t frage = 0;
        ZUSAGE(!uft_format_endungen_sammeln(NULL, 0, &frage)
               && frage == noetig,
               "ohne Puffer: false, aber `needed` antwortet");
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
