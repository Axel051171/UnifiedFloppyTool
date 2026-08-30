/* Der Anker. `main` ist kuerzer als die Namensregel und taucht darum nie
 * als Export auf — diese Datei kann also nie "verwaist" werden und
 * traegt die lebenden Aufrufe, wie die Oberflaeche im echten Baum.
 *
 * Erwaehnung ist kein Aufruf: uft_fix_kommentar() steht hier nur in
 * diesem Kommentar und gleich darunter in einem String-Literal. */
extern int uft_fix_live(void);
extern int uft_fix_b(void);

int main(void)
{
    const char *s = "uft_fix_kommentar";
    (void)s;
    return uft_fix_live() + uft_fix_b();
}
