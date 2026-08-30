/* Der Header, den ein FREMDES Teilsystem einbindet.
 *
 * uft_fix_zusage() ist hier deklariert, src/gui/ bindet diesen Header
 * ein — das Symbol liegt dort also auf dem Tisch — und trotzdem ruft es
 * nur ein Test.  ->  ANGEBOT_OHNE_ABNEHMER.
 *
 * Frueher stand hier auch:  int uft_fix_erwaehnt_im_header(void);
 * — auskommentiert, also NICHT deklariert. Wer den Kommentar-Strip
 * abschaltet, macht daraus ein Angebot, das es nie gab. Genau das
 * prueft der Rotbeweis. */
int uft_fix_zusage(void);
int uft_fix_live(void);
