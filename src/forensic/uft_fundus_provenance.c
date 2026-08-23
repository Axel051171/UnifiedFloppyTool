/**
 * @file uft_fundus_provenance.c
 * @brief Ein Artefakt zitiert seine Herkunftskette (MF-504).
 *
 * Begruendung und Vertrag stehen im Header.
 */

#include "uft/forensic/uft_fundus_provenance.h"

#include <stdio.h>
#include <string.h>

bool uft_fundus_add_from_chain(uft_fundus_t *f, const void *data, size_t size,
                               const char *suffix,
                               const uft_provenance_chain_t *chain,
                               const uft_fundus_meta_t *extra,
                               char *out_path, size_t out_path_size)
{
    if (!f || !data || size == 0 || !chain) return false;

    /* Eine leere Kette hat nichts zu zitieren. Ein Artefakt mit einem
     * Verweis auf nichts waere schlechter als eines ohne Verweis: das
     * erste sieht belegt aus.
     *
     * Diese Zeile ist zugleich eine GRENZPRUEFUNG und nicht bloss eine
     * Hoeflichkeit: `uft_prov_verify()` gibt fuer eine leere Kette
     * ausdruecklich `true` zurueck (uft_provenance.c: "if (chain->count
     * == 0) return true;"). Ohne sie liefe der Code weiter und griffe auf
     * `entries[count - 1]` zu — also auf `entries[-1]`.
     *
     * Ihr Rotbeweis feuert deshalb NICHT, und das ist kein Schein-Test:
     * die Zeile zu entfernen erzeugt undefiniertes Verhalten statt einer
     * definierten falschen Antwort. Der Versuch scheitert dann zufaellig
     * (der Muell hinter dem Feld sprengt die Manifest-Zeile), und ein
     * Beweis, der aus Zufall gruen wird, belegt nichts. Wo so etwas
     * auffallen wuerde, ist der Sanitizer-Lauf der CI — lokal fehlt
     * libubsan in dieser MinGW-Installation. */
    if (chain->count == 0) return false;

    /* Geprueft, nicht geglaubt — und VOR dem Schreiben, damit eine
     * abgelehnte Kette kein Artefakt hinterlaesst. */
    if (!uft_prov_verify((uft_provenance_chain_t *)chain)) return false;

    /* Was die Kette weiss, darf der Aufrufer nicht danebenlegen. Absage
     * statt stillem Vorrang: sonst gaebe es die Doppelung wieder, nur
     * eine Ebene tiefer. */
    if (extra && ((extra->operator_id && *extra->operator_id) ||
                  (extra->tool && *extra->tool) ||
                  (extra->chain_hash && *extra->chain_hash)))
        return false;

    const uft_prov_entry_t *head = &chain->entries[chain->count - 1];

    char hex[65];
    for (int i = 0; i < UFT_PROV_HASH_SIZE && i < 32; i++)
        snprintf(hex + i * 2, 3, "%02x", head->chain_hash[i]);
    hex[64] = '\0';

    /* Der Fundus schreibt; diese Datei entscheidet nur, WAS er schreibt.
     * Die Herkunftsfelder stammen ausschliesslich aus der Kette. */
    uft_fundus_meta_t meta;
    if (extra) meta = *extra;
    else       memset(&meta, 0, sizeof(meta));

    meta.operator_id = head->operator_id[0]  ? head->operator_id  : NULL;
    meta.tool        = head->tool_version[0] ? head->tool_version : NULL;
    meta.chain_hash  = hex;

    return uft_fundus_add(f, data, size, suffix, &meta,
                          out_path, out_path_size);
}

/* ── Gegenrichtung: die Kette lernt, wohin ihr Ergebnis ging (MF-505) ── */

bool uft_fundus_store_and_record(uft_fundus_t *f,
                                 uft_provenance_chain_t *chain,
                                 const void *data, size_t size,
                                 const char *suffix,
                                 const uft_fundus_meta_t *extra,
                                 char *out_path, size_t out_path_size)
{
    if (!f || !chain || !data || size == 0) return false;

    /* Den Bediener VOR dem Anhaengen holen: danach ist der Kopf der neue
     * Eintrag, und der hat seinen Bediener erst noch bekommen. Auch hier
     * gilt, dass die Kette die Quelle ist (MF-504). */
    char op[sizeof(chain->entries[0].operator_id)];
    op[0] = '\0';
    if (chain->count > 0)
        snprintf(op, sizeof(op), "%s",
                 chain->entries[chain->count - 1].operator_id);

    /* Erst ablegen. Das prueft zugleich die Kette und lehnt eine leere
     * oder kaputte ab, bevor irgendetwas geschrieben wird. */
    char path[UFT_FUNDUS_PATH_MAX];
    path[0] = '\0';
    if (!uft_fundus_add_from_chain(f, data, size, suffix, chain, extra,
                                   path, sizeof(path)))
        return false;

    /* Ab hier ist das Artefakt geschrieben. Was auch immer jetzt
     * schiefgeht: es bleibt liegen, und der Aufrufer erfaehrt wo. Ein
     * forensisches Werkzeug loescht keine aufgenommenen Daten, um seine
     * Buchfuehrung aufzuraeumen. */
    if (out_path && out_path_size)
        snprintf(out_path, out_path_size, "%s", path);

    /* Nur den Dateinamen vermerken, nicht den ganzen Pfad: ein Fundus
     * wird verschoben, kopiert, umbenannt — ein absoluter Pfad in der
     * Kette waere danach eine Angabe, die ins Leere zeigt. Wo der Fundus
     * liegt, weiss der, der ihn oeffnet. */
    const char *name = path;
    for (const char *p = path; *p; p++)
        if (*p == '/' || *p == '\\') name = p + 1;

    char desc[128];
    snprintf(desc, sizeof(desc), "Fundus: %s", name);

    if (uft_prov_add(chain, UFT_PROV_EXPORT, (const uint8_t *)data, size,
                     desc, op[0] ? op : NULL) != 0)
        return false;

    return true;
}
