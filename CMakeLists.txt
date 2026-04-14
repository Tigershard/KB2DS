cmake_minimum_required(VERSION 3.20)
project(KB2DS LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-result)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

add_executable(KB2DS
    src/main.cpp
    src/apptheme.cpp
    src/mainwindow.cpp
    src/inputworker.cpp
    src/mappingstorage.cpp
    src/mappingeditorwidget.cpp
    src/controllermapwidget.cpp
    src/igdbclient.cpp
    src/saveprofiledialog.cpp
    assets/resources.qrc
)

target_include_directories(KB2DS PRIVATE src relay-core)
target_link_libraries(KB2DS PRIVATE Qt6::Widgets Qt6::Network)

install(TARGETS KB2DS DESTINATION bin)
install(FILES 99-KB2DS.rules DESTINATION /etc/udev/rules.d)
