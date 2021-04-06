// Copyright (c) 2019-2020 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SENDCONFIRMDIALOG_H
#define SENDCONFIRMDIALOG_H

#include "walletmodeltransaction.h"
#include "qt/agenor/focuseddialog.h"
#include "qt/agenor/snackbar.h"

class WalletModelTransaction;
class WalletModel;

namespace Ui {
class TxDetailDialog;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class TxDetailDialog : public FocusedDialog
{
    Q_OBJECT

public:
    explicit TxDetailDialog(QWidget *parent = nullptr, bool isConfirmDialog = true, const QString& warningStr = QString());
    ~TxDetailDialog();

    bool isConfirm() { return this->confirm;}
    WalletModel::SendCoinsReturn getStatus() { return this->sendStatus;}

    void setData(WalletModel *model, WalletModelTransaction& tx);
    void setData(WalletModel *model, const QModelIndex &index);
    void setDisplayUnit(int unit){this->nDisplayUnit = unit;};

public Q_SLOTS:
    void accept() override;
    void reject() override;
    void onInputsClicked();
    void onOutputsClicked();

private:
    Ui::TxDetailDialog *ui;
    SnackBar *snackBar = nullptr;
    int nDisplayUnit = 0;
    bool isConfirmDialog = false;
    bool confirm = false;
    WalletModel *model = nullptr;
    WalletModel::SendCoinsReturn sendStatus;
    WalletModelTransaction *tx = nullptr;
    uint256 txHash;

    bool inputsLoaded = false;
    bool outputsLoaded = false;
};

#endif // SENDCONFIRMDIALOG_H
