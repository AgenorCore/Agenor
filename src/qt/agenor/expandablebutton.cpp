// Copyright (c) 2019-2020 The PIVX developers
// Copyright (c) 2021 The AgenorCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/agenor/expandablebutton.h"
#include "qt/agenor/forms/ui_expandablebutton.h"
#include "qt/agenor/qtutils.h"
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QStyle>
#include <iostream>

ExpandableButton::ExpandableButton(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ExpandableButton),
    isAnimating(false)
{
    ui->setupUi(this);

    this->setStyleSheet(parent->styleSheet());
    ui->pushButton->setCheckable(true);
    this->layout()->setSizeConstraint(QLayout::SetFixedSize);

    connect(ui->pushButton, &QPushButton::clicked, this, &ExpandableButton::innerMousePressEvent);
}

void ExpandableButton::setButtonClassStyle(const char *name, const QVariant &value, bool forceUpdate)
{
    ui->pushButton->setProperty(name, value);
    if (forceUpdate) {
        updateStyle(ui->pushButton);
    }
}

void ExpandableButton::setIcon(QString path)
{
    ui->pushButton->setIcon(QIcon(path));
}

void ExpandableButton::setButtonText(const QString& _text)
{
    this->text = _text;
    if (this->isExpanded) {
        ui->pushButton->setText(_text);
    }
}

void ExpandableButton::setNoIconText(const QString& _text)
{
    notExpandedText = _text;
    if (!this->isExpanded)
        ui->pushButton->setText(_text);
}

void ExpandableButton::setText2(QString text2)
{
    this->text = text2;
    ui->pushButton->setText(text2);
}

ExpandableButton::~ExpandableButton()
{
    delete ui;
}

bool ExpandableButton::isChecked()
{
    return ui->pushButton->isChecked();
}

void ExpandableButton::setChecked(bool check)
{
    ui->pushButton->setChecked(check);
}

void ExpandableButton::setSmall()
{
    ui->pushButton->setText(notExpandedText);
    this->setMaximumWidth(36);
    this->isExpanded = false;
    update();
}

void ExpandableButton::setExpanded()
{
    this->setMaximumWidth(100);
    ui->pushButton->setText(text);
    this->isExpanded = true;
}

void ExpandableButton::enterEvent(QEvent *)
{
    if (!this->isAnimating) {
        setExpanded();
        Q_EMIT Mouse_Hover();
    }
    update();
}

void ExpandableButton::leaveEvent(QEvent *)
{
    if (!keepExpanded) {
        this->setSmall();
    }
    Q_EMIT Mouse_HoverLeave();
}

void ExpandableButton::innerMousePressEvent()
{
    Q_EMIT Mouse_Pressed();
}

void ExpandableButton::on_pushButton_clicked(bool checked)
{
    // TODO: Add callback event
}
