# UFT Memory Ownership Policy

**Version:** 1.0  
**Status:** VERBINDLICH  
**Datum:** Januar 2026

---

## 1. Grundprinzipien

### 1.1 Jeder Speicher hat genau EINEN Owner

Der Owner ist verantwortlich für:
- Freigabe des Speichers (free)
- Weitergabe der Ownership (Transfer)
- Keine Verwendung nach Transfer

### 1.2 Ownership-Typen

| Typ | Wer gibt frei? | Caller nach Call |
|-----|----------------|------------------|
| **OWNS (Transfer)** | Callee | Pointer ist UNGÜLTIG |
| **BORROWS** | Caller | Pointer bleibt gültig |
| **OUT** | Caller (erhält) | Neuer gültiger Pointer |
| **INOUT** | Caller (behält) | Möglicherweise neuer Pointer |

---

## 2. Core API Ownership-Regeln

### 2.1 uft_track_t

```c
typedef struct {
    uft_sector_t* sectors;  // OWNED by track, freed by uft_track_cleanup()
    size_t sector_count;
    
    uint8_t* raw_data;      // OWNED by track, freed by uft_track_cleanup()
    size_t raw_size;
    
    uint32_t* flux;         // OWNED by track, freed by uft_track_cleanup()
    size_t flux_count;
} uft_track_t;
```

**REGELN:**
1. `uft_track_cleanup()` gibt ALLE internen Pointer frei
2. Nach Zuweisung zu `track->raw_data` besitzt Track den Speicher
3. Parser DARF raw_data nach Zuweisung NICHT freigeben

### 2.2 uft_sector_t

```c
typedef struct {
    uint8_t* data;          // OWNED by sector, freed by uft_sector_cleanup()
    size_t data_size;
} uft_sector_t;
```

---

## 3. Funktion-für-Funktion Ownership

### 3.1 Track-Funktionen

```c
/**
 * uft_track_init - Initialisiert Track-Struktur
 * 
 * @param track [OUT] Zu initialisierende Struktur (Stack oder Heap)
 * 
 * Ownership: Keine Allokation, kein Transfer
 */
void uft_track_init(uft_track_t* track, int cylinder, int head);

/**
 * uft_track_add_sector - Fügt Sektor zum Track hinzu
 * 
 * @param track [INOUT] Track dem Sektor hinzugefügt wird
 * @param sector [BORROWS] Sektor-Daten werden KOPIERT
 * 
 * Ownership: 
 * - Track kopiert sector->data (Deep Copy)
 * - Caller behält Ownership von sector->data
 * - Caller MUSS sector->data selbst freigeben (oder Speicher wiederverwenden)
 * 
 * KORREKT:
 *   uft_sector_t s = {.data = malloc(256), .data_size = 256};
 *   uft_track_add_sector(track, &s);
 *   free(s.data);  // MUSS freigegeben werden!
 * 
 * FALSCH:
 *   uft_sector_t s = {.data = malloc(256), .data_size = 256};
 *   uft_track_add_sector(track, &s);
 *   // Memory Leak: s.data wird nie freigegeben!
 */
uft_error_t uft_track_add_sector(uft_track_t* track, 
                                  UFT_BORROWS const uft_sector_t* sector);

/**
 * uft_track_set_flux - Setzt Flux-Daten für Track
 * 
 * @param track [INOUT] Track
 * @param flux [BORROWS] Flux-Array wird KOPIERT
 * 
 * Ownership:
 * - Track kopiert flux-Array (Deep Copy)
 * - Alte flux-Daten werden automatisch freigegeben
 * - Caller behält Ownership von flux
 */
uft_error_t uft_track_set_flux(uft_track_t* track,
                                UFT_BORROWS const uint32_t* flux,
                                size_t count, uint32_t tick_ns);

/**
 * uft_track_cleanup - Gibt alle Track-Ressourcen frei
 * 
 * @param track [INOUT] Track wird auf 0 gesetzt
 * 
 * Ownership:
 * - Gibt frei: sectors[], jedes sector.data, raw_data, flux
 * - Setzt alle Pointer auf NULL
 */
void uft_track_cleanup(uft_track_t* track);
```

### 3.2 Raw-Data Zuweisung (Parser)

```c
/**
 * Parser-Pattern für raw_data:
 * 
 * Ownership: raw_data wird an Track TRANSFERIERT
 * Parser DARF raw_data nach Zuweisung NICHT freigeben!
 */

// KORREKT:
uint8_t* gcr_data = malloc(track_size);
fread(gcr_data, track_size, 1, file);
track->raw_data = gcr_data;  // Transfer!
track->raw_size = track_size;
// gcr_data ist jetzt UNGÜLTIG für den Parser
// NICHT: free(gcr_data);
return UFT_OK;

// KORREKT bei Fehler:
uint8_t* gcr_data = malloc(track_size);
if (fread(...) != 1) {
    free(gcr_data);  // Noch nicht transferiert, Parser gibt frei
    return UFT_ERROR_FILE_READ;
}
track->raw_data = gcr_data;  // Ab jetzt ungültig
return UFT_OK;

// FALSCH:
uint8_t* gcr_data = malloc(track_size);
track->raw_data = gcr_data;
free(gcr_data);  // DOUBLE-FREE später in uft_track_cleanup()!
```

### 3.3 Sektor-Funktionen

