#include "uft/uft_error.h"
#include "flux_format/flux_format.h"
#include "flux_format/registry.h"

/* auto-generated registry */
#include "flux_format/dsk.h"
#include "flux_format/td0.h"
#include "flux_format/sad.h"
#include "flux_format/scl.h"
#include "flux_format/fdi.h"
#include "flux_format/dti.h"
#include "flux_format/ipf.h"
#include "flux_format/msa.h"
#include "flux_format/cqm.h"
#include "flux_format/cwtool.h"
#include "flux_format/udi.h"
#include "flux_format/imd.h"
#include "flux_format/sbt.h"
#include "flux_format/dfi.h"
#include "flux_format/scp.h"
#include "flux_format/stream.h"
#include "flux_format/hfe.h"
#include "flux_format/mfi.h"
#include "flux_format/qdos.h"
#include "flux_format/sap.h"
#include "flux_format/woz.h"
#include "flux_format/pdi.h"
#include "flux_format/a2r.h"
#include "flux_format/d80.h"
#include "flux_format/st.h"
#include "flux_format/bpb.h"
#include "flux_format/adf.h"
#include "flux_format/dmk.h"
#include "flux_format/mbd.h"
#include "flux_format/opd.h"
#include "flux_format/d88.h"
#include "flux_format/1dd.h"
#include "flux_format/2d.h"
#include "flux_format/trd.h"
#include "flux_format/lif.h"
#include "flux_format/cfi.h"
#include "flux_format/dsc.h"
#include "flux_format/sdf.h"
#include "flux_format/s24.h"
#include "flux_format/d2m.h"
#include "flux_format/d4m.h"
#include "flux_format/d81.h"
#include "flux_format/mgt.h"
#include "flux_format/ds2.h"
#include "flux_format/cpm.h"
#include "flux_format/fd.h"
#include "flux_format/do.h"
#include "flux_format/raw.h"

static const fluxfmt_plugin_t* const g_plugins[] = {
    &fluxfmt_dsk_plugin,
    &fluxfmt_td0_plugin,
    &fluxfmt_sad_plugin,
    &fluxfmt_scl_plugin,
    &fluxfmt_fdi_plugin,
    &fluxfmt_dti_plugin,
    &fluxfmt_ipf_plugin,
    &fluxfmt_msa_plugin,
    &fluxfmt_cqm_plugin,
    &fluxfmt_cwtool_plugin,
    &fluxfmt_udi_plugin,
    &fluxfmt_imd_plugin,
    &fluxfmt_sbt_plugin,
    &fluxfmt_dfi_plugin,
    &fluxfmt_scp_plugin,
    &fluxfmt_stream_plugin,
    &fluxfmt_hfe_plugin,
    &fluxfmt_mfi_plugin,
    &fluxfmt_qdos_plugin,
    &fluxfmt_sap_plugin,
    &fluxfmt_woz_plugin,
    &fluxfmt_pdi_plugin,
    &fluxfmt_a2r_plugin,
    &fluxfmt_d80_plugin,
    &fluxfmt_st_plugin,
    &fluxfmt_bpb_plugin,
    &fluxfmt_adf_plugin,
    &fluxfmt_dmk_plugin,
    &fluxfmt_mbd_plugin,
    &fluxfmt_opd_plugin,
    &fluxfmt_d88_plugin,
    &fluxfmt_1dd_plugin,
    &fluxfmt_2d_plugin,
    &fluxfmt_trd_plugin,
    &fluxfmt_lif_plugin,
    &fluxfmt_cfi_plugin,
    &fluxfmt_dsc_plugin,
    &fluxfmt_sdf_plugin,
    &fluxfmt_s24_plugin,
    &fluxfmt_d2m_plugin,
    &fluxfmt_d4m_plugin,
    &fluxfmt_d81_plugin,
    &fluxfmt_mgt_plugin,
    &fluxfmt_ds2_plugin,
    &fluxfmt_cpm_plugin,
    &fluxfmt_fd_plugin,
    &fluxfmt_do_plugin,
    &fluxfmt_raw_plugin,
    NULL
};

const fluxfmt_plugin_t* fluxfmt_registry(size_t *count)
{
    size_t n = 0;
    while (g_plugins[n]) n++;
    if (count) *count = n;
    return n ? g_plugins[0] : NULL;
}
