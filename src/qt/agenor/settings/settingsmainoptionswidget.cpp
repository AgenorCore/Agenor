// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/agenor/settings/settingsmainoptionswidget.h"
#include "qt/agenor/settings/forms/ui_settingsmainoptionswidget.h"
#include "QListView"

#if defined(HAVE_CONFIG_H)
#include "config/agenor-config.h"
#endif

#include "bitcoinunits.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "clientmodel.h"
#include "qt/agenor/qtutils.h"

#include "main.h" // for MAX_SCRIPTCHECK_THREADS
#include "netbase.h"
#include "txdb.h" // for -dbcache defaults

#ifdef ENABLE_WALLET
#include "wallet/wallet.h" // for CWallet::minTxFee
#endif

#include <boost/thread.hpp>

#include <QDataWidgetMapper>
#include <QIntValidator>
#include <QLocale>
#include <QTimer>


SettingsMainOptionsWidget::SettingsMainOptionsWidget(AgenorGUI* _window, QWidget *parent) :
    PWidget(_window, parent),
    ui(new Ui::SettingsMainOptionsWidget)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());

    // Containers
    ui->left->setProperty("cssClass", "container");
    ui->left->setContentsMargins(10,10,10,10);
    ui->labelDivider->setProperty("cssClass", "container-divider");

    // Title - Subtitle
    setCssTitleScreen(ui->labelTitle);
    setCssSubtitleScreen(ui->labelSubtitle1);
    setCssTitleScreen(ui->labelTitleDown);
    setCssSubtitleScreen(ui->labelSubtitleDown);

    setCssProperty({ui->labelTitleSizeDb, ui->labelTitleThreads}, "text-main-settings");

    // Switch
    ui->pushSwitchStart->setText(tr("Start Agenor on system login"));
    ui->pushSwitchStart->setProperty("cssClass", "btn-switch");

    // Combobox
    ui->databaseCache->setProperty("cssClass", "btn-spin-box");
    ui->databaseCache->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->databaseCache);
    ui->threadsScriptVerif->setProperty("cssClass", "btn-spin-box");
    ui->threadsScriptVerif->setAttribute(Qt::WA_MacShowFocusRect, 0);
    setShadow(ui->threadsScriptVerif);

    // Buttons
    setCssBtnPrimary(ui->pushButtonSave);
    setCssBtnSecondary(ui->pushButtonReset);
    setCssBtnSecondary(ui->pushButtonClean);

    /* Main elements init */
    ui->databaseCache->setMinimum(nMinDbCache);
    ui->databaseCache->setMaximum(nMaxDbCache);
    ui->threadsScriptVerif->setMinimum(-GetNumCores());
    ui->threadsScriptVerif->setMaximum(MAX_SCRIPTCHECK_THREADS);

    connect(ui->pushButtonSave, &QPushButton::clicked, [this] { Q_EMIT saveSettings(); });
    connect(ui->pushButtonReset, &QPushButton::clicked, this, &SettingsMainOptionsWidget::onResetClicked);
    connect(ui->pushButtonClean, &QPushButton::clicked, [this] { Q_EMIT discardSettings(); });
}

void SettingsMainOptionsWidget::onResetClicked()
{
    if (clientModel) {
        if (!ask(tr("Reset Options"), tr("You are just about to reset the app\'s options to the default values.\n\nAre you sure?\n")))
            return;
        OptionsModel *optionsModel = clientModel->getOptionsModel();
        QSettings settings;
        // default setting for OptionsModel::StartAtStartup - disabled
        if (GUIUtil::GetStartOnSystemStartup())
            GUIUtil::SetStartOnSystemStartup(false);
        optionsModel->setMainDefaultOptions(settings, true);
        optionsModel->setWindowDefaultOptions(settings, true);
        inform(tr("Options reset succeed"));
    }
}

void SettingsMainOptionsWidget::setMapper(QDataWidgetMapper *mapper)
{
    mapper->addMapping(ui->pushSwitchStart, OptionsModel::StartAtStartup);
    mapper->addMapping(ui->threadsScriptVerif, OptionsModel::ThreadsScriptVerif);
    mapper->addMapping(ui->databaseCache, OptionsModel::DatabaseCache);
    /* Window */
#ifndef Q_OS_MAC
    mapper->addMapping(ui->checkBoxMinTaskbar, OptionsModel::MinimizeToTray);
    mapper->addMapping(ui->checkBoxMinClose, OptionsModel::MinimizeOnClose);
#endif
}

SettingsMainOptionsWidget::~SettingsMainOptionsWidget()
{
    delete ui;
}