```c
/**
 * uft_sector_copy - Erstellt Deep Copy eines Sektors
 * 
 * @param dst [OUT] Ziel (alle Felder werden überschrieben)
 * @param src [BORROWS] Quelle wird kopiert
 * 
 * Ownership:
 * - dst->data wird neu allokiert (Caller muss freigeben)
 * - src->data bleibt unverändert bei Caller
 */
uft_error_t uft_sector_copy(UFT_OUT uft_sector_t* dst,
                             UFT_BORROWS const uft_sector_t* src);

/**
 * uft_sector_cleanup - Gibt Sektor-Ressourcen frei
 * 
 * @param sector [INOUT] Sektor wird auf 0 gesetzt
 */
void uft_sector_cleanup(uft_sector_t* sector);
```

---

## 4. Parser-Implementierungs-Regeln

### 4.1 Sektor-Daten lesen

```c
// MUSTER: Sektor lesen und zu Track hinzufügen
static uft_error_t read_track(uft_disk_t* disk, int cyl, int head,
                               uft_track_t* track) {
    uft_track_init(track, cyl, head);
    
    for (int s = 0; s < sectors_per_track; s++) {
        uft_sector_t sector = {0};  // Stack-allokiert
        
        // Sektor-Daten allokieren
        sector.data = malloc(SECTOR_SIZE);
        if (!sector.data) {
            uft_track_cleanup(track);  // Bisherige Sektoren freigeben
            return UFT_ERROR_NO_MEMORY;
        }
        
        // Daten lesen
        if (fread(sector.data, SECTOR_SIZE, 1, f) != 1) {
            free(sector.data);  // Noch nicht zu Track hinzugefügt
            uft_track_cleanup(track);
            return UFT_ERROR_FILE_READ;
        }
        
        sector.data_size = SECTOR_SIZE;
        
        // Zu Track hinzufügen (macht Deep Copy)
        uft_error_t err = uft_track_add_sector(track, &sector);
        
        // WICHTIG: Immer freigeben, egal ob Erfolg oder Fehler!
        free(sector.data);
        sector.data = NULL;
        
        if (UFT_FAILED(err)) {
            uft_track_cleanup(track);
            return err;
        }
    }
    
    track->status = UFT_TRACK_OK;
    return UFT_OK;
}
```

### 4.2 Raw-Data zuweisen

```c
// MUSTER: GCR/MFM-Rohdaten zu Track zuweisen
static uft_error_t read_raw_track(uft_disk_t* disk, int cyl, int head,
                                   uft_track_t* track) {
    uft_track_init(track, cyl, head);
    
    uint8_t* raw = malloc(raw_size);
    if (!raw) {
        return UFT_ERROR_NO_MEMORY;
    }
    
    if (fread(raw, raw_size, 1, f) != 1) {
        free(raw);  // Noch nicht transferiert!
        return UFT_ERROR_FILE_READ;
    }
    
    // Ownership-Transfer: Ab hier ungültig für uns
    track->raw_data = raw;
    track->raw_size = raw_size;
    // NICHT: free(raw);
    
    track->status = UFT_TRACK_OK;
    return UFT_OK;
}
```

---

## 5. Häufige Fehler

### 5.1 Memory Leak nach uft_track_add_sector

```c
// FALSCH - Memory Leak:
uft_sector_t s = {.data = malloc(256)};
uft_track_add_sector(track, &s);
// s.data wurde NICHT freigegeben -> LEAK

// RICHTIG:
uft_sector_t s = {.data = malloc(256)};
uft_track_add_sector(track, &s);
free(s.data);  // Caller muss freigeben
```

### 5.2 Double-Free bei raw_data

```c
// FALSCH - Double Free:
uint8_t* data = malloc(1000);
track->raw_data = data;
free(data);  // Später: uft_track_cleanup() -> CRASH

// RICHTIG:
uint8_t* data = malloc(1000);
track->raw_data = data;  // Transfer
// NICHT freigeben - track ist jetzt Owner
```

### 5.3 Use-After-Free

```c
// FALSCH - Use After Free:
uint8_t* data = malloc(1000);
track->raw_data = data;
memset(data, 0, 1000);  // Zugriff auf transferierten Pointer

// RICHTIG:
uint8_t* data = malloc(1000);
memset(data, 0, 1000);  // VOR Transfer initialisieren
track->raw_data = data;  // Transfer
// Ab hier: data nicht mehr verwenden
```

---

## 6. Checkliste für Code-Review

- [ ] Jeder `malloc()` hat ein passendes `free()` ODER Transfer
- [ ] Nach `uft_track_add_sector()`: `sector.data` wird freigegeben
- [ ] Nach `track->raw_data = ptr`: `ptr` wird NICHT freigegeben
- [ ] Bei Fehler VOR Transfer: Speicher wird freigegeben
- [ ] Bei Fehler NACH Transfer: Speicher wird NICHT freigegeben
- [ ] `uft_track_cleanup()` wird bei Fehler aufgerufen
- [ ] Keine Pointer-Verwendung nach Transfer

---

## 7. Debugging-Hilfen

### 7.1 AddressSanitizer

```bash
# Kompilieren mit ASan
gcc -fsanitize=address -g source.c -o program

# Findet:
# - Use-after-free
# - Double-free
# - Memory leaks
# - Buffer overflows
```

### 7.2 Valgrind

```bash
valgrind --leak-check=full --track-origins=yes ./program

# Findet:
# - Memory leaks
# - Invalid reads/writes
# - Use of uninitialized memory
```

---

**DOKUMENT ENDE**
