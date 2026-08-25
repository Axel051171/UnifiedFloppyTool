# Bedien-Abnahme v4.1.6 — MF-496 und MF-501

**Stand:** offen. Dieses Blatt existiert, damit die Sitzung kurz wird.

Die Regel des Projekts verlangt für benutzersichtbare Funktionen einen
dokumentierten Klick-Test. MF-496 (Feineinsteller-Vorschlag) und MF-501
(Schadenslage über die Umdrehung) sind seit 4.1.5 dazugekommen.

---

## Was du wissen solltest, bevor du klickst

**Beide Funktionen fassen keine einzige GUI-Datei an.** Sie sitzen im
Decoder und geben ihre Befunde über `uftc_add_warning()` in
`result->warnings[]` aus.

Bis MF-568 hat diese Ausgabe **nie einen Benutzer erreicht**: der
Konvertieren-Knopf war `QFile::copy()` und rief den Wandler gar nicht
auf. Beide Funktionen sind also erst in 4.1.6 überhaupt sichtbar
geworden — über genau diese eine Leitung.

Das ist der Grund, warum die Abnahme jetzt fällig ist und nicht bei
4.1.5 hätte stattfinden können.

---

## Was bereits maschinell abgedeckt ist

Nicht noch einmal prüfen — das läuft in jedem CI-Lauf:

| Was | Wo |
|---|---|
| Der Feineinsteller rechnet richtig | `tests/test_convert_cell_adjust.c` |
| Die Scheibenkarte ist lückenlos, überschneidungsfrei, erfindet nichts | `tests/test_decode_timeline.c` (392 Zeilen) |
| Die Winkelangabe kommt nie ohne Zeitbasis | ebenda |
| **Wandler-Warnungen erreichen das Ausgabefeld** | `tests/test_tools_tab_convert.cpp::converterWarningsReachTheOutputPane` (MF-585, rotbewiesen: Leitung gekappt → Test fällt) |
| Der Konvertieren-Knopf schreibt nichts bei Ablehnung | ebenda |

---

## Was nur Augen entscheiden können

Fünf Punkte. Jeder mit dem, was du erwarten solltest.

### 1. Die Meldung ist lesbar

Wandle irgendeine SCP-Datei nach D64 oder ADF. Im Ausgabefeld des
**Tools**-Reiters müssen Zeilen mit `  ! ` davor erscheinen.

**Erwartet:** deutscher Text, vollständig, nicht abgeschnitten, nicht in
einer Zeile zusammengequetscht. Die Meldungen sind lang — bis ~200
Zeichen.

**Zu prüfen:** bricht das Feld um, oder muss man waagerecht scrollen?

### 2. Der Feineinsteller-Vorschlag ergibt Sinn (MF-496)

Bei einer Aufnahme mit abweichender Datenrate erscheint sinngemäß:

> Die Sync-Marken messen eine Zellendauer von **N %** der abgeleiteten
> (M Spuren gemittelt). Wer das bestätigen will, setzt den
> Feineinsteller auf diesen Wert …

**Erwartet:** die genannte Zahl lässt sich in der Oberfläche auch
tatsächlich einstellen — der Vorschlag zeigt auf einen Regler, den es
gibt.

**Zu prüfen:** findest du den Feineinsteller, ohne zu suchen? Nimmt er
den vorgeschlagenen Wert an? Ändert sich das Ergebnis danach?

Wenn der Vorschlag auf etwas zeigt, das der Benutzer nicht erreichen
kann, ist er wertlos — das ist der eigentliche Prüfpunkt.

### 3. Die Schadenslage ist verständlich (MF-501)

Bei einer beschädigten Aufnahme erscheint sinngemäß:

> Schadenslage über die Umdrehung (8 Achtel, Promille der Spurlänge):
> … — beschädigt auf N von M Spuren

**Erwartet:** die acht Achtel sind als Verteilung erkennbar.

**Zu prüfen:** versteht man ohne Handbuch, was „Promille der Spurlänge"
bedeutet? Wenn nicht, ist das ein Formulierungsfehler und kein
Bedienfehler — bitte so melden.

### 4. Der Zustimmungs-Dialog

Wandle etwas Verlustbehaftetes (z. B. SCP → D64). Es muss ein Dialog
kommen, der **nennt, was verloren geht**, mit Ja/Nein.

**Erwartet:** „Nein" schreibt **keine** Datei.

**Zu prüfen:** steht im Dialog wirklich der Verlust, oder nur „sind Sie
sicher"? — Dieser Punkt ist kopflos nicht prüfbar: ein modaler Dialog
hält einen automatischen Lauf an. Deshalb steht er hier.

### 5. Die zurückgenommenen Anzeigen

4.1.6 hat drei Dinge **entfernt**, die wie Funktionen aussahen. Bitte
einmal ansehen, ob die Ersatztexte verständlich sind:

| Wo | Sagt jetzt |
|---|---|
| Datei-Browser | „(no directory listing — filesystem reading is not wired)" |
| Belegungskarte (Status-Reiter) | graue `?`-Felder, Satz über der Tabelle |
| Forensik-Bericht | „— not checked" statt „✓ Valid" |

**Zu prüfen:** liest sich das wie ein Fehler oder wie eine ehrliche
Auskunft? Wenn es wie ein Fehler wirkt, ist der Text zu ändern — nicht
das Verhalten.

---

## Ergebnis eintragen

```
Datum:        ____________
Version:      4.1.6 (Help → About prüfen)
Betriebssystem: ____________

1. Meldung lesbar             [ ] ok   [ ] Problem: ______________
2. Feineinsteller erreichbar  [ ] ok   [ ] Problem: ______________
3. Schadenslage verständlich  [ ] ok   [ ] Problem: ______________
4. Zustimmungs-Dialog         [ ] ok   [ ] Problem: ______________
5. Ersatztexte verständlich   [ ] ok   [ ] Problem: ______________
```

Ein „Problem" ist kein Rückschlag — vier der sechs Klasse-A-Befunde
dieses Releases waren Texte, die etwas anderes sagten als der Code tat.
