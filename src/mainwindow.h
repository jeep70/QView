#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "qvinfodialog.h"
#include "qvimagecore.h"
#include "qvgraphicsview.h"
#include "openwith.h"

#include <QMainWindow>
#include <QShortcut>
#include <QNetworkAccessManager>
#include <QStack>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    struct DeletedPaths
    {
        QString pathInTrash;
        QString previousPath;
    };

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void requestPopulateOpenWithMenu();

    void populateOpenWithMenu(const QList<OpenWith::OpenWithItem> openWithItems);

    void refreshProperties();

    void buildWindowTitle();

    void updateWindowFilePath();

    bool getTitlebarHidden() const;

    void setTitlebarHidden(const bool hide);

    void setWindowSize();

    bool getIsPixmapLoaded() const;

    void setJustLaunchedWithImage(bool value);

    QScreen *screenContaining(const QRect &rect);

    void openRecent(int i);

    void openUrl(const QUrl &url);

    void pickUrl();

    void reloadFile();

    void openContainingFolder();

    void openWith(const OpenWith::OpenWithItem &exec);

    void showFileInfo();

    void askDeleteFile(bool permanent);

    void deleteFile(bool permanent);

    QString deleteFileLinuxFallback(const QString &path, bool putBack);

    void undoDelete();

    void copy();

    void paste();

    void rename();

    void zoomIn();

    void zoomOut();

    void resetZoom();

    void originalSize();

    void rotateRight();

    void rotateLeft();

    void mirror();

    void flip();

    void firstFile();

    void previousFile();

    void nextFile();

    void lastFile();

    void saveFrameAs();

    void pause();

    void nextFrame();

    void decreaseSpeed();

    void resetSpeed();

    void increaseSpeed();

    void toggleFullScreen();

    void toggleTitlebarHidden();

    int getTitlebarOverlap() const;

    const QVImageCore::FileDetails& getCurrentFileDetails() const { return graphicsView->getCurrentFileDetails(); }

public slots:
    void openFile(const QString &fileName);

    void toggleSlideshow();

    void slideshowAction();

    void cancelSlideshow();

    void fileChanged();

    void disableActions();

protected:
    bool event(QEvent *event) override;

    void contextMenuEvent(QContextMenuEvent *event) override;

    void showEvent(QShowEvent *event) override;

    void closeEvent(QCloseEvent *event) override;

    void changeEvent(QEvent *event) override;

    void mousePressEvent(QMouseEvent *event) override;

    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void paintEvent(QPaintEvent *event) override;

    void fullscreenChanged();

protected slots:
    void settingsUpdated();
    void shortcutsUpdated();

private:
    Ui::MainWindow *ui;
    QVGraphicsView *graphicsView;

    QMenu *contextMenu;
    QMenu *virtualMenu;

    QTimer *slideshowTimer;

    QShortcut *escShortcut;

    QVInfoDialog *info;

    QColor customBackgroundColor;

    bool justLaunchedWithImage;

    Qt::WindowStates storedWindowState;
    bool storedTitlebarHidden;

    QNetworkAccessManager networkAccessManager;

    QStack<DeletedPaths> lastDeletedFiles;

    QTimer *populateOpenWithTimer;
    QFutureWatcher<QList<OpenWith::OpenWithItem>> openWithFutureWatcher;
};

#endif // MAINWINDOW_H
