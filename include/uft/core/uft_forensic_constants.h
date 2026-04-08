#ifndef UFT_FORENSIC_CONSTANTS_H
#define UFT_FORENSIC_CONSTANTS_H

/* Forensic fill byte for unreadable sectors.
 * 0x01 chosen because:
 *   - Not 0x00 (common in blank/formatted sectors)
 *   - Not 0xE5 (CP/M format byte, also used by MS-DOS)
 *   - Not 0xF6 (MS-DOS format byte)
 *   - Unlikely to appear as real data in most systems
 *   - Easy to identify in hex editors
 */
#define UFT_FORENSIC_FILL_BYTE  0x01

/* Sector source flags for forensic provenance */
#define UFT_SECTOR_FLAG_ORIGINAL    0x00
#define UFT_SECTOR_FLAG_FILLED      0x01
#define UFT_SECTOR_FLAG_RECOVERED   0x02
#define UFT_SECTOR_FLAG_FUSED       0x03

#endif
