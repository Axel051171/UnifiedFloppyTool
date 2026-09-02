"""Registry benannter Referenz-Werkzeuge (MF-499).

Seit MF-498 ist „geprueft" operational definiert (docs/VERIFICATION_PLAN.md
§Einfrier-Regel): neuer Code im Format- oder Decoder-Layer braucht eine
**benannte** Referenz. Diese Datei ist die Stelle, an der die benannten
Referenzen stehen — damit „benannt" nicht „irgendwo im Commit-Text
erwaehnt" heisst und damit ein T1b-Manifest die Angaben mechanisch
bekommt, statt sie von Hand nachzutragen.

Jeder Eintrag beantwortet vier Fragen:

  1. **Wofuer ist es Referenz?** Nicht „was kann das Werkzeug", sondern
     welche Behauptung es entscheiden kann. Ein Werkzeug, das nichts
     entscheidet, gehoert nicht in diese Liste.
  2. **Wie wird es gefunden?** Umgebungsvariable, dann PATH. Kein Raten in
     Installationsverzeichnissen — wer ein Werkzeug benutzt, soll wissen
     welches.
  3. **Wie wird die VERSION festgestellt?** Ohne Version ist ein
     T1b-Manifest unvollstaendig und der Eintrag zaehlt nicht
     (VERIFICATION_PLAN, Provenienz-Regel). Ein Oracle ohne
     Versionsabfrage waere also ein Oracle, dessen Aussage nicht
     zitierfaehig ist.
  4. **Was ist mit der Lizenz?** Hier wird ausschliesslich die AUSGABE
     eines fremden Programms verglichen — es wandert kein Code ein. Das
     ist der Unterschied, der die Lizenzfrage bei Oracles entschaerft, und
     er steht deshalb bei jedem Eintrag ausdruecklich da.

── Ehrlich zum Ist-Stand ────────────────────────────────────────────────

Diese Datei registriert Werkzeuge; sie installiert keines. Auf einer
Maschine ohne die Werkzeuge loest sie nichts auf, und jeder Test, der
eines braucht, ueberspringt sich sauber. Der Selbsttest unten sagt, wie
viele gefunden wurden — er meldet **nicht** Erfolg, wenn es keines ist.
"""
from __future__ import annotations

import hashlib
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path


@dataclass(frozen=True)
class Oracle:
    """Ein benanntes Referenz-Werkzeug."""

    name: str
    """Kurzname, unter dem es zitiert wird (auch im Manifest)."""

    env: str
    """Umgebungsvariable, die den Pfad ueberschreibt."""

    exes: tuple[str, ...]
    """Kandidaten fuer den ausfuehrbaren Namen, in Reihenfolge."""

    version_args: tuple[str, ...]
    """Argumente, die eine Versionsangabe erzeugen."""

    version_re: str
    """Regulaerer Ausdruck; Gruppe 1 ist die Version."""

    reference_for: str
    """Welche Behauptung dieses Werkzeug entscheiden kann."""

    origin: str
    """Woher es kommt — Projekt/URL, damit es beschaffbar bleibt."""

    licence: str
    """Lizenz des Werkzeugs."""

    code_import: bool = False
    """Darf Code daraus einwandern? Fuer Oracles grundsaetzlich NEIN —
    verglichen wird die Ausgabe. Ein True hier waere eine eigene
    Entscheidung und braucht eine eigene Begruendung."""

    version_exit_ok: tuple[int, ...] = field(default=(0,))
    """Rueckgabewerte, die als Erfolg gelten. Manche Werkzeuge geben ihre
    Version mit einem Fehlercode aus (Aufruf ohne Argumente = Usage)."""

    version_via: tuple[str, ...] = ()
    """Vollstaendiges Kommando, das die Version liefert, wenn sie NICHT im
    Startprogramm steckt (MF-693).

    Der dritte Fall neben „das Programm sagt seine Version" und „es kann
    sie ueberhaupt nicht sagen": ein Werkzeug, dessen Version an einer
    anderen, ebenso pinbaren Stelle liegt.

    Gemessen an `xdftool`: das ist ein Python-Einstiegspunkt aus dem Paket
    `amitools`. Weder `--version` noch `-v` noch der argumentlose Aufruf
    geben eine Versionszeile aus — die Version steht im **Paket**
    (`importlib.metadata.version("amitools")` -> `0.8.1`).

    Warum hier nicht `version_is_unaskable` mit SHA-256 gesetzt wird,
    obwohl das der naheliegende Weg waere: die aufgeloeste Datei ist bei
    einem Python-Einstiegspunkt ein **Startprogramm-Rumpf**. Sein Hash
    pinnt den Rumpf, nicht `amitools`. Das waere der schwaechere Anker in
    der Gestalt des staerkeren — genau der Fehler, vor dem die
    Beschreibung von `version_is_unaskable` warnt.

    Der Preis ist benannt: das Kommando fragt das PAKET, nicht die
    aufgeloeste Datei. Laege ein fremdes `xdftool` im PATH, meldete es
    trotzdem die Version des installierten `amitools`. Deshalb steht die
    SHA-256 der aufgeloesten Datei weiterhin im Manifest daneben — sie
    zeigt, WELCHES Programm gelaufen ist, waehrend `version_via` sagt,
    WELCHES Paket dahinterliegt. Erst beide zusammen pinnen den Lauf."""

    version_is_unaskable: bool = False
    abstammung: str = ""
    """PFLICHTFELD seit MF-760 — die fuenfte Frage, VOR dem Eintrag.

    Steht dieses Werkzeug in derselben Linie wie das, was es pruefen
    soll? Ein Oracle, das von derselben Quelle abstammt wie unsere
    Umsetzung, ist kein Zeuge — ein geerbter Fehler steckt dann in
    beiden, und der Differenzlauf entscheidet nichts.

    DIE REGEL: der KOPFKOMMENTAR zuerst, nie der Lauf. Bei `xadundisk`
    stand die Antwort in vierzig Zeilen Kopfkommentar von `DMS.c`
    („lossely based ... on xDMS source“) — es haette sonst eine
    Registrierung, einen Bau und einen Lauf gekostet, sie zu finden.

    Wer die Frage nicht beantworten kann, schreibt `UNGEPRUEFT` und
    sagt, was zu pruefen waere. Diese Eintraege werden gezaehlt und
    beim Lauf ausgewiesen — ein Rueckstand, kein Freibrief.
    """
    """Dieses Werkzeug kann seine Version NICHT nennen (MF-623).

    Gemessen an floptool aus mame0289b: weder `--version` noch `-version`
    noch der argumentlose Aufruf geben eine Versionszeile aus. Ohne diese
    Erklaerung bliebe der Eintrag dauerhaft `complete: False`.

    Die Erklaerung weicht die Provenienz-Regel nicht auf. Was die Regel
    schuetzt, ist die Nachbeschaffbarkeit — und die SHA-256 des
    Binaerprogramms leistet das strenger als eine Versionszeile: eine
    Version gibt es in vielen Uebersetzungen, den Hash genau einmal.
    Wer dieses Flag setzt, tauscht den schwaecheren Anker gegen den
    staerkeren; wer es bei einem Werkzeug MIT Versionsabfrage setzt,
    verschenkt eine Angabe und begeht damit den Fehler, den die Regel
    verhindern will."""


