 * Generated for UnifiedFloppyTool
 */

#ifndef CONFIG_H
#define CONFIG_H

/* Define the basic polynomial type */
#define BMP_T unsigned long
#define BMP_C(n) (n##UL)

/* Use preset CRC models */
#define PRESETS 1

/* Enable bitmap macros for better performance */
#define BMPMACRO 1

/* Maximum polynomial width in bits (32 for unsigned long on most systems) */
#define BMP_BIT 32

/* Highest power of two strictly less than BMP_BIT */
#define BMP_SUB 16

#endif /* CONFIG_H */
