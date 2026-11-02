if (ANDROID)
    # SDLActivity loads libmain.so and calls SDL_main (SDL_main.h renames main()).
    add_library(xr_3da SHARED)
    set_target_properties(xr_3da PROPERTIES OUTPUT_NAME main)
elseif (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    # SDL2main supplies main(), which runs SDL_main inside UIApplicationMain.
    add_executable(xr_3da MACOSX_BUNDLE)
else()
    add_executable(xr_3da)
endif()

target_sources(xr_3da PRIVATE
    entry_point.cpp
    stdafx.h

    resource.h
    resource.rc

    splash_oxr.bmp
    stalker_cop.ico
    stalker_cs.ico
    stalker_soc.ico
)

set_property(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    PROPERTY
    VS_STARTUP_PROJECT xr_3da
)

target_include_directories(xr_3da
    PRIVATE
    "${XRAY_SOURCE_DIR}/src"
)

target_link_libraries(xr_3da
    PRIVATE
    xrCore
    xrAPI
    xrEngine
    xrRender_GL
    xrGame
)

set_target_properties(xr_3da PROPERTIES
    PREFIX ""
)

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(XRAY_IOS_BUNDLE_ID "org.xraymobile.soc" CACHE STRING "iOS bundle identifier")
    set(XRAY_IOS_BUILD_NUMBER "1" CACHE STRING "iOS CFBundleVersion")
    set(XRAY_IOS_EXECUTABLE "OpenXRay")
    configure_file("${XRAY_SOURCE_DIR}/mobile/ios/Info.plist.in" "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)

    set_target_properties(xr_3da PROPERTIES
        OUTPUT_NAME "${XRAY_IOS_EXECUTABLE}"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
    )

    target_link_libraries(xr_3da
        PRIVATE
        SDL2::SDL2main
        "-framework AVFoundation"
        "-framework AudioToolbox"
        "-framework CoreAudio"
        "-framework CoreFoundation"
        "-framework CoreGraphics"
        "-framework CoreHaptics"
        "-framework CoreMotion"
        "-framework Foundation"
        "-framework GameController"
        "-framework Metal"
        "-framework OpenGLES"
        "-framework QuartzCore"
        "-framework UIKit"
    )
endif()

install(TARGETS xr_3da RUNTIME
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
