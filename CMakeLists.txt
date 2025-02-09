# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

cmake_minimum_required(VERSION 3.20)
project(sharks LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# for QHotkey
set(QT_DEFAULT_MAJOR_VERSION 6)

include_directories(${PROJECT_SOURCE_DIR}/QHotkey)
add_subdirectory("QHotkey")

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGLWidgets Network)
qt_standard_project_setup()
set(QT Qt6)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

if (UNIX)
	find_package(XCB COMPONENTS XFIXES SHM)
	if (XCB_FOUND)
		set(HAS_X 1)
		add_compile_definitions(SHARKS_HAS_X)
	endif ()

	find_package(Wayland COMPONENTS Client)
	find_package(WaylandScanner)
	if (Wayland_FOUND AND WaylandScanner_FOUND)
		set(HAS_WAYLAND 1)
		add_compile_definitions(SHARKS_HAS_WAYLAND)
	endif ()
endif ()

include(FetchContent)
FetchContent_Declare(
	tomlplusplus
	GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
	GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

add_executable(sharks
	main.cxx
	selectionwindow.cxx
	selectionwindow.hxx
	traymenu.cxx
	traymenu.hxx
	killexisting_linux.cxx
	killexisting.hxx
	resources/resources.qrc
	platform.cxx
	platform.hxx
	x11/x11atoms.cxx
	x11/x11atoms.hxx
	x11/x11platform.cxx
	x11/x11platform.hxx
	wayland/waylandplatform.cxx
	wayland/waylandplatform.hxx
	wayland/sway.cxx
	wayland/sway.hxx
	wayland/wlrscreengrabber.cxx
	wayland/wlrscreengrabber.hxx
	wayland/hyprland.cxx
	wayland/hyprland.hxx
	config.cxx
	config.hxx
)

if (HAS_WAYLAND)
	ecm_add_wayland_client_protocol(sharks PROTOCOL wayland-proto/xdg-output-unstable-v1.xml BASENAME xdg-output-unstable-v1)
	ecm_add_wayland_client_protocol(sharks PROTOCOL wayland-proto/wlr-screencopy-unstable-v1.xml BASENAME wlr-screencopy-unstable-v1)
	target_link_libraries(sharks PRIVATE Wayland::Client)
endif ()
if (HAS_X)
	target_link_libraries(sharks PRIVATE XCB::XFIXES XCB::SHM)
endif ()

target_link_libraries(sharks PRIVATE qhotkey)
target_link_libraries(sharks PRIVATE tomlplusplus)
target_link_libraries(sharks PRIVATE ${QT}::Widgets ${QT}::Network ${QT}::OpenGLWidgets)

target_compile_options(sharks PRIVATE -Wall -Wextra -pedantic -Wno-multichar -Wno-unused)
