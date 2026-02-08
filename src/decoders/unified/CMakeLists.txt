# UFT Unified Decoders - TEMPORARILY DISABLED
# These files have struct member conflicts with uft_sector_t
# TODO P0-2: Unify sector type definitions

# Disabled sources:
# - uft_amiga_mfm_decoder_v2.c (uft_sector_t.id, data_size, status members)

# Create stub library
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/uft_decoders_unified_stub.c "/* stub - pending P0-2 */\n")
add_library(uft_decoders_unified STATIC ${CMAKE_CURRENT_BINARY_DIR}/uft_decoders_unified_stub.c)
message(STATUS "uft_decoders_unified: Disabled pending sector type unification (P0-2)")
