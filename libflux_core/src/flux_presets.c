#include "uft/uft_error.h"
#include "flux_presets.h"

#include <string.h>
#include "ufm_media.h"


static ufm_media_profile_t media_unknown(void)
{
    ufm_media_profile_t m = {0};
    m.encoding = UFM_ENC_UNKNOWN;
    return m;
}

static ufm_media_profile_t media_profile(const char *name, const char *title, ufm_encoding_t enc,
                                         uint16_t rpm, uint32_t kbps,
                                         uint16_t cyls, uint16_t heads,
                                         uint16_t spt, uint16_t ssize,
                                         bool has_index, bool variable_spt)
{
    ufm_media_profile_t m = {0};
    m.name = name;
    m.title = title;
    m.encoding = enc;
    m.rpm = rpm;
    m.bitrate_kbps = kbps;
    m.cylinders = cyls;
    m.heads = heads;
    m.sectors_per_track = spt;
    m.sector_size = ssize;
    m.has_index = has_index;
    m.variable_spt = variable_spt;
    return m;
}

static flux_policy_t make_preset_safe_archive(void)
{
    flux_policy_t p;
    flux_policy_init_default(&p);
    p.read.speed = FLUX_SPEED_MINIMUM;
    p.read.errors = FLUX_ERR_IGNORE;
    p.read.scan = FLUX_SCAN_ADVANCED;
    p.read.ignore_read_errors = true;
    p.read.fast_error_skip = false;
    p.read.retry.max_revs = 9;
    p.read.retry.max_retries = 6;
    p.read.dpm = FLUX_DPM_HIGH;
    p.write.verify_after_write = true;
    return p;
}

static flux_policy_t make_preset_fast_dump(void)
{
    flux_policy_t p;
    flux_policy_init_default(&p);
    p.read.speed = FLUX_SPEED_MAXIMUM;
    p.read.errors = FLUX_ERR_TOLERANT;
    p.read.scan = FLUX_SCAN_STANDARD;
    p.read.fast_error_skip = true;
    p.read.retry.max_revs = 2;
    p.read.retry.max_retries = 1;
    p.read.dpm = FLUX_DPM_OFF;
    p.write.verify_after_write = false;
    return p;
}

static flux_policy_t make_preset_weakbit_hunter(void)
{
    flux_policy_t p;
    flux_policy_init_default(&p);
    p.read.speed = FLUX_SPEED_NORMAL;
    p.read.errors = FLUX_ERR_IGNORE;
    p.read.scan = FLUX_SCAN_ADVANCED;
    p.read.retry.max_revs = 15;
    p.read.retry.max_resyncs = 255;
    p.read.retry.max_retries = 8;
    p.read.advanced_scan_factor = 200;
    p.read.dpm = FLUX_DPM_HIGH;
    return p;
}

static flux_policy_t make_preset_writer_verify(void)
{
    flux_policy_t p;
    flux_policy_init_default(&p);
    p.read.scan = FLUX_SCAN_STANDARD;
    p.write.errors = FLUX_ERR_STRICT;
    p.write.max_retries = 5;
    p.write.verify_after_write = true;
    p.write.underrun_protect = true;
    return p;
}

