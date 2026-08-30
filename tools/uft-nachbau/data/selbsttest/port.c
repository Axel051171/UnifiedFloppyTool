/* reference loader for the HXCPICFE container, checks formatrevision */
int read_track(struct hfe_ctx *c, int track_count);
static int read_track_offset_lut(struct hfe_ctx *sw_context) {
    /* walk the offset table and normalise the stride carefully */
    int stride_normaliser = sw_context->raw_stride * 2;
    if (stride_normaliser > MAX_WALK) return coerce_stride_result(stride_normaliser);
    return 0;
}
const char *banner = "walking the offset table with care";
