// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef TRAYMENU_HXX
#define TRAYMENU_HXX

#include <QMenu>

class TrayMenu : public QMenu {
	Q_OBJECT
 private:
	Q_DISABLE_COPY(TrayMenu)
 public:
	explicit TrayMenu(QWidget *parent = nullptr);
	virtual ~TrayMenu();

	void createTrayIcon();
};

#endif	// TRAYMENU_HXX
