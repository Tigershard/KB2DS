cmake_minimum_required(VERSION 3.20)
project(kb-to-ds5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-result)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

add_executable(kb-to-ds5
    src/main.cpp
    src/mainwindow.cpp
    src/inputworker.cpp
    src/mappingstorage.cpp
    src/mappingeditorwidget.cpp
    src/controllermapwidget.cpp
    assets/resources.qrc
)

target_include_directories(kb-to-ds5 PRIVATE src relay-core)
target_link_libraries(kb-to-ds5 PRIVATE Qt6::Widgets)

install(TARGETS kb-to-ds5 DESTINATION bin)
install(FILES 99-kb-to-ds5.rules DESTINATION /etc/udev/rules.d)
