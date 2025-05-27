cmake_minimum_required(VERSION 3.21)

project(qt6purchasing LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Qml
)

qt_standard_project_setup()

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
    RESOURCE_PREFIX /
    VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml/Qt6Purchasing"
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
    PUBLIC
        Qt6::Qml
        ${PLATFORM_LIBS}
)
