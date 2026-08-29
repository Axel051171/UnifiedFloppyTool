# Playbook uft-variants

Stufenfolge und Regeln: [`../AGENT.md`](../AGENT.md).

## Tiefenprüfung (Stufe 4b)

Mensch oder LLM lädt `AGENT.md` + Übergabe-Entwurf + Evidenz und füllt
den UNGEKLÄRT-Block. **Jede Antwort mit Quelle** — Spec-URL mit
Abrufdatum, Datei:Zeile, oder Messlauf. Nie raten.

Die mechanischen Indizien aus `variantensucher.py` sind **Evidenz, kein
Urteil**. Zu entscheiden ist je Indiz:

* echte Feldvariante oder bloß ein interner Versionszähler?
* Stille-Falschaussage-Stufe 1–4 (AGENT.md §Maßstab)
* PROBE / READ / WRITE / SKIP
* Kennzahl — oder Fundus

## Vorbild in Ton und Tiefe

Es gibt **kein** `docs/FORMAT_VARIANTS*.md` im Baum. Der erste Entwurf
dieses Playbooks verwies darauf; das war ein erfundener Anker (die
Fehlerklasse aus `docs/plans/README.md`). Nimm stattdessen, was
wirklich da ist:

* `docs/OPEN_ITEMS.md` — die Gutachten-Einträge der Scout-Zyklen zeigen
  den Ton: jede Zahl gemessen, jede Quelle mit Datei:Zeile, und die
  eigene Fehlmessung mitprotokolliert.
* `docs/VERIFICATION_TIERS.md` + `docs/spec_verification.json` — die
  Form, in der eine benannte Referenz am Ende landet. Dein Dossier
  liefert den Inhalt für genau diese Felder.
* `docs/ORACLES.md` §„Was ein Eintrag braucht" — die drei Fragen an ein
  Werkzeug; sie gelten sinngemäß auch für ein Fixture.

Wird ein Varianten-Dokument im Baum angelegt, gehört es hierhin
verlinkt — aber erst, wenn es existiert.

## Spec-Sicherung

Fragile Quellen (Foren-Specs, Seiten die Archivierung blockieren) als
Kopie nach `docs/specs/<format>/` mit Abrufdatum. Die SCP-Lektion.
