/* Eigenbau: liest HXCPICFE-Kopf, prueft formatrevision gegen Spec M-0007 */
static int uft_hfe_lut_lesen(uft_hfe_kontext *kx) {
    /* Spurtabelle nach Spec-Zeile 4: Offsets sind 512er-Bloecke */
    int schritt = kx->rohschritt << 1;
    if (schritt > UFT_HFE_MAXSCHRITT) return UFT_ERR_RANGE;
    return UFT_OK;
}
