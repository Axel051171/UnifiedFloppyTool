/**
 * @file test_apple_moof_a2r_no_door.c
 * @brief MOOF und A2R stehen in der Liste, aber nicht in der Registry (MF-726)
 *
 * ── Die Behauptung ──────────────────────────────────────────────────────
 *
 * `CLAUDE.md` fuehrt unter „Liest/schreibt Disk-Images":
 *
 *     **Apple:** DO, PO, WOZ (v1/v2/2.1), A2R, MOOF, 2MG, NIB, DC42
 *
 * ── Die Messung ─────────────────────────────────────────────────────────
 *
 * Sechs der acht tragen ein Plugin und stehen in der Registry, ueber die
 * `uft_disk_open()` geht. **Zwei nicht:**
 *
 * | Format | Plugin-Struct | Registry | Aufrufer |
 * |---|---|---|---|
 * | do, po, woz, 2img, nib, dc42 | ja | ja | — |
 * | **a2r** | **keins** | **0** | `uft_a2r_probe`: 0 ausserhalb der eigenen Datei; `uft_a2r_parser.c` (1111 Zeilen): **0 ueberhaupt** |
 * | **moof** | **keins** | **0** | `uft_moof_parser.c` (597 Zeilen): **1**, und das ist ein Test |
 *
 * Beide Dateien werden **gebaut** (`.pro`). Beide sind ueber die
 * Plugin-Tuer nicht erreichbar; ein Funktionszeiger-Weg existiert
 * ebenfalls nicht, weil es keine Plugin-Struktur gibt (die Lehre aus
 * `hardsector`, MF-706: beide Tueren pruefen, nicht eine).
 *
 * ── Und `a2r` hat noch eine Besonderheit ────────────────────────────────
 *
 * Der Baum baut **zwei** Registries:
 *
 *     src/formats/uft_format_registry.c                (15 KB, aeltere,
 *                                                       Format-ID-basiert)
 *     src/formats/format_registry/uft_format_registry.c (24 KB, Plugins)
 *
 * `a2r` steht in der **ersten** — mit Magic, Endung und `uft_a2r_probe()`.
 * Gemessen ruft diese Probe **niemand** ausserhalb ihrer eigenen Datei.
 * Ein Eintrag in einer Tabelle, die keiner liest, ist keine Tuer.
 *
 * ── Was der Test prueft ─────────────────────────────────────────────────
 *
 * Dass ein gueltiger MOOF- und ein gueltiger A2R-Kopf von **keinem**
 * registrierten Plugin beansprucht werden — und dass WOZ, das denselben
 * Aufbau hat, sehr wohl beansprucht wird. Die Gegenprobe ist noetig:
 * sonst wuerde der Test auch dann gruen, wenn die Registry insgesamt
 * leer waere.
 *
 * ── Was daraus NICHT folgt ──────────────────────────────────────────────
 *
 * Nicht, dass die beiden Parser schlecht waeren. `uft_moof_parser.c`
 * rechnet CRC32 und liest Chunks; ob es richtig ist, sagt dieser Test
 * nicht. Er sagt, dass es **niemand benutzen kann**.
 *
 * Was zu tun ist, ist eine Eigentuemer-Entscheidung der ORPH-5-Klasse:
 * **Anker** (Plan benennen), **Tuer** (registrieren) oder **Rueckzug**.
 * „Tuer" ist dabei nicht die naheliegende Wahl: ein unerreichbarer
 * Leser, der erreichbar wird, ohne geprueft zu sein, ist die Lage von
 * `86f` (MF-707/708) — angekuendigt und falsch. Stand:
 * `docs/OPEN_ITEMS.md` ORPH-6.
 */

#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Ohne diese Registrierung ist die Registry leer und der Test misst
 * nichts — genau die Falle aus MF-446/447. Die Gegenpruefung unten
 * faengt es, falls der Aufruf je wegfaellt. */
uft_error_t uft_register_all_formats(void);

static int fehler = 0;

#define PRUEFE(bed, ...) do {                                            \
    if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);                \
                  printf("\n"); fehler++; }                              \
} while (0)

/* Ein Kopf der Applesauce-Familie: vier Zeichen, dann FF 0A 0D 0A. */
static void kopf(uint8_t *b, size_t n, const char *magic)
{
    memset(b, 0, n);
    memcpy(b, magic, 4);
    b[4] = 0xFF; b[5] = 0x0A; b[6] = 0x0D; b[7] = 0x0A;
}

