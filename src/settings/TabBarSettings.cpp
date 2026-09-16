/*
  SPDX-FileCopyrightText: 2011 Kurt Hindenburg <kurt.hindenburg@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

// Own
#include "TabBarSettings.h"

#include <QTabBar>

using namespace Konsole;

TabBarSettings::TabBarSettings(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    tabs->setDocumentMode(true);
    tabs->tabBar()->setExpanding(true);

    kcfg_TabBarUserStyleSheetFile->setMimeTypeFilters({QStringLiteral("text/css")});

    // For some reason these layouts have invalid sizes when
    // sizeHint() is read before the widget is shown.
    appearanceTabLayout->activate();
    behaviorTabLayout->activate();

    // Enable CSS file selector only when tabbar is visible and custom css is active
    const auto updateStyleSheetFileEnable = [this](bool) {
        kcfg_TabBarUserStyleSheetFile->setEnabled(kcfg_TabBarUseUserStyleSheet->isChecked() && !AlwaysHideTabBar->isChecked());
    };
    connect(kcfg_TabBarUseUserStyleSheet, &QAbstractButton::toggled, this, updateStyleSheetFileEnable);
    connect(AlwaysHideTabBar, &QAbstractButton::toggled, this, updateStyleSheetFileEnable);

    const auto updateTextAlignmentEnable = [this]() {
        const bool enabled = (Left->isChecked() || Right->isChecked()) && !AlwaysHideTabBar->isChecked();
        sideTabTextAlignmentLabel->setEnabled(enabled);
        kcfg_SideTabTextAlignment->setEnabled(enabled);
    };
    connect(Left, &QAbstractButton::toggled, this, updateTextAlignmentEnable);
    connect(Right, &QAbstractButton::toggled, this, updateTextAlignmentEnable);
    connect(AlwaysHideTabBar, &QAbstractButton::toggled, this, updateTextAlignmentEnable);
    updateTextAlignmentEnable();
}

TabBarSettings::~TabBarSettings() = default;

#include "moc_TabBarSettings.cpp"
