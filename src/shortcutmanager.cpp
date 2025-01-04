#include "shortcutmanager.h"
#include "qvapplication.h"
#include "actionmanager.h"

#include <QSettings>

ShortcutManager::ShortcutManager(QObject *parent) : QObject(parent)
{
    initializeShortcutsList();
    updateShortcuts();
    hideShortcuts();
}

void ShortcutManager::updateShortcuts()
{
    QSettings settings;
    settings.beginGroup("shortcuts");

    // Set all shortcuts to the user-set shortcut or the default
    for (auto &shortcut : shortcutsList)
    {
        shortcut.shortcuts = settings.value(shortcut.name, shortcut.defaultShortcuts).toStringList();
    }

    // Set all action shortcuts now that the shortcuts have changed
    const auto &actionManager = qvApp->getActionManager();
    for (const auto &shortcut : getShortcutsList())
    {
        const auto actionList = actionManager.getAllInstancesOfAction(shortcut.name);

        for (const auto &action : actionList)
        {
            if (action)
                action->setShortcuts(stringListToKeySequenceList(shortcut.shortcuts));
        }
    }

    hideShortcuts();

    emit shortcutsUpdated();
}

void ShortcutManager::initializeShortcutsList()
{
    shortcutsList.append({tr("Open"), "open", keyBindingsToStringList(QKeySequence::Open), {}});
    shortcutsList.append({tr("Open URL"), "openurl", QStringList(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O).toString()), {}});
    shortcutsList.append({tr("Reload File"), "reloadfile", keyBindingsToStringList(QKeySequence::Refresh), {}});
    shortcutsList.append({tr("Open Containing Folder"), "opencontainingfolder", {}, {}});
    //Sets open containing folder action name to platform-appropriate alternative
#ifdef Q_OS_WIN
    shortcutsList.last().readableName = tr("Show in Explorer");
#elif defined Q_OS_MACOS
    shortcutsList.last().readableName  = tr("Show in Finder");
#endif
    shortcutsList.append({tr("Show File Info"), "showfileinfo", QStringList(QKeySequence(Qt::Key_I).toString()), {}});
    shortcutsList.append({tr("Restore from Trash"), "undo", keyBindingsToStringList(QKeySequence::Undo), {}});
#ifdef Q_OS_WIN
    shortcutsList.last().readableName = tr("Undo Delete");
#endif
    shortcutsList.append({tr("Copy"), "copy", keyBindingsToStringList(QKeySequence::Copy), {}});
    shortcutsList.append({tr("Paste"), "paste", keyBindingsToStringList(QKeySequence::Paste), {}});
    shortcutsList.append({tr("Rename"), "rename", QStringList(QKeySequence(Qt::Key_F2).toString()), {}});
    // ctrl+r for renaming, unless it conflicts with refresh (i.e. reload file)
    if (!QKeySequence::keyBindings(QKeySequence::Refresh).contains(QKeySequence(Qt::CTRL | Qt::Key_R)))
        shortcutsList.last().defaultShortcuts << QKeySequence(Qt::CTRL | Qt::Key_R).toString();
    // cmd+enter for renaming, mac-style
    shortcutsList.last().defaultShortcuts.prepend(QKeySequence(Qt::CTRL | Qt::Key_Return).toString());

    shortcutsList.append({tr("Move to Trash"), "delete", keyBindingsToStringList(QKeySequence::Delete), {}});
    // cmd+backspace for deleting, mac-style
    shortcutsList.last().defaultShortcuts.prepend(QKeySequence(Qt::CTRL | Qt::Key_Backspace).toString());
#ifdef Q_OS_WIN
    shortcutsList.last().readableName = tr("Delete");
#endif
    shortcutsList.append({tr("Delete Permanently"), "deletepermanent", QStringList(QKeySequence(Qt::SHIFT | Qt::Key_Delete).toString()), {}});
#ifdef Q_OS_MACOS
    // cmd+option+backspace
    shortcutsList.last().defaultShortcuts.prepend(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_Backspace).toString());
