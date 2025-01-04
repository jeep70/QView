#ifndef QVAPPLICATION_H
#define QVAPPLICATION_H

#include "mainwindow.h"
#include "settingsmanager.h"
#include "shortcutmanager.h"
#include "actionmanager.h"
#include "updatechecker.h"
#include "qvoptionsdialog.h"
#include "qvaboutdialog.h"
#include "qvwelcomedialog.h"

#include <QApplication>
#include <QRegularExpression>

#if defined(qvApp)
#undef qvApp
#endif

#define qvApp (qobject_cast<QVApplication *>(QCoreApplication::instance()))	// global qvapplication object

class QVApplication : public QApplication
{
    Q_OBJECT

public:
    explicit QVApplication(int &argc, char **argv);
    ~QVApplication() override;

    bool event(QEvent *event) override;

    static void openFile(MainWindow *window, const QString &file, bool resize = true);

    static void openFile(const QString &file, bool resize = true);

    static void pickFile(MainWindow *parent = nullptr);

    static void pickUrl(MainWindow *parnet = nullptr);

    static MainWindow *newWindow();

    MainWindow *getMainWindow(bool shouldBeEmpty);

    void checkUpdates(bool isStartupCheck);

    void checkedUpdates();

    void recentsMenuUpdated();

    void addToLastActiveWindows(MainWindow *window);

    void deleteFromLastActiveWindows(MainWindow *window);

    void openOptionsDialog(QWidget *parent = nullptr);

    void openWelcomeDialog(QWidget *parent = nullptr);

    void openAboutDialog(QWidget *parent = nullptr);

    void hideIncompatibleActions();

    void defineFilterLists();

    QMenuBar *getMenuBar() const {  return menuBar; }

    const QStringList &getNameFilterList() const { return nameFilterList; }

    const QStringList &getFileExtensionList() const { return fileExtensionList; }

    const QStringList &getMimeTypeNameList() const { return mimeTypeNameList; }

    SettingsManager &getSettingsManager() { return settingsManager; }

    ShortcutManager &getShortcutManager() { return shortcutManager; }

    ActionManager &getActionManager() { return actionManager; }

    static bool supportsTitlebarHiding();

    static qreal getPerceivedBrightness(const QColor &color);

private:

    QList<MainWindow*> lastActiveWindows;

    QMenu *dockMenu;

    QMenuBar *menuBar;

    QStringList nameFilterList;
    QStringList fileExtensionList;
    QStringList mimeTypeNameList;

    // This order is very important
    SettingsManager settingsManager; 
    ActionManager actionManager;
    ShortcutManager shortcutManager;

    QPointer<QVOptionsDialog> optionsDialog;
    QPointer<QVWelcomeDialog> welcomeDialog;
    QPointer<QVAboutDialog> aboutDialog;

#ifndef QV_DISABLE_ONLINE_VERSION_CHECK
    UpdateChecker updateChecker;
#endif //QV_DISABLE_ONLINE_VERSION_CHECK
};

#endif // QVAPPLICATION_H
