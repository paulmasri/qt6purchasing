cmake_minimum_required(VERSION 3.21)

project(qt6purchasing LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Gui
    Qml
)

qt_standard_project_setup(REQUIRES 6.8)
set(QT_QML_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Platform-specific sources and libraries
set(PLATFORM_LIBS "")
set(PLATFORM_SOURCES "")
set(PLATFORM_HEADERS "")
set(PLATFORM_INCLUDE "")

if(ANDROID)
    list(APPEND PLATFORM_SOURCES
        android/googleplaystorebackend.cpp
        android/googleplaystoreproduct.cpp
        android/googleplaystoretransaction.cpp
    )
    list(APPEND PLATFORM_HEADERS
        android/googleplaystorebackend.h
        android/googleplaystoreproduct.h
        android/googleplaystoretransaction.h
    )

    set(PLATFORM_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/android)

    # Add .java file for visibility in Qt Creator (excluded from build process)
    add_custom_target(GooglePlayJavaSource
        SOURCES
            android/GooglePlayBilling.java
    )
elseif(APPLE)
    find_library(FOUNDATIONLIB Foundation)
    find_library(STOREKITLIB StoreKit)
    list(APPEND PLATFORM_LIBS
        ${FOUNDATIONLIB}
        ${STOREKITLIB}
    )

    list(APPEND PLATFORM_SOURCES
        apple/appleappstorebackend.mm
        apple/appleappstoreproduct.mm
        apple/appleappstoretransaction.mm
    )
    list(APPEND PLATFORM_HEADERS
        apple/appleappstorebackend.h
        apple/appleappstoreproduct.h
        apple/appleappstoretransaction.h
    )

    set(PLATFORM_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/apple)

    # Ensure ARC is enabled for Objective-C++ files
    set_source_files_properties(
        apple/appleappstorebackend.mm
        apple/appleappstoreproduct.mm
        apple/appleappstoretransaction.mm
        PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
elseif(WIN32)
    # Windows Runtime libraries for Microsoft Store APIs
    find_library(WINDOWSAPPLIB WindowsApp)
    list(APPEND PLATFORM_LIBS ${WINDOWSAPPLIB} oleaut32)

    list(APPEND PLATFORM_SOURCES
        windows/microsoftstorebackend.cpp
        windows/microsoftstoreproduct.cpp
        windows/microsoftstoretransaction.cpp
        windows/microsoftstoreworkers.cpp
    )
    list(APPEND PLATFORM_HEADERS
        windows/microsoftstorebackend.h
        windows/microsoftstoreproduct.h
        windows/microsoftstoretransaction.h
        windows/windowsstorewrappers.h
        windows/microsoftstoreworkers.h
    )

    set(PLATFORM_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/windows)
endif()

# The public-facing library
set(CORE_SOURCES
    abstractproduct.cpp
    abstractstorebackend.cpp
    abstracttransaction.cpp
)
set(CORE_HEADERS
    include/qt6purchasing/abstractproduct.h
    include/qt6purchasing/abstractstorebackend.h
    include/qt6purchasing/abstracttransaction.h
)

qt_add_library(qt6purchasinglib STATIC
        ${CORE_SOURCES}
        ${CORE_HEADERS}
        ${PLATFORM_SOURCES}
        ${PLATFORM_HEADERS}
)


qt_add_qml_module(qt6purchasinglib
    URI Qt6Purchasing
    VERSION 1.0
)

target_include_directories(qt6purchasinglib
    PRIVATE
        ${PLATFORM_INCLUDE}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(qt6purchasinglib
    PRIVATE
        Qt6::Core
        Qt6::Gui
    PUBLIC
        Qt6::Qml
        ${PLATFORM_LIBS}
)
