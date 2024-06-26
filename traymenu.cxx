// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "traymenu.hxx"

#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QSystemTrayIcon>
#include <QThread>

#include "QHotkey/qhotkey.h"
#include "selectionwindow.hxx"

TrayMenu::TrayMenu(QWidget *parent) : QMenu(parent) {
	auto *takeScreenshot = new QAction(QIcon(":/icon.svg"), "Take screenshot", this);
	connect(takeScreenshot, &QAction::triggered, this, [this]() {
		auto *win = new SelectionWindow(this);
		win->setVisible(true);
	});
	this->addAction(takeScreenshot);
	if (QHotkey::isPlatformSupported()) {
		auto *takeScreenshotHotkey = new QHotkey(QKeySequence(Qt::CTRL | Qt::Key_Print), true, this);
		connect(takeScreenshotHotkey, &QHotkey::activated, this, [takeScreenshot]() {
			takeScreenshot->activate(QAction::Trigger);
		});
		if (!takeScreenshotHotkey->isRegistered()) {
			// sometimes the killed duplicate is still holding the keybind, so wait a bit for it to finish
			QThread::sleep(50);
			takeScreenshotHotkey->setRegistered(true);
			if (takeScreenshotHotkey->isRegistered()) {
				qInfo() << "hotkey now registered";
			}
		}
	}

	auto *picker = new QAction(QIcon("find-location-symbolic"), "Color picker", this);
	connect(picker, &QAction::triggered, this, [this]() {
		auto *win = new SelectionWindow(this);
		win->setPicking(true);
		win->setVisible(true);
	});
	this->addAction(picker);
	if (QHotkey::isPlatformSupported()) {
		auto *pickerHotkey = new QHotkey(QKeySequence(Qt::ALT | Qt::Key_Print), true, this);
		connect(pickerHotkey, &QHotkey::activated, this, [picker]() {
			picker->activate(QAction::Trigger);
		});
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
