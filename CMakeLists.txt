cmake_minimum_required(VERSION 3.16)

project(Ct5WallpaperPackager VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(Ct5WallpaperPackager WIN32
    CropPreviewWidget.cpp
    CropPreviewWidget.h
    main.cpp
    MainWindow.cpp
    MainWindow.h
    WallpaperPackager.cpp
    WallpaperPackager.h
)

target_link_libraries(Ct5WallpaperPackager PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
)

set(ASTCENC_NATIVE_EXE "${CMAKE_CURRENT_SOURCE_DIR}/bin/x64/Release/astcenc-native.exe")
if(EXISTS "${ASTCENC_NATIVE_EXE}")
    add_custom_command(TARGET Ct5WallpaperPackager POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ASTCENC_NATIVE_EXE}"
            "$<TARGET_FILE_DIR:Ct5WallpaperPackager>/astcenc-native.exe"
        VERBATIM
    )
endif()

add_custom_command(TARGET Ct5WallpaperPackager POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/x64/$<CONFIG>"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:Ct5WallpaperPackager>"
        "${CMAKE_CURRENT_SOURCE_DIR}/bin/x64/$<CONFIG>/Ct5WallpaperPackager.exe"
    VERBATIM
)

if(EXISTS "${ASTCENC_NATIVE_EXE}")
    add_custom_command(TARGET Ct5WallpaperPackager POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE_DIR:Ct5WallpaperPackager>/astcenc-native.exe"
            "${CMAKE_CURRENT_SOURCE_DIR}/bin/x64/$<CONFIG>/astcenc-native.exe"
        VERBATIM
    )
endif()

if(MSVC)
    target_compile_options(Ct5WallpaperPackager PRIVATE /utf-8)
endif()