REGISTRY: tuple[Oracle, ...] = (
    Oracle(
        name="xcopy",
        env="XCOPY_ORACLE",
        exes=("xcopy-oracle",),
        version_args=("--fassung",),
        version_re=r"([0-9]+\.[0-9]+)",
        reference_for="Das Spurverdikt eines historischen Werkzeugs — "
                      "welche Fehlerziffer X-Copy fuer welchen "
                      "Medienzustand zeigt. Bezug fuer die Vorrangkette in "
                      "`src/flux/uft_track_verdikt.c`, deren Reihenfolge "
                      "dort ausdruecklich als SETZUNG gekennzeichnet ist "
                      "(MF-765). Fixtures und Fragen: "
                      "`docs/nachbau/XCOPY_EMULATIONSSITZUNG.md`.",
        origin="X-Copy Professional (Cachet Software), Fassungen 3.4 "
               "(1991-02-17), 5.21 und ein 01/95-Stand — unter WinUAE mit "
               "Kickstart 1.3 ausgefuehrt, nicht weitergegeben",
        licence="proprietaer; Kanal ist AUSFUEHRUNG, nicht Uebernahme "
                "(MF-695). Weder Binary noch Text gehen ins Repo.",
        abstammung=(
            "UNABHAENGIG, und zwar auf die staerkste denkbare Weise: das "
            "Werkzeug ist von 1991 und kennt UFT nicht. Es ist damit das "
            "einzige Oracle im Register, bei dem die Frage „dieselbe "
            "Hand?“ gar nicht erst entstehen kann. "
            "Die Gegenrichtung ist die eigentliche Gefahr und wird von "
            "der Zwei-Haende-Brandmauer getragen: UFT darf nicht aus "
            "seiner QUELLE abgeleitet sein. Hand A hat sie gelesen und "
            "eine Verhaltens-Spec geliefert "
            "(`docs/nachbau/XCOPY_VERHALTEN_HAND-A2.md`); die "
            "Implementierungsseite hat sie nie gesehen. "
            "ACHTUNG BEIM UMFANG: das Oracle beobachtet nur, was auf dem "
            "Schirm steht. Alles unterhalb der MFM-Wortebene — "
            "Zellendauer, Intervall-Histogramm, Rauschen — sieht X-Copy "
            "nie, weil der Emulator (wie Paula auf echter Hardware) den "
            "Fluss vorher dekodiert. Eine Frage aus dieser Schicht ist "
            "hier nicht stellbar, sondern nur ueber `gw` oder eine "
            "echte Aufnahme. "
        ),
    ),
    Oracle(
        name="gw",
        env="GW",
        exes=("gw", "gw.exe"),
        # MF-776: `--version` gibt es NICHT — die Option faellt auf den
        # Usage-Text zurueck. Eine erste Fassung las die „1.23" daraus ab
        # und hielt sie fuer eine Versionsmeldung; tatsaechlich stand sie
        # im INSTALLATIONSPFAD (`greaseweazle-1.23\gw.exe`), der in der
        # Usage-Zeile mitgedruckt wird. Woanders installiert haette das
        # still eine falsche oder gar keine Zahl geliefert.
        #
        # `gw info` meldet „Host Tools: 1.23" und laeuft OHNE Geraet
        # („Device: Not found"), ist also die richtige Quelle.
        version_args=("info",),
        version_re=r"Host Tools:\s*([0-9]+\.[0-9]+)",
        reference_for="Flux-Aufnahme und -Wandlung am Greaseweazle; "
                      "Bezug fuer die gw-vs-UFT-Differenztests (P3.2)",
        origin="https://github.com/keirf/greaseweazle",
        licence="Unlicense (public domain)",
        abstammung=(
            "GEMESSEN (MF-774), und die Antwort ist ZWEIGETEILT. "
            "Der Kopf von `src/hal/uft_greaseweazle_full.c` sagt woertlich "
            "`Verified against keirf/greaseweazle v1.23 (usb.py, flux.py)` "
            "und listet 13 Fehler, die beim Lesen dieser Dateien gefunden "
            "wurden — Opcode-Deutung, Divisor 255, `cue_at_index`, "
            "Endmarke, ClearComms. Das ist eine Ableitungs-Erklaerung nach "
            "MF-636: fuer das WIRE-PROTOKOLL ist gw DIESELBE HAND, und ein "
            "Differenzlauf UFT-HAL gegen gw faengt dort keine Fehler. "
            "Fuer alles ANDERE ist gw unabhaengig — insbesondere fuer den "
            "Amiga-MFM-Encoder (P3-9, MF-539), der mit dem HAL-Protokoll "
            "nichts zu tun hat; in DIESEM Umfang ist gw ein taugliches "
            "zweites Oracle. Die Lizenzfrage stellt sich nicht: Unlicense "
            "ist Public Domain und stellt keine Bedingung. "
            "Der frueher hier notierte Verdacht bleibt ausgeraeumt: "
            "der Treffer `uft_gw2dmk_panel.cpp` („qbarnes/gw2dmk concept“) "
            "meint gw2dmk von Quentin Barnes, NICHT die "
            "Greaseweazle-Hostwerkzeuge von Keir Fraser — zwei Projekte, "
            "gw2dmk BENUTZT Greaseweazle. Zu pruefen bleibt, ob unser "
            "HAL-Protokollwissen aus der gw-Quelle oder aus deren "
            "Protokolldoku stammt. "
        ),
    ),
    Oracle(
        name="bluemsx",
        env="BLUEMSX",
        exes=("bluemsx", "blueMSX", "bluemsx.exe", "blueMSX.exe"),
        version_args=("/help",),
        version_re=r"blueMSX\s+v?([0-9]+\.[0-9]+(?:\.[0-9]+)?)",
        version_exit_ok=(0, 1),
        reference_for=(
            "ERZEUGT KORPUS fuer msx_disk (heute T3: null Tests, keine "
            "Spec-Quelle). Unter dem Emulator laeuft Nextor bzw. MSX-DOS "
            "und formatiert eine Diskette; Bootsektor, BPB, "
            "Media-Descriptor und FAT stammen damit von der KANONISCHEN "
            "Implementierung, nicht von UFTs Annahme. Fuer den "
            "msx-LESER ist blueMSX danach kein Pruefer mehr — dieselbe "
            "Trennung wie bei c1541 und atrcopy."
        ),
        origin="https://bluemsx.msxblue.com/ (Daniel Vik u. a.)",
        licence="GPL-2.0",
        abstammung=(
            "UNABHAENGIG — und das ist hier keine Formalie, sondern der "
            "GRUND fuer die Wahl. "
            "GEMESSEN (MF-780): `openMSX` waere der bequemere Emulator "
            "(Tcl-Konsole, skriptbar, damit eine reproduzierbare "
            "Befehlszeile statt einer Klickfolge). Er faellt aber aus, "
            "und zwar messbar: `src/formats/dsk_msx/uft_dsk_msx.c` "
            "erklaert im Kopf `Reference: MSX-DOS specification, openMSX "
            "source` — und GENAU diese Datei IST das Plugin `msx_disk`, "
            "das gehoben werden soll. openMSX ist fuer dieses Format "
            "DIESELBE HAND; ein Differenzlauf wuerde nur bestaetigen, "
            "dass UFT reproduziert, wovon es abgeleitet ist. "
            "(`src/formats/cas/uft_cas.c` traegt dieselbe Erklaerung — "
            "fuer CAS gilt sie entsprechend.) "
            "blueMSX wird im Baum an keiner Stelle genannt; gemessen mit "
            "`git ls-files` ueber `src/` und `include/`, kein Treffer. "
            "DER PREIS, benannt: blueMSX wird ueber eine Oberflaeche "
            "bedient. T1b verlangt einen reproduzierbaren Erzeugungsweg — "
            "hier ist das eine DOKUMENTIERTE KLICKFOLGE, nicht eine "
            "Befehlszeile. Das ist schwaecher, und `KNOWN_ISSUES.md` "
            "fuehrt Klick-Sitzungen als das, was am haeufigsten nicht "
            "stattfindet. Unabhaengigkeit schlaegt hier Bequemlichkeit: "
            "ein bequemes Oracle, das dieselbe Hand ist, misst nichts. "
            "FREMDCODE, vorab und noch ungemessen: eine von "
            "MSX-DOS/Nextor formatierte Diskette traegt einen BOOTSEKTOR "
            "mit Startroutine — also `ja (was)`, nicht `nein`. Der "
            "Media-Descriptor, an dem `uft_dsk_msx.c` erkennt, ist dabei "
            "DATEN (eine Tatsache ueber die Geometrie); die Startroutine "
            "daneben ist CODE. Ein solches Abbild gehoert deshalb nach "
            "`tests/corpus/` (gitignored, nur SHA-256 im Manifest) und "
            "NICHT nach `tests/corpus_free/`. "
        ),
    ),
    Oracle(
        name="mformat",
        env="MFORMAT",
        exes=("mformat", "mformat.exe"),
        version_args=("--version",),
        version_re=r"mformat \(GNU mtools\) ([0-9]+\.[0-9]+\.[0-9]+)",
        version_exit_ok=(0, 1),
        reference_for=(
            "ERZEUGT FAT12-Abbilder mit EXPLIZITER Geometrie und ist damit "
            "der Bezug fuer `src/fs/uft_fat12.c` — das bis MF-789 auf "
            "FS-T1 stand mit dem Vermerk „alle Tests bauen ihre Eingabe "
            "selbst, geprueft gegen den eigenen Erzeuger\". Fuer den "
            "FAT12-LESER ist mformat danach kein Pruefer mehr; wer eine "
            "zweite Meinung will, braucht eine andere Hand (Flopgen, "
            "P3-16)."
        ),
        origin="https://www.gnu.org/software/mtools/ (mtools 4.0.49)",
        licence="GPL-3.0-or-later",
        abstammung=(
            "UNABHAENGIG, gemessen: `git ls-files` ueber `src/` und "
            "`include/` nennt weder `mtools` noch `mformat` an einer "
            "einzigen Stelle. "
            "WICHTIG BEIM GEBRAUCH — der Bootcode: `mformat` schreibt "
            "standardmaessig 44 Byte ausfuehrbaren Code hinter den BPB "
            "(gemessen: Sprung eb3c90, OEM „MTOO4049\"). Fuer ein "
            "Korpus-Abbild ist das ein Derivat in diesen Bytes. Mit "
            "`-B <vorlage>` faellt er weg; die Vorlage muss dann Sprung "
            "und 0xAA55 selbst tragen, sonst fehlen sie. UFT benutzt eine "
            "EIGENE Vorlage mit fuenf Bytes (EB FE 90 / 55 AA) — gemessen "
            "0 Byte Fremdcode im Bereich 0x3E..0x1FD. "
            "Gebaut unter WSL Ubuntu mit gcc 15.2 ohne `make` (das dort "
            "fehlt): die 68 Quellen ohne die eigenstaendigen Programme "
            "`floppyd*`, `mkmanifest`, `privtest`, `file_read` in einem "
            "Aufruf, mit -DSYSCONFDIR. Das Windows-Binaerprogramm loeste "
            "eine Defender-Heuristik aus (P3-15) — der WSL-Weg umgeht das. "
        ),
    ),
    Oracle(
        name="c1541",
        env="C1541",
        exes=("c1541", "c1541.exe"),
        version_args=("-?",),
        version_re=r"[Vv]ersion\s+([0-9]+\.[0-9]+)",
        version_exit_ok=(0, 1, 2),
        reference_for=(
            "ERZEUGT KORPUS fuer d64, d67, d71, d80, d81, d82, g64, g71 — "
            "und ist damit fuer diese Formate KEIN PRUEFER. Ein Werkzeug, "
            "das die Fixture geschrieben hat, kann nicht zugleich "
            "bestaetigen, dass sie richtig gelesen wird; es wuerde nur "
            "seine eigene Ausgabe wiedererkennen. Wer d64 gegen eine "
            "zweite Hand messen will, braucht eine ANDERE."
        ),
        origin="https://sourceforge.net/projects/vice-emu/ (VICE-Team)",
        licence="GPL-2.0-or-later",
        abstammung=(
            "FUENFTE FRAGE (MF-644), beantwortet: c1541 steht hier als "
            "ERZEUGER, nicht als Pruefer — die Frage „dieselbe Hand?“ ist "
            "fuer seine eigenen Fixtures mit JA zu beantworten, und genau "
            "deshalb steht es im `reference_for` oben statt in einer "
            "Fussnote. "
            "ACHTUNG BEI DER VERSION: das Manifest nennt DREI verschiedene "
            "Bauten derselben Fassung 3.10 — `GTK3VICE-3.10-win64`, "
            "`SDLVICE-3.10-win64-r46215` und ein blosses "
            "`VICE-Team/svn-mirror release 3.10.0`. Die Revision `r46215` "
            "unterscheidet den SDL-Bau; `c1541 -?` meldet nur `3.10`. Der "
            "Korpus-Eintrag muss den BAU nennen, das Register kann es "
            "nicht. "
        ),
    ),
    Oracle(
        name="atrcopy",
        env="ATRCOPY",
        exes=("atrcopy", "atrcopy.exe"),
        # Wie `xdftool`: die Version kommt aus den Paket-Metadaten, nicht
        # aus dem Werkzeug. Gemessen: `atrcopy 10.1`.
        version_args=(),
        version_via=(sys.executable, "-c",
                     "import importlib.metadata as m; "
                     "print('atrcopy', m.version('atrcopy'))"),
        version_re=r"atrcopy ([0-9]+\.[0-9]+(?:\.[0-9]+)?)",
        reference_for=(
            "ERZEUGT KORPUS fuer atr und xfd — und ist damit fuer diese "
            "Formate KEIN PRUEFER, aus demselben Grund wie c1541. "
            "Besonderheit: die beiden Fixtures stammen aus EINER Vorlage "
            "(`dos2sd.atr` aus dem pip-Paket); das xfd ist dasselbe Abbild "
            "ohne den 16-Byte-Kopf (MF-426). Zwei Eintraege, eine Quelle — "
            "wer sie als zwei unabhaengige Belege zaehlt, zaehlt einen "
            "doppelt."
        ),
        origin="https://github.com/robmcmullen/atrcopy",
        licence="MPL-2.0",
        abstammung=(
            "FUENFTE FRAGE (MF-644), beantwortet: Erzeuger, nicht Pruefer "
            "— siehe `reference_for`. Fuer den ATR-LESER des Baums ist "
            "atrcopy damit dieselbe Hand; eine echte Hebung von `atr` "
            "braucht eine zweite Quelle oder einen realen Dump. "
        ),
    ),
    Oracle(
        name="cpmls",
        env="CPMLS",
        exes=("cpmls", "cpmls.exe"),
        version_args=("-h",),
        version_re=r"cpmtools[- ]([0-9][0-9.]*)",
        reference_for="CP/M-Verzeichnislesung gegen eine `diskdefs`-"
                      "Definition — der Bezug fuer den diskdefs-Parser "
                      "(Mammut 1.1). Liest cpmls ein Abbild und UFT nicht "
                      "gleich, liegt es an UFT.",
        origin="https://github.com/lipro-cpm4l/cpmtools",
        licence="GPL-3.0",
        version_exit_ok=(0, 1, 2),
        abstammung=(
            "VERDACHT, gemessen: `include/uft/formats/uft_cpm_defs.h` "
            "nennt „libdsk diskdefs“, und "
            "`src/formats/cpm/uft_cpm_diskdefs.c` fuehrt 55 fest "
            "verdrahtete Definitionen mit cpmtools als Referenz im Kopf "
            "(CLAUDE.md, LIZ-1). Fuer einen Vergleich der DISKDEFS ist "
            "cpmls damit moeglicherweise dieselbe Hand; fuer das Lesen "
            "eines konkreten Abbilds nicht zwingend. VOR dem ersten Lauf "
            "trennen. "
        ),
    ),
    Oracle(
        name="hxcfe",
        env="HXCFE",
        exes=("hxcfe", "hxcfe.exe"),
        version_args=("-help",),
        version_re=r"([0-9]+\.[0-9]+\.[0-9]+)",
        reference_for="Format-Wandlung ueber viele Container (HFE, IMG, "
                      "DSK, …); Bezug fuer T1b-Eingaben, die UFT lesen "
                      "koennen muss",
        origin="https://hxc2001.com/floppy_drive_emulator/ "
               "(HxCFloppyEmulator, Quellen auf GitHub)",
        licence="GPL-2.0",
        version_exit_ok=(0, 1),
        abstammung=(
            "VERDACHT, gemessen: `include/uft/formats/flux/uft_hxcstream.h` "
            "nennt „HxC Floppy Emulator project by Jean-Francois DEL "
            "NERO“, `rawformatdialog.h` „der Oberflaeche von "
            "HxCFloppyEmulator“. Ob die Attributionen Doku oder Code "
            "meinen, ist ungeklaert — die erste ist eine CODE-Erklaerung "
            "ohne Lizenz (MF-743). "
        ),
    ),
    Oracle(
        name="samdisk",
        env="SAMDISK",
        exes=("samdisk", "samdisk.exe"),
        version_args=("--version",),
        version_re=r"SAMdisk\s+([0-9]+\.[0-9]+)",
        # MF-785: `--version` meldet die Fassung auf stdout und kehrt
        # mit RUECKGABEWERT 1 zurueck. Ohne diese Zeile blieb der
        # Eintrag „Version unbekannt", obwohl die Zahl dastand.
        version_exit_ok=(0, 1),
        reference_for="Container-Formate und ihre Randfaelle; die QUELLE "
                      "liegt zusaetzlich im Baum (src/samdisk/) und dient "
                      "als Spec-Referenz, etwa fuer `.tc` (Mammut 1.4)",
        origin="https://github.com/simonowen/samdisk",
        licence="MIT",
        abstammung=(
            "VERDACHT, und der schaerfste im Register: dieser Baum FUEHRT "
            "147 SamDisk-Quelldateien in `src/samdisk/` mit, im Bauplan, "
            "von der SPDX-Politik ausgenommen (`audit_spdx_policy.py:68`). "
            "Ein Differenzlauf verglichte UFT mit einem Werkzeug, dessen "
            "Quelltext wir mitfuehren. Vor jedem Urteil ist zu klaeren, ob "
            "der gepruefte Pfad durch vendorierten SamDisk-Code laeuft. "
        ),
    ),
    Oracle(
        name="dtc",
        env="DTC",
        exes=("dtc", "dtc.exe"),
        version_args=("-h",),
        version_re=r"([0-9]+\.[0-9]+)",
        reference_for="KryoFlux-Rohstrom-Aufnahme; Bezug fuer den "
                      "KryoFlux-Lesepfad",
        origin="https://kryoflux.com (DTC, Binaerdistribution)",
        licence="proprietaer, nur Ausfuehrung",
        version_exit_ok=(0, 1, 255),
        abstammung=(
            "UNGEPRUEFT, Lage schwaecher als bei den anderen: die Treffer "
            "(`uft_kryoflux_stream.c` „Reference: KryoFlux Stream "
            "Protocol, DTC documentation“, `uft_protection_extended.h` "
            "„DTC learnings“) nennen DOKUMENTATION, nicht Code — und dtc "
            "ist proprietaer, sein Quelltext war nie zugaenglich. Eine "
            "Code-Abstammung ist damit unwahrscheinlich; belegt ist sie "
            "nicht. "
        ),
    ),
    Oracle(
        name="floptool",
        env="FLOPTOOL",
        exes=("floptool", "floptool.exe"),
        version_args=(),
        version_re=r"(?!x)x",
        version_is_unaskable=True,
        reference_for=(
            "Verzeichnislesung mit AUSDRUECKLICH genanntem Container und "
            "Dateisystem: `floptool flopdir <format> <fs> <datei>`. "
            "STAERKER als flopdir und seit MF-629 die bevorzugte Form: "
            "`flophashes <format> <fs> <datei>` gibt CRC32 UND SHA-1 je "
            "Datei aus, `flopread ... <pfad> <ziel>` holt die Bytes "
            "heraus. Damit heisst der Differenzlauf nicht mehr 'das "
            "Verzeichnis sieht gleich aus', sondern 'derselbe Inhalt, "
            "byteweise nachgerechnet'. Gemessen am Korpus-D64: "
            "`UFT MARKER`, 254 Byte, sha1 56fea729e9e37c473b12c3b76fc0d"
            "3e387b39b5a. Auch `pc_fat` liest floptool vollstaendig "
            "(gegen ein selbstgebautes 720K-FAT12 geprueft) — es ist "
            "also NICHT auf CBM beschraenkt. "
            "Gemessen am freien Korpus (mame0289b): D64, D71 und G64 "
            "liefern gegen `cbmdos` eine echte Auflistung samt Volumename "
            "und Disk-ID. NICHT geeignet fuer die uebrigen Phase-1-Ziele — "
            "floptool kennt weder ein Amiga- noch ein Atari-DOS-"
            "Dateisystem (62 Dateisystem-Eintraege, keiner davon). "
            "FALLSTRICK: der Container wird geprueft, das Dateisystem "
            "NICHT. `flopdir adf cbmdos` auf einem AmigaDOS-Abbild endet "
            "mit rc=0, leerem Volumenamen und leerer Liste — ein "
            "schweigender Fehlgriff. Zufallsbytes und ein falscher "
            "Container fliegen dagegen laut heraus. Wer dieses Oracle "
            "benutzt, wertet eine LEERE Auflistung als „kein Ergebnis\", "
            "nie als „leere Diskette\". Zusaetzlich gemessen: auf .d80 "
            "und .d82 haengt floptool (>9 min, abgebrochen) — jeder "
            "Aufruf braucht ein Zeitlimit."),
        origin="MAME-Distribution (https://github.com/mamedev/mame), "
               "Werkzeug floptool; hier aus mame0289b, dessen SHA-256 "
               "gegen die offiziellen SHA256SUMS des Release geprueft "
               "wurde",
        licence="GPL-2.0-or-later (MAME); hier wird nur die AUSGABE "
                "verglichen, es wandert kein Code ein",
        abstammung=(
            "VERDACHT, gemessen: floptool IST das MAME-Werkzeug, und "
            "`include/uft/protection/uft_magnetic_state.h` traegt „Based "
            "on MAME lib/formats/flopimg.h“ — eine CODE-Erklaerung. Fuer "
            "alles, was ueber `uft_magnetic_state` laeuft, ist floptool "
            "damit dieselbe Hand. Fuer Formate ohne diesen Bezug nicht. "
        ),
    ),
    Oracle(
        name="lsatr",
        env="LSATR",
        exes=("lsatr", "lsatr.exe"),
        version_args=("-v",),
        version_re=r"mkatr version ([0-9]+\.[0-9]+)",
        reference_for=(
            "Atari-DOS-Dateisysteme in ATR und XFD: Sektorgeometrie, "
            "DOS-Variante und freie Sektoren (`lsatr <datei>`), Inhalte "
            "je Datei ueber `-x`/`-X`. Loest die ZIRKULARITAET des "
            "ATR-Korpus: der stammt von atrcopy (robmcmullen), lsatr ist "
            "eine unabhaengige C-Codebasis von Daniel Serpell — 0 Treffer "
            "fuer atrcopy/robmcmullen/omnivore im ganzen Quellbaum. Der "
            "Nachfolger atrip ist dagegen KEIN Oracle: `README.rst:6` "
            "nennt ihn woertlich den Nachfolger von atrcopy, also "
            "dieselbe Hand (fuenfte Registrierungsfrage, MF-644). "
            "Selbst gemessen am freien Korpus: `atrcopy_dos2sd.atr` und "
            "`.xfd` liefern ZEICHENGLEICH `720 sectors of 128 bytes, "
            "DOS 2.0s, 707 sectors free of 707 total.` (je rc=0) — das "
            "ist zugleich die dritte unabhaengige Bestaetigung, dass XFD "
            "das ATR ohne den 16-Byte-Kopf ist. "
            "FALLSTRICK, ebenfalls gemessen: dieses Korpus-Abbild ist "
            "LEER (707 von 707 Sektoren frei, kein Verzeichniseintrag). "
            "Ein Differenzlauf darauf entscheidet auf Datei-Ebene NICHTS "
            "— wer lsatr fuer eine Inhaltspruefung benutzt, braucht "
            "zuerst ein nicht-leeres Abbild."),
        origin="https://github.com/dmsc/mkatr (Daniel Serpell); hier mit "
               "`mingw32-make CC=gcc` aus dem Klon gebaut, gcc 13.1.0, "
               "rc=0 ohne Warnungen",
        licence="GPL-2.0-or-later — 26 von 26 Quelldateien tragen "
                "\"either version 2 ... or (at your option) any later "
                "version\" im Kopf; nur die AUSGABE wird verglichen, es "
                "wandert kein Code ein",
        abstammung=(
            "GEPRUEFT UND UNABHAENGIG: eigene C-Codebasis von Daniel "
            "Serpell; der ATR-Korpus stammt von atrcopy (robmcmullen), und "
            "im ganzen Quellbaum stehen 0 Treffer fuer "
            "atrcopy/robmcmullen/omnivore. Der Nachfolger atrip ist "
            "ausdruecklich KEIN Oracle — `README.rst:6` nennt ihn den "
            "Nachfolger von atrcopy, also dieselbe Hand. "
        ),
    ),
    Oracle(
        name="xdftool",
        env="XDFTOOL",
        exes=("xdftool", "xdftool.exe"),
        version_args=(),
        version_via=(sys.executable, "-c",
                     "import importlib.metadata as m; "
                     "print('amitools', m.version('amitools'))"),
        version_re=r"amitools ([0-9]+\.[0-9]+(?:\.[0-9]+)?)",
        reference_for=(
            "AmigaDOS-Verzeichnis und Dateiinhalte in ADF (`xdftool <img> "
            "list`, `... read <datei>`). Es ist zugleich der ERZEUGER des "
            "Korpus-Abbilds `tests/corpus_free/xdftool_dd_ofs.adf` "
            "(`tests/corpus_manifest/manifest.json:27`) — es beantwortet "
            "damit die Provenienzfrage, NICHT die Richtigkeitsfrage: ein "
            "Abbild mit seinem eigenen Erzeuger zu pruefen waere "
            "zirkulaer. "
            "LAENGENSEMANTIK: **roh**. Heute erneut gemessen an genau "
            "diesem Abbild — `marker.txt` wird mit **127** gemeldet, "
            "nicht mit 488 (der OFS-Blockkapazitaet). Das Hausmass ist "
            "krumm, damit eine gepolsterte Antwort auffaellt (MF-684/685). "
            "FUENFTE FRAGE (MF-644), gemessen statt vermutet: die zweite "
            "Hand gegen dieses Abbild ist `adfrescue` (dschwen, Commit "
            "`0cbc5ff`, 2015-12-21) — **225 Zeilen eigenstaendiges C++** "
            "mit nur `stdio/stdlib/string.h`, kein ADFlib, kein amitools, "
            "kein gemeinsamer Unterbau; `xdftool` ist Python. Geteilt ist "
            "allein die **Doku** (ADF-FAQ, Laurent Clevy) — eine Spec, "
            "kein Code. Die Byte-Identitaet der 127 Byte ist damit ein "
            "Beleg und keine Tautologie. Was das NICHT ausschliesst: "
            "einen Fehler, den beide aus derselben FAQ uebernommen "
            "haetten."),
        origin="https://github.com/cnvogelg/amitools — hier als "
               "pip-Installation, Paketversion ueber "
               "`importlib.metadata.version(\"amitools\")` abgefragt, weil "
               "das Werkzeug selbst keine Versionszeile ausgibt (siehe "
               "`version_via`). Die SHA-256 der aufgeloesten Datei steht "
               "im Manifest daneben und sagt, welches Programm lief",
        licence="GPL-2.0-or-later (Paket-Metadatum "
                "`License-Expression`); nur die AUSGABE wird verglichen, "
                "es wandert kein Code ein",
        abstammung=(
            "GEPRUEFT: amitools (Christian Vogelgsang) ist gegenueber "
            "unserem AmigaDOS-Leser eine eigene Codebasis; der Baum traegt "
            "keine amitools-Attribution. Die Grenze steht im "
            "`reference_for`. "
        ),
    ),
    Oracle(
        name="to_woz2",
        env="TO_WOZ2",
        exes=("to_woz2", "to_woz2.exe"),
        version_args=(),
        version_re=r"(?!x)x",
        version_is_unaskable=True,
        reference_for=(
            "Apple-II-Sektorabbild -> WOZ 2.0: `to_woz2 <ein.dsk|.d13> "
            "<aus.woz>`. Es SYNTHETISIERT den GCR-Bitstrom (6-and-2 fuer "
            "16 Sektoren, 5-and-3 fuer 13) und ist damit die fremde Hand "
            "fuer genau das, was dieser Baum gemessen NICHT hat: einen "
            "gebauten GCR-Encoder (`uft_nib_parser_v2.c:13` sagt selbst "
            "\"decoding\"). Damit ist es der Erzeuger von "
            "Fremdwerkzeug-Abbildern fuer `do`, `po` und `d13` — auf der "
            "Leiter aus `scripts/gen_verification_tiers.py` ist das "
            "**T1b** (cross-tool image, read back by UFT), NICHT T2. "
            "T1b steht dort ueber T2. "
            "GEMESSEN, und das ist der Grund fuer "
            "`version_is_unaskable`: das Werkzeug hat weder `--version` "
            "noch `-V`; der argumentlose Aufruf gibt seinen "
            "Gebrauchstext und rc=0. "
            "DER ANKER IST NICHT DER BINAERHASH. Zwei unabhaengige Baue "
            "aus demselben Quellstand (Scout-Bau und Gegenbau) haben "
            "VERSCHIEDENE Binaerhashes (`434cfbda...`, `4dbb8def...`) "
            "und liefern BYTEIDENTISCHE Ausgabe (16-Sektor "
            "`0015aa1e20247177f48a...`, 13-Sektor "
            "`a5ff575f7f82592c80e6...`). Ein Binaerhash pinnt hier den "
            "Uebersetzer, nicht das Werkzeug. Zitierfaehig ist der "
            "QUELLSTAND (Commit `639dc1c`) plus das Baurezept aus "
            "`origin` plus die AUSGABE-SHA fuer eine benannte Eingabe. "
            "Der Hash im Manifest sagt weiterhin, WELCHER BAU lief — das "
            "ist eine Bau-Angabe, keine Werkzeug-Identitaet. "
            "FUENFTE FRAGE (MF-644), ehrlich beantwortet: eine zweite, "
            "unabhaengige Hand fuer WOZ 2.0 gibt es hier NICHT. Der "
            "Abgleich stuetzt sich auf die veroeffentlichte "
            "WOZ-2.0-Spezifikation (applesaucefdc.com) — eine Spec, kein "
            "zweites Werkzeug. Ein Fehler, den `to_woz2` aus der Spec "
            "uebernommen haette, faellt damit NICHT auf. "
            "FALLSTRICK, gemessen und nicht theoretisch (MF-712): mit "
            "einem ABSOLUTEN Pfad bricht `to_woz2` mit "
            "**0xC0000374 (STATUS_HEAP_CORRUPTION)** ab — das ist der "
            "1-Byte-Ueberlauf in `parse_filename` "
            "(`to_woz2.c:367-369`), und die erste Fassung des "
            "Eich-Tests hat ihn ausgeloest. BENUTZUNGSREGEL: immer aus "
            "dem Arbeitsverzeichnis mit RELATIVEN Namen rufen "
            "(`cwd=<verzeichnis>`, Argumente `e.dsk e.woz`). So "
            "gemessen: rc=0, 252416 Byte, Kopf `WOZ2 FF 0A 0D 0A`. "
            "Wer das Werkzeug mit fremden Dateinamen fuettert, fuettert "
            "einen Ueberlauf. "
            "WEITERE GRENZEN, gemessen: kein `.po` (nur DSK/DO/D13); "
            "der Kopf-CRC bleibt 0 (spec-legal)."),
        origin="https://github.com/cmosher01/Apple-II-Disk-Tools "
               "(Charles Mosher), Commit `639dc1c`, vom Upstream als "
               "DEPRECATED gekennzeichnet. Autotools werden NICHT "
               "gebraucht; hier direkt gebaut, gcc 13.1.0 (MinGW), rc=0, "
               "0 Warnungen, aus `src/`: "
               "`gcc -O2 -o to_woz2 to_woz2.c nibblize_4_4.c "
               "nibblize_5_3.c nibblize_5_3_alt.c nibblize_5_3_common.c "
               "nibblize_6_2.c ctest/ctest.c -I.` "
               "(das ctest-Submodul haengt an einem toten `git://` und "
               "wird so umgangen)",
        licence="GPL-3.0 (`COPYING`, woertlicher Text; Zone GELB) — kein "
                "Port zulaessig, verglichen wird ausschliesslich die "
                "AUSGABE, es wandert kein Code ein. Nachgelagerte "
                "Attribution im Quellkopf: `nibblize` \"Based on code by "
                "Andy McFadden\" (CiderPress, BSD-3) — eine zweistufige "
                "Kette, die nur bei einem Port zu klaeren waere",
        abstammung=(
            "GEPRUEFT, mit benannter Luecke: `to_woz2` (cmosher01) ist "
            "gegenueber unserem Baum eine fremde Hand — der Baum hatte "
            "gemessen KEINEN GCR-Encoder. Die fuenfte Frage bleibt dennoch "
            "offen fuer die SPEZIFIKATION: beide stuetzen sich auf die "
            "veroeffentlichte WOZ-2.0-Beschreibung, ein Fehler AUS DER "
            "SPEC faellt nicht auf. "
        ),
    ),
    # NICHT registriert: `adfrescue`. Die Unabhaengigkeits-Messung oben
    # steht, aber der Eintrag haengt an einer Eigentuemer-Entscheidung —
    # das Repo hat **keine** Lizenzdatei (Zone ROT: alle Rechte
    # vorbehalten) und liefert kein Binaerprogramm; der Bau braucht
    # `-include cstdint -Du_int32_t=uint32_t`, weil `u_int32_t` unter
    # MinGW nicht existiert (heute nachgemessen: ohne die Zusaetze bricht
    # g++ 13.1.0 in `checksum()` ab). Lizenz vor Faehigkeit — die drei
    # Wege stehen in `docs/OPEN_ITEMS.md` unter ORAK-1.
    Oracle(
        name="xadundisk",
        env="XADUNDISK",
        exes=("xadUnDisk", "xadundisk", "xadUnDisk.exe"),
        version_args=(),
        version_re=r"(?!x)x",
        version_is_unaskable=True,
        reference_for=(
            "Amiga-Disketten-ARCHIVE: `xadUnDisk <archiv> <ziel.adf>`. "
            "Deckt in diesem Baum genau ZWEI Plugins ab, und beide "
            "stehen auf T3 — `dms` (Disk Masher System) und `trd` "
            "(TR-DOS). Das ist der ganze Ertrag, und er ist echt: "
            "„ungeprueft runter“ ist die erste der vier "
            "Release-Kennzahlen. "
            "WARUM GERADE HIER: `dms` traegt heute die Attribution "
            "„Based on xDMS source, dms2adf, AROS source“ "
            "OHNE genannte Lizenz — einer der offenen Faelle aus "
            "MF-743. Ein Oracle-Weg verifiziert das Format, ohne diese "
            "Frage anzufassen: das Werkzeug laeuft extern, verglichen "
            "wird nur die Ausgabe, es entsteht kein abgeleitetes Werk "
            "(MF-695, Kanal „Oracle: Ausfuehrung frei, Weitergabe "
            "nicht“, derselbe wie bei `dtc`). "
            "WAS HIER NICHT GEMESSEN IST, und das ist wichtig: dieses "
            "Werkzeug wurde in diesem Baum **nie ausgefuehrt**. Der "
            "Programmname stammt aus der Projektdokumentation (die "
            "Shell-Werkzeuge liegen im `C`-Verzeichnis von xadmaster; "
            "`xadUnDisk` fuer Disketten-, `xadUnFile` fuer "
            "Datei-Archive), nicht aus einem Aufruf. Solange kein "
            "Differenzlauf existiert, traegt dieser Eintrag KEIN "
            "T1b-Manifest — er macht die Referenz nach ORAK-1 nur "
            "zitierfaehig. Vor dem ersten Urteil zu messen: Bauweg, "
            "Aufrufform, und ob die Ausgabe fuer `dms` und `trd` "
            "ueberhaupt mit unserer vergleichbar ist. "
            "FUENFTE FRAGE (MF-644) — GEMESSEN UND BEANTWORTET, MF-758: "
            "**fuer `dms` ist es DIESELBE HAND.** Der Kopf von "
            "`DMS.c` sagt woertlich „This client is lossely "
            "based (mainly decrunch stuff) on xDMS source made by "
            "Andre R. de la Rocha“ — und unsere Attribution "
            "lautet „Based on xDMS source, dms2adf, AROS "
            "source“. Ein von xDMS geerbter Fehler steckt damit "
            "in BEIDEN, und ein Differenzlauf entscheidet darueber "
            "nichts. "
            "PRAEZISIERUNG, ebenfalls aus dem Kopf: geteilt ist "
            "ausdruecklich nur die ENTPACKUNG („mainly decrunch "
            "stuff“); derselbe Kopf sagt, der Client benutze "
            "„not DMS header information (except password flag), "
            "but always the track data“. Fuer einen Vergleich der "
            "SPURDATEN kann er also unabhaengig sein, fuer die "
            "Entpackung ist er es nicht. Wer `dms` trotzdem hierueber "
            "pruefen will, trennt beides — sonst ist der Lauf "
            "zirkulaer. "
            "DER ERSTE LAUF IST DESHALB `trd`, NICHT `dms`. TR-DOS hat "
            "die Abstammungsfrage nicht; dort faellt die Kennzahl "
            "zuerst. Fuer `dms` ist der saubere Weg das ORIGINAL-DMS "
            "unter Emulation — dasselbe Muster wie bei X-Copy. "
            "DIE ANDEREN ACHT Clients (SuperDuper3, CrunchDisk, "
            "PackDisk, PackDev, Zoom, xDisk, MDC, DCS) haben in diesem "
            "Baum KEIN Plugin. Sie sind Fundus, nicht Auftrag — die "
            "EINFRIER-REGEL sperrt neue Format-Plugins ausdruecklich "
            "auch als Vorschlag."),
        origin="XAD von Dirk Stoecker (1998); Architektur "
               "`xadmaster.library` plus Clients, Kommandozeilen-"
               "werkzeuge fuer Linux und Windows. NICHT hier gebaut, "
               "NICHT hier gelaufen — Angaben aus der "
               "Projektdokumentation.",
        licence="LGPL — als Oracle unerheblich, weil das Werkzeug "
                "extern laeuft und nur die AUSGABE verglichen wird; es "
                "wandert kein Code ein. Die genaue Fassung "
                "(2.1-or-later?) ist NICHT am Lizenztext gemessen.",
        abstammung=(
            "GEMESSEN, MF-758, und gespalten: fuer `dms` DIESELBE HAND — "
            "der Kopf von `DMS.c` sagt „lossely based (mainly decrunch "
            "stuff) on xDMS source“, unsere Attribution „Based on xDMS "
            "source, dms2adf, AROS source“. Fuer `trd` besteht die Frage "
            "nicht. Praezisierung aus demselben Kopf: geteilt ist nur die "
            "ENTPACKUNG, der Client benutze „not DMS header information "
            "... but always the track data“ — fuer die SPURDATEN kann er "
            "unabhaengig sein. "
        ),
    ),
)


