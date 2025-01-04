#include "actionmanager.h"
#include "qvapplication.h"
#include "qvcocoafunctions.h"
#include "openwith.h"

#include <QSettings>
#include <QMimeDatabase>
#include <QFileIconProvider>

ActionManager::ActionManager(QObject *parent) : QObject(parent)
{
    isSaveRecentsEnabled = true;
    recentsListMaxLength = 10;
    openWithMaxLength = 10;

    // Connect to settings signal
    connect(&qvApp->getSettingsManager(), &SettingsManager::settingsUpdated, this, &ActionManager::settingsUpdated);
    settingsUpdated();

    initializeActionLibrary();

    recentsSaveTimer = new QTimer(this);
    recentsSaveTimer->setSingleShot(true);
    recentsSaveTimer->setInterval(500);
    connect(recentsSaveTimer, &QTimer::timeout, this, &ActionManager::saveRecentsList);

    loadRecentsList();

#ifdef COCOA_LOADED
    windowMenu = new QMenu(tr("Window"));
    QVCocoaFunctions::setWindowMenu(windowMenu);
#endif
}

ActionManager::~ActionManager()
{
    qDeleteAll(actionLibrary);
}

void ActionManager::settingsUpdated()
{
    isSaveRecentsEnabled = qvApp->getSettingsManager().getBoolean("saverecents");

    auto const recentsMenus = menuCloneLibrary.values("recents");
    for (const auto &recentsMenu : recentsMenus)
    {
        recentsMenu->menuAction()->setVisible(isSaveRecentsEnabled);
    }
    if (!isSaveRecentsEnabled)
        clearRecentsList();
}

QAction *ActionManager::addCloneOfAction(QWidget *parent, const QString &key)
{
    if (auto action = getAction(key))
    {
        auto newAction = new QAction(parent);
        newAction->setIcon(action->icon());
        newAction->setData(action->data());
        newAction->setText(action->text());
        newAction->setMenuRole(action->menuRole());
        newAction->setEnabled(action->isEnabled());
        newAction->setShortcuts(action->shortcuts());
        newAction->setVisible(action->isVisible());
        actionCloneLibrary.insert(key, newAction);
        parent->addAction(newAction);
        return newAction;
    }

    return nullptr;
}

QAction *ActionManager::getAction(const QString &key) const
{
    if (auto action = actionLibrary.value(key))
        return action;

    return nullptr;
}

QList<QAction*> ActionManager::getAllInstancesOfAction(const QString &key) const
{
    QList<QAction*> listOfActions = getAllClonesOfAction(key);

    if (auto mainAction = getAction(key))
            listOfActions.append(mainAction);

    return listOfActions;
}

QList<QAction*> ActionManager::getAllClonesOfAction(const QString &key) const
{
    return actionCloneLibrary.values(key);
}

QList<QAction*> ActionManager::getAllClonesOfAction(const QString &key, QWidget *parent) const
{
    QList<QAction*> listOfChildActions;
    const auto &actions = getAllClonesOfAction(key);
    for (const auto &action : actions)
    {
        if (hasAncestor(action, parent))
        {
            listOfChildActions.append(action);
        }
    };
    return listOfChildActions;
}

QList<QMenu*> ActionManager::getAllClonesOfMenu(const QString &key) const
{
    return menuCloneLibrary.values(key);
}

QList<QMenu*> ActionManager::getAllClonesOfMenu(const QString &key, QWidget *parent) const
{
    QList<QMenu*> listOfChildMenus;
    const auto &menus = getAllClonesOfMenu(key);
    for (const auto &menu : menus)
    {
        if (hasAncestor(menu->menuAction(), parent))
        {
            listOfChildMenus.append(menu);
        }
    };
    return listOfChildMenus;
}

void ActionManager::untrackClonedActions(const QList<QAction*> &actions)
{
    for (const auto &action : actions)
    {
        if (auto menu = action->menu())
        {
            QString key = action->data().toString();
            if (key.length() != 0)
                menuCloneLibrary.remove(key, menu);
        }
        else
        {
            QString key = action->data().toStringList().first();
            actionCloneLibrary.remove(key, action);
        }
    }
}

