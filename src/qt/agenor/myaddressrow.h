// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MYADDRESSROW_H
#define MYADDRESSROW_H

#include <QWidget>

namespace Ui {
class MyAddressRow;
}

class MyAddressRow : public QWidget
{
    Q_OBJECT

public:
    explicit MyAddressRow(QWidget *parent = nullptr);
    ~MyAddressRow();

    void updateView(QString address, QString label, QString date);

private:
    Ui::MyAddressRow *ui;
};

#endif // MYADDRESSROW_H
