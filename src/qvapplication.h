#ifndef QVAPPLICATION_H
#define QVAPPLICATION_H

#include <QApplication>
#include "mainwindow.h"

class QVApplication : public QApplication
{
    Q_OBJECT

public:
    QVApplication(int &argc, char **argv);

    bool event(QEvent *event) override;

    void openFile(const QString file);

    MainWindow *newWindow();

    MainWindow *getMainWindow();
};

#endif // QVAPPLICATION_H
