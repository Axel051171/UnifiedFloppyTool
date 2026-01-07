#ifndef UFT_FORMATS_AMIGA_EXT_FAST_CHAR_H
#define UFT_FORMATS_AMIGA_EXT_FAST_CHAR_H

extern void fast_print_char(void * dest, void * src);
extern void fast_print_inv_char( void * dest, void * src);
extern void fast_clear_line( void * dest,unsigned long fill,unsigned long linesperchar);
extern void fast_inverse_line( void * dest, unsigned long linesperchar );

#endif /* UFT_FORMATS_AMIGA_EXT_FAST_CHAR_H */