MIN_PURPOSE_CHARS = 20
"""Kuerzester zulaessiger `reference_for`-Text.

Die Schwelle steht HIER und nicht zusaetzlich im Test: zwei Schwellen fuer
dieselbe Regel driften auseinander, und dann faengt eine der beiden Stellen
etwas, das die andere durchlaesst — genau das ist beim ersten Rotbeweis
passiert."""

_BY_NAME = {o.name: o for o in REGISTRY}


def get(name: str) -> Oracle:
    """Eintrag holen. Unbekannter Name ist ein Programmierfehler."""
    try:
        return _BY_NAME[name]
    except KeyError:
        raise KeyError(
            "Kein Oracle namens %r. Bekannt: %s"
            % (name, ", ".join(sorted(_BY_NAME)))
        ) from None


def resolve_oracle(o: Oracle) -> Path | None:
    """Pfad zum Werkzeug, oder None.

    Reihenfolge: Umgebungsvariable, dann PATH. Es wird NICHT in
    Installationsverzeichnissen gesucht — wer eine Referenz zitiert, soll
    wissen, welches Binaerprogramm gemeint war.

    Nimmt einen Eintrag statt eines Namens, damit sich die Aufloesung auch
    dann pruefen laesst, wenn keines der registrierten Werkzeuge auf der
    Maschine liegt — sonst waere genau der Code ungeprueft, der entscheidet,
    ob eine Referenz gefunden wurde.
    """
    override = os.environ.get(o.env)
    if override:
        p = Path(override)
        return p if p.exists() else None
    for exe in o.exes:
        found = shutil.which(exe)
        if found:
            return Path(found)
    return None


