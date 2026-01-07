# UFT fs Module
set(FS_SOURCES
    uft_amigados.c
    uft_amigados_bitmap.c
    uft_amigados_core.c
    uft_amigados_dir.c
    uft_amigados_file.c
    uft_apple_core.c
    uft_apple_dos33.c
    uft_apple_dos_core.c
    uft_apple_file.c
    uft_apple_format.c
    uft_apple_prodos.c
    uft_atari_dos_core.c
    uft_atari_dos_dir.c
    uft_atari_dos_file.c
    uft_bbc_dfs.c
    uft_bbc_dfs_core.c
    uft_cbm_fs.c
    uft_cbm_fs_bam.c
    uft_cbm_fs_file.c
    uft_cpm_fs_core.c
    uft_cpmfs.c
    uft_cpmfs_enhanced.c
    uft_cpmfs_impl.c
    uft_fat12_core.c
    uft_fat12_dir.c
    uft_fat12_file.c
    uft_fat12_format.c
    uft_fat12_lfn.c
    uft_fat12_validate.c
    uft_fat_entry.c
    uft_fat_table.c
    uft_human68k.c
    uft_scandisk.c
    uft_spartados.c
    uft_ti99_fs.c
    uft_ti99_fs_file.c
    uft_trsdos_core.c
    uft_trsdos_dir.c
    uft_trsdos_file.c
)

add_library(uft_fs STATIC ${FS_SOURCES})

target_include_directories(uft_fs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

set_target_properties(uft_fs PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
)
