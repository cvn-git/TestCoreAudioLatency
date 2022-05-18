cmake_minimum_required(VERSION 3.5)

project(TestCoreAudioLatency VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(S src)
set(PROJECT_SOURCES
        ${S}/coreaudioqt.h
        ${S}/coreaudioqt.cpp
        ${S}/main.cpp
        ${S}/mainwindow.cpp
        ${S}/mainwindow.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(TestCoreAudioLatency
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET TestCoreAudioLatency APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(TestCoreAudioLatency SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(TestCoreAudioLatency
            ${PROJECT_SOURCES}
        )
    endif()
endif()

set(FRAMEWORK_ROOT "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks")

target_include_directories(TestCoreAudioLatency
    PRIVATE ${FRAMEWORK_ROOT}/CoreAudio.framework/Headers
)

target_link_libraries(TestCoreAudioLatency
    PRIVATE Qt${QT_VERSION_MAJOR}::Widgets
    PRIVATE "-framework CoreAudio"
)

set_target_properties(TestCoreAudioLatency PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(TestCoreAudioLatency)
endif()
