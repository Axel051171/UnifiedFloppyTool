/* MF-591: Waechter war `UFT_CRC_H` — derselbe wie in dem
 * gleichnamigen oeffentlichen Header unter include/uft/. Wer beide
 * erreichte, sah nur den ERSTEN; welchen, entschied die
 * Include-Reihenfolge. Der private traegt jetzt einen eigenen Namen.
 */
#ifndef UFT_SRC_CRC_H
#define UFT_SRC_CRC_H

/*
 * CRC16 "Register". This is implemented as two 8bit values
 */

void CRC16_Update(unsigned char *CRC16_High, unsigned char *CRC16_Low, unsigned char val,unsigned char * crctable);
void CRC16_Init  (unsigned char *CRC16_High, unsigned char *CRC16_Low, unsigned char * crctable,unsigned short polynome,unsigned short initvalue);

#endif /* UFT_SRC_CRC_H */
