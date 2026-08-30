/* Der Gegenfall: dieser Header wird NUR innerhalb von src/kern/
 * eingebunden. uft_fix_intern() ist damit ein Hausmittel des eigenen
 * Teilsystems, kein Angebot nach aussen — es darf NICHT als
 * ANGEBOT_OHNE_ABNEHMER erscheinen, obwohl es genauso verwaist ist. */
int uft_fix_intern(void);
