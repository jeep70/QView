#include "actionmanager.h"
#include "qvcocoafunctions.h"

#include <QUrl>
#include <QDebug>
#include <QFileIconProvider>
#include <QCollator>

#import <Cocoa/Cocoa.h>


static void fixNativeMenuEccentricities(QMenu *menu, NSMenu *nativeMenu)
{
    // Stop menu items with no actions being disabled automatically
    [nativeMenu setAutoenablesItems:false];
    int i = 0;
    for (NSMenuItem *item in nativeMenu.itemArray)
    {
        // Set menu items as disabled again (setAutoenablesItems resets them all to enabled)
        [item setEnabled:menu->actions().value(i)->isEnabled()];
        // Update each item so the submenus actually show up
        [nativeMenu.delegate menu:nativeMenu updateItem:item atIndex:0 shouldCancel:false];
        // Hide shortcuts from menu as is typical for context menus
        [item setKeyEquivalent:@""];
        if (item.hasSubmenu)
        {
            // Stop items with submenus from being clickable
            [item setAction:nil];

            // Do all this stuff for all items within submenu
            fixNativeMenuEccentricities(menu->actions().value(i)->menu(), item.submenu);
        }
        i++;
    }
}

void QVCocoaFunctions::showMenu(QMenu *menu, const QPoint &point, QWindow *window)
{
    auto *view = reinterpret_cast<NSView*>(window->winId());

    NSMenu *nativeMenu = menu->toNSMenu();
    fixNativeMenuEccentricities(menu, nativeMenu);

    NSPoint transposedPoint = QPoint(point.x(), static_cast<int>(view.frame.size.height)-point.y()).toCGPoint();
    NSGraphicsContext *graphicsContext = [NSGraphicsContext currentContext];

    // Synthesize event to open menu
    NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown location:transposedPoint modifierFlags:0
            timestamp:0 windowNumber:view.window.windowNumber context:graphicsContext eventNumber:0 clickCount:0 pressure:1];
    [NSMenu popUpContextMenu:nativeMenu withEvent:event forView:view];

    // Send left and right up events to replace ones that aren't sent automatically
    NSEvent *eventRightUp = [NSEvent mouseEventWithType:NSEventTypeRightMouseUp location:transposedPoint modifierFlags:0
            timestamp:0 windowNumber:view.window.windowNumber context:graphicsContext eventNumber:0 clickCount:0 pressure:1];
    [view rightMouseUp:eventRightUp];

    NSEvent *eventLeftUp = [NSEvent mouseEventWithType:NSEventTypeLeftMouseUp location:transposedPoint modifierFlags:0
            timestamp:0 windowNumber:view.window.windowNumber context:graphicsContext eventNumber:0 clickCount:0 pressure:1];
    [view mouseUp:eventLeftUp];
}

void QVCocoaFunctions::setUserDefaults()
{
    [[NSUserDefaults standardUserDefaults] setBool:NO forKey:@"NSFullScreenMenuItemEverywhere"];
}

// This function should only be enabled once because it sets observers
void QVCocoaFunctions::setFullSizeContentView(QWindow *window, const bool enable)
{
    auto *view = reinterpret_cast<NSView*>(window->winId());

    // Make sure the requested state isn't already in effect
    if (enable == (view.window.styleMask & NSWindowStyleMaskFullSizeContentView))
        return;

    // Changing the style mask causes the window to resize, so snapshot the original size
    NSRect originalFrame = view.window.frame;

    if (enable)
    {
        // Proceed only if this Qt and macOS version combination is already using layer-backed view
        if (!view.wantsLayer)
            return;
        view.window.styleMask |= NSWindowStyleMaskFullSizeContentView;
    }
    else
    {
        view.window.styleMask &= ~NSWindowStyleMaskFullSizeContentView;
    }

    // Restore original size after style mask change
    [view.window setFrame:originalFrame display:YES];

#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
    // workaround for QTBUG-69975
    if (enable)
    {
        [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidExitFullScreenNotification object:view.window queue:nil usingBlock:^(NSNotification *notification){
            auto *window = reinterpret_cast<NSWindow*>(notification.object);
            window.styleMask |= NSWindowStyleMaskFullSizeContentView;
        }];

        [[NSNotificationCenter defaultCenter] addObserverForName:NSWindowDidEnterFullScreenNotification object:view.window queue:nil usingBlock:^(NSNotification *notification){
            auto *window = reinterpret_cast<NSWindow*>(notification.object);
            window.styleMask |= NSWindowStyleMaskFullSizeContentView;
        }];
    }
#endif
}

bool QVCocoaFunctions::getTitlebarHidden(const QWidget *window)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    return window->windowFlags().testFlags(Qt::NoTitleBarBackgroundHint);
#else
    auto *view = reinterpret_cast<NSView*>(window->winId());
    return view.window.titleVisibility == NSWindowTitleHidden;
#endif
}

void QVCocoaFunctions::setTitlebarHidden(QWidget *window, const bool hide)
{
    auto *view = reinterpret_cast<NSView*>(window->winId());
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    Qt::WindowFlags newFlags = window->windowFlags().setFlag(Qt::NoTitleBarBackgroundHint, hide);
    window->overrideWindowFlags(newFlags);
    window->windowHandle()->setFlags(newFlags);
#else
    view.window.titlebarAppearsTransparent = hide;
#endif
    view.window.titleVisibility = hide ? NSWindowTitleHidden : NSWindowTitleVisible;
}

