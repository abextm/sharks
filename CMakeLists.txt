# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.20)
project(sharks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${PROJECT_SOURCE_DIR}/QHotkey)
add_subdirectory("QHotkey")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if (NOT "${QT_DEFAULT_MAJOR_VERSION}" STREQUAL 5)
	find_package(Qt6 COMPONENTS Core Widgets OpenGLWidgets)
endif()
if (Qt6_FOUND)
	qt_standard_project_setup()
	set(QT Qt6)
else()
	find_package(Qt5 REQUIRED COMPONENTS Core Widgets X11Extras)
	set(QT Qt5)
endif()
message(STATUS "Using ${QT} (${QT_MAJOR_VERSION})")

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
find_package(XCB COMPONENTS XFIXES SHM REQUIRED)

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

target_link_libraries(sharks PRIVATE XCB::XFIXES XCB::SHM)
target_link_libraries(sharks PRIVATE qhotkey)
target_link_libraries(sharks PRIVATE ${QT}::Widgets)
if ("${QT}" STREQUAL "Qt5")
	target_link_libraries(sharks PRIVATE ${QT}::X11Extras)
endif()
if ("${QT}" STREQUAL "Qt6")
	target_link_libraries(sharks PRIVATE ${QT}::OpenGLWidgets)
endif()

target_compile_options(sharks PRIVATE -Wall -Wextra -pedantic -Werror -Wno-multichar)