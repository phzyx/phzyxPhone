// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Michael Bradeen and phzyxPhone contributors.
#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("phzyxPhone");
    QApplication::setOrganizationName("phzyxPhone");

    MainWindow w;
    w.show();
    return app.exec();
}
