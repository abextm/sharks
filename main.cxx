// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include <QApplication>
#include <QCommandLineParser>
#include <QLabel>

#include "config.hxx"
#include "killexisting.hxx"
#include "platform.hxx"
#include "selectionwindow.hxx"
#include "traymenu.hxx"

int main(int argc, char *argv[]) {
	QApplication app(argc, argv);
	QApplication::setApplicationName("sharks");
	QApplication::setApplicationDisplayName("Sharks");

	Platform::init();

	QCommandLineParser cli;
	cli.addHelpOption();
	cli.addVersionOption();

	QCommandLineOption now("now", "Immediately takes a screenshot and exits");
	cli.addOption(now);

#ifdef HAS_KILLEXISTING
	QCommandLineOption noKillOther("nokill", "Don't kill existing instances");
	cli.addOption(noKillOther);
#endif

	cli.process(app);

	Config::init();

	{
		QLabel foo("foo");
		// Calling this early saves some time opening the screenshot window, since it can be quite slow to load the default fonts
		foo.minimumSizeHint();
	}

	if (cli.isSet(now)) {
		auto *w = new SelectionWindow;
		w->setAttribute(Qt::WA_QuitOnClose);
		w->setVisible(true);
		return app.exec();
	}

#ifdef HAS_KILLEXISTING
	if (!cli.isSet(noKillOther)) {
		setupKillExisting();
	}
#endif

	app.setQuitOnLastWindowClosed(false);

	TrayMenu m;
	m.createTrayIcon();

	return app.exec();
}