static const flux_preset_t g_presets[] = {
    { "safe_archive", "Safe Archive", "Maximum preservation fidelity (slow, many revs, high DPM).", {0}, {0} },
    { "fast_dump", "Fast Dump", "Quick read with minimal retries (useful for healthy disks).", {0}, {0} },
    { "weakbit_hunter", "Weak-Bit Hunter", "Aggressive multi-rev consensus to expose instability.", {0}, {0} },
    { "writer_verify", "Writer (Verify)", "Writer preset with verify-after-write enabled.", {0}, {0} },
{ "cpc_3in_dd_180k", "CPC 3" DD (180K)", "Amstrad CPC 3-inch SS/DD (9x512 @ 250kbps, 300rpm).", {0}, {0} },
{ "cpc_3in_dd_360k", "CPC 3" DD (360K)", "Amstrad CPC 3-inch DS/DD (9x512 @ 250kbps, 300rpm).", {0}, {0} },
{ "pc_35_dd_720k", "PC 3.5" DD (720K)", "IBM PC 3.5-inch DS/DD (9x512 @ 250kbps, 300rpm).", {0}, {0} },
{ "amiga_35_dd_880k", "Amiga 3.5" DD (880K)", "Amiga DS/DD (11x512 @ 250kbps, 300rpm), gapless MFM.", {0}, {0} },
{ "pc_35_hd_1440k", "PC 3.5" HD (1.44M)", "IBM PC 3.5-inch DS/HD (18x512 @ 500kbps, 300rpm).", {0}, {0} },
{ "pc_525_dd_360k", "PC 5.25" DD (360K)", "IBM PC 5.25-inch DS/DD (9x512 @ 250kbps, 300rpm).", {0}, {0} },
{ "pc_525_hd_1200k", "PC 5.25" HD (1.2M)", "IBM PC/AT 5.25-inch DS/HD (15x512 @ 500kbps, 360rpm).", {0}, {0} },
{ "ibm_8_ss_sd", "IBM 8" SS/SD", "Legacy 8-inch single-sided, single-density (FM).", {0}, {0} },
{ "quickdisk_fds", "QuickDisk FDS", "Mitsumi Quick Disk (spiral) as used by Famicom Disk System.", {0}, {0} },
};

static void init_once(void)
{
    static int inited = 0;
    if (inited) return;
    ((flux_preset_t*)&g_presets[0])->policy = make_preset_safe_archive();
    ((flux_preset_t*)&g_presets[1])->policy = make_preset_fast_dump();
    ((flux_preset_t*)&g_presets[2])->policy = make_preset_weakbit_hunter();
    ((flux_preset_t*)&g_presets[3])->policy = make_preset_writer_verify();
/* Media presets: attach a media profile + a sane policy default. */
((flux_preset_t*)&g_presets[4])->media = media_profile("cpc_3in_dd_180k","CPC 3\" DD (180K)", UFM_ENC_MFM_IBM, 300, 250, 40, 1, 9, 512, true, false);
((flux_preset_t*)&g_presets[4])->policy = make_preset_safe_archive();

((flux_preset_t*)&g_presets[5])->media = media_profile("cpc_3in_dd_360k","CPC 3\" DD (360K)", UFM_ENC_MFM_IBM, 300, 250, 40, 2, 9, 512, true, false);
((flux_preset_t*)&g_presets[5])->policy = make_preset_safe_archive();

((flux_preset_t*)&g_presets[6])->media = media_profile("pc_35_dd_720k","PC 3.5\" DD (720K)", UFM_ENC_MFM_IBM, 300, 250, 80, 2, 9, 512, true, false);
((flux_preset_t*)&g_presets[6])->policy = make_preset_fast_dump();

((flux_preset_t*)&g_presets[7])->media = media_profile("amiga_35_dd_880k","Amiga 3.5\" DD (880K)", UFM_ENC_MFM_AMIGA, 300, 250, 80, 2, 11, 512, true, false);
((flux_preset_t*)&g_presets[7])->policy = make_preset_safe_archive();

((flux_preset_t*)&g_presets[8])->media = media_profile("pc_35_hd_1440k","PC 3.5\" HD (1.44M)", UFM_ENC_MFM_IBM, 300, 500, 80, 2, 18, 512, true, false);
((flux_preset_t*)&g_presets[8])->policy = make_preset_fast_dump();

((flux_preset_t*)&g_presets[9])->media = media_profile("pc_525_dd_360k","PC 5.25\" DD (360K)", UFM_ENC_MFM_IBM, 300, 250, 40, 2, 9, 512, true, false);
((flux_preset_t*)&g_presets[9])->policy = make_preset_fast_dump();

((flux_preset_t*)&g_presets[10])->media = media_profile("pc_525_hd_1200k","PC 5.25\" HD (1.2M)", UFM_ENC_MFM_IBM, 360, 500, 80, 2, 15, 512, true, false);
((flux_preset_t*)&g_presets[10])->policy = make_preset_safe_archive();

((flux_preset_t*)&g_presets[11])->media = media_profile("ibm_8_ss_sd","IBM 8\" SS/SD", UFM_ENC_FM_IBM, 360, 250, 77, 1, 26, 128, true, false);
((flux_preset_t*)&g_presets[11])->policy = make_preset_safe_archive();

((flux_preset_t*)&g_presets[12])->media = media_profile("quickdisk_fds","QuickDisk FDS", UFM_ENC_SPIRAL_QUICKDISK, 360, 125, 0, 0, 0, 0, true, false);
((flux_preset_t*)&g_presets[12])->policy = make_preset_safe_archive();

    inited = 1;
}

const flux_preset_t* flux_presets_all(size_t *count)
{
    init_once();
    if (count) *count = sizeof(g_presets)/sizeof(g_presets[0]);
    return g_presets;
}

const flux_preset_t* flux_preset_find(const char *name)
{
    init_once();
    if (!name) return NULL;
    for (size_t i=0;i<sizeof(g_presets)/sizeof(g_presets[0]);++i) {
        if (strcmp(g_presets[i].name, name)==0) return &g_presets[i];
    }
    return NULL;
}

int flux_preset_get_policy(const char *name, flux_policy_t *out_policy)
{
    const flux_preset_t *p = flux_preset_find(name);
    if (!p || !out_policy) return -1;
    *out_policy = p->policy;
    return 0;
}
