 #ifndef QVGRAPHICSVIEW_H
#define QVGRAPHICSVIEW_H

#include "mainwindow.h"
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

    enum class scaleMode
    {
       resetScale,
       zoomIn,
       zoomOut
    };
    Q_ENUM(scaleMode)

    enum class goToFileMode
    {
       constant,
       first,
       previous,
       next,
       last
    };
    Q_ENUM(goToFileMode)

    void zoom(int DeltaY, QPoint pos = QPoint());

    void loadMimeData(const QMimeData *mimeData);
    void loadFile(const QString &fileName);
    void updateRecentFiles(const QFileInfo &file);
    void setWindowTitle();

    void resetScale();
    void scaleExpensively(scaleMode mode);
    void originalSize();

    void goToFile(const goToFileMode mode, const int index = 0);

    void jumpToNextFrame();
    void setPaused(bool desiredState);
    void setSpeed(int desiredSpeed);

    qreal getCurrentScale() const;
    void setCurrentScale(const qreal &value);

    bool getIsFilteringEnabled() const;
    void setIsFilteringEnabled(bool value);

    bool getIsScalingEnabled() const;
    void setIsScalingEnabled(bool value);

    int getTitlebarMode() const;
    void setTitlebarMode(int value);

    int getCropMode() const;
    void setCropMode(int value);

    qreal getScaleFactor() const;
    void setScaleFactor(const qreal &value);

    bool getIsScalingTwoEnabled() const;
    void setIsScalingTwoEnabled(bool value);

    bool getIsResetOnResizeEnabled() const;
    void setIsResetOnResizeEnabled(bool value);

    bool getIsPastActualSizeEnabled() const;
    void setIsPastActualSizeEnabled(bool value);

    const QVImageCore::QVFileDetails& getCurrentFileDetails() const;
    const QPixmap& getLoadedPixmap() const;
    const QMovie& getLoadedMovie() const;

protected:
    void wheelEvent(QWheelEvent *event) override;

    void resizeEvent(QResizeEvent *event) override;

    void dropEvent(QDropEvent *event) override;

    void dragEnterEvent(QDragEnterEvent *event) override;

    void dragMoveEvent(QDragMoveEvent *event) override;

    void dragLeaveEvent(QDragLeaveEvent *event) override;

    void enterEvent(QEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

    void fitInViewMarginless(bool setVariables = true);


private slots:
    void timerExpired();

    void animatedFrameChanged(QRect rect);

private:

    MainWindow *parentMainWindow;

    qreal currentScale;
    qreal scaleFactor;
    qreal fittedWidth;
    qreal fittedHeight;
    bool isOriginalSize;

    QGraphicsPixmapItem *loadedPixmapItem;
    QRectF alternateBoundingBox;

    QTransform fittedMatrix;
    QTransform scaledMatrix;
    QTimer *timer;

    bool movieCenterNeedsUpdating;
    bool isFilteringEnabled;
    bool isScalingEnabled;
    int titlebarMode;
    int cropMode;
    bool isScalingTwoEnabled;
    bool isResetOnResizeEnabled;
    bool isPastActualSizeEnabled;

    qreal maxScalingTwoSize;
    bool cheapScaledLast;

    const QStringList filterList = (QStringList() << "*.bmp" << "*.cur" << "*.gif" << "*.icns" << "*.ico" << "*.jp2" << "*.jpeg" << "*.jpe" << "*.jpg" << "*.mng" << "*.pbm" << "*.pgm" << "*.png" << "*.ppm" << "*.svg" << "*.svgz" << "*.tif" << "*.tiff" << "*.wbmp" << "*.webp" << "*.xbm" << "*.xpm");

    QVImageCore imageCore;
};
#endif // QVGRAPHICSVIEW_H