void ActionManager::untrackClonedActions(const QMenu *menu)
{
    untrackClonedActions(getAllNestedActions(menu->actions()));
}

void ActionManager::untrackClonedActions(const QMenuBar *menuBar)
{
    untrackClonedActions(getAllNestedActions(menuBar->actions()));
}

void ActionManager::hideAllInstancesOfAction(const QString &key)
{
    auto actions = getAllInstancesOfAction(key);
    for (auto &action : actions)
    {
        action->setVisible(false);
    }
}

QMenuBar *ActionManager::buildMenuBar(QWidget *parent)
{
    // Global menubar
    auto *menuBar = new QMenuBar(parent);

    // Beginning of file menu
    auto *fileMenu = new QMenu(tr("&File"), menuBar);

#ifdef Q_OS_MACOS
    addCloneOfAction(fileMenu, "newwindow");
#endif
    addCloneOfAction(fileMenu, "open");
    addCloneOfAction(fileMenu, "openurl");
    fileMenu->addMenu(buildRecentsMenu(true, fileMenu));
    addCloneOfAction(fileMenu, "reloadfile");
    fileMenu->addSeparator();
#ifdef Q_OS_MACOS
    fileMenu->addSeparator();
    addCloneOfAction(fileMenu, "closewindow");
    addCloneOfAction(fileMenu, "closeallwindows");
#endif
#ifdef COCOA_LOADED
    QVCocoaFunctions::setAlternate(fileMenu, fileMenu->actions().length()-1);
#endif
    fileMenu->addSeparator();
    fileMenu->addMenu(buildOpenWithMenu(fileMenu));
    addCloneOfAction(fileMenu, "opencontainingfolder");
    addCloneOfAction(fileMenu, "showfileinfo");
    fileMenu->addSeparator();
    addCloneOfAction(fileMenu, "quit");

    menuBar->addMenu(fileMenu);
    // End of file menu

    // Beginning of edit menu
    auto *editMenu = new QMenu(tr("&Edit"), menuBar);

    addCloneOfAction(editMenu, "undo");
    editMenu->addSeparator();
    addCloneOfAction(editMenu, "copy");
    addCloneOfAction(editMenu, "paste");
    addCloneOfAction(editMenu, "rename");
    editMenu->addSeparator();
    addCloneOfAction(editMenu, "delete");
    addCloneOfAction(editMenu, "deletepermanent");
#ifdef COCOA_LOADED
    QVCocoaFunctions::setAlternate(editMenu, editMenu->actions().length()-1);
#endif

    menuBar->addMenu(editMenu);
    // End of edit menu

    // Beginning of view menu
    menuBar->addMenu(buildViewMenu(false, menuBar));
    // End of view menu

    // Beginning of go menu
    auto *goMenu = new QMenu(tr("&Go"), menuBar);

    addCloneOfAction(goMenu, "firstfile");
    addCloneOfAction(goMenu, "previousfile");
    addCloneOfAction(goMenu, "nextfile");
    addCloneOfAction(goMenu, "lastfile");

    menuBar->addMenu(goMenu);
    // End of go menu

    // Beginning of tools menu
    menuBar->addMenu(buildToolsMenu(false, menuBar));
    // End of tools menu

    // Beginning of window menu
#ifdef COCOA_LOADED
    menuBar->addMenu(windowMenu);
#endif
    // End of window menu

    // Beginning of help menu
    menuBar->addMenu(buildHelpMenu(false, menuBar));
    // End of help menu

    return menuBar;
}

QMenu *ActionManager::buildViewMenu(bool addIcon, QWidget *parent)
{
    auto *viewMenu = new QMenu(tr("&View"), parent);
    viewMenu->menuAction()->setData("view");
    if (addIcon)
        viewMenu->setIcon(QIcon::fromTheme("zoom-fit-best"));

    addCloneOfAction(viewMenu, "zoomin");
    addCloneOfAction(viewMenu, "zoomout");
    addCloneOfAction(viewMenu, "resetzoom");
    addCloneOfAction(viewMenu, "originalsize");
    viewMenu->addSeparator();
    addCloneOfAction(viewMenu, "rotateright");
    addCloneOfAction(viewMenu, "rotateleft");
    viewMenu->addSeparator();
    addCloneOfAction(viewMenu, "mirror");
    addCloneOfAction(viewMenu, "flip");
    viewMenu->addSeparator();
    if (qvApp->supportsTitlebarHiding())
        addCloneOfAction(viewMenu, "toggletitlebar");
    addCloneOfAction(viewMenu, "fullscreen");

    menuCloneLibrary.insert(viewMenu->menuAction()->data().toString(), viewMenu);
    return viewMenu;
}

