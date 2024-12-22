#ifndef QVGRAPHICSVIEW_H
#define QVGRAPHICSVIEW_H

#include "qvimagecore.h"
#include <QGraphicsView>
#include <QImageReader>
#include <QMimeData>
#include <QDir>
#include <QTimer>
#include <QFileInfo>

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

    const QVImageCore::FileDetails& getCurrentFileDetails() const { return imageCore.getCurrentFileDetails(); }
    const QPixmap& getLoadedPixmap() const { return imageCore.getLoadedPixmap(); }
    const QMovie& getLoadedMovie() const { return imageCore.getLoadedMovie(); }
    qreal getZoomLevel() const { return zoomLevel; }

    static int roundToCompleteLogicalPixel(const qreal value, const qreal logicalScale);

    static qreal reverseLogicalPixelRounding(const int value, const qreal logicalScale);

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

    bool isSmoothScalingRequested() const;

    bool isExpensiveScalingRequested() const;

    QRect getContentRect() const;

    QRect getUsableViewportRect() const;

    void setTransformScale(qreal absoluteScale);

    QTransform getTransformWithNoScaling() const;

    qreal getDpiAdjustment() const;

    void handleDpiAdjustmentChange();

    void handleSmoothScalingChange();

private slots:
    void animatedFrameChanged(QRect rect);

    void postLoad();

private:


    QGraphicsPixmapItem *loadedPixmapItem;

    int smoothScalingMode;
    bool expensiveScalingAboveWindowSize;
    qreal smoothScalingLimit;
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
