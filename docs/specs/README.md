# Verhaltens-Specs für Clean-Room-Neubauten

> **Nicht zu verwechseln (MF-689):** gesicherte **Fremdkopien** — Foren-
> Specs, Herstellerseiten, Handbuchauszüge — liegen in
> [`../format_refs/<format>/`](../format_refs/README.md), nicht hier.
> Hier wohnt nur, was **wir** selbst geschrieben haben. Der Unterschied
> ist die Lizenzlage, und sie zu vermischen macht beide unbrauchbar.

Hier liegen die Specs, die **Weg 2** des Quarantäne-Verfahrens verlangt
([`../QUARANTINE_PROCESS.md`](../QUARANTINE_PROCESS.md) §5).

Je Quelle ein Unterverzeichnis: `docs/specs/<quelle>/`.
Vorlage: `tools/uft-scout/templates/spec.md`.

## Die eine Regel, die zählt

**Eine Spec wird aus Dokumentation und Blackbox-Läufen geschrieben, nie
aus dem fremden Quelltext.** Wer vom Code abschreibt, baut einen Port
mit Zwischenschritt — und die Ableitung, die der Neubau auflösen sollte,
bleibt bestehen.

Praktisch heißt das je Aussage in der Spec eine Herkunft:

* aus veröffentlichter Doku → Fundstelle nennen
* aus einem Blackbox-Lauf → Eingabe, Ausgabe, Werkzeugversion
* aus dem fremden Quelltext → **gehört nicht hierher**

## Stand

Leer. Der erste Kandidat ist `nibtools/` für den Neubau von
`uft_track_align.c` (Quarantäne MF-635, Weg 2, Oracle `nibscan`) —
sobald ein Baustein ihn verlangt. Bis dahin ist das Verzeichnis
absichtlich leer statt mit Platzhaltern gefüllt.