QMenu *ActionManager::buildToolsMenu(bool addIcon, QWidget *parent)
{
    auto *toolsMenu = new QMenu(tr("&Tools"), parent);
    toolsMenu->menuAction()->setData("tools");
    if (addIcon)
        toolsMenu->setIcon(QIcon::fromTheme("configure", QIcon::fromTheme("preferences-other")));

    addCloneOfAction(toolsMenu, "saveframeas");
    addCloneOfAction(toolsMenu, "pause");
    addCloneOfAction(toolsMenu, "nextframe");
    toolsMenu->addSeparator();
    addCloneOfAction(toolsMenu, "decreasespeed");
    addCloneOfAction(toolsMenu, "resetspeed");
    addCloneOfAction(toolsMenu, "increasespeed");
    toolsMenu->addSeparator();
    addCloneOfAction(toolsMenu, "slideshow");
    addCloneOfAction(toolsMenu, "options");

    menuCloneLibrary.insert(toolsMenu->menuAction()->data().toString(), toolsMenu);
    return toolsMenu;
}

QMenu *ActionManager::buildHelpMenu(bool addIcon, QWidget *parent)
{
    auto *helpMenu = new QMenu(tr("&Help"), parent);
    helpMenu->menuAction()->setData("help");
    if (addIcon)
        helpMenu->setIcon(QIcon::fromTheme("help-about"));

    addCloneOfAction(helpMenu, "about");
    addCloneOfAction(helpMenu, "welcome");

    menuCloneLibrary.insert(helpMenu->menuAction()->data().toString(), helpMenu);
    return helpMenu;
}

QMenu *ActionManager::buildRecentsMenu(bool includeClearAction, QWidget *parent)
{
    auto *recentsMenu = new QMenu(tr("Open &Recent"), parent);
    recentsMenu->menuAction()->setData("recents");
    recentsMenu->setIcon(QIcon::fromTheme("document-open-recent"));

    connect(recentsMenu, &QMenu::aboutToShow, this, [this]{
        this->loadRecentsList();
    });

    for ( int i = 0; i < recentsListMaxLength; i++ )
    {
        auto action = new QAction(tr("Empty"), recentsMenu);
        action->setVisible(false);
        action->setIconVisibleInMenu(true);
        action->setData("recent" + QString::number(i));

        recentsMenu->addAction(action);
        actionCloneLibrary.insert(action->data().toStringList().first(), action);
    }

    if (includeClearAction)
    {
        recentsMenu->addSeparator();
        addCloneOfAction(recentsMenu, "clearrecents");
    }

    menuCloneLibrary.insert(recentsMenu->menuAction()->data().toString(), recentsMenu);
    updateRecentsMenu();
    // update settings whenever recent menu is created so it can possibly be hidden
    settingsUpdated();
    return recentsMenu;
}

void ActionManager::loadRecentsList()
{
    // Prevents weird bugs when opening the recent menu while the save timer is still running
    if (recentsSaveTimer->isActive())
        return;

    QSettings settings;
    settings.beginGroup("recents");

    QVariantList variantListRecents = settings.value("recentFiles").toList();
    recentsList = variantListToRecentsList(variantListRecents);

    auditRecentsList();
}

void ActionManager::saveRecentsList()
{
    auditRecentsList();

    QSettings settings;
    settings.beginGroup("recents");

    auto variantList = recentsListToVariantList(recentsList);

    settings.setValue("recentFiles", variantList);
}


void ActionManager::addFileToRecentsList(const QFileInfo &file)
{
    recentsList.prepend({file.fileName(), file.filePath()});
    auditRecentsList();
    recentsSaveTimer->start();
}