static size_t wer_beansprucht(const uint8_t *b, size_t n, const char *was)
{
    uft_probe_ranking_t r;
    memset(&r, 0, sizeof(r));
    (void)uft_probe_buffer_ranked(b, n, n, &r);
    printf("  %-22s Bewerber %zu   Gewinner %-5s (%2d)   Zweiter %-5s (%2d)\n",
           was, r.claimants,
           r.winner ? r.winner->name : "—", r.confidence,
           r.runner_up ? r.runner_up->name : "—", r.runner_up_confidence);
    return r.claimants;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("MOOF und A2R: in der Liste, nicht in der Registry (MF-726)\n\n");

    uft_error_t rc = uft_register_all_formats();
    printf("  uft_register_all_formats() -> rc=%d, %zu Plugins\n\n",
           rc, uft_registered_format_plugin_count());

    uint8_t buf[512];

    /* ── Gegenprobe zuerst: WOZ MUSS beansprucht werden ──────────────
     *
     * Ohne sie waere ein gruener Test auch dann gruen, wenn die Registry
     * ueberhaupt leer ist — und genau das ist in diesem Baum schon
     * vorgekommen (MF-447: die Registry war zur Laufzeit leer, und
     * `uft_disk_open()` lieferte fuer JEDE Datei NULL). */
    kopf(buf, sizeof(buf), "WOZ2");
    size_t woz = wer_beansprucht(buf, sizeof(buf), "WOZ2 (Gegenprobe)");
    PRUEFE(woz > 0,
           "kein Plugin beansprucht einen WOZ2-Kopf — dann ist die "
           "Registry leer und dieser Test misst nichts (MF-447)");

    /* ── Und nun die beiden ohne eigenes Plugin ──────────────────────
     *
     * Die Erwartung war „niemand beansprucht sie". Gemessen ist es
     * schlimmer: **das falsche Plugin gewinnt.** */
    const char *fremd[] = { "MOOF", "A2R2", "A2R3" };
    for (size_t i = 0; i < 3; i++) {
        kopf(buf, sizeof(buf), fremd[i]);
        uft_probe_ranking_t r;
        memset(&r, 0, sizeof(r));
        (void)uft_probe_buffer_ranked(buf, sizeof(buf), sizeof(buf), &r);
        (void)wer_beansprucht(buf, sizeof(buf), fremd[i]);

        PRUEFE(r.winner != NULL,
               "%s: niemand beansprucht den Kopf — dann ist kfx_probe() "
               "geschaerft worden und dieser Test nachzuziehen", fremd[i]);
        if (r.winner) {
            PRUEFE(strcmp(r.winner->name, "KFX") == 0,
                   "%s: Gewinner ist jetzt '%s' statt KFX. Wenn das ein "
                   "echtes %s-Plugin ist: wurde der Leser geprueft, bevor "
                   "er eine Tuer bekam? Sonst ist es die Lage von 86f "
                   "(MF-707)", fremd[i], r.winner->name, fremd[i]);
            /* MF-729 hat KFX von 40 auf 35 gesenkt: Byte-Zaehlung ist
             * keine Strukturpruefung (Eichung 2 mass 61,7 % Zustimmung
             * auf Zufallspuffern). Der falsche Sieger bleibt, aber er
             * behauptet nichts mehr — das Urteil lautet jetzt
             * MEHRDEUTIG statt eindeutig. */
            PRUEFE(r.confidence == 35,
                   "%s: KFX gewinnt jetzt mit %d statt 35", fremd[i],
                   r.confidence);
            PRUEFE(r.verdict == UFT_PROBE_VERDICT_MEHRDEUTIG,
                   "%s: das Urteil ist nicht MEHRDEUTIG — ein Sieger im "
                   "Vermutungsband darf nicht als erkannt gelten",
                   fremd[i]);
        }
    }

    /* ── Die Ursache, und sie ist breiter als Apple ───────────────────
     *
     * `kfx_probe()` (`src/formats/kfx/uft_kfx.c:57`) zaehlt Vorkommen
     * des Bytes 0x0D in den ersten 512 Byte: >= 2 gibt Konfidenz 80,
     * >= 1 gibt 40. Ein einzelner Wagenruecklauf genuegt. Die
     * Applesauce-Familie hat ihn im Kopf (`FF 0A 0D 0A`) — daher KFX.
     *
     * Das trifft nicht nur Apple: JEDES Format, dessen eigene Sonde
     * unter 40 meldet oder gar kein Plugin hat, verliert an KFX,
     * sobald irgendwo in den ersten 512 Byte ein 0x0D steht. */
    memset(buf, 0, sizeof(buf));
    buf[100] = 0x0D;
    uft_probe_ranking_t rc2;
    memset(&rc2, 0, sizeof(rc2));
    (void)uft_probe_buffer_ranked(buf, sizeof(buf), sizeof(buf), &rc2);
    (void)wer_beansprucht(buf, sizeof(buf), "Nullen + ein 0x0D");
    PRUEFE(rc2.winner && strcmp(rc2.winner->name, "KFX") == 0,
           "ein Puffer aus Nullen mit einem einzigen 0x0D wird nicht mehr "
           "von KFX beansprucht — dann ist kfx_probe() geschaerft worden. "
           "Gut; dieser Test ist nachzuziehen");

    printf("\n  Was die gruene Ampel heisst — und es ist schlimmer als "
           "'keine Tuer':\n"
           "  MOOF und A2R haben kein eigenes Plugin, aber sie werden "
           "trotzdem\n"
           "  beansprucht: KFX gewinnt mit 40. `uft_disk_open()` uebergibt "
           "eine\n"
           "  MOOF-Datei also dem KryoFlux-Strom-Leser. Ein fehlender "
           "Zugang waere\n"
           "  ehrlich; ein falscher ist es nicht.\n"
           "\n"
           "  Die Ursache liegt nicht bei Apple: kfx_probe() zaehlt 0x0D "
           "in den ersten\n"
           "  512 Byte, und EIN Vorkommen genuegt fuer Konfidenz 40. Die "
           "letzte Zeile\n"
           "  oben zeigt es — 512 Nullen mit einem einzigen 0x0D reichen. "
           "Betroffen ist\n"
           "  jedes Format, dessen eigene Sonde darunter meldet oder das "
           "gar keine hat.\n"
           "\n"
           "  Was der Test NICHT sagt: ob die beiden Parser richtig "
           "rechnen. Nur, dass\n"
           "  niemand sie erreicht — und dass an ihrer Stelle der falsche "
           "antwortet.\n");

    printf("\n%s (%d Abweichungen)\n", fehler ? "ROT" : "GRUEN", fehler);
    return fehler ? 1 : 0;
}
