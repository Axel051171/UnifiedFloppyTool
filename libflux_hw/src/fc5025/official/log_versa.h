/* Ref: M68000 Family VERSAdos System Facilities Reference Manual
        Seventh Edition, October 1985 (M68KVSF/D7) */

/* Volume ID Block (sector 0) (p. 239) */
struct vid {
	uint8_t vol[4];		/* Volume ASCII identifier */
	uint16_t usn;		/* User number */
	uint32_t sat;		/* Start of Sector Allocation Table (bitmap) */
	uint16_t sal;		/* Length of Sector Allocation Table */
	uint32_t sds;		/* Secondary directory start */
	uint32_t pdl;		/* Reserved (primary directory PSN list start)*/
	uint32_t oss;		/* Start of IPL file */
	uint16_t osl;		/* IPL length */
	uint32_t ose;		/* IPL execution address */
	uint32_t osa;		/* IPL load address */
	uint32_t dte;		/* Generation date */
	uint8_t vd[20];		/* Volume descriptor */
	uint8_t vno[4];		/* Initial version/revision */
	uint16_t chk;		/* VID checksum */
	uint8_t dtp[64];	/* Diagnostic test pattern */
	uint32_t dta;		/* Diagnostic test area directory */
	uint32_t das;		/* Start of dump area */
	uint16_t dal;		/* Length of dump area */
	uint32_t slt;		/* Start of Sector Lockout Table (defect list)*/
	uint16_t sll;		/* Length of Sector Lockout Table */
	uint32_t cas;		/* Configuration area starting sector */
	uint8_t cal;		/* Configuration area length */
	uint8_t ipc;		/* IPC format disk type code */
	uint8_t alt;		/* Alternates used indicator */
	uint8_t rs1[97];	/* Reserved */
	uint8_t mac[8];		/* Signature "EXORMACS" */
} __attribute__ ((__packed__));

/* Secondary Directory Block header (p. 240) */
struct sdbhdr {
	uint32_t fpt;		/* PSN of next SDB (zero if none) */
	uint8_t padding[12];
} __attribute__ ((__packed__));

/* Secondary Directory Entry (p. 240) */
struct sde {
	uint16_t usn;		/* User number */
	uint8_t clg[8];		/* Catalog name */
	uint32_t pdp;		/* phys sect of 1st primary dir blk; 0=none*/
	uint8_t ac1;		/* Reserved */
	uint8_t rs1;		/* Reserved */
} __attribute__ ((__packed__));

/* Primary Directory Block header (p. 241) */
struct pdbhdr {
	uint32_t fpt;		/* PSN of next PDB (zero if none) */
	uint16_t usn;		/* User number */
	uint8_t clg[8];		/* Catalog name */
	uint8_t padding[2];
} __attribute__ ((__packed__));

/* Primary Directory Entry (p. 241) */
struct pde {
	uint8_t fil[8];		/* Filename */
	uint8_t ext[2];		/* Extension */
	uint16_t rs1;		/* Reserved */
	uint32_t fs;		/* File starting PSN (contiguous) or first FAB
				   pointer (noncontiguous) */
	uint32_t fe;		/* Physical EOF LSN (contiguous) or last FAB
				   pointer (noncontiguous) */
	uint32_t eof;		/* End of file LSN (zero if contiguous) */
	uint32_t eor;		/* End of file logical record number (LRN)
				   (zero if contiguous) */
	uint8_t wcd;		/* Write access code */
	uint8_t rcd;		/* Read access code */
	uint8_t att;		/* File attributes */
	uint8_t lbz;		/* Last data block size (zero if contiguous) */
	uint16_t lrl;		/* Record size (zero if variable record length
				   or contiguous) */
	uint8_t rs2;		/* Reserved */
	uint8_t key;		/* Key size (zero if null keys or non-ISAM) */
	uint8_t fab;		/* FAB size (zero if contiguous) */
	uint8_t dat;		/* Data block size (zero if contiguous) */
	uint16_t dtec;		/* Date file created or updated */
	uint16_t dtea;		/* Last date file assigned (opened) */
	uint8_t rs3[8];		/* Reserved */
} __attribute__ ((__packed__));

/* File attributes (DIRATT) low 4 bits (p. 242) */
#define DATCON 0	/* Contiguous */
#define DATSEQ 1	/* Sequential (variable or fixed rec. length) */
#define DATISK 2	/* Keyed ISAM, no duplicate keys */
#define DATISD 3	/* Keyed ISAM, duplicate/null keys allowed */

/* File Allocation/Access Block header (p. 242) */
struct fabhdr {
	uint32_t flk;		/* PSN of file's next FAB */
	uint32_t blk;		/* PSN of file's previous FAB */
	uint8_t use;		/* Fraction of FAB in use, in 16ths */
	uint8_t pky;		/* Length of prev FAB's last key; =DIRKEY */
} __attribute__ ((__packed__));

/* FAB segment descriptor */
struct fabseg {
	uint32_t psn;		/* PSN of data block start; 0=end of this FAB */
	uint16_t rec;		/* Data block record count */
	uint8_t sgs;		/* Sectors in use in this data block */
	uint8_t key;		/* =DIRKEY */
} __attribute__ ((__packed__));
