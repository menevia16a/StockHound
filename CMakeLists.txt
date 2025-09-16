cmake_minimum_required(VERSION 3.5)

project(StockHound VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Sql)

set(TS_FILES StockHound_en_US.ts)

set(PROJECT_SOURCES
    Main.cpp
        MainWindow.cpp
        MainWindow.h
        MainWindow.ui
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(StockHound
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES} 
        Analysis/StockAnalysis.h Analysis/StockAnalysis.cpp
    )

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    add_executable(StockHound
        ${PROJECT_SOURCES}
    )

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(StockHound PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Sql)

if(WIN32)
    set_target_properties(StockHound PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

include(GNUInstallDirs)

install(TARGETS StockHound
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(StockHound)

    # On Windows, copy required Qt DLLs after build
    if(WIN32)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/bin")

        if(WINDEPLOYQT_EXECUTABLE)
            add_custom_command(TARGET StockHound POST_BUILD
                COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release $<TARGET_FILE:StockHound>
                COMMENT "Running windeployqt to bundle Qt runtime libraries"
            )
        endif()
    endif()
endif()

# StockHound required libraries
add_subdirectory(ThirdParty/alpaca-trade-api-cpp)

find_package(SQLite3 REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# Enforce UTF-8 for MSVC, and treat warnings as errors
if(MSVC)
    target_compile_options(StockHound PRIVATE /utf-8 /W4 /WX /permissive-)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(StockHound PRIVATE -Wall -Wextra -Werror)
endif()

target_link_libraries(StockHound PRIVATE alpaca)
target_link_libraries(StockHound PRIVATE SQLite::SQLite3)
target_link_libraries(StockHound PRIVATE CURL::libcurl)
target_link_libraries(StockHound PRIVATE nlohmann_json::nlohmann_json)

# Windows DLL deployment
if(WIN32)
    # Helper function to copy DLLs next to exe
    function(copy_runtime_dll target dll_location)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${dll_location}"
                $<TARGET_FILE_DIR:${target}>
        )
    endfunction()

    # Try to find the DLL paths for SQLite and CURL
    get_target_property(sqlite3_location SQLite::SQLite3 IMPORTED_LOCATION_RELEASE)
    if(sqlite3_location)
        copy_runtime_dll(StockHound "${sqlite3_location}")
    endif()

    get_target_property(curl_location CURL::libcurl IMPORTED_LOCATION_RELEASE)
    if(curl_location)
        copy_runtime_dll(StockHound "${curl_location}")
    endif()
endif()
