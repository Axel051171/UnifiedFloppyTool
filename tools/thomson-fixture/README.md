# SAP-Prüfabbilder aus der Referenzumsetzung (MF-926)

**Zweck:** P3-199 (Thomson-Dateisystemebene) war auf *„ein SAP-Abbild im
Korpus"* blockiert. Dieses Verzeichnis hält fest, wie sich solche
Abbilder **selbst herstellen** lassen — aus der Referenzumsetzung, nicht
aus einer fremden Sammlung.

Der Stand ist **halb**, und das steht hier so, weil es so ist.

---

## Was funktioniert

`sapfs` aus `nils-eilers/sap2` (GPL-2.0-or-later; Abstammung Alexandre
Pukall 1998 → Eric Botcazou 2000–2003 → Nils Eilers 2016) baut unter
MinGW und **erzeugt Abbilder beider Formatvarianten**:

| Variante | Aufruf | Größe |
|---|---|---|
| Format 1 | `sapfs -c f1.sap 80 2` | **335 426** Byte |
| Format 2 | `sapfs -c f2.sap 40 1` | **85 826** Byte |

Die beiden Größen sind der erste eigene Beleg dafür, dass UFT-28 recht
hat: es sind **zwei** Varianten, nicht eine. Das dritte Argument ist die
Dichte — `1` wählt `SAP_FORMAT2`, alles andere `SAP_FORMAT1`
(`sapfs.c`, Zweig `case 'c'`).

### Baurezept

```bash
cd tools/uft-scout/work/sap2        # geklont, gitignored
export PATH="/c/Qt/Tools/mingw1310_64/bin:$PATH"

# gettext liegt auf dieser Werkzeugkette nicht — ein Schein reicht.
# Er ersetzt drei Aufrufe durch Nichtstun und ändert KEINE Logik,
# nur die Sprache der Meldungen. Inhalt siehe unten.
mkdir -p /tmp/sapshim && cat > /tmp/sapshim/libintl.h <<'EOF'
static inline char *gettext(const char *s) { return (char *)s; }
static inline char *bindtextdomain(const char *d, const char *p)
{ (void)d; return (char *)p; }
static inline char *textdomain(const char *d) { return (char *)d; }
EOF

gcc -std=gnu99 -O1 -I/tmp/sapshim -DPREFIX_MO='"."' \
    -o sapfs.exe sapfs.c libsap.c
./sapfs.exe --version      # SAPfs version 0.9.6 for MSDOS
```

---

## Was NICHT funktioniert — und warum das hier steht

**Die Dateisystem-Operationen stürzen ab.** Gemessen mit demselben Bau,
auf einem Abbild, das dasselbe Programm gerade erzeugt hat:

```
$ ./sapfs.exe -a f1.sap HELLO.BAS
Segmentation fault        (Rückgabe 139)
$ ./sapfs.exe -t f1.sap
Segmentation fault        (Rückgabe 139)
```

Die Versionszeile nennt den wahrscheinlichen Grund selbst: *„SAPfs
version 0.9.6 **for MSDOS**"*. Ein Programm von 2003, geschrieben für
16/32 Bit, als 64-Bit-Binär gebaut — die klassische Lage. Ein
`-m32`-Bau scheitert auf dieser Maschine, weil die MinGW-Installation
keine 32-Bit-Bibliotheken mitbringt (`cannot find -lmingw32`).

**Ausdrücklich nicht behauptet:** dass der Fehler an der Bibliothek
liegt. Er ist nicht eingegrenzt. Der Schein für `libintl` scheidet als
Ursache praktisch aus — er berührt nur Zeichenketten —, aber gemessen
ist auch das nicht.

---

## Was das für P3-199 bedeutet

Der Blocker ist **kleiner geworden, nicht weg**:

| vorher | jetzt |
|---|---|
| kein SAP-Abbild, keine Quelle dafür | Abbilder **beider Varianten** herstellbar, aus der Referenzumsetzung |
| — | Inhalt (Verzeichnis, Dateien) weiterhin nicht herstellbar |

Ein Leser für die Container-Ebene ließe sich damit schon gegen fremde
Hand prüfen. Ein Leser für die **Dateisystem**-Ebene nicht — und genau
der ist der Gegenstand von P3-199. Die EINFRIER-REGEL bleibt damit in
Kraft.

**Drei Wege weiter, in der Reihenfolge ihres Aufwands:**

1. **32-Bit-Werkzeugkette** (MSYS2 `mingw-w64-i686-gcc`) — dann
   wahrscheinlich ohne Codeänderung lauffähig.
2. **Emulator** — DCMOTO oder Teo erzeugen Thomson-Disketten. Das ist
   die Cross-Tool-Klasse, die dieser Baum ohnehin bevorzugt; es kostet
   eine Installation und einen Nachmittag.
3. **Den Absturz eingrenzen** — der Quelltext liegt vor, GPL-2-or-later.
   Ein Fix wäre allerdings eine Änderung am **Oracle**, und ein
   selbstrepariertes Oracle ist eine schwächere Referenz als ein
   unverändertes.

Weg 1 ist der billigste und lässt die Referenz unangetastet.

---

## Warum hier keine Abbilder liegen

Erzeugt, aber **nicht in den Korpus aufgenommen**: zwei leere,
formatierte Archive ohne nachprüfbaren Inhalt sind als
Container-Fixture brauchbar und als Dateisystem-Fixture wertlos. Wer
sie aufnimmt, bindet sich an eine Herkunftszeile im Manifest für etwas,
das die eigentliche Lücke nicht schließt.

Sie sind aus diesem Rezept in zwei Minuten wieder da.
