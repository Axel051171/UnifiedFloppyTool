/**
 * @file uft_fundus_provenance.h
 * @brief Ein Artefakt zitiert seine Herkunftskette (MF-504).
 *
 * ── Die Doppelung, die hier verschwindet ─────────────────────────────────
 *
 * Seit MF-503 fuehren zwei Dinge Herkunft:
 *
 *   - die **Provenienz-Kette** (`uft_provenance.h`) — was mit den Daten
 *     geschah, hash-verkettet ueber Aufnahme, Dekodierung, Analyse, Export
 *   - der **Fundus** (`uft_fundus.h`) — was gespeichert ist, mit Bediener,
 *     Werkzeug und Aufnahme-Rezept
 *
 * Beide kennen Bediener und Werkzeug. Zwei Quellen fuer dieselbe Tatsache
 * sind aber kein Komfort, sondern ein Widerspruch in Wartestellung: wer
 * denselben Wert an zwei Stellen angeben kann, wird es irgendwann
 * verschieden tun, und dann gibt es keinen Weg mehr zu entscheiden,
 * welcher gilt. Genau diese Krankheit hat dieses Projekt in dieser Woche
 * fuenfzehnmal diagnostiziert; sie hier selbst einzubauen waere schwer zu
 * verteidigen.
 *
 * **Die Kette ist die Quelle, der Fundus zitiert sie.**
 *
 * ── Warum eine eigene Datei ──────────────────────────────────────────────
 *
 * Damit der Fundus ohne die Kette benutzbar bleibt. Wer nur ablegen will,
 * braucht keinen Hash-Baum; wer die Herkunft mitfuehrt, bindet diese eine
 * Uebersetzungseinheit dazu. Der Fundus-Header weiss deshalb nichts von
 * der Kette.
 */
#ifndef UFT_FUNDUS_PROVENANCE_H
#define UFT_FUNDUS_PROVENANCE_H

#include "uft/forensic/uft_fundus.h"
#include "uft/forensic/uft_provenance.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Ein Artefakt anhaengen und dabei seine Herkunftskette zitieren.
 *
 * Bediener, Werkzeug und Kettenhash kommen aus der **Kette**. @p extra
 * liefert nur, was die Kette nicht weiss: Kennung, Beschreibung, Notizen,
 * Aufnahme-Rezept.
 *
 * @param chain  wird **geprueft, nicht geglaubt**: eine Kette, deren
 *               Verkettung nicht mehr aufgeht, wird nicht zitiert. Ein
 *               Verweis auf eine kaputte Kette wuerde sie waschen — das
 *               Artefakt saehe belegt aus, und der Beleg waere wertlos.
 *               Eine leere Kette hat nichts zu zitieren.
 * @param extra  darf NULL sein. Setzt es @ref uft_fundus_meta_t::operator_id,
 *               ::tool oder ::chain_hash, ist das eine **Absage** und kein
 *               stiller Vorrang — sonst gaebe es die Doppelung wieder, nur
 *               eine Ebene tiefer.
 * @return false bei unbrauchbaren Argumenten, leerer oder kaputter Kette,
 *         doppelt angegebenen Feldern oder Schreibfehler. Im Fehlerfall
 *         bleibt **nichts** zurueck — kein Artefakt, kein Manifest-Eintrag.
 */
bool uft_fundus_add_from_chain(uft_fundus_t *f, const void *data, size_t size,
                               const char *suffix,
                               const uft_provenance_chain_t *chain,
                               const uft_fundus_meta_t *extra,
                               char *out_path, size_t out_path_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FUNDUS_PROVENANCE_H */