#endif
    shortcutsList.append({tr("First File"), "firstfile", QStringList(QKeySequence(Qt::Key_Home).toString()), {}});
    shortcutsList.append({tr("Previous File"), "previousfile", QStringList(QKeySequence(Qt::Key_Left).toString()), {}});
    shortcutsList.append({tr("Next File"), "nextfile", QStringList(QKeySequence(Qt::Key_Right).toString()), {}});
    shortcutsList.append({tr("Last File"), "lastfile", QStringList(QKeySequence(Qt::Key_End).toString()), {}});
    shortcutsList.append({tr("Zoom In"), "zoomin", keyBindingsToStringList(QKeySequence::ZoomIn), {}});
    // Allow zooming with Ctrl + plus like a regular person (without holding shift)
    if (!shortcutsList.last().defaultShortcuts.contains(QKeySequence(Qt::CTRL | Qt::Key_Equal).toString()))
        shortcutsList.last().defaultShortcuts << QKeySequence(Qt::CTRL | Qt::Key_Equal).toString();

    shortcutsList.append({tr("Zoom Out"), "zoomout", keyBindingsToStringList(QKeySequence::ZoomOut), {}});
    shortcutsList.append({tr("Reset Zoom"), "resetzoom", QStringList(QKeySequence(Qt::CTRL | Qt::Key_0).toString()), {}});
    shortcutsList.append({tr("Original Size"), "originalsize", QStringList(QKeySequence(Qt::Key_O).toString()), {}});
    shortcutsList.append({tr("Rotate Right"), "rotateright", QStringList(QKeySequence(Qt::Key_Up).toString()), {}});
    shortcutsList.append({tr("Rotate Left"), "rotateleft", QStringList(QKeySequence(Qt::Key_Down).toString()), {}});
    shortcutsList.append({tr("Mirror"), "mirror", QStringList(QKeySequence(Qt::Key_F).toString()), {}});
    shortcutsList.append({tr("Flip"), "flip", QStringList(QKeySequence(Qt::CTRL | Qt::Key_F).toString()), {}});
    if (qvApp->supportsTitlebarHiding())
        shortcutsList.append({tr("Toggle Titlebar Hidden"), "toggletitlebar", {}, {}});
    shortcutsList.append({tr("Full Screen"), "fullscreen", keyBindingsToStringList(QKeySequence::FullScreen), {}});
    //Fixes alt+enter only working with numpad enter when using qt's standard keybinds
#ifdef Q_OS_WIN
    shortcutsList.last().defaultShortcuts << QKeySequence(Qt::ALT | Qt::Key_Return).toString();
#elif defined Q_OS_UNIX & !defined Q_OS_MACOS
    // F11 is for some reason not there by default in GNOME
    if (shortcutsList.last().defaultShortcuts.contains(QKeySequence(Qt::CTRL | Qt::Key_F11).toString()) &&
        !shortcutsList.last().defaultShortcuts.contains(QKeySequence(Qt::Key_F11).toString()))
    {
        shortcutsList.last().defaultShortcuts << QKeySequence(Qt::Key_F11).toString();
    }
#endif
    shortcutsList.append({tr("Save Frame As"), "saveframeas", keyBindingsToStringList(QKeySequence::Save), {}});
    shortcutsList.append({tr("Pause"), "pause", QStringList(QKeySequence(Qt::Key_P).toString()), {}});
    shortcutsList.append({tr("Next Frame"), "nextframe", QStringList(QKeySequence(Qt::Key_N).toString()), {}});
    shortcutsList.append({tr("Decrease Speed"), "decreasespeed", QStringList(QKeySequence(Qt::Key_BracketLeft).toString()), {}});
    shortcutsList.append({tr("Reset Speed"), "resetspeed", QStringList(QKeySequence(Qt::Key_Backslash).toString()), {}});
    shortcutsList.append({tr("Increase Speed"), "increasespeed", QStringList(QKeySequence(Qt::Key_BracketRight).toString()), {}});
    shortcutsList.append({tr("Toggle Slideshow"), "slideshow", {}, {}});
    shortcutsList.append({tr("Settings"), "options", keyBindingsToStringList(QKeySequence::Preferences), {}});
    if (QOperatingSystemVersion::current() < QOperatingSystemVersion(QOperatingSystemVersion::MacOS, 13)) {
        shortcutsList.last().readableName = tr("Preferences");
    }
    // mac exclusive shortcuts
#ifdef Q_OS_MACOS
    shortcutsList.append({tr("New Window"), "newwindow", keyBindingsToStringList(QKeySequence::New), {}});
    shortcutsList.append({tr("Close Window"), "closewindow", QStringList(QKeySequence(Qt::CTRL | Qt::Key_W).toString()), {}});
    shortcutsList.append({tr("Close All"), "closeallwindows", QStringList(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_W).toString()), {}});
#endif
    shortcutsList.append({tr("Quit"), "quit", keyBindingsToStringList(QKeySequence::Quit), {}});
#ifndef Q_OS_MACOS
    shortcutsList.last().defaultShortcuts << QKeySequence(Qt::CTRL | Qt::Key_W).toString();
#endif
#ifdef Q_OS_WIN
    shortcutsList.last().readableName = tr("Exit");
#endif
}

void ShortcutManager::hideShortcuts()
{
    QMutableListIterator<SShortcut> i(shortcutsList);
    while (i.hasNext())
    {
        if (hiddenShortcuts.contains(i.next().name))
        {
            i.remove();
        }
    }
}

void ShortcutManager::setShortcutHidden(const QString &shortcut)
{
    hiddenShortcuts.append(shortcut);
    hideShortcuts();
}

void ShortcutManager::setShortcutsHidden(const QStringList &shortcuts)
{
    hiddenShortcuts.append(shortcuts);
    hideShortcuts();
}
