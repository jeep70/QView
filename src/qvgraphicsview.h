#ifndef QVGRAPHICSVIEW_H
#define QVGRAPHICSVIEW_H

#include "qvimagecore.h"
#include "logicalpixelfitter.h"
#include <QGraphicsView>
#include <QImageReader>
#include <QMimeData>
#include <QDir>
#include <QTimer>
#include <QFileInfo>

class MainWindow;

class QVGraphicsView : public QGraphicsView
{
    Q_OBJECT

public:
    QVGraphicsView(QWidget *parent = nullptr);

    enum class GoToFileMode
    {
       constant,
       first,
       previous,
       next,
       last
    };
    Q_ENUM(GoToFileMode)


    QMimeData* getMimeData() const;
    void loadMimeData(const QMimeData *mimeData);
    void loadFile(const QString &fileName);

    void reloadFile();

    void zoomIn(const QPoint &pos = QPoint(-1, -1));

    void zoomOut(const QPoint &pos = QPoint(-1, -1));

    void zoomRelative(const qreal relativeLevel, const QPoint &pos = QPoint(-1, -1));

    void zoomAbsolute(const qreal absoluteLevel, const QPoint &pos = QPoint(-1, -1));

    void applyExpensiveScaling();
    void removeExpensiveScaling();

    void zoomToFit();
    void originalSize();

    void centerImage();

    void goToFile(const GoToFileMode &mode, int index = 0);

    void settingsUpdated();

    void closeImage();
    void jumpToNextFrame();
    void setPaused(const bool &desiredState);
    void setSpeed(const int &desiredSpeed);
    void rotateImage(const int relativeAngle);
    void mirrorImage();
    void flipImage();

    QSizeF getEffectiveOriginalSize() const;

    LogicalPixelFitter getPixelFitter() const;

    const QVImageCore::FileDetails& getCurrentFileDetails() const { return imageCore.getCurrentFileDetails(); }
    const QPixmap& getLoadedPixmap() const { return imageCore.getLoadedPixmap(); }
    const QMovie& getLoadedMovie() const { return imageCore.getLoadedMovie(); }
    qreal getZoomLevel() const { return zoomLevel; }

signals:
    void cancelSlideshow();

    void fileChanged();

    void zoomLevelChanged();

protected:
    void wheelEvent(QWheelEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

    void paintEvent(QPaintEvent *event) override;

    void dropEvent(QDropEvent *event) override;

    void dragEnterEvent(QDragEnterEvent *event) override;

    void dragMoveEvent(QDragMoveEvent *event) override;

    void dragLeaveEvent(QDragLeaveEvent *event) override;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    void enterEvent(QEvent *event) override;
#else
    void enterEvent(QEnterEvent *event) override;
#endif

    void mouseReleaseEvent(QMouseEvent *event) override;

    bool event(QEvent *event) override;

    QRect getContentRect() const;

    QRect getUsableViewportRect() const;

    void setTransformScale(const qreal absoluteScale);

    void setTransformWithNormalization(const QTransform &matrix);

    QTransform getUnspecializedTransform() const;

    QTransform normalizeTransformOrigin(const QTransform &matrix, const QSizeF &pixmapSize) const;

    qreal getDpiAdjustment() const;

    void handleDpiAdjustmentChange();

    MainWindow* getMainWindow() const;

private slots:
    void animatedFrameChanged(QRect rect);

    void postLoad();

private:


    QGraphicsPixmapItem *loadedPixmapItem;

    bool isFilteringEnabled;
    bool isScalingEnabled;
    bool isScalingTwoEnabled;
    bool isPastActualSizeEnabled;
    bool isScrollZoomsEnabled;
    bool isLoopFoldersEnabled;
    bool isCursorZoomEnabled;
    bool isOneToOnePixelSizingEnabled;
    int cropMode;
    qreal zoomMultiplier;

    qreal zoomLevel;
    qreal appliedDpiAdjustment;
    qreal appliedExpensiveScaleZoomLevel;
    QPoint lastZoomEventPos;
    QPointF lastZoomRoundingError;
    QPointF lastScrollRoundingError;

    QVImageCore imageCore { this };

    QTimer *expensiveScaleTimer;
};
#endif // QVGRAPHICSVIEW_H