void QVCocoaFunctions::setVibrancy(bool alwaysDark, QWindow *window)
{
    auto *view = reinterpret_cast<NSView*>(window->winId());

    if (alwaysDark)
    {

        [view.window setAppearance: [NSAppearance appearanceNamed:NSAppearanceNameVibrantDark]];
    }
    else
    {
        [view.window setAppearance: nil];
    }
}

int QVCocoaFunctions::getObscuredHeight(QWindow *window)
{
    if (!window)
        return 0;

    auto *view = reinterpret_cast<NSView*>(window->winId());

    if (view.window.titlebarAppearsTransparent)
        return 0;

    int visibleHeight = view.window.contentLayoutRect.size.height;
    int totalHeight = view.window.contentView.frame.size.height;

    return totalHeight - visibleHeight;
}

void QVCocoaFunctions::closeWindow(QWindow *window)
{
    auto *view = reinterpret_cast<NSView*>(window->winId());
    [view.window close];
}

void QVCocoaFunctions::setWindowMenu(QMenu *menu)
{
    NSMenu *nativeMenu = menu->toNSMenu();
    [nativeMenu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];

    [nativeMenu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];
    [[NSApplication sharedApplication] setWindowsMenu:nativeMenu];
}

void QVCocoaFunctions::setAlternate(QMenu *menu, int index)
{
    NSMenu *nativeMenu = menu->toNSMenu();
    [[nativeMenu.itemArray objectAtIndex:index] setAlternate:true];
}

void QVCocoaFunctions::setDockRecents(const QStringList &recentPathsList)
{
    NSDocumentController *documentController = [NSDocumentController sharedDocumentController];
    [documentController clearRecentDocuments:documentController];
    for (int i = recentPathsList.size()-1; i >= 0; i--)
    {
        const auto &path = recentPathsList[i];
        auto url = QUrl::fromLocalFile(path);
        [documentController noteNewRecentDocumentURL:url.toNSURL()];
    }
}

QList<OpenWith::OpenWithItem> QVCocoaFunctions::getOpenWithItems(const QString &filePath)
{
    auto fileUrl = QUrl(filePath);
    fileUrl.setScheme("file");

    NSString *utiType = nil;
    NSError *error = nil;
    BOOL success = [fileUrl.toNSURL() getResourceValue:&utiType forKey:NSURLTypeIdentifierKey error:&error];

    if (!success)
    {
        NSLog(@"getResourceValue:forKey:error: returned error == %@", error);
        return QList<OpenWith::OpenWithItem>();
    }


    NSArray *supportedApplications = [(NSArray *)LSCopyAllRoleHandlersForContentType((CFStringRef)utiType, kLSRolesAll) autorelease];
    NSString *defaultApplication = [(NSString *)LSCopyDefaultRoleHandlerForContentType((CFStringRef)utiType, kLSRolesAll) autorelease];

    QList<OpenWith::OpenWithItem> listOfOpenWithItems;
    for (NSString *appId in supportedApplications)
    {
        if ([appId isEqualToString:@"com.qview.qView"] || [appId isEqualToString:@"com.interversehq.qView"])
            continue;

        OpenWith::OpenWithItem openWithItem;
        openWithItem.exec = "open";
        openWithItem.args.append({"-b", QString::fromNSString(appId)});

        NSString *absolutePath = [[NSWorkspace sharedWorkspace] absolutePathForAppBundleWithIdentifier:appId];

        NSString *appName = [[NSFileManager defaultManager] displayNameAtPath:absolutePath];
        openWithItem.name = QString::fromNSString(appName);

        QFileIconProvider fiProvider;
        QIcon icon = fiProvider.icon(QFileInfo(QString::fromNSString(absolutePath)));
        openWithItem.icon = ActionManager::getCacheableIcon("application:" + QString::fromNSString(appId), icon);

        // If the program is the default program, save it to add to the beginning after sorting
        if ([appId isEqualToString:defaultApplication])
        {
            openWithItem.isDefault = true;
            openWithItem.name += QT_TR_NOOP(" (default)");
        }

        listOfOpenWithItems.append(openWithItem);
    }

    return listOfOpenWithItems;
}

QString QVCocoaFunctions::deleteFile(const QString &filePath)
{
    auto fileUrl = QUrl(filePath);
    fileUrl.setScheme("file");

    NSURL *resultUrl = nil;
    NSError *error = nil;
    BOOL success = [[NSFileManager defaultManager] trashItemAtURL:fileUrl.toNSURL() resultingItemURL:&resultUrl error:&error];
    if (!success)
    {
        NSLog(@"trashItemAtUrl:resultingItemUrl:error: returned error == %@", error);
        return "";
    }

    return QString::fromNSString(resultUrl.absoluteString);
}

QByteArray QVCocoaFunctions::getIccProfileForWindow(const QWindow *window)
{
    NSView *view = reinterpret_cast<NSView*>(window->winId());
    NSColorSpace *nsColorSpace = view.window.colorSpace;
    if (nsColorSpace)
    {
        NSData *iccProfileData = nsColorSpace.ICCProfileData;
        if (iccProfileData)
        {
            return QByteArray::fromNSData(iccProfileData);
        }
    }
    return {};
}
