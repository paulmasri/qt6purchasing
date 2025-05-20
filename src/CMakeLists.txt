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

if(ANDROID)
    list(APPEND PLATFORM_SOURCES
        android/googleplaystorebackend.cpp
        android/googleplaystoreproduct.cpp
        android/googleplaystoretransaction.cpp
    )
    list(APPEND PLATFORM_HEADERS
        src/com/climbsmart/crimptronic/GooglePlayBilling.java
        android/googleplaystorebackend.h
        android/googleplaystoreproduct.h
        android/googleplaystoretransaction.h
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
endif()

# The public-facing library
qt_add_library(qt6purchasing)
target_include_directories(qt6purchasing PRIVATE ${CMAKE_SOURCE_DIR})
target_sources(qt6purchasing
    PRIVATE
        abstractstorebackend.cpp
        abstractproduct.cpp
        abstracttransaction.cpp
        ${PLATFORM_SOURCES}
    PUBLIC
        abstractstorebackend.h
        abstractproduct.h
        abstracttransaction.h
        ${PLATFORM_HEADERS}
)
target_link_libraries(qt6purchasing
    PRIVATE
        Qt6::Core
    PUBLIC
        Qt6::Qml
        ${PLATFORM_LIBS}
)
