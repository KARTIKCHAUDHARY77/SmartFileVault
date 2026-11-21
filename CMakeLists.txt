cmake_minimum_required(VERSION 3.21)

project(SmartFileVaultDesktop VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.12 REQUIRED COMPONENTS Quick QuickControls2 Concurrent QuickDialogs2)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

qt_standard_project_setup()

set(BACKEND_SOURCES
    src/application_service.cpp
    src/file_manager.cpp
    src/inactivity.cpp
    src/scan_report.cpp
    src/vault_service.cpp
    src/archive_service.cpp
    src/archive.cpp
    src/compression.cpp
    src/compression_policy.cpp
    src/file_utils.cpp
    src/restore.cpp
    src/restore_safety.cpp
    src/vault_report.cpp
)

qt_add_executable(sfv_desktop
    MACOSX_BUNDLE
    desktop/main.cpp
    desktop/dashboard_controller.cpp
    desktop/dashboard_controller.h
    ${BACKEND_SOURCES}
)

qt_add_qml_module(sfv_desktop
    URI SmartFileVault
    VERSION 1.0
    QML_FILES
        desktop/Main.qml
)

target_include_directories(sfv_desktop PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(sfv_desktop PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Concurrent
    Qt6::QuickDialogs2
    OpenSSL::Crypto
    ZLIB::ZLIB
)
