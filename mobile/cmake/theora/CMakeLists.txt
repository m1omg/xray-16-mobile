# Minimal CMake build for libtheora (decoder + legacy theora_* API, encoder stubbed out).
# Upstream only ships autotools; this is enough for X-Ray's xrTheora_Stream FMV playback.
cmake_minimum_required(VERSION 3.16)
project(theora C)

if (NOT THEORA_SOURCE_DIR)
    message(FATAL_ERROR "Set THEORA_SOURCE_DIR to the libtheora source tree")
endif()

find_package(Ogg REQUIRED CONFIG)

set(lib ${THEORA_SOURCE_DIR}/lib)
set(decoder_sources
    ${lib}/apiwrapper.c
    ${lib}/bitpack.c
    ${lib}/decapiwrapper.c
    ${lib}/decinfo.c
    ${lib}/decode.c
    ${lib}/dequant.c
    ${lib}/fragment.c
    ${lib}/huffdec.c
    ${lib}/idct.c
    ${lib}/info.c
    ${lib}/internal.c
    ${lib}/quant.c
    ${lib}/state.c
)

foreach(target theoradec theora)
    if (target STREQUAL "theora")
        add_library(${target} STATIC ${decoder_sources} ${lib}/encoder_disabled.c)
    else()
        add_library(${target} STATIC ${decoder_sources})
    endif()
    target_include_directories(${target}
        PUBLIC $<BUILD_INTERFACE:${THEORA_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>
        PRIVATE ${lib}
    )
    target_compile_definitions(${target} PRIVATE OC_DISABLE_ASM)
    target_link_libraries(${target} PUBLIC Ogg::ogg)
endforeach()

install(TARGETS theoradec theora ARCHIVE DESTINATION lib)
install(DIRECTORY ${THEORA_SOURCE_DIR}/include/theora DESTINATION include)
