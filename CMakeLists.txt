# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.20)
project(sharks LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(${PROJECT_SOURCE_DIR}/QHotkey)
add_subdirectory("QHotkey")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

if (NOT "${QT_DEFAULT_MAJOR_VERSION}" STREQUAL 5)
	find_package(Qt6 COMPONENTS Core Widgets OpenGLWidgets Network)
endif()
if (Qt6_FOUND)
	qt_standard_project_setup()
	set(QT Qt6)
else()
	find_package(Qt5 REQUIRED COMPONENTS Core Widgets X11Extras Network)
	set(QT Qt5)
endif()
message(STATUS "Using ${QT} (${QT_MAJOR_VERSION})")

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
find_package(XCB COMPONENTS XFIXES SHM REQUIRED)
find_package(Wayland COMPONENTS Client REQUIRED)
find_package(WaylandScanner REQUIRED)

add_executable(sharks
	main.cxx
	selectionwindow_linux.cxx
	selectionwindow.cxx
	selectionwindow.hxx
	traymenu.cxx
	traymenu.hxx
	killexisting_linux.cxx
	killexisting.hxx
	wayland.cxx
	wayland.hxx
	x11atoms.cxx
	x11atoms.hxx
	resources/resources.qrc
)

ecm_add_wayland_client_protocol(sharks PROTOCOL wayland-proto/xdg-output-unstable-v1.xml BASENAME xdg-output-unstable-v1)
ecm_add_wayland_client_protocol(sharks PROTOCOL wayland-proto/wlr-screencopy-unstable-v1.xml BASENAME wlr-screencopy-unstable-v1)

target_link_libraries(sharks PRIVATE XCB::XFIXES XCB::SHM)
target_link_libraries(sharks PRIVATE Wayland::Client)
target_link_libraries(sharks PRIVATE qhotkey)
target_link_libraries(sharks PRIVATE ${QT}::Widgets ${QT}::Network)
if ("${QT}" STREQUAL "Qt5")
	target_link_libraries(sharks PRIVATE ${QT}::X11Extras)
endif()
if ("${QT}" STREQUAL "Qt6")
	target_link_libraries(sharks PRIVATE ${QT}::OpenGLWidgets)
endif()

add_compile_definitions(QT_DISABLE_DEPRECATED_UP_TO=0x050F00)
target_compile_options(sharks PRIVATE -Wall -Wextra -pedantic -Wno-multichar)
