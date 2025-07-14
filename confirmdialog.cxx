// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "confirmdialog.hxx"

#include "ui_confirmdialog.h"

ConfirmDialog::ConfirmDialog(QPixmap pixmap, QString message, QWidget *parent)
	: QDialog(parent),
		ui(new Ui::ConfirmDialog) {
	ui->setupUi(this);

	this->setAttribute(Qt::WA_DeleteOnClose);

	this->ui->image->setPixmap(pixmap);
	this->ui->message->setText(message);

	this->setWindowTitle("Sharks - Confirm");
}

ConfirmDialog::~ConfirmDialog() {
	delete ui;
}
