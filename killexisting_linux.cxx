// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "killexisting.hxx"

#ifdef Q_OS_LINUX
#include <sys/socket.h>
#include <unistd.h>

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QSocketNotifier>
#include <csignal>

#include "selectionwindow.hxx"

static const quint32 MAGIC_SIG_EXIT = 'SKEX';
static const quint32 MAGIC_SIG_SCREENSHOT = 'SKSC';
static const quint32 MAGIC_SIG_PICKER = 'SKPK';
static int sigNotifierFd[2] = {-1, -1};

void sigusr1Action(int sig, siginfo_t *info, void *ucontext) {
	Q_UNUSED(sig)
	Q_UNUSED(ucontext);

	quint32 v = info->si_int;
	write(sigNotifierFd[0], &v, sizeof(v));
}

void setupKillExisting() {
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sigNotifierFd)) {
		qWarning() << "unable to create kill socket notifier";
	}

	auto *exitNotifier = new QSocketNotifier(sigNotifierFd[1], QSocketNotifier::Read, QApplication::instance());
	QObject::connect(exitNotifier, &QSocketNotifier::activated, []() {
		quint32 a;
		read(sigNotifierFd[1], &a, sizeof(a));

		if (a == MAGIC_SIG_EXIT) {
			qInfo() << "Killed by new instance";
			QApplication::exit(0);
		} else if (a == MAGIC_SIG_SCREENSHOT) {
			auto *win = new SelectionWindow();
			win->setVisible(true);
		} else if (a == MAGIC_SIG_PICKER) {
			auto *win = new SelectionWindow();
			win->setPicking(true);
			win->setVisible(true);
		}
	});

	struct sigaction act = {};
	sigaction(SIGUSR1, nullptr, &act);
	act.sa_flags |= SA_SIGINFO | SA_RESTART;
	act.sa_sigaction = sigusr1Action;
	sigaction(SIGUSR1, &act, nullptr);

	qint64 myPid = QApplication::applicationPid();

	// for some reason QDirIterator is about 1000 times slower than entryList
	const auto dirEntries = QDir("/proc/").entryList();
	for (const QString &fileName : dirEntries) {
		QString filePath = "/proc/" + fileName;
		qint64 pid = fileName.toLongLong();
		if (pid == 0 || pid == myPid) {
			continue;
		}

		{
			QFile cmdlineFile(filePath + "/cmdline");
			if (!cmdlineFile.open(QFile::ReadOnly)) {
				continue;
			}
			QByteArray cmdline = cmdlineFile.readAll();
			auto procName = cmdline.split(0).at(0);
			if (procName != "sharks" && !procName.endsWith("/sharks")) {
				continue;
			}
		}

		{
			QFile statusFile(filePath + "/status");
			if (!statusFile.open(QFile::ReadOnly)) {
				qWarning() << "unable to read status for pid" << pid;
				continue;
			}
			for (QByteArray &line : statusFile.readAll().split('\n')) {
				QList<QByteArray> parts = line.split(':');
				if (parts.at(0) == "Tgid") {
					qint64 tgid = parts.at(1).trimmed().toLongLong();
					if (tgid != pid) {
						// this is a thread, not the actual process
						goto nextfile;
					}
				}
			}
		}

		{
			qInfo() << "asking" << pid << "to exit";
			union sigval sig = {};
			sig.sival_int = MAGIC_SIG_EXIT;
			sigqueue(pid, SIGUSR1, sig);
		}
	nextfile:;
	}
}
#endif
