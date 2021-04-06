// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ADDNEWCONTACTDIALOG_H
#define ADDNEWCONTACTDIALOG_H

#include "qt/agenor/focuseddialog.h"

namespace Ui {
class AddNewContactDialog;
}

class AddNewContactDialog : public FocusedDialog
{
    Q_OBJECT

public:
    explicit AddNewContactDialog(QWidget *parent = nullptr);
    ~AddNewContactDialog();

    void setTexts(QString title, const char* message = nullptr);
    void setData(QString address, QString label);
    void showEvent(QShowEvent *event) override;
    QString getLabel();

    bool res = false;

public Q_SLOTS:
    void accept() override;
private:
    Ui::AddNewContactDialog *ui;
    const char* message = nullptr;
};

#endif // ADDNEWCONTACTDIALOG_H
