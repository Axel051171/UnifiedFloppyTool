# MFM Disk Format Detection Module
# =================================
# 3-stage detection: Physical → Boot Sector → Filesystem Heuristic
# Supports: FAT12, Amiga OFS/FFS, CP/M 2.2/3.0, Atari ST, MSX, and more

cmake_minimum_required(VERSION 3.14)

set(MFM_DETECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(MFM_DETECT_INC ${CMAKE_SOURCE_DIR}/include/uft/detect)

# --- Static libraries ---

add_library(mfm_detect STATIC
    ${MFM_DETECT_DIR}/mfm_detect.c
)
target_include_directories(mfm_detect PUBLIC ${MFM_DETECT_INC})
target_link_libraries(mfm_detect m)

add_library(cpm_fs STATIC
    ${MFM_DETECT_DIR}/cpm_fs.c
)
target_include_directories(cpm_fs PUBLIC ${MFM_DETECT_INC})
target_link_libraries(cpm_fs m)

add_library(uft_mfm_detect_bridge STATIC
    ${MFM_DETECT_DIR}/uft_mfm_detect_bridge.c
)
target_include_directories(uft_mfm_detect_bridge PUBLIC ${MFM_DETECT_INC})
target_link_libraries(uft_mfm_detect_bridge mfm_detect cpm_fs)

# --- Tests ---

add_executable(test_mfm_detect ${MFM_DETECT_DIR}/test_mfm_detect.c)
target_link_libraries(test_mfm_detect mfm_detect)
add_test(NAME MFM_Detect COMMAND test_mfm_detect test)

add_executable(test_cpm_fs ${MFM_DETECT_DIR}/test_cpm_fs.c)
target_link_libraries(test_cpm_fs cpm_fs)
add_test(NAME CPM_FS COMMAND test_cpm_fs test)

add_executable(test_mfm_bridge ${MFM_DETECT_DIR}/test_mfm_bridge.c)
target_link_libraries(test_mfm_bridge uft_mfm_detect_bridge)
add_test(NAME MFM_Bridge COMMAND test_mfm_bridge)
