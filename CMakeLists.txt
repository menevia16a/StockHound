cmake_minimum_required(VERSION 3.5)

project(StockHound VERSION 1.0 LANGUAGES CXX)

# Enable Qt automations
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt5 packages
find_package(Qt5 REQUIRED COMPONENTS Widgets Sql)

# Project sources
set(PROJECT_SOURCES
    Main.cpp
    MainWindow.cpp
    MainWindow.h
    MainWindow.ui
    Analysis/StockAnalysis.cpp
    Analysis/StockAnalysis.h
)

# Create executable
add_executable(StockHound ${PROJECT_SOURCES})

# Link Qt5
target_link_libraries(StockHound PRIVATE Qt5::Widgets Qt5::Sql)

# Windows executable property
if(WIN32)
    set_target_properties(StockHound PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Installation
include(GNUInstallDirs)
install(TARGETS StockHound
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Third-party libraries
add_subdirectory(ThirdParty/alpaca-trade-api-cpp)

find_package(SQLite3 REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

target_link_libraries(StockHound PRIVATE
    alpaca
    SQLite::SQLite3
    CURL::libcurl
    nlohmann_json::nlohmann_json
)

# Compiler flags
if(MSVC)
    target_compile_options(StockHound PRIVATE /utf-8 /W4 /WX /permissive-)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(StockHound PRIVATE -Wall -Wextra -Werror)
endif()

# Windows DLL and plugin deployment
if(WIN32)

    # Helper function to copy a file next to the exe
    function(copy_runtime_file target src_file dest_subdir)
        get_filename_component(filename "${src_file}" NAME)
        set(dest_dir "$<TARGET_FILE_DIR:${target}>/${dest_subdir}")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${dest_dir}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${src_file}" "${dest_dir}/${filename}"
            COMMENT "Copying ${filename} to ${dest_subdir}"
        )
    endfunction()

    # Copy SQLite DLL
    get_target_property(sqlite3_location SQLite::SQLite3 IMPORTED_LOCATION_RELEASE)

    if(sqlite3_location)
        copy_runtime_file(StockHound "${sqlite3_location}" "")
    endif()

    # Copy CURL DLL
    get_target_property(curl_location CURL::libcurl IMPORTED_LOCATION_RELEASE)

    if(curl_location)
        copy_runtime_file(StockHound "${curl_location}" "")
    endif()

    # Copy Qt SQLite plugin (qsqlite.dll)
    # Adjust this path if your Qt installation is elsewhere
    set(QT_SQLITE_PLUGIN "$ENV{VCPKG_ROOT}/installed/x64-windows/plugins/sqldrivers/qsqlite.dll")

    if(EXISTS "${QT_SQLITE_PLUGIN}")
        copy_runtime_file(StockHound "${QT_SQLITE_PLUGIN}" "plugins/sqldrivers")
    else()
        message(WARNING "qsqlite.dll not found, Qt SQL driver may fail!")
    endif()

    # Deploy remaining Qt runtime libraries with windeployqt
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "$ENV{VCPKG_ROOT}/installed/x64-windows/tools/qt5/bin")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET StockHound POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release $<TARGET_FILE:StockHound>
            COMMENT "Running windeployqt to bundle Qt5 runtime libraries"
        )
    endif()
endif()