void ActionManager::auditRecentsList()
{
    // This function should be called whenever there is a change to recentsList,
    // and take care not to call any functions that call it.
    if (!isSaveRecentsEnabled)
    {
        recentsList.clear();
    }

    for (int i = 0; i < recentsList.length(); i++)
    {
        // Ensure each file exists
        auto recent =  recentsList.value(i);
        if (!QFileInfo::exists(recent.filePath))
        {
            recentsList.removeAt(i);
        }

        // Check for duplicates
        for (int i2 = 0; i2 < recentsList.length(); i2++)
        {
            if (i == i2)
                continue;

            if (recent == recentsList.value(i2))
            {
                recentsList.removeAt(i2);
            }
        }
    }

    while (recentsList.size() > recentsListMaxLength)
    {
        recentsList.removeLast();
    }

    updateRecentsMenu();
}

void ActionManager::clearRecentsList()
{
    recentsList.clear();
    saveRecentsList();
}

void ActionManager::updateRecentsMenu()
{
    for (int i = 0; i < recentsListMaxLength; i++)
    {
        const auto values = actionCloneLibrary.values("recent" + QString::number(i));
        for (const auto &action : values)
        {
            // If we are within the bounds of the recent list
            if (i < recentsList.length())
            {
                auto recent = recentsList.value(i);

                action->setVisible(true);
                action->setIconVisibleInMenu(false); // Hide icon temporarily to speed up updates in certain cases
                action->setText(recent.fileName);

#if defined Q_OS_UNIX && !defined Q_OS_MACOS
                // set icons for linux users
                QMimeDatabase mimedb;
                QMimeType type = mimedb.mimeTypeForFile(recent.filePath);
                action->setIcon(QIcon::fromTheme(type.iconName(), QIcon::fromTheme(type.genericIconName())));
#else
                // set icons for mac/windows users
                QFileInfo fileInfo(recent.filePath);
                QFileIconProvider provider;
                QIcon icon = provider.icon(fileInfo);
#ifdef Q_OS_MACOS
                // Workaround for native menu slowness
                if (!fileInfo.suffix().isEmpty())
                    icon = getCacheableIcon("filetype:" + fileInfo.suffix(), icon);
#endif
                action->setIcon(icon);
#endif
                action->setIconVisibleInMenu(true);
            }
            else
            {
                action->setVisible(false);
            }
        }
    }
    emit recentsMenuUpdated();
}

QMenu *ActionManager::buildOpenWithMenu(QWidget *parent)
{
    auto *openWithMenu = new QMenu(tr("Open With"), parent);
    openWithMenu->menuAction()->setData("openwith");
    openWithMenu->setIcon(QIcon::fromTheme("system-run"));
    openWithMenu->setDisabled(true);

    for (int i = 0; i < openWithMaxLength; i++)
    {

        auto action = new QAction(tr("Empty"), openWithMenu);
        action->setVisible(false);
        action->setIconVisibleInMenu(true);
        action->setData(QVariantList({"openwith" + QString::number(i), ""}));

        openWithMenu->addAction(action);
        actionCloneLibrary.insert(action->data().toStringList().first(), action);
        // Some madness that will show or hide separator if an item marked as default is in the first position
        if (i == 0)
        {
            connect(action, &QAction::changed, action, [action, openWithMenu]{
                // If this menu item is default
                if (action->data().toList().at(1).value<OpenWith::OpenWithItem>().isDefault)
                {
                    if (!openWithMenu->actions().at(1)->isSeparator())
                        openWithMenu->insertSeparator(openWithMenu->actions().at(1));
                }
                else
                {
                    if (openWithMenu->actions().at(1)->isSeparator())
                        openWithMenu->removeAction(openWithMenu->actions().at(1));
                }
            });
        }
    }

    openWithMenu->addSeparator();
    addCloneOfAction(openWithMenu, "openwithother");

    menuCloneLibrary.insert(openWithMenu->menuAction()->data().toString(), openWithMenu);
    return openWithMenu;
}

