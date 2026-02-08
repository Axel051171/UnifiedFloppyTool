/**
 * @file fdc_ctrl.h
 * @brief FDC (Floppy Disk Controller) definitions for UFT compatibility
 */

#ifndef FDC_CTRL_H
#define FDC_CTRL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FDC Types
 * ============================================================================ */

typedef enum {
    FDC_TYPE_UNKNOWN = 0,
    FDC_TYPE_WD1770,
    FDC_TYPE_WD1772,
    FDC_TYPE_WD1793,
    FDC_TYPE_WD2793,
    FDC_TYPE_8272,          /* IBM PC */
    FDC_TYPE_765,           /* NEC µPD765 */
    FDC_TYPE_82077,
    FDC_TYPE_AMIGA,
    FDC_TYPE_C64,
    FDC_TYPE_APPLE
} fdc_type_t;

/* ============================================================================
 * FDC Status Bits
 * ============================================================================ */

/* Type I commands status */
#define FDC_STAT_BUSY           0x01
#define FDC_STAT_INDEX          0x02
#define FDC_STAT_TRACK0         0x04
#define FDC_STAT_CRC_ERROR      0x08
#define FDC_STAT_SEEK_ERROR     0x10
#define FDC_STAT_HEAD_LOADED    0x20
#define FDC_STAT_WRITE_PROTECT  0x40
#define FDC_STAT_NOT_READY      0x80

/* Type II/III commands status */
#define FDC_STAT_DRQ            0x02
#define FDC_STAT_LOST_DATA      0x04
#define FDC_STAT_RNF            0x10    /* Record Not Found */
#define FDC_STAT_RECORD_TYPE    0x20
#define FDC_STAT_FAULT          0x20

/* ============================================================================
 * FDC Commands
 * ============================================================================ */

/* Type I - Stepping */
#define FDC_CMD_RESTORE         0x00
#define FDC_CMD_SEEK            0x10
#define FDC_CMD_STEP            0x20
#define FDC_CMD_STEP_IN         0x40
#define FDC_CMD_STEP_OUT        0x60

/* Type II - Read/Write */
#define FDC_CMD_READ_SECTOR     0x80
#define FDC_CMD_WRITE_SECTOR    0xA0

/* Type III - Read/Write Track */
#define FDC_CMD_READ_ADDRESS    0xC0
#define FDC_CMD_READ_TRACK      0xE0
#define FDC_CMD_WRITE_TRACK     0xF0

/* Type IV - Force Interrupt */
#define FDC_CMD_FORCE_INT       0xD0

/* ============================================================================
 * FDC Timing (microseconds)
 * ============================================================================ */

#define FDC_STEP_RATE_6MS       0x00
#define FDC_STEP_RATE_12MS      0x01
#define FDC_STEP_RATE_2MS       0x02
#define FDC_STEP_RATE_3MS       0x03

#define FDC_INDEX_PULSE_US      2000    /* ~2ms index pulse */
#define FDC_MOTOR_SPINUP_MS     500     /* Motor spin-up time */

/* ============================================================================
 * FDC State Machine
 * ============================================================================ */

typedef struct {
    fdc_type_t type;
    
    /* Registers */
    uint8_t status;
    uint8_t track;
    uint8_t sector;
    uint8_t data;
    uint8_t command;
    
    /* Internal state */
    uint8_t direction;      /* 0=out, 1=in */
    uint8_t side;
    uint8_t drive;
    uint8_t motor_on;
    uint8_t head_loaded;
    
    /* Timing */
    uint32_t step_rate;
    uint32_t head_settle;
    uint32_t index_count;
    
    /* DMA/IRQ */
    uint8_t drq;
    uint8_t irq;
    
} fdc_state_t;

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

void fdc_init(fdc_state_t* fdc, fdc_type_t type);
void fdc_reset(fdc_state_t* fdc);
void fdc_write_command(fdc_state_t* fdc, uint8_t cmd);
uint8_t fdc_read_status(fdc_state_t* fdc);
void fdc_write_data(fdc_state_t* fdc, uint8_t data);
uint8_t fdc_read_data(fdc_state_t* fdc);

#ifdef __cplusplus
}
#endif

#endif /* FDC_CTRL_H */
