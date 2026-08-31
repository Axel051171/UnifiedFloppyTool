# Lizenz-Anfragen an Dritte

Fehlende Lizenz heißt **„alle Rechte vorbehalten"**, nicht „frei
verwendbar". Eine Anfrage ist der *Versuch* einer Auflösung, nicht die
Auflösung selbst: bis eine Antwort vorliegt — und bei Projekten dieses
Alters kommt sie oft nie — bleibt die Stelle rechtlich ungeklärt und
steht in Zone PRÜFEN (MF-679).

Deshalb wird hier jede Anfrage mit **Datum, Adressat und Stand**
geführt. Eine gestellte Frage ohne Antwort ist kein Fortschritt, den
man abhaken darf.

| Quelle | betroffene Stellen | gefragt am | Antwort | Stand |
|---|---|---|---|---|
| nibtools | 3 | — (Entwurf steht) | — | offen |
| msa-to-zip | 2 | — | — | offen |
| bbctapedisc | 3 | — | — | Adressat unklar |
| dec0de | 3 | — | — | Adressat unklar |

---

## nibtools — Entwurf

**Adressaten:** Pete Rittwage (peter@rittwage.com), Markus Brenner
(markus@brenner.de) — beide stehen als Copyright-Inhaber im Kopf von
`gcr.c`.

**Warum konkret nach `gcr.c` gefragt wird und nicht allgemein:** eine
allgemeine Frage („unter welcher Lizenz steht nibtools?") verlangt vom
Empfänger eine Entscheidung über sein ganzes Projekt und bleibt deshalb
oft liegen. Eine Frage nach *einer benannten Datei*, mit dem Grund und
einem fertigen Vorschlag, ist in zwei Minuten zu beantworten.

**Gemessener Ausgangspunkt** (2026-08-31, über die GitHub-API):
`OpenCBM/nibtools` hat **keine `LICENSE`-Datei**, `gcr.c` trägt **keinen
Lizenzkopf** (nur den Copyright-Vermerk), und `readme.txt` enthält
**keine** Lizenzaussage.

---

> **Betreff:** Lizenzangabe für nibtools `gcr.c` — kurze Rückfrage
>
> Hallo Pete, hallo Markus,
>
> ich arbeite an UnifiedFloppyTool, einem quelloffenen Werkzeug zur
> forensischen Sicherung historischer Disketten (GPL-2.0-or-later).
>
> Zwei unserer Header verweisen auf nibtools als Referenz für die
> GCR-Kodierung des 1541-Laufwerks, einer davon namentlich auf
> `gcr.c`. Wir möchten diese Angaben rechtlich sauber halten und sind
> dabei auf eine offene Frage gestoßen:
>
> In `OpenCBM/nibtools` finde ich keine Lizenzdatei, und `gcr.c` trägt
> nur den Copyright-Vermerk („(C) 2001-2005 Markus Brenner and Pete
> Rittwage, based on code by Andreas Boose") ohne Lizenzangabe. Auch
> `readme.txt` nennt keine.
>
> **Meine Frage ist bewusst schmal:** unter welcher Lizenz steht
> `gcr.c`? Ein Satz genügt uns — etwa „gcr.c steht unter GPL-2.0-or-later"
> oder „unter derselben Lizenz wie OpenCBM".
>
> Falls die Antwort GPL-2.0(-or-later) lautet, ist alles geklärt und
> wir tragen sie bei uns ein. Falls nicht, ziehen wir den Verweis
> zurück und implementieren die Stelle eigenständig — kein Problem,
> wir möchten es nur wissen statt annehmen.
>
> Vielen Dank für nibtools; es ist seit zwanzig Jahren die Referenz
> für dieses Thema.
>
> Viele Grüße
> Axel

---

**Wenn keine Antwort kommt:** die drei Stellen
(`include/uft/formats/c64/uft_d64_g64.h`,
`include/uft/formats/c64/uft_gcr_ops.h`, `src/formats/c64/uft_g71.c`)
bleiben in Zone PRÜFEN. Der stärkste dann noch offene Kanal ist der
**Nachbau** (`docs/QUARANTINE_PROCESS.md` §5 Weg 2): die
GCR-Kodierung des 1541 ist in der Commodore-Literatur vollständig
beschrieben (*Inside Commodore DOS*, das 1541-Servicehandbuch), also
gibt es eine Doku-Route ohne fremden Quelltext.
