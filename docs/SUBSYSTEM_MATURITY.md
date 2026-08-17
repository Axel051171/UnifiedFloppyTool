# Subsystem-Reifegrad (ehrliche Gesamtübersicht)

Stand 2026-08-16 (MF-366, Phase 3 des [Verifikationsplans](VERIFICATION_PLAN.md)).
Hand-kuratiert; Format-Layer-Zeile ist skript-generiert
([VERIFICATION_TIERS.md](VERIFICATION_TIERS.md)). Dieselbe Ehrlichkeits-Frage
wie beim Format-Layer, auf alles angewandt: **wogegen wurde je verifiziert?**

Legende: ✅ real verifiziert · 🟡 synthetisch/Spec-verifiziert · 🔴 ungeprüft
(Behauptung ohne Test-Nachweis) · ⬜ Scaffold/geplant

| Subsystem | Umfang (behauptet) | Verifikations-Stand | Evidenz |
|---|---|---|---|
| **Format-Layer** | 88 Plugins / 138 IDs | 🟡 T1=0, T1b=3, T2=14, 🔴 T3=71 | skript-generiert, [VERIFICATION_TIERS.md](VERIFICATION_TIERS.md) |
| **HAL: Greaseweazle** | production | 🟡 code-stabil (byte-identisch zu HW-verifiziertem v4.1.4-rc1-Stand), **kein formaler Bench-Pass in diesem Release** | README §Hardware; HIL.GW → v4.1.6 |
| **HAL: SCP-Direct** | read-flux wired | 🟡 22/22 USB-Opcodes byte-verifiziert vs. samdisk, 9 Mock-Tests; **Tier-3 Real-HW ausstehend** (UFT-008) | `impl_complete=false` |
| **HAL: KryoFlux / FC5025** | subprocess-Wrapper | 🔴 nie gegen reales Tool+Hardware gelaufen | keine HW vorhanden (MF-310) |
| **HAL: XUM1541 / Applesauce / FluxEngine / ADFCopy / USBFloppy** | honest-stubs / partial | ⬜ Utility+Lifecycle real, I/O nicht verdrahtet (M3.x) | Stub-Honesty-Asserts grün |
| **HW-Emulatoren (Tier-2.5)** | 9/9 Controller | 🟡 firmware-realistische Emulatoren, synthetisch; ersetzen keinen Bench | `tests/emulators/` |
| **DeepRead** (8 Module) | Adaptive Decode, Weighted Voting, Splice/Aging/Correlation/Fingerprint/LLR | 🔴 **0 Tests im aktiven Suite-Lauf**; nie gegen reale beschädigte Disk | ctest-Census 2026-08-16 |
| **OTDR-Pipeline** (12 Stufen) | Signal-Analyse, Confidence | 🟡 seit MF-374: 8 aktive Tests (otdr_bridge, Event-Kette v8-v12, denoise, align_fuse, tdfc_plus) — synthetisch; nie gegen reale Captures | MF-374 Reaktivierung |
| **ML: Protection-Classifier** | Cosine-Similarity, V-MAX/RapidLok/… Referenz-Vektoren | 🔴 0 Tests; Referenz-Vektoren nie gegen echte geschützte Disks validiert | ctest-Census |
| **ML: Decoder/Training-Gen** | Header-API | ⬜ UFT_SKELETON_PLANNED — bewusst unimplementiert | `uft_ml_decoder.h` |
| **Kopierschutz-Erkennung** (55+ Schemes) | Heuristik-Detektoren + Titel-DB | 🟡 5 aktive Tests (synthetische Signaturen) + seit MF-377 **1 Scheme gegen realen geschützten Code**: CopyLock-ST Series 2 auf 14 echten 1989er-Loadern verifiziert (`magic32`/Offset exakt), Series 1 auf allen 16 realen Samples strukturell blind (PROT-1); Series 1 seit MF-380 **16/16** realer Loader (Erkennung am unverschlüsselten TVD-Prolog, Varianten a–e differenziert); zusammen also **30/30 CopyLock-Samples bei 0 Falsch-Positiven** über 15 Fremdschutzsysteme — PROT-1 geschlossen; C64-Pfad seit MF-381 durchgehend (G64 → GCR → `ufm_c64_metrics_from_gcr()` → Klassifikator, gegen reales VICE-G64 verifiziert, Negativkontrolle grün) — offen bleiben dort V-MAX!/RapidLok mangels belegter `custom sync`-Definition und die Positivkontrolle auf echter geschützter Disk (PROT-2); der fabrizierte `uft_dec0de_detect()` (0/34) wurde entfernt (PROT-3 ✓); 🔴 übrige 50+ Schemes weiterhin nie gegen echte Daten | test_protection_*, test_corpus_protection_copylock |
| **Recovery-Pipeline** | Multiread-Voting, Adaptive, CRC-Korrektur | 🟡 1 aktiver Test (`test_recovery`, synthetisch) | ctest-Census |
| **Filesystems** | AmigaDOS, FAT12/16, CBM DOS, Apple, CP/M, … | 🟡 FAT: 3 aktive Tests (inkl. FAT32/MBR seit MF-373); AmigaDOS-Validate + TI-99 seit MF-374 aktiv; CP/M-Test excluded (Header-Zwillinge, MF-374-Befund); übrige FS ungeprüft | ctest-Census + MF-373/374 |
| **Format-Konverter** | 45 Pfade | 🟡 13 Roundtrip-Matrix-Einträge (SSOT `uft_roundtrip.c`); übrige 32 Pfade 🔴 | `src/core/uft_roundtrip.c` |
| **Audit-Trail / Forensik-Report** | 40+ Event-Typen, 6 Export-Formate, Hash-Chain | 🔴 kein aktiver Test des Audit-Trail-Subsystems | ctest-Census |
| **Flux-Decoder/PLL-Kern** | MFM/FM/GCR, PLL, Sync | 🟡 PLL/MFM-Tests aktiv (synthetisch); nie gegen reale Flux-Captures | test_pll*, test_mfm* |

## Konsequenzen (verbindlich)

1. **Werbe-Aussagen** über 🔴-Subsysteme sind unzulässig, solange sie 🔴 sind —
   dieselbe Regel wie beim Format-Layer (MF-363). „Erkennt V-MAX!" ohne je eine
   echte V-MAX-Disk gesehen zu haben ist eine Behauptung, kein Feature.
2. **DeepRead/OTDR/ML** sind konzeptionell wertvoll, aber empirisch
   unvalidiert: sie benötigen reale Flux-Captures beschädigter/geschützter
   Disks — dieselbe Korpus-Abhängigkeit wie T1. Ohne Korpus bleiben sie 🔴.
3. Diese Tabelle wird bei jedem Release gegen den ctest-Census aktualisiert
   (Kandidat für spätere Skript-Generierung; bewusst billig gehalten,
   Beschluss Q11).
