# UnifiedFloppyTool Format Plugins CMake

set(UFT_FORMAT_SOURCES
    # Core formats (existierend)
    d64/uft_d64.c
    g64/uft_g64.c
    adf/uft_adf.c
    img/uft_img.c
    
    # HDF (Amiga Hard Disk) - P1 Feature
    uft_hdf_parser.c
    scp/uft_scp.c
    hfe/uft_hfe.c
    
    # New API-compliant formats
    d71/uft_d71.c
    d80/uft_d80.c
    d81/uft_d81.c
    d82/uft_d82.c
    g71/uft_g71.c
    atr/uft_atr.c
    msa/uft_msa.c
    ipf/uft_ipf.c
    td0/uft_td0.c
    imd/uft_imd.c
    dsk_cpc/uft_dsk_cpc.c
    nib/uft_nib.c
    woz/uft_woz.c
    trd/uft_trd.c
    ssd/uft_ssd.c
    d88/uft_d88.c
    sad/uft_sad.c
    dmk/uft_dmk.c
    fdi/uft_fdi.c
    cqm/uft_cqm.c
    
    # ZX Spectrum / Sinclair (integrated from uft_drive_trx_scl_pack_v2)
    scl/uft_scl.c
    
    # TI-99/4A Formats
    ti99/uft_tifiles.c
    ti99/uft_fiad.c
    
    # XDF (IBM Extended Density Format)
    uft_xdf.c
    
    # APD (Acorn Protected Disk)
    uft_apd_format.c
    
    # Registry
    format_registry/uft_format_registry.c
    
    # V3 Bridge (Handler für D64, G64, SCP)
    uft_v3_bridge.c
)

add_library(uft_formats STATIC ${UFT_FORMAT_SOURCES})
target_include_directories(uft_formats PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(uft_formats uft_core)
