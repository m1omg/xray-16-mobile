cmake_minimum_required(VERSION 3.23)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(PreProjectInit)

calculate_xray_build_id(XRAY_BUILD_ID)
project(OpenXRay
    DESCRIPTION "OpenXRay is an improved version of the X-Ray Engine, \
                the game engine used in the world-famous S.T.A.L.K.E.R. game series by GSC Game World."
    VERSION 1.6.02.${XRAY_BUILD_ID}
    HOMEPAGE_URL "https://github.com/OpenXRay/xray-16"
    LANGUAGES CXX C
)

# Keep a stable engine root when OpenXRay is embedded with add_subdirectory().
# CMAKE_SOURCE_DIR belongs to the outer application in that configuration.
set(XRAY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

option(BUILD_SHARED_LIBS "Build shared libraries"     ${BUILD_SHARED_LIBS_DEFAULT_VALUE})

option(XRAY_USE_LUAJIT   "Use LuaJIT"                 ON)
option(XRAY_USE_ASAN     "Use AddressSanitizer"       OFF)
option(XRAY_ENABLE_TRACY "Enable tracy profiler"      OFF)
option(XRAY_USE_LTO      "Use link-time optimization in release builds" ON)

include(XRay.Build)
include(XRay.Packaging)

add_subdirectory(Externals)
add_subdirectory(src)
add_subdirectory(res)
add_subdirectory(misc)

# Android only installs and System.loadLibrary()s native libraries named lib*.so,
# but the engine (and some submodules) set PREFIX "" on their shared libraries.
# Restore the prefix centrally so submodule CMake files stay untouched;
# ModuleLookup adds the same prefix when it dlopen()s modules at runtime.
if (ANDROID)
    function(xray_collect_targets dir out)
        get_property(targets DIRECTORY "${dir}" PROPERTY BUILDSYSTEM_TARGETS)
        get_property(subdirs DIRECTORY "${dir}" PROPERTY SUBDIRECTORIES)
        foreach (subdir IN LISTS subdirs)
            xray_collect_targets("${subdir}" sub_targets)
            list(APPEND targets ${sub_targets})
        endforeach()
        set(${out} ${targets} PARENT_SCOPE)
    endfunction()

    xray_collect_targets("${CMAKE_CURRENT_SOURCE_DIR}" xray_all_targets)
    foreach (target IN LISTS xray_all_targets)
        get_target_property(type ${target} TYPE)
        if (type STREQUAL "SHARED_LIBRARY" OR type STREQUAL "MODULE_LIBRARY")
            set_target_properties(${target} PROPERTIES PREFIX "lib")
        elseif (type STREQUAL "STATIC_LIBRARY")
            # Static builds still end up in libmain.so, so everything must be PIC
            # (several targets tie POSITION_INDEPENDENT_CODE to BUILD_SHARED_LIBS)
            set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
        endif()
    endforeach()
endif()
