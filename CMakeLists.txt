cmake_minimum_required(VERSION 3.28)
project(InputTester LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

if (WIN32)
    set(inputBackendSources src/platform/win/winInputBackend.cpp)
else()
    set(inputBackendSources
        src/platform/linux/linuxInputBackend.cpp
        src/platform/linux/linuxKeymapParser.cpp
    )
endif()

add_library(inputBackend STATIC ${inputBackendSources})
target_link_libraries(inputBackend PRIVATE Qt6::Core Qt6::Gui)
target_include_directories(inputBackend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (WIN32)
    target_link_libraries(inputBackend PRIVATE user32)
endif()

add_executable(InputTester
    apps/qtKeyLog/qtKeyLog.cpp
    apps/qtKeyLog/keyboardView.cpp
    apps/qtKeyLog/layoutParser.cpp
)
qt_add_resources(inputTesterResources resources/inputtester.qrc)
target_sources(InputTester PRIVATE ${inputTesterResources})
target_link_libraries(InputTester PRIVATE Qt6::Widgets inputBackend)
set_target_properties(InputTester PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)

add_custom_command(TARGET InputTester POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/layouts
    $<TARGET_FILE_DIR:InputTester>/layouts
)

if (WIN32)
    target_sources(InputTester PRIVATE resources/windows/InputTester.rc)
    find_program(windeployqtPath windeployqt HINTS "${Qt6_DIR}/../../../bin" "${CMAKE_PREFIX_PATH}/bin")
    if (windeployqtPath)
        add_custom_target(deployInputTester
            COMMAND "${windeployqtPath}" "$<TARGET_FILE:InputTester>"
            DEPENDS InputTester
        )
        add_custom_target(deployInputTesterDebug
            COMMAND "${windeployqtPath}" --debug "$<TARGET_FILE:InputTester>"
            DEPENDS InputTester
        )
    else()
        message(WARNING "windeployqt not found")
    endif()
endif()

if (UNIX AND NOT APPLE)
    install(TARGETS InputTester RUNTIME DESTINATION bin)
    install(FILES assets/icons/InputTester.png DESTINATION share/icons/hicolor/256x256/apps)
    install(FILES resources/linux/InputTester.desktop DESTINATION share/applications)
endif()

include(CTest)
if (BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(layoutParserTests
        tests/layoutParserTests.cpp
        apps/qtKeyLog/layoutParser.cpp
    )
    target_include_directories(layoutParserTests PRIVATE apps/qtKeyLog)
    target_compile_definitions(layoutParserTests PRIVATE INPUTTESTER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    set_target_properties(layoutParserTests PROPERTIES AUTOMOC ON)
    target_link_libraries(layoutParserTests PRIVATE Qt6::Test Qt6::Core)
    add_test(NAME layoutParserTests COMMAND layoutParserTests)

    add_executable(linuxKeymapTests
        tests/linuxKeymapTests.cpp
        src/platform/linux/linuxKeymapParser.cpp
    )
    target_include_directories(linuxKeymapTests PRIVATE src/platform/linux)
    target_compile_definitions(linuxKeymapTests PRIVATE INPUTTESTER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    set_target_properties(linuxKeymapTests PROPERTIES AUTOMOC ON)
    target_link_libraries(linuxKeymapTests PRIVATE Qt6::Test Qt6::Core)
    add_test(NAME linuxKeymapTests COMMAND linuxKeymapTests)
endif()

option(INPUTTESTER_BUILD_BENCHMARKS "Build microbenchmarks" OFF)
if (INPUTTESTER_BUILD_BENCHMARKS)
    if (UNIX AND NOT APPLE)
        find_package(Threads REQUIRED)

        find_package(benchmark CONFIG QUIET)
        if (NOT benchmark_FOUND)
            find_package(benchmark QUIET)
        endif()
        if (NOT benchmark_FOUND OR NOT TARGET benchmark::benchmark)
            message(FATAL_ERROR
                "Google Benchmark not found. Install it (e.g. Ubuntu: sudo apt-get install libbenchmark-dev) "
                "or disable INPUTTESTER_BUILD_BENCHMARKS."
            )
        endif()

        add_executable(spscBench
            tests/spscBench.cpp
        )
        target_include_directories(spscBench PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
        )
        target_link_libraries(spscBench PRIVATE benchmark::benchmark Threads::Threads)
    endif()
endif()
