/**
 * @file nfd_loader.c
 * @brief NFD Image Loader/Writer for NEC PC-98
 * 
 * NFD is a flexible disk image format for PC-98 computers.
 * Supports variable sector sizes and copy protection data.
 * 
 * NFD r0: Simple format with fixed geometry
 * NFD r1: Extended format with per-track sector info
 * 
 * @version 5.30.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* NFD Constants */
#define NFD_R0_SIGNATURE    "T98FDDIMAGE.R0"
#define NFD_R1_SIGNATURE    "T98FDDIMAGE.R1"
#define NFD_SIGNATURE_LEN   14

/* PC-98 Standard geometries */
#define PC98_2HD_TRACKS     77
#define PC98_2HD_HEADS      2
#define PC98_2HD_SECTORS    8
#define PC98_2HD_SECTOR_SIZE 1024

#define PC98_2DD_TRACKS     80
#define PC98_2DD_HEADS      2
#define PC98_2DD_SECTORS    8
#define PC98_2DD_SECTOR_SIZE 512

#pragma pack(push, 1)
/* NFD r0 Header */
typedef struct {
    char     signature[15];     /* "T98FDDIMAGE.R0\0" */
    char     reserved1[1];
    char     title[100];        /* Disk title */
    uint8_t  reserved2[8];
    uint8_t  write_protect;     /* 0=RW, other=RO */
    uint8_t  heads;             /* Number of heads */
    uint8_t  reserved3[10];
    uint32_t track_offset[164]; /* Offsets to track data (82 tracks × 2 heads) */
    uint32_t additional[164];   /* Additional info offsets */
} nfd_r0_header_t;

/* NFD r1 Header */
typedef struct {
    char     signature[15];     /* "T98FDDIMAGE.R1\0" */
    char     reserved1[1];
    char     title[100];        /* Disk title */
    uint8_t  reserved2[8];
    uint8_t  write_protect;
    uint8_t  heads;
    uint8_t  reserved3[10];
    /* Track headers follow */
} nfd_r1_header_t;

/* NFD r1 Track header */
typedef struct {
    uint8_t  sectors;           /* Number of sectors */
    uint8_t  sectors_diag;      /* Diagnostic sectors */
    uint8_t  reserved[2];
} nfd_r1_track_header_t;

/* NFD r1 Sector header */
typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size;              /* Size code (0=128, 1=256, 2=512, 3=1024) */
    uint8_t  mfm;               /* 0=FM, 1=MFM */
    uint8_t  deleted;           /* Deleted mark flag */
    uint8_t  status;            /* Status (FDC result) */
    uint8_t  st0, st1, st2;     /* FDC status bytes */
    uint8_t  pda_low, pda_high; /* Physical disk address */
    uint8_t  reserved[2];
} nfd_r1_sector_header_t;
#pragma pack(pop)

typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;
    int      size;
    bool     mfm;
    bool     deleted;
    uint8_t *data;
} nfd_sector_t;

typedef struct {
    int          sector_count;
    nfd_sector_t *sectors;
} nfd_track_t;

typedef struct {
    char         title[101];
    bool         write_protect;
    int          heads;
    int          tracks;
    nfd_track_t  track_data[164];  /* 82 tracks × 2 heads */
    bool         is_r1;
} nfd_image_t;

/* ============================================================================
 * NFD Helper Functions
 * ============================================================================ */

/**
 * @brief Get sector size from size code
 */
static int nfd_sector_size(uint8_t code)
{
    switch (code) {
        case 0: return 128;
        case 1: return 256;
        case 2: return 512;
        case 3: return 1024;
        case 4: return 2048;
        case 5: return 4096;
        default: return 512;
    }
}

/**
 * @brief Get size code from sector size
 */
static uint8_t nfd_size_code(int size)
{
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        case 2048: return 4;
        case 4096: return 5;
        default:   return 2;
    }
}

/* ============================================================================
 * NFD Loader
 * ============================================================================ */

/**
 * @brief Create empty NFD image
 */
int nfd_create(nfd_image_t *img, int tracks, int heads, bool r1_format)
{
    if (!img) return -1;
    
    memset(img, 0, sizeof(*img));
    
    img->tracks = tracks;
    img->heads = heads;
    img->is_r1 = r1_format;
    
    return 0;
}

/**
 * @brief Load NFD file
 */