void ActionManager::actionTriggered(QAction *triggeredAction)
{
    auto key = triggeredAction->data().toStringList().first();

    // For some actions, do not look for a relevant window
    QStringList windowlessActions = {"newwindow", "quit", "clearrecents", "open"};
#ifdef Q_OS_MACOS
    windowlessActions << "about" << "welcome" << "options";
#endif
    for (const auto &actionName : qAsConst(windowlessActions))
    {
        if (key == actionName)
        {
            actionTriggered(triggeredAction, nullptr);
            return;
        }
    }

    // If some actions are triggered without an explicit window, we want
    // to give them a window without an image open
    bool shouldBeEmpty = false;
    if (key.startsWith("recent") || key == "openurl")
        shouldBeEmpty = true;

    if (auto *window = qvApp->getMainWindow(shouldBeEmpty))
        actionTriggered(triggeredAction, window);
}

void ActionManager::actionTriggered(QAction *triggeredAction, MainWindow *relevantWindow)
{
    // Conditions that will work with a nullptr window passed
    auto key = triggeredAction->data().toStringList().first();

    if (key == "quit") {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        // Workaround to allow for a graceful exit (e.g. fire window close events)
        // since QCoreApplication::quit can't accomplish this prior to Qt 6
        qGuiApp->postEvent(qGuiApp, new QEvent(QEvent::Quit));
#else
        QCoreApplication::quit();
#endif
    } else if (key == "newwindow") {
        qvApp->newWindow();
    } else if (key == "open") {
        qvApp->pickFile(relevantWindow);
    } else if (key == "open") {
        qvApp->pickFile(relevantWindow);
    } else if (key == "closewindow") {
        auto *active = QApplication::activeWindow();
#ifdef COCOA_LOADED
        QVCocoaFunctions::closeWindow(active->windowHandle());
#endif
        active->close();
    } else if (key == "closeallwindows") {
        const auto topLevelWindows = QApplication::topLevelWindows();
        for (auto *window : topLevelWindows) {
#ifdef COCOA_LOADED
            QVCocoaFunctions::closeWindow(window);
#endif
            window->close();
        }
    } else if (key == "options") {
        qvApp->openOptionsDialog(relevantWindow);
    } else if (key == "about") {
        qvApp->openAboutDialog(relevantWindow);
    } else if (key == "welcome") {
        qvApp->openWelcomeDialog(relevantWindow);
    } else if (key == "clearrecents") {
        qvApp->getActionManager().clearRecentsList();
    }

    // The great filter
    if (!relevantWindow)
        return;

    // Conditions that require a valid window pointer
    if (key.startsWith("recent")) {
        QChar finalChar = key.at(key.length()-1);
        relevantWindow->openRecent(finalChar.digitValue());
    } else if (key == "openwithother") {
        OpenWith::showOpenWithDialog(relevantWindow);
    } else if (key.startsWith("openwith")) {
        const auto &openWithItem = triggeredAction->data().toList().at(1).value<OpenWith::OpenWithItem>();
        relevantWindow->openWith(openWithItem);
    } else if (key == "openurl") {
        relevantWindow->pickUrl();
    } else if (key == "reloadfile") {
        relevantWindow->reloadFile();
    } else if (key == "opencontainingfolder") {
        relevantWindow->openContainingFolder();
    } else if (key == "showfileinfo") {
        relevantWindow->showFileInfo();
    } else if (key == "delete") {
        relevantWindow->askDeleteFile(false);
    } else if (key == "deletepermanent") {
        relevantWindow->askDeleteFile(true);
    } else if (key == "undo") {
        relevantWindow->undoDelete();
    } else if (key == "copy") {
        relevantWindow->copy();
    } else if (key == "paste") {
        relevantWindow->paste();
    } else if (key == "rename") {
        relevantWindow->rename();
    } else if (key == "zoomin") {
        relevantWindow->zoomIn();
    } else if (key == "zoomout") {
        relevantWindow->zoomOut();
    } else if (key == "resetzoom") {
        relevantWindow->resetZoom();
    } else if (key == "originalsize") {
        relevantWindow->originalSize();
    } else if (key == "rotateright") {
        relevantWindow->rotateRight();
    } else if (key == "rotateleft") {
        relevantWindow->rotateLeft();
    } else if (key == "mirror") {
        relevantWindow->mirror();
    } else if (key == "flip") {
        relevantWindow->flip();
    } else if (key == "toggletitlebar") {
        relevantWindow->toggleTitlebarHidden();
    } else if (key == "fullscreen") {
        relevantWindow->toggleFullScreen();
    } else if (key == "firstfile") {
        relevantWindow->firstFile();
    } else if (key == "previousfile") {
        relevantWindow->previousFile();
    } else if (key == "nextfile") {
        relevantWindow->nextFile();
    } else if (key == "lastfile") {
        relevantWindow->lastFile();
    } else if (key == "saveframeas") {
        relevantWindow->saveFrameAs();
    } else if (key == "pause") {
        relevantWindow->pause();
    } else if (key == "nextframe") {
        relevantWindow->nextFrame();
    } else if (key == "decreasespeed") {
        relevantWindow->decreaseSpeed();
    } else if (key == "resetspeed") {
        relevantWindow->resetSpeed();
    } else if (key == "increasespeed") {
        relevantWindow->increaseSpeed();
    } else if (key == "slideshow") {
        relevantWindow->toggleSlideshow();
    }
}

