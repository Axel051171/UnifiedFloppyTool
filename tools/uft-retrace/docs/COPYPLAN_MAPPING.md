# Anbindung an den UFT-CopyPlan

ReTrace liefert **Evidenz**, aber schaltet nicht automatisch gefährliche
Schreibfähigkeiten frei. Die Übergabe wird zunächst als experimentelles
Backend registriert.

| Beobachtung | mögliche Fähigkeit | zusätzliche Pflichtprüfung |
|---|---|---|
| Sektor-ID, Daten und CRC gelesen | `sector.read` | mehrere Images und Fehlerpfad |
| kompletter Spurpuffer gelesen | `track.read.raw` | Längen-, Bounds- und Indexprüfung |
| DMA-Writes mit reproduziertem Track | `track.write.raw` | Write-Protect, Verify und Testmedium |
| mehrere Umläufe verglichen | `read.consensus` | deterministische Abstimmungsregel |
| CRC-Fehler bewusst erhalten | `preserve.bad_crc` | Zielformat kann Zustand darstellen |
| Timing-/Indexdaten beobachtet | `preserve.timing` | Quelle und Ziel besitzen Timing-API |
| instabile Zellen belegt | `preserve.weak_bits` | Flux-/Mehrfachlese-Backend nötig |

## Schaltregeln

1. `detected`: Beobachtung ist vorhanden.
2. `implemented`: UFT besitzt eine bounds-sichere Neuimplementierung.
3. `tested`: Fixture testet Erfolg, Fehler und Grenzwerte.
4. `available`: Quell- und Zielbackend erfüllen die Fähigkeit.
5. Erst dann darf der CopyPlan den Modus anbieten.

Ein Raw-Byte-Treffer darf nie `available=true` setzen. Automatische Auswahl
verwendet nur lebende Callbacks und erfolgreiche Capability-Probes.