int nfd_load(const char *filename, nfd_image_t *img)
{
    if (!filename || !img) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    memset(img, 0, sizeof(*img));
    
    /* Read signature */
    char sig[16];
    if (fread(sig, 1, 15, fp) != 15) { /* I/O error */ }
    sig[15] = '\0';
    
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    if (strncmp(sig, NFD_R0_SIGNATURE, 14) == 0) {
        /* NFD r0 format */
        img->is_r1 = false;
        
        nfd_r0_header_t header;
        if (fread(&header, sizeof(header), 1, fp) != 1) { /* I/O error */ }
        memcpy(img->title, header.title, 100);
        img->title[100] = '\0';
        img->write_protect = header.write_protect != 0;
        img->heads = header.heads;
        img->tracks = 82;  /* Standard for r0 */
        
        /* Read tracks */
        for (int t = 0; t < 82; t++) {
            for (int h = 0; h < img->heads; h++) {
                int idx = t * 2 + h;
                
                if (header.track_offset[idx] == 0) continue;
                
                if (fseek(fp, header.track_offset[idx], SEEK_SET) != 0) { /* seek error */ }
                /* r0 assumes fixed 8 sectors of 1024 bytes for 2HD */
                int sectors = 8;
                int sector_size = 1024;
                
                img->track_data[idx].sector_count = sectors;
                img->track_data[idx].sectors = calloc(sectors, sizeof(nfd_sector_t));
                
                for (int s = 0; s < sectors; s++) {
                    img->track_data[idx].sectors[s].cylinder = t;
                    img->track_data[idx].sectors[s].head = h;
                    img->track_data[idx].sectors[s].sector = s + 1;
                    img->track_data[idx].sectors[s].size = sector_size;
                    img->track_data[idx].sectors[s].mfm = true;
                    img->track_data[idx].sectors[s].data = malloc(sector_size);
                    
                    if (fread(img->track_data[idx].sectors[s].data, 1, sector_size, fp) != sector_size) { /* I/O error */ }
                }
            }
        }
        
    } else if (strncmp(sig, NFD_R1_SIGNATURE, 14) == 0) {
        /* NFD r1 format */
        img->is_r1 = true;
        
        nfd_r1_header_t header;
        if (fread(&header, sizeof(header), 1, fp) != 1) { /* I/O error */ }
        memcpy(img->title, header.title, 100);
        img->title[100] = '\0';
        img->write_protect = header.write_protect != 0;
        img->heads = header.heads;
        img->tracks = 82;
        
        /* Read track headers and data */
        for (int t = 0; t < 164; t++) {
            nfd_r1_track_header_t track_hdr;
            if (fread(&track_hdr, sizeof(track_hdr), 1, fp) != 1) break;
            
            if (track_hdr.sectors == 0) continue;
            
            img->track_data[t].sector_count = track_hdr.sectors;
            img->track_data[t].sectors = calloc(track_hdr.sectors, sizeof(nfd_sector_t));
            
            /* Read sector headers */
            for (int s = 0; s < track_hdr.sectors; s++) {
                nfd_r1_sector_header_t sect_hdr;
                if (fread(&sect_hdr, sizeof(sect_hdr), 1, fp) != 1) { /* I/O error */ }
                img->track_data[t].sectors[s].cylinder = sect_hdr.cylinder;
                img->track_data[t].sectors[s].head = sect_hdr.head;
                img->track_data[t].sectors[s].sector = sect_hdr.sector;
                img->track_data[t].sectors[s].size = nfd_sector_size(sect_hdr.size);
                img->track_data[t].sectors[s].mfm = sect_hdr.mfm != 0;
                img->track_data[t].sectors[s].deleted = sect_hdr.deleted != 0;
            }
            
            /* Read sector data */
            for (int s = 0; s < track_hdr.sectors; s++) {
                int size = img->track_data[t].sectors[s].size;
                img->track_data[t].sectors[s].data = malloc(size);
                if (fread(img->track_data[t].sectors[s].data, 1, size, fp) != size) { /* I/O error */ }
            }
        }
        
    } else {
        fclose(fp);
        return -1;
    }
    
    fclose(fp);
    return 0;
}

/**
 * @brief Add sector to track
 */
int nfd_add_sector(nfd_image_t *img, int track, int head, int sector,
                   int size, const uint8_t *data)
{
    if (!img || !data) return -1;
    
    int idx = track * 2 + head;
    if (idx >= 164) return -1;
    
    int s = img->track_data[idx].sector_count;
    img->track_data[idx].sectors = realloc(img->track_data[idx].sectors,
                                           (s + 1) * sizeof(nfd_sector_t));
    
    img->track_data[idx].sectors[s].cylinder = track;
    img->track_data[idx].sectors[s].head = head;
    img->track_data[idx].sectors[s].sector = sector;
    img->track_data[idx].sectors[s].size = size;
    img->track_data[idx].sectors[s].mfm = true;
    img->track_data[idx].sectors[s].data = malloc(size);
    if(!img->track_data[idx].sectors[s].data) return -1; /* P0-SEC-001 */
    memcpy(img->track_data[idx].sectors[s].data, data, size);
    
    img->track_data[idx].sector_count++;
    
    return 0;
}