def version_of(o: Oracle, path: Path | None = None) -> str | None:
    """Versionszeichenkette, oder None wenn sie sich nicht abfragen laesst.

    None ist ein ehrliches Ergebnis und kein Fehler — aber ein Oracle ohne
    Version taugt nicht fuer ein T1b-Manifest, und `manifest_entry()`
    sagt das dann auch.
    """
    exe = path or resolve_oracle(o)
    if exe is None:
        return None
    # `version_via` fragt eine andere Stelle als das Startprogramm — aber
    # erst, NACHDEM das Programm aufgeloest wurde. Ein Paket ohne sein
    # Werkzeug darf keine Version melden, sonst stuende im Manifest eine
    # Version fuer einen Lauf, den es nicht gab.
    kommando = list(o.version_via) if o.version_via \
        else [str(exe), *o.version_args]
    try:
        r = subprocess.run(kommando, capture_output=True,
                           text=True, timeout=20)
    except (OSError, subprocess.SubprocessError):
        return None
    if r.returncode not in o.version_exit_ok:
        return None
    m = re.search(o.version_re, (r.stdout or "") + (r.stderr or ""))
    return m.group(1) if m else None


def resolve(name: str) -> Path | None:
    """Wie @ref resolve_oracle, ueber den Kurznamen."""
    return resolve_oracle(get(name))


