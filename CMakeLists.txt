# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.14)
project(sharks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${PROJECT_SOURCE_DIR}/QHotkey)
add_subdirectory("QHotkey")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets X11Extras REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets X11Extras REQUIRED)

find_package(XCB COMPONENTS XFIXES REQUIRED)

add_executable(sharks
	main.cxx
	selectionwindow_linux.cxx
	selectionwindow.cxx
	selectionwindow.hxx
	traymenu.cxx
	traymenu.hxx
	killexisting_linux.cxx
	killexisting.hxx
	x11atoms.cxx
	x11atoms.hxx
	resources/resources.qrc
)

target_link_libraries(sharks PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::X11Extras XCB::XFIXES qhotkey)

target_compile_options(sharks PRIVATE -Wall -Wextra -pedantic -Werror -Wno-multichar)