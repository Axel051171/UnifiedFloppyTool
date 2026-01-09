# UFT Cloud Backup Module
# Provides cloud storage integration (optional libcurl)

set(CLOUD_SOURCES
    uft_cloud.c
)

add_library(uft_cloud OBJECT ${CLOUD_SOURCES})

target_include_directories(uft_cloud PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/uft
    ${CMAKE_SOURCE_DIR}/include/uft/compat
)

# Optional: libcurl support
find_package(CURL QUIET)
if(CURL_FOUND)
    target_compile_definitions(uft_cloud PRIVATE UFT_HAVE_CURL)
    target_include_directories(uft_cloud PRIVATE ${CURL_INCLUDE_DIRS})
    message(STATUS "Cloud module: libcurl found, full HTTP support enabled")
else()
    message(STATUS "Cloud module: libcurl not found, local testing mode only")
endif()