void ActionManager::initializeActionLibrary()
{
    auto *quitAction = new QAction(QIcon::fromTheme("application-exit"), tr("&Quit"));
    actionLibrary.insert("quit", quitAction);
#ifdef Q_OS_WIN
    //: The quit action is called "Exit" on windows
    quitAction->setText(tr("Exit"));
#endif

    auto *newWindowAction = new QAction(QIcon::fromTheme("window-new"), tr("New Window"));
    actionLibrary.insert("newwindow", newWindowAction);

    auto *openAction = new QAction(QIcon::fromTheme("document-open"), tr("&Open..."));
    actionLibrary.insert("open", openAction);

    auto *openUrlAction = new QAction(QIcon::fromTheme("document-open-remote", QIcon::fromTheme("folder-remote")), tr("Open &URL..."));
    actionLibrary.insert("openurl", openUrlAction);

    auto *reloadFileAction = new QAction(QIcon::fromTheme("view-refresh"), tr("Re&load File"));
    reloadFileAction->setData({"disable"});
    actionLibrary.insert("reloadfile", reloadFileAction);

    auto *closeWindowAction = new QAction(QIcon::fromTheme("window-close"), tr("Close Window"));
    actionLibrary.insert("closewindow", closeWindowAction);

    //: Close all windows, that is
    auto *closeAllWindowsAction = new QAction(QIcon::fromTheme("window-close"), tr("Close All"));
    actionLibrary.insert("closeallwindows", closeAllWindowsAction);

    auto *openContainingFolderAction = new QAction(QIcon::fromTheme("document-open"), tr("Open Containing &Folder"));
#ifdef Q_OS_WIN
    //: Open containing folder on windows
    openContainingFolderAction->setText(tr("Show in E&xplorer"));
#elif defined Q_OS_MACOS
    //: Open containing folder on macOS
    openContainingFolderAction->setText(tr("Show in &Finder"));
#endif
    openContainingFolderAction->setData({"disable"});
    actionLibrary.insert("opencontainingfolder", openContainingFolderAction);

    auto *showFileInfoAction = new QAction(QIcon::fromTheme("document-properties"), tr("Show File &Info"));
    showFileInfoAction->setData({"disable"});
    actionLibrary.insert("showfileinfo", showFileInfoAction);

    auto *deleteAction = new QAction(QIcon::fromTheme("edit-delete"), tr("&Move to Trash"));
#ifdef Q_OS_WIN
    deleteAction->setText(tr("&Delete"));
#endif
    deleteAction->setData({"disable"});
    actionLibrary.insert("delete", deleteAction);

    auto *deletePermanentAction = new QAction(QIcon::fromTheme("edit-delete"), tr("Delete Permanently"));
    deletePermanentAction->setData({"disable"});
    actionLibrary.insert("deletepermanent", deletePermanentAction);

    auto *undoAction = new QAction(QIcon::fromTheme("edit-undo"), tr("&Restore from Trash"));
#ifdef Q_OS_WIN
    undoAction->setText(tr("&Undo Delete"));
#endif
    undoAction->setData({"undodisable"});
    actionLibrary.insert("undo", undoAction);

    auto *copyAction = new QAction(QIcon::fromTheme("edit-copy"), tr("&Copy"));
    copyAction->setData({"disable"});
    actionLibrary.insert("copy", copyAction);

    auto *pasteAction = new QAction(QIcon::fromTheme("edit-paste"), tr("&Paste"));
    actionLibrary.insert("paste", pasteAction);

    auto *renameAction = new QAction(QIcon::fromTheme("edit-rename", QIcon::fromTheme("document-properties")) , tr("R&ename..."));
    renameAction->setData({"disable"});
    actionLibrary.insert("rename", renameAction);

    auto *zoomInAction = new QAction(QIcon::fromTheme("zoom-in"), tr("Zoom &In"));
    zoomInAction->setData({"disable"});
    actionLibrary.insert("zoomin", zoomInAction);

    auto *zoomOutAction = new QAction(QIcon::fromTheme("zoom-out"), tr("Zoom &Out"));
    zoomOutAction->setData({"disable"});
    actionLibrary.insert("zoomout", zoomOutAction);

    auto *resetZoomAction = new QAction(QIcon::fromTheme("zoom-fit-best"), tr("Reset &Zoom"));
    resetZoomAction->setData({"disable"});
    actionLibrary.insert("resetzoom", resetZoomAction);

    auto *originalSizeAction = new QAction(QIcon::fromTheme("zoom-original"), tr("Ori&ginal Size"));
    originalSizeAction->setData({"disable"});
    actionLibrary.insert("originalsize", originalSizeAction);

    auto *rotateRightAction = new QAction(QIcon::fromTheme("object-rotate-right"), tr("Rotate &Right"));
    rotateRightAction->setData({"disable"});
    actionLibrary.insert("rotateright", rotateRightAction);

    auto *rotateLeftAction = new QAction(QIcon::fromTheme("object-rotate-left"), tr("Rotate &Left"));
    rotateLeftAction->setData({"disable"});
    actionLibrary.insert("rotateleft", rotateLeftAction);

    auto *mirrorAction = new QAction(QIcon::fromTheme("object-flip-horizontal"), tr("&Mirror"));
    mirrorAction->setData({"disable"});
    actionLibrary.insert("mirror", mirrorAction);

    auto *flipAction = new QAction(QIcon::fromTheme("object-flip-vertical"), tr("&Flip"));
    flipAction->setData({"disable"});
    actionLibrary.insert("flip", flipAction);

    auto *toggleTitlebarAction = new QAction(tr("Hide Title&bar"));
    toggleTitlebarAction->setData({"windowdisable"});
    actionLibrary.insert("toggletitlebar", toggleTitlebarAction);

    auto *fullScreenAction = new QAction(QIcon::fromTheme("view-fullscreen"), tr("Enter F&ull Screen"));
    fullScreenAction->setMenuRole(QAction::NoRole);
    fullScreenAction->setData({"windowdisable"});
    actionLibrary.insert("fullscreen", fullScreenAction);

    auto *firstFileAction = new QAction(QIcon::fromTheme("go-first"), tr("&First File"));
    firstFileAction->setData({"folderdisable"});
    actionLibrary.insert("firstfile", firstFileAction);

    auto *previousFileAction = new QAction(QIcon::fromTheme("go-previous"), tr("Previous Fi&le"));
    previousFileAction->setData({"folderdisable"});
    actionLibrary.insert("previousfile", previousFileAction);

    auto *nextFileAction = new QAction(QIcon::fromTheme("go-next"), tr("&Next File"));
    nextFileAction->setData({"folderdisable"});
    actionLibrary.insert("nextfile", nextFileAction);

    auto *lastFileAction = new QAction(QIcon::fromTheme("go-last"), tr("Las&t File"));
    lastFileAction->setData({"folderdisable"});
    actionLibrary.insert("lastfile", lastFileAction);

    auto *saveFrameAsAction = new QAction(QIcon::fromTheme("document-save-as"), tr("Save Frame &As..."));
    saveFrameAsAction->setData({"gifdisable"});
    actionLibrary.insert("saveframeas", saveFrameAsAction);

    auto *pauseAction = new QAction(QIcon::fromTheme("media-playback-pause"), tr("Pa&use"));
    pauseAction->setData({"gifdisable"});
    actionLibrary.insert("pause", pauseAction);

    auto *nextFrameAction = new QAction(QIcon::fromTheme("media-skip-forward"), tr("&Next Frame"));
    nextFrameAction->setData({"gifdisable"});
    actionLibrary.insert("nextframe", nextFrameAction);

    auto *decreaseSpeedAction = new QAction(QIcon::fromTheme("media-seek-backward"), tr("&Decrease Speed"));
    decreaseSpeedAction->setData({"gifdisable"});
    actionLibrary.insert("decreasespeed", decreaseSpeedAction);

    auto *resetSpeedAction = new QAction(QIcon::fromTheme("media-playback-start"), tr("&Reset Speed"));
    resetSpeedAction->setData({"gifdisable"});
    actionLibrary.insert("resetspeed", resetSpeedAction);

    auto *increaseSpeedAction = new QAction(QIcon::fromTheme("media-skip-forward"), tr("&Increase Speed"));
    increaseSpeedAction->setData({"gifdisable"});
    actionLibrary.insert("increasespeed", increaseSpeedAction);

    auto *slideshowAction = new QAction(QIcon::fromTheme("media-playback-start"), tr("Start S&lideshow"));
    slideshowAction->setData({"disable"});
    actionLibrary.insert("slideshow", slideshowAction);

    //: This is for the options dialog on windows
    auto *optionsAction = new QAction(QIcon::fromTheme("configure", QIcon::fromTheme("preferences-other")), tr("&Settings"));
#ifdef Q_OS_MACOS
    if (QOperatingSystemVersion::current() < QOperatingSystemVersion(QOperatingSystemVersion::MacOS, 13)) {
        //: This is for the options dialog on older mac versions
        optionsAction->setText(tr("Preference&s..."));
    } else {
        optionsAction->setText(tr("Setting&s..."));
    }
#endif
    actionLibrary.insert("options", optionsAction);

    auto *aboutAction = new QAction(QIcon::fromTheme("help-about"), tr("&About"));
#ifdef Q_OS_MACOS
    //: This is for the about dialog on mac
    aboutAction->setText(tr("&About qView"));
#endif
    actionLibrary.insert("about", aboutAction);

    auto *welcomeAction = new QAction(QIcon::fromTheme("help-faq", QIcon::fromTheme("help-about")), tr("&Welcome"));
    actionLibrary.insert("welcome", welcomeAction);

    //: This is for clearing the recents menu
    auto *clearRecentsAction = new QAction(QIcon::fromTheme("edit-delete"), tr("Clear &Menu"));
    actionLibrary.insert("clearrecents", clearRecentsAction);

    //: Open with other program for unix non-mac
    auto *openWithOtherAction = new QAction(QIcon::fromTheme("system-run"), tr("Other Application..."));
#ifdef Q_OS_WIN
    //: Open with other program for windows
    openWithOtherAction->setText(tr("Choose another app"));
#elif defined Q_OS_MACOS
    //: Open with other program for macos
    openWithOtherAction->setText(tr("Other..."));
#endif
    actionLibrary.insert("openwithother", openWithOtherAction);

    // Set data values and disable actions
    const auto keys = actionLibrary.keys();
    for (const auto &key : keys)
    {
        auto *value = actionLibrary.value(key);
        auto data = value->data().toStringList();
        data.prepend(key);
        value->setData(data);

        if (data.last().contains("disable"))
            value->setEnabled(false);
    }
}

QIcon ActionManager::getCacheableIcon(const QString &cacheKey, const QIcon &icon)
{
    static QMutex mutex;
    static QCache<QString, QIcon> cache;
    QMutexLocker locker(&mutex);
    QIcon *cacheEntry = cache.take(cacheKey);
    if (cacheEntry == nullptr)
    {
        cacheEntry = new QIcon();
        // Depending on the source icon's implementation (e.g. if it's backed by a file engine), it may
        // not allow pixmap caching, so get a pixmap of each size once and copy it to a generic QIcon
        for (const auto &iconSize : icon.availableSizes())
            cacheEntry->addPixmap(icon.pixmap(iconSize));
    }
    QIcon cacheableIcon = *cacheEntry;
    cache.insert(cacheKey, cacheEntry);
    return cacheableIcon;
}

bool ActionManager::hasAncestor(QObject *object, QObject *ancestor)
{
    while (object)
    {
        if (object == ancestor)
            return true;
        object = object->parent();
    }
    return false;
}
