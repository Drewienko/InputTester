cmake_minimum_required(VERSION 3.28)
project(InputTester LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

if (WIN32)
    set(inputBackendSources src/platform/win/winInputBackend.cpp)
else()
    set(inputBackendSources src/platform/linux/linuxInputBackend.cpp)
endif()

add_library(inputBackend STATIC ${inputBackendSources})
target_link_libraries(inputBackend PRIVATE Qt6::Core Qt6::Gui)
target_include_directories(inputBackend PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if (WIN32)
    target_link_libraries(inputBackend PRIVATE user32)
endif()

add_executable(qtKeyLog
    apps/qtKeyLog/qtKeyLog.cpp
    apps/qtKeyLog/keyboardView.cpp
)
target_link_libraries(qtKeyLog PRIVATE Qt6::Widgets inputBackend)

if (WIN32)
    find_program(windeployqtPath windeployqt HINTS "${Qt6_DIR}/../../../bin" "${CMAKE_PREFIX_PATH}/bin")
    if (windeployqtPath)
        add_custom_target(deployQtKeyLog
            COMMAND "${windeployqtPath}" "$<TARGET_FILE:qtKeyLog>"
            DEPENDS qtKeyLog
        )
        add_custom_target(deployQtKeyLogDebug
            COMMAND "${windeployqtPath}" --debug "$<TARGET_FILE:qtKeyLog>"
            DEPENDS qtKeyLog
        )
    else()
        message(WARNING "windeployqt not found")
    endif()
endif()