/**
 * @brief Read sector from NFD
 */
int nfd_read_sector(const nfd_image_t *img, int track, int head, int sector,
                    uint8_t *data)
{
    if (!img || !data) return -1;
    
    int idx = track * 2 + head;
    if (idx >= 164) return -1;
    
    for (int s = 0; s < img->track_data[idx].sector_count; s++) {
        if (img->track_data[idx].sectors[s].sector == sector) {
            int size = img->track_data[idx].sectors[s].size;
            memcpy(data, img->track_data[idx].sectors[s].data, size);
            return size;
        }
    }
    
    return -1;
}

/**
 * @brief Save NFD image
 */
int nfd_save(const nfd_image_t *img, const char *filename)
{
    if (!img || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    if (img->is_r1) {
        /* Write r1 format */
        nfd_r1_header_t header = {0};
        memcpy(header.signature, NFD_R1_SIGNATURE, 15);
        memcpy(header.title, img->title, 100);
        header.write_protect = img->write_protect ? 1 : 0;
        header.heads = img->heads;
        
        if (fwrite(&header, sizeof(header), 1, fp) != 1) { /* I/O error */ }
        /* Write tracks */
        for (int t = 0; t < 164; t++) {
            nfd_r1_track_header_t track_hdr = {0};
            track_hdr.sectors = img->track_data[t].sector_count;
            
            if (fwrite(&track_hdr, sizeof(track_hdr), 1, fp) != 1) { /* I/O error */ }
            if (track_hdr.sectors == 0) continue;
            
            /* Write sector headers */
            for (int s = 0; s < track_hdr.sectors; s++) {
                nfd_r1_sector_header_t sect_hdr = {0};
                sect_hdr.cylinder = img->track_data[t].sectors[s].cylinder;
                sect_hdr.head = img->track_data[t].sectors[s].head;
                sect_hdr.sector = img->track_data[t].sectors[s].sector;
                sect_hdr.size = nfd_size_code(img->track_data[t].sectors[s].size);
                sect_hdr.mfm = img->track_data[t].sectors[s].mfm ? 1 : 0;
                sect_hdr.deleted = img->track_data[t].sectors[s].deleted ? 1 : 0;
                
                if (fwrite(&sect_hdr, sizeof(sect_hdr), 1, fp) != 1) { /* I/O error */ }
            }
            
            /* Write sector data */
            for (int s = 0; s < track_hdr.sectors; s++) {
                if (fwrite(img->track_data[t].sectors[s].data, 1,
                       img->track_data[t].sectors[s].size, fp) != img->track_data[t].sectors[s].size) { /* I/O error */ }
            }
        }
        
    } else {
        /* Write r0 format */
        nfd_r0_header_t header = {0};
        memcpy(header.signature, NFD_R0_SIGNATURE, 15);
        memcpy(header.title, img->title, 100);
        header.write_protect = img->write_protect ? 1 : 0;
        header.heads = img->heads;
        
        /* Calculate offsets */
        uint32_t offset = sizeof(header);
        for (int t = 0; t < 82; t++) {
            for (int h = 0; h < img->heads; h++) {
                int idx = t * 2 + h;
                if (img->track_data[idx].sector_count > 0) {
                    header.track_offset[idx] = offset;
                    for (int s = 0; s < img->track_data[idx].sector_count; s++) {
                        offset += img->track_data[idx].sectors[s].size;
                    }
                }
            }
        }
        
        if (fwrite(&header, sizeof(header), 1, fp) != 1) { /* I/O error */ }
        /* Write track data */
        for (int t = 0; t < 164; t++) {
            for (int s = 0; s < img->track_data[t].sector_count; s++) {
                if (fwrite(img->track_data[t].sectors[s].data, 1,
                       img->track_data[t].sectors[s].size, fp) != img->track_data[t].sectors[s].size) { /* I/O error */ }
            }
        }
    }
    
    fclose(fp);
    return 0;
}

/**
 * @brief Free NFD image
 */
void nfd_free(nfd_image_t *img)
{
    if (img) {
        for (int t = 0; t < 164; t++) {
            for (int s = 0; s < img->track_data[t].sector_count; s++) {
                free(img->track_data[t].sectors[s].data);
            }
            free(img->track_data[t].sectors);
        }
        memset(img, 0, sizeof(*img));
    }
}
