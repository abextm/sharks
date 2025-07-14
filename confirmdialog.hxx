// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef CONFIRMDIALOG_HXX
#define CONFIRMDIALOG_HXX

#include <QDialog>

namespace Ui {
class ConfirmDialog;
}

class ConfirmDialog : public QDialog {
	Q_OBJECT

 public:
	explicit ConfirmDialog(QPixmap pixmap, QString message, QWidget *parent = nullptr);
	virtual ~ConfirmDialog();

 private:
	Ui::ConfirmDialog *ui;
};

#endif	// CONFIRMDIALOG_HXX