def version(name: str, path: Path | None = None) -> str | None:
    """Wie @ref version_of, ueber den Kurznamen."""
    return version_of(get(name), path)


def sha256_of(path: Path | None) -> str | None:
    """SHA-256 der Werkzeug-Datei — der starke Herkunfts-Anker (MF-623).

    Steht bei JEDEM aufgeloesten Eintrag im Manifest, nicht nur bei denen
    ohne Versionsabfrage: eine Version sagt, welche Fassung gemeint war,
    der Hash sagt, welches Programm tatsaechlich gelaufen ist.
    """
    if path is None:
        return None
    try:
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for block in iter(lambda: f.read(1 << 20), b""):
                h.update(block)
        return h.hexdigest()
    except OSError:
        return None


def manifest_entry_for(o: Oracle, path: Path | None = None) -> dict:
    """Die Angaben, die ein T1b-Manifest ueber seinen Erzeuger braucht.

    `complete` ist genau dann True, wenn das Werkzeug gefunden wurde UND
    seine Herkunft gepinnt ist — durch die Version, oder, bei einem
    Werkzeug das seine Version nicht nennen kann und das ausdruecklich
    erklaert, durch die SHA-256 seiner Datei. Fehlt beides, zaehlt der
    Korpus-Eintrag nicht (VERIFICATION_PLAN, Provenienz-Regel), und
    dieses Feld sagt es, statt es offenzulassen.
    """
    p = path if path is not None else resolve_oracle(o)
    v = version_of(o, p) if p else None
    sha = sha256_of(p)
    anker = bool(v) or bool(o.version_is_unaskable and sha)
    return {
        "tool": o.name,
        "path": str(p) if p else None,
        "version": v,
        "sha256": sha,
        "origin": o.origin,
        "licence": o.licence,
        "reference_for": o.reference_for,
        "complete": bool(p and anker),
    }


