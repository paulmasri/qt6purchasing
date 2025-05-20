cmake_minimum_required(VERSION 3.21)

project(qt6purchasing LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Qml
)

qt_standard_project_setup()

if(ANDROID)
    set(PLATFORM_STORE_SOURCES
        android/googleplaystorebackend.cpp
        android/googleplaystoreproduct.cpp
        android/googleplaystoretransaction.cpp
    )
    set(PLATFORM_STORE_HEADERS
        src/com/climbsmart/crimptronic/GooglePlayBilling.java
        android/googleplaystorebackend.h
        android/googleplaystoreproduct.h
        android/googleplaystoretransaction.h
    )
elseif(APPLE)
    include_directories(/System/Library/Frameworks)
    find_library(FOUNDATIONLIB Foundation)
    find_library(STOREKITLIB StoreKit)
    set(PLATFORM_STORE_LIBS
        ${FOUNDATIONLIB}
        ${STOREKITLIB}
    )

    set(PLATFORM_STORE_SOURCES
        apple/appleappstorebackend.mm
        apple/appleappstoreproduct.mm
        apple/appleappstoretransaction.mm
    )
    set(PLATFORM_STORE_HEADERS
        apple/appleappstorebackend.h
        apple/appleappstoreproduct.h
        apple/appleappstoretransaction.h
    )
endif()

qt_add_library(qt6purchasing)
target_include_directories(qt6purchasing PRIVATE ${CMAKE_SOURCE_DIR})
target_sources(qt6purchasing
    PRIVATE
        abstractstorebackend.cpp
        abstractproduct.cpp
        abstracttransaction.cpp
        ${PLATFORM_STORE_SOURCES}
    PUBLIC
        abstractstorebackend.h
        abstractproduct.h
        abstracttransaction.h
        ${PLATFORM_STORE_HEADERS}
)
target_link_libraries(qt6purchasing
    PRIVATE
        Qt6::Core
    PUBLIC
        Qt6::Qml
        ${PLATFORM_STORE_LIBS}
)
