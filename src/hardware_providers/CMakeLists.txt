cmake_minimum_required(VERSION 3.16)

# C++ Hardware Provider Layer (Qt-friendly wrappers)
set(CMAKE_AUTOMOC ON)

file(GLOB HWPROVIDER_SOURCES *.cpp)
file(GLOB HWPROVIDER_HEADERS *.h)

# MF-249 (v4.1.5-hardening): the QSerialPort-backed transports
# (qserial_applesauce_transport.cpp, qserial_adfcopy_transport.cpp when
# it lands) hard-depend on Qt6::SerialPort which is an OPTIONAL Qt module
# on many installs (notably the default Qt 6.10.x without an explicit
# QtSerialPort component). Exclude them from the build when the module
# is absent — the providers themselves stay as honest-stub via nullptr
# runners, which is the pre-MF-249 behaviour and forensically safe.
find_package(Qt6 COMPONENTS SerialPort QUIET)
if(NOT TARGET Qt6::SerialPort)
    list(FILTER HWPROVIDER_SOURCES EXCLUDE REGEX "qserial_[a-z]+_transport\\.cpp$")
    list(FILTER HWPROVIDER_HEADERS EXCLUDE REGEX "qserial_[a-z]+_transport\\.h$")
    message(STATUS "UFT hardware_providers: Qt6::SerialPort NOT FOUND — "
                   "QSerialPort transports disabled (providers stay honest-stub)")
else()
    message(STATUS "UFT hardware_providers: Qt6::SerialPort FOUND — "
                   "QSerialPort transports ENABLED")
endif()

add_library(hardware_providers STATIC
    ${HWPROVIDER_SOURCES}
    ${HWPROVIDER_HEADERS}
)

target_include_directories(hardware_providers PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Qt dependency
find_package(Qt6 COMPONENTS Core QUIET)
if(Qt6_FOUND)
    target_link_libraries(hardware_providers PUBLIC Qt6::Core)
else()
    find_package(Qt5 COMPONENTS Core REQUIRED)
    target_link_libraries(hardware_providers PUBLIC Qt5::Core)
endif()

# Link to C hardware backends (if available)
if(TARGET flux_hw)
    target_link_libraries(hardware_providers PUBLIC flux_hw)
endif()

# Link SerialPort if available
if(TARGET Qt6::SerialPort)
    target_link_libraries(hardware_providers PUBLIC Qt6::SerialPort)
elseif(TARGET Qt5::SerialPort)
    target_link_libraries(hardware_providers PUBLIC Qt5::SerialPort)
endif()

# Compiler warnings
target_compile_options(hardware_providers PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall> $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wextra>>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)
