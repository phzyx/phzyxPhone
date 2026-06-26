// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Michael Bradeen and phzyxPhone contributors.
#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("phzyxPhone");
    QApplication::setOrganizationName("phzyxPhone");
    QApplication::setWindowIcon(QIcon(":/phzyxphone.png"));
    // Lets GNOME/Wayland (and other DEs) match this window to the
    // installed phzyxphone.desktop file, so the dock shows our icon.
    QGuiApplication::setDesktopFileName("phzyxphone");

    MainWindow w;
    w.show();
    return app.exec();
}
