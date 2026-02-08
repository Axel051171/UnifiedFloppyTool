cmake_minimum_required(VERSION 3.10)
project(uft_events C)

set(CMAKE_C_STANDARD 99)

include_directories(${CMAKE_SOURCE_DIR}/../../../include/uft/analysis)

# ── Libraries ──────────────────────────────────────────────
add_library(otdr_event_core_v2 STATIC otdr_event_core_v2.c)
target_link_libraries(otdr_event_core_v2 m)

add_library(uft_event_bridge STATIC uft_event_bridge.c)
target_link_libraries(uft_event_bridge otdr_event_core_v2 m)

add_library(otdr_align_fuse_v7 STATIC otdr_align_fuse_v7.c)
target_link_libraries(otdr_align_fuse_v7 m)

add_library(uft_align_fuse_bridge STATIC uft_align_fuse_bridge.c)
target_link_libraries(uft_align_fuse_bridge otdr_align_fuse_v7 m)

# ── Tests ──────────────────────────────────────────────────
enable_testing()

add_executable(test_event_bridge test_event_bridge.c)
target_link_libraries(test_event_bridge uft_event_bridge otdr_event_core_v2 m)
add_test(NAME event_bridge COMMAND test_event_bridge)

add_executable(test_align_fuse_bridge test_align_fuse_bridge.c)
target_link_libraries(test_align_fuse_bridge uft_align_fuse_bridge otdr_align_fuse_v7 m)
add_test(NAME align_fuse_bridge COMMAND test_align_fuse_bridge)

add_library(otdr_event_core_v8 STATIC otdr_event_core_v8.c)
target_link_libraries(otdr_event_core_v8 m)

add_library(uft_event_v8_bridge STATIC uft_event_v8_bridge.c)
target_link_libraries(uft_event_v8_bridge otdr_event_core_v8 m)

add_executable(test_event_v8 test_event_v8.c)
target_link_libraries(test_event_v8 uft_event_v8_bridge otdr_event_core_v8 m)
add_test(NAME event_v8 COMMAND test_event_v8)

add_library(otdr_event_core_v9 STATIC otdr_event_core_v9.c)
target_link_libraries(otdr_event_core_v9 m)

add_library(uft_integrity_bridge STATIC uft_integrity_bridge.c)
target_link_libraries(uft_integrity_bridge otdr_event_core_v9 m)

add_executable(test_integrity_v9 test_integrity_v9.c)
target_link_libraries(test_integrity_v9 uft_integrity_bridge otdr_event_core_v9 m)
add_test(NAME integrity_v9 COMMAND test_integrity_v9)

add_library(otdr_event_core_v10 STATIC otdr_event_core_v10.c)
target_link_libraries(otdr_event_core_v10 m)

add_library(uft_confidence_bridge STATIC uft_confidence_bridge.c)
target_link_libraries(uft_confidence_bridge otdr_event_core_v10 m)

add_executable(test_confidence_v10 test_confidence_v10.c)
target_link_libraries(test_confidence_v10 uft_confidence_bridge otdr_event_core_v10 m)
add_test(NAME confidence_v10 COMMAND test_confidence_v10)

add_library(otdr_event_core_v11 STATIC otdr_event_core_v11.c)
target_link_libraries(otdr_event_core_v11 otdr_event_core_v9 otdr_event_core_v8 otdr_event_core_v10 m)

add_library(uft_pipeline_bridge STATIC uft_pipeline_bridge.c)
target_link_libraries(uft_pipeline_bridge otdr_event_core_v11 m)

add_executable(test_pipeline_v11 test_pipeline_v11.c)
target_link_libraries(test_pipeline_v11 uft_pipeline_bridge otdr_event_core_v11 otdr_event_core_v9 otdr_event_core_v8 otdr_event_core_v10 m)
add_test(NAME pipeline_v11 COMMAND test_pipeline_v11)

add_library(otdr_event_core_v12 STATIC otdr_event_core_v12.c)
target_link_libraries(otdr_event_core_v12 otdr_event_core_v9 otdr_event_core_v10 m)

add_library(uft_export_bridge STATIC uft_export_bridge.c)
target_link_libraries(uft_export_bridge otdr_event_core_v12 m)

add_executable(test_export_v12 test_export_v12.c)
target_link_libraries(test_export_v12 uft_export_bridge otdr_event_core_v12 otdr_event_core_v9 otdr_event_core_v10 m)
add_test(NAME export_v12 COMMAND test_export_v12)