def manifest_entry(name: str) -> dict:
    """Wie @ref manifest_entry_for, ueber den Kurznamen."""
    return manifest_entry_for(get(name))


def available() -> dict[str, bool]:
    """Welche Oracles sind auf dieser Maschine da?"""
    return {o.name: resolve(o.name) is not None for o in REGISTRY}


# ── Selbsttest ───────────────────────────────────────────────────────────
#
# Laeuft ohne pytest, damit ihn auch eine CI ohne pytest ausfuehren kann.
# Er prueft die REGISTRY auf Widersprueche (das ist die Zusicherung, die
# immer gilt) und BERICHTET, welche Werkzeuge gefunden wurden — ohne
# Fundstuecke zu verlangen. Ein fehlendes Werkzeug ist eine Luecke der
# Umgebung, kein Fehler des Codes.

def _selfcheck() -> int:
    problems: list[str] = []

    seen: set[str] = set()
    for o in REGISTRY:
        if o.name in seen:
            problems.append("Name doppelt: %s" % o.name)
        seen.add(o.name)
        if not o.exes:
            problems.append("%s: keine ausfuehrbaren Namen" % o.name)
        # Genau EINE der drei Antworten auf "wie wird die Version
        # festgestellt?" darf gesetzt sein. Keine heisst: die Aussage ist
        # nicht zitierfaehig. Zwei heissen: es gibt zwei Wahrheiten ueber
        # denselben Anker, und die driften (MF-693).
        wege = [bool(o.version_args), bool(o.version_via),
                bool(o.version_is_unaskable)]
        if not any(wege):
            problems.append("%s: keine Versionsabfrage - dann ist seine "
                            "Aussage nicht zitierfaehig" % o.name)
        if sum(wege) > 1:
            problems.append("%s: mehr als ein Weg zur Version gesetzt "
                            "(version_args / version_via / "
                            "version_is_unaskable) - hoechstens einer ist "
                            "richtig" % o.name)
        try:
            re.compile(o.version_re)
        except re.error as exc:
            problems.append("%s: Versions-Muster unbrauchbar (%s)"
                            % (o.name, exc))
        if o.version_re.count("(") == 0:
            problems.append("%s: Versions-Muster ohne Gruppe" % o.name)
        if len(o.reference_for.strip()) < MIN_PURPOSE_CHARS:
            problems.append("%s: sagt nicht (genug), wofuer es Referenz "
                            "ist - mindestens %d Zeichen"
                            % (o.name, MIN_PURPOSE_CHARS))
        if not o.origin.strip():
            problems.append("%s: nicht beschaffbar (keine Herkunft)" % o.name)
        if not o.licence.strip():
            problems.append("%s: Lizenz unbenannt" % o.name)
        if not o.abstammung.strip():
            problems.append("%s: ABSTAMMUNG unbenannt — die fuenfte "
                            "Frage gehoert VOR den Eintrag, nicht "
                            "nach dem ersten Lauf (MF-760)" % o.name)
        if o.code_import:
            problems.append("%s: code_import=True - ein Oracle vergleicht "
                            "Ausgaben; Code-Import braucht eine eigene "
                            "Entscheidung, nicht diese Tabelle" % o.name)

    offen = [o.name for o in REGISTRY
             if "UNGEPRUEFT" in o.abstammung or "VERDACHT" in o.abstammung]
    print("Oracle-Registry: %d Eintraege, %d mit offener Abstammung%s"
          % (len(REGISTRY), len(offen),
             (" (" + ", ".join(offen) + ")") if offen else ""))
    have = available()
    for o in REGISTRY:
        if have[o.name]:
            v = version(o.name)
            if not v:
                v = ("per SHA-256 verankert" if o.version_is_unaskable
                     else "Version unbekannt")
            print("  [da]     %-9s %-22s %s"
                  % (o.name, v, resolve(o.name)))
        else:
            print("  [fehlt]  %-9s %s" % (o.name, o.origin))
    n = sum(1 for v in have.values() if v)
    print("%d von %d verfuegbar%s"
          % (n, len(REGISTRY),
             " - Tests, die eines brauchen, ueberspringen sich" if n < len(REGISTRY) else ""))

    if problems:
        print("\nREGISTRY-FEHLER:")
        for p in problems:
            print("  " + p)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(_selfcheck())
