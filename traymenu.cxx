// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "traymenu.hxx"

#include <QApplication>
#include <QIcon>
#include <QSystemTrayIcon>
#include <QThread>

#include "QHotkey/qhotkey.h"
#include "config.hxx"
#include "selectionwindow.hxx"

static void addGlobalKey(QAction *action, std::string_view name, bool *retryAdd) {
	for (const auto &key : Config::parseHotkeys(config->root["globalkeys"][name])) {
		auto *hotkey = new QHotkey(key, true, action);
		QObject::connect(hotkey, &QHotkey::activated, action, [action]() {
			action->activate(QAction::Trigger);
		});
		if (!hotkey->isRegistered()) {
			if (*retryAdd) {
				// sometimes the killed duplicate is still holding the keybind, so wait a bit for it to finish
				QThread::sleep(50);
				*retryAdd = false;
				hotkey->setRegistered(true);
				if (!hotkey->isRegistered()) {
					qInfo() << "Failed to register" << key << "for" << name;
				}
			}
		}
	}
}

TrayMenu::TrayMenu(QWidget *parent) : QMenu(parent) {
	auto *takeScreenshot = new QAction(QIcon(":/icon.svg"), "Take screenshot", this);
	connect(takeScreenshot, &QAction::triggered, this, [this]() {
		auto *win = new SelectionWindow(this);
		win->setVisible(true);
	});
	this->addAction(takeScreenshot);

	auto *picker = new QAction(QIcon("find-location-symbolic"), "Color picker", this);
	connect(picker, &QAction::triggered, this, [this]() {
		auto *win = new SelectionWindow(this);
		win->setPicking(true);
		win->setVisible(true);
	});
	this->addAction(picker);

	if (QHotkey::isPlatformSupported()) {
		bool retryAdd = true;
		addGlobalKey(takeScreenshot, "screenshot", &retryAdd);
		addGlobalKey(picker, "picker", &retryAdd);
	}

	addSeparator();

	auto *exit = new QAction(QIcon::fromTheme("exit"), "Exit", this);
	connect(exit, &QAction::triggered, this, []() {
		QApplication::exit(0);
	});
	this->addAction(exit);
}

TrayMenu::~TrayMenu() {}

void TrayMenu::createTrayIcon() {
	auto *tray = new QSystemTrayIcon(QIcon(":/icon.svg"), this);
	tray->setToolTip(QApplication::applicationDisplayName());
	tray->setContextMenu(this);
	tray->setVisible(true);
}
