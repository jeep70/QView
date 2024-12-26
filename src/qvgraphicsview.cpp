#include "qvgraphicsview.h"
#include "qvapplication.h"
#include "qvinfodialog.h"
#include "qvcocoafunctions.h"
#include <QWheelEvent>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QSettings>
#include <QMessageBox>
#include <QMovie>
#include <QtMath>
#include <QGestureEvent>
#include <QScrollBar>

QVGraphicsView::QVGraphicsView(QWidget *parent) : QGraphicsView(parent)
{
    // GraphicsView setup
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setDragMode(QGraphicsView::ScrollHandDrag);
    setFrameShape(QFrame::NoFrame);
    setTransformationAnchor(QGraphicsView::NoAnchor);
    viewport()->setAutoFillBackground(false);

    // part of a pathetic attempt at gesture support
    grabGesture(Qt::PinchGesture);

    // Scene setup
    auto *scene = new QGraphicsScene(-1000000.0, -1000000.0, 2000000.0, 2000000.0, this);
    setScene(scene);

    // Initialize configurable variables
    smoothScalingMode = 2;
    expensiveScalingAboveWindowSize = true;
    smoothScalingLimit = 0;
    isPastActualSizeEnabled = true;
    isScrollZoomsEnabled = true;
    isLoopFoldersEnabled = true;
    isCursorZoomEnabled = true;
    isOneToOnePixelSizingEnabled = true;
    cropMode = 0;
    zoomMultiplier = 1.25;

    // Initialize other variables
    zoomLevel = 1.0;
    appliedDpiAdjustment = 1.0;
    appliedExpensiveScaleZoomLevel = 0.0;
    lastZoomEventPos = QPoint(-1, -1);
    lastZoomRoundingError = QPointF();
    lastScrollRoundingError = QPointF();

    connect(&imageCore, &QVImageCore::animatedFrameChanged, this, &QVGraphicsView::animatedFrameChanged);
    connect(&imageCore, &QVImageCore::fileChanged, this, &QVGraphicsView::postLoad);

    expensiveScaleTimer = new QTimer(this);
    expensiveScaleTimer->setSingleShot(true);
    expensiveScaleTimer->setInterval(50);
    connect(expensiveScaleTimer, &QTimer::timeout, this, [this]{applyExpensiveScaling();});

    loadedPixmapItem = new QGraphicsPixmapItem();
    scene->addItem(loadedPixmapItem);

    // Connect to settings signal
    connect(&qvApp->getSettingsManager(), &SettingsManager::settingsUpdated, this, &QVGraphicsView::settingsUpdated);
    settingsUpdated();
}


// Events

void QVGraphicsView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    zoomToFit();
}

void QVGraphicsView::paintEvent(QPaintEvent *event)
{
    // This is the most reliable place to detect DPI changes. QWindow::screenChanged()
    // doesn't detect when the DPI is changed on the current monitor, for example.
    handleDpiAdjustmentChange();

    QGraphicsView::paintEvent(event);
}

void QVGraphicsView::dropEvent(QDropEvent *event)
{
    QGraphicsView::dropEvent(event);
    loadMimeData(event->mimeData());
}

void QVGraphicsView::dragEnterEvent(QDragEnterEvent *event)
{
    QGraphicsView::dragEnterEvent(event);
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void QVGraphicsView::dragMoveEvent(QDragMoveEvent *event)
{
    QGraphicsView::dragMoveEvent(event);
    event->acceptProposedAction();
}

void QVGraphicsView::dragLeaveEvent(QDragLeaveEvent *event)
{
    QGraphicsView::dragLeaveEvent(event);
    event->accept();
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
void QVGraphicsView::enterEvent(QEvent *event)
#else
void QVGraphicsView::enterEvent(QEnterEvent *event)
#endif
{
    QGraphicsView::enterEvent(event);
    viewport()->setCursor(Qt::ArrowCursor);
}

void QVGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    QGraphicsView::mouseReleaseEvent(event);
    viewport()->setCursor(Qt::ArrowCursor);
}

bool QVGraphicsView::event(QEvent *event)
{
    //this is for touchpad pinch gestures
    if (event->type() == QEvent::Gesture)
    {
        auto *gestureEvent = static_cast<QGestureEvent*>(event);
        if (QGesture *pinch = gestureEvent->gesture(Qt::PinchGesture))
        {
            auto *pinchGesture = static_cast<QPinchGesture *>(pinch);
            QPinchGesture::ChangeFlags changeFlags = pinchGesture->changeFlags();

            if (changeFlags & QPinchGesture::ScaleFactorChanged) {
                const QPoint hotPoint = mapFromGlobal(pinchGesture->hotSpot().toPoint());
                zoomRelative(pinchGesture->scaleFactor(), hotPoint);
            }

            // Fun rotation stuff maybe later
//            if (changeFlags & QPinchGesture::RotationAngleChanged) {
//                qreal rotationDelta = pinchGesture->rotationAngle() - pinchGesture->lastRotationAngle();
//                rotate(rotationDelta);
//                centerImage();
//            }
            return true;
        }
    }
    return QGraphicsView::event(event);
}

void QVGraphicsView::wheelEvent(QWheelEvent *event)
{
    #if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    const QPoint eventPos = event->position().toPoint();
    #else
    const QPoint eventPos = event->pos();
    #endif

    //Basically, if you are holding ctrl then it scrolls instead of zooms (the shift bit is for horizontal scrolling)
    bool willZoom = isScrollZoomsEnabled;
    if (event->modifiers() == Qt::ControlModifier || event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier))
        willZoom = !willZoom;

    if (!willZoom)
    {
        const qreal scrollDivisor = 2.0; // To make scrolling less sensitive
        qreal scrollX = event->angleDelta().x() * (isRightToLeft() ? 1 : -1) / scrollDivisor;
        qreal scrollY = event->angleDelta().y() * -1 / scrollDivisor;

        if (event->modifiers() & Qt::ShiftModifier)
            std::swap(scrollX, scrollY);

        QPointF targetScrollDelta = QPointF(scrollX, scrollY) - lastScrollRoundingError;
        QPoint roundedScrollDelta = targetScrollDelta.toPoint();

        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + roundedScrollDelta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() + roundedScrollDelta.y());

        lastScrollRoundingError = roundedScrollDelta - targetScrollDelta;

        return;
    }

    const int yDelta = event->angleDelta().y();
    const qreal yScale = 120.0;

    if (yDelta == 0 || !getCurrentFileDetails().isPixmapLoaded)
        return;

    const qreal fractionalWheelClicks = qFabs(yDelta) / yScale;
    const qreal zoomAmountPerWheelClick = zoomMultiplier - 1.0;
    qreal zoomFactor = 1.0 + (fractionalWheelClicks * zoomAmountPerWheelClick);

    if (yDelta < 0)
        zoomFactor = qPow(zoomFactor, -1);

    zoomRelative(zoomFactor, eventPos);
}

// Functions

QMimeData *QVGraphicsView::getMimeData() const
{
    auto *mimeData = new QMimeData();
    if (!getCurrentFileDetails().isPixmapLoaded)
        return mimeData;

    mimeData->setUrls({QUrl::fromLocalFile(imageCore.getCurrentFileDetails().fileInfo.absoluteFilePath())});
    mimeData->setImageData(imageCore.getLoadedPixmap().toImage());
    return mimeData;
}

void QVGraphicsView::loadMimeData(const QMimeData *mimeData)
{
    if (mimeData == nullptr)
        return;

    if (!mimeData->hasUrls())
        return;

    const QList<QUrl> urlList = mimeData->urls();

    bool first = true;
    for (const auto &url : urlList)
    {
        if (first)
        {
            loadFile(url.toString());
            emit cancelSlideshow();
            first = false;
            continue;
        }
        QVApplication::openFile(url.toString());
    }
}

void QVGraphicsView::loadFile(const QString &fileName)
{
    imageCore.loadFile(fileName);
}

void QVGraphicsView::reloadFile()
{
    if (!getCurrentFileDetails().isPixmapLoaded)
        return;

    imageCore.loadFile(getCurrentFileDetails().fileInfo.absoluteFilePath(), true);
}

void QVGraphicsView::postLoad()
{
    // Set the pixmap to the new image and reset the transform's scale to a known value
    removeExpensiveScaling();

    zoomToFit();

    expensiveScaleTimer->start();

    qvApp->getActionManager().addFileToRecentsList(getCurrentFileDetails().fileInfo);

    emit fileChanged();
}

void QVGraphicsView::zoomIn(const QPoint &pos)
{
    zoomRelative(zoomMultiplier, pos);
}

void QVGraphicsView::zoomOut(const QPoint &pos)
{
    zoomRelative(qPow(zoomMultiplier, -1), pos);
}

void QVGraphicsView::zoomRelative(qreal relativeLevel, const QPoint &pos)
{
    const qreal absoluteLevel = zoomLevel * relativeLevel;

    if (absoluteLevel >= 500 || absoluteLevel <= 0.01)
        return;

    zoomAbsolute(absoluteLevel, pos);
}

void QVGraphicsView::zoomAbsolute(const qreal absoluteLevel, const QPoint &pos)
{
    if (pos != lastZoomEventPos)
    {
        lastZoomEventPos = pos;
        lastZoomRoundingError = QPointF();
    }
    const QPointF scenePos = mapToScene(pos) - lastZoomRoundingError;

    if (appliedExpensiveScaleZoomLevel != 0.0)
    {
        const qreal baseTransformScale = 1.0 / devicePixelRatioF();
        const qreal relativeLevel = absoluteLevel / appliedExpensiveScaleZoomLevel;
        setTransformScale(baseTransformScale * relativeLevel);
    }
    else
    {
        setTransformScale(absoluteLevel / appliedDpiAdjustment);
    }
    zoomLevel = absoluteLevel;

    // If we are zooming in, we have a point to zoom towards, the mouse is on top of the viewport, and cursor zooming is enabled
    const QSize contentSize = getContentRect().size();
    const QSize viewportSize = getUsableViewportRect().size();
    const bool reachedViewportSize = contentSize.width() >= viewportSize.width() || contentSize.height() >= viewportSize.height();
    if (reachedViewportSize && pos != QPoint(-1, -1) && underMouse() && isCursorZoomEnabled)
    {
        const QPointF p1mouse = mapFromScene(scenePos);
        const QPointF move = p1mouse - pos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() + (move.x() * (isRightToLeft() ? -1 : 1)));
        verticalScrollBar()->setValue(verticalScrollBar()->value() + move.y());
        lastZoomRoundingError = mapToScene(pos) - scenePos;
    }
    else
    {
        centerImage();
    }

    handleSmoothScalingChange();

    emit zoomLevelChanged();
}

void QVGraphicsView::applyExpensiveScaling()
{
    if (!isExpensiveScalingRequested())
        return;

    // Calculate scaled resolution
    const qreal dpiAdjustment = getDpiAdjustment();
    const QSizeF mappedSize = QSizeF(getCurrentFileDetails().loadedPixmapSize) * zoomLevel * (devicePixelRatioF() / dpiAdjustment);

    // Set image to scaled version
    loadedPixmapItem->setPixmap(imageCore.scaleExpensively(mappedSize));

    // Set appropriate scale factor
    const qreal newTransformScale = 1.0 / devicePixelRatioF();
    setTransformScale(newTransformScale);
    appliedDpiAdjustment = dpiAdjustment;
    appliedExpensiveScaleZoomLevel = zoomLevel;
}

void QVGraphicsView::removeExpensiveScaling()
{
    // Return to original size
    if (getCurrentFileDetails().isMovieLoaded)
        loadedPixmapItem->setPixmap(getLoadedMovie().currentPixmap());
    else
        loadedPixmapItem->setPixmap(getLoadedPixmap());

    // Set appropriate scale factor
    const qreal dpiAdjustment = getDpiAdjustment();
    const qreal newTransformScale = zoomLevel / dpiAdjustment;
    setTransformScale(newTransformScale);
    appliedDpiAdjustment = dpiAdjustment;
    appliedExpensiveScaleZoomLevel = 0.0;
}

void QVGraphicsView::animatedFrameChanged(QRect rect)
{
    Q_UNUSED(rect)

    if (isExpensiveScalingRequested())
    {
        applyExpensiveScaling();
    }
    else
    {
        loadedPixmapItem->setPixmap(getLoadedMovie().currentPixmap());
    }
}

void QVGraphicsView::zoomToFit()
{
    if (!getCurrentFileDetails().isPixmapLoaded)
        return;

    const QSizeF imageSize = getEffectiveOriginalSize();
    const QSize viewSize = getUsableViewportRect().size();

    if (viewSize.isEmpty())
        return;

    const qreal logicalPixelScale = devicePixelRatioF();
    const auto gvRound = [logicalPixelScale](const qreal value) {
        return roundToCompleteLogicalPixel(value, logicalPixelScale);
    };
    const auto gvReverseRound = [logicalPixelScale](const int value) {
        return reverseLogicalPixelRounding(value, logicalPixelScale);
    };

    const qreal fitXRatio = gvReverseRound(viewSize.width()) / imageSize.width();
    const qreal fitYRatio = gvReverseRound(viewSize.height()) / imageSize.height();

    // Each mode will check if the rounded image size already produces the desired fit,
    // in which case we can use exactly 1.0 to avoid unnecessary scaling
    const int imageOverflowX = gvRound(imageSize.width()) - viewSize.width();
    const int imageOverflowY = gvRound(imageSize.height()) - viewSize.height();

    qreal targetRatio;

    switch (cropMode) { // should be enum tbh
    case 1: // only take into account height
        if (imageOverflowY == 0)
            targetRatio = 1.0;
        else
            targetRatio = fitYRatio;
        break;
    case 2: // only take into account width
        if (imageOverflowX == 0)
            targetRatio = 1.0;
        else
            targetRatio = fitXRatio;
        break;
    default:
        // In rare cases, if the window sizing code just barely increased the size to enforce
        // the minimum and intends for a tiny upscale to occur (e.g. to 100.3%), that could get
        // misdetected as the special case for 1.0 here and leave an unintentional 1 pixel
        // border. So if we match on only one dimension, make sure the other dimension will have
        // at least a few pixels of border showing.
        if ((imageOverflowX == 0 && (imageOverflowY == 0 || imageOverflowY <= -2)) ||
            (imageOverflowY == 0 && (imageOverflowX == 0 || imageOverflowX <= -2)))
        {
            targetRatio = 1.0;
        }
        else
        {
            // If the fit ratios are extremely close, it's possible that both are sufficient to
            // contain the image, but one results in the opposing dimension getting rounded down
            // to just under the view size, so use the larger of the two ratios in that case.
            const bool isOverallFitToXRatio = gvRound(imageSize.height() * fitXRatio) == viewSize.height();
            const bool isOverallFitToYRatio = gvRound(imageSize.width() * fitYRatio) == viewSize.width();
            if (isOverallFitToXRatio || isOverallFitToYRatio)
                targetRatio = qMax(fitXRatio, fitYRatio);
            else
                targetRatio = qMin(fitXRatio, fitYRatio);
        }
        break;
    }

    if (targetRatio > 1.0 && !isPastActualSizeEnabled)
        targetRatio = 1.0;

    zoomAbsolute(targetRatio);
}

void QVGraphicsView::originalSize()
{
    const bool originalSizeAsToggle = true;
    if (originalSizeAsToggle && zoomLevel == 1.0)
        zoomToFit();
    else
        zoomAbsolute(1.0);
}

void QVGraphicsView::centerImage()
{
    const QRect viewRect = getUsableViewportRect();
    const QRect contentRect = getContentRect();
    const int hOffset = isRightToLeft() ?
        horizontalScrollBar()->minimum() + horizontalScrollBar()->maximum() - contentRect.left() :
        contentRect.left();
    const int vOffset = contentRect.top() - viewRect.top();
    const int hOverflow = contentRect.width() - viewRect.width();
    const int vOverflow = contentRect.height() - viewRect.height();

    horizontalScrollBar()->setValue(hOffset + (hOverflow / (isRightToLeft() ? -2 : 2)));
    verticalScrollBar()->setValue(vOffset + (vOverflow / 2));
}

void QVGraphicsView::goToFile(const GoToFileMode &mode, int index)
{
    bool shouldRetryFolderInfoUpdate = false;

    // Update folder info only after a little idle time as an optimization for when
    // the user is rapidly navigating through files.
    if (!getCurrentFileDetails().timeSinceLoaded.isValid() || getCurrentFileDetails().timeSinceLoaded.hasExpired(3000))
    {
        // Make sure the file still exists because if it disappears from the file listing we'll lose
        // track of our index within the folder. Use the static 'exists' method to avoid caching.
        // If we skip updating now, flag it for retry later once we locate a new file.
        if (QFile::exists(getCurrentFileDetails().fileInfo.absoluteFilePath()))
            imageCore.updateFolderInfo();
        else
            shouldRetryFolderInfoUpdate = true;
    }

    const auto &fileList = getCurrentFileDetails().folderFileInfoList;
    if (fileList.isEmpty())
        return;

    int newIndex = getCurrentFileDetails().loadedIndexInFolder;
    int searchDirection = 0;

    switch (mode) {
    case GoToFileMode::constant:
    {
        newIndex = index;
        break;
    }
    case GoToFileMode::first:
    {
        newIndex = 0;
        searchDirection = 1;
        break;
    }
    case GoToFileMode::previous:
    {
        if (newIndex == 0)
        {
            if (isLoopFoldersEnabled)
                newIndex = fileList.size()-1;
            else
                emit cancelSlideshow();
        }
        else
            newIndex--;
        searchDirection = -1;
        break;
    }
    case GoToFileMode::next:
    {
        if (fileList.size()-1 == newIndex)
        {
            if (isLoopFoldersEnabled)
                newIndex = 0;
            else
                emit cancelSlideshow();
        }
        else
            newIndex++;
        searchDirection = 1;
        break;
    }
    case GoToFileMode::last:
    {
        newIndex = fileList.size()-1;
        searchDirection = -1;
        break;
    }
    }

    if (searchDirection != 0)
    {
        while (searchDirection == 1 && newIndex < fileList.size()-1 && !QFile::exists(fileList.value(newIndex).absoluteFilePath))
            newIndex++;
        while (searchDirection == -1 && newIndex > 0 && !QFile::exists(fileList.value(newIndex).absoluteFilePath))
            newIndex--;
    }

    const QString nextImageFilePath = fileList.value(newIndex).absoluteFilePath;

    if (!QFile::exists(nextImageFilePath) || nextImageFilePath == getCurrentFileDetails().fileInfo.absoluteFilePath())
        return;

    if (shouldRetryFolderInfoUpdate)
    {
        // If the user just deleted a file through qView, closeImage will have been called which empties
        // currentFileDetails.fileInfo. In this case updateFolderInfo can't infer the directory from
        // fileInfo like it normally does, so we'll explicity pass in the folder here.
        imageCore.updateFolderInfo(QFileInfo(nextImageFilePath).path());
    }

    loadFile(nextImageFilePath);
}

bool QVGraphicsView::isSmoothScalingRequested() const
{
    return smoothScalingMode != 0 && (smoothScalingLimit == 0 || zoomLevel < smoothScalingLimit);
}

bool QVGraphicsView::isExpensiveScalingRequested() const
{
    if (!isSmoothScalingRequested() || smoothScalingMode != 2 || !getCurrentFileDetails().isPixmapLoaded)
        return false;

    // Don't go over the maximum scaling size - small tolerance added to cover rounding errors
    const QSize contentSize = getContentRect().size();
    const QSize maxSize = getUsableViewportRect().size() * (expensiveScalingAboveWindowSize ? 3 : 1) + QSize(2, 2);
    return contentSize.width() <= maxSize.width() && contentSize.height() <= maxSize.height();
}

QSizeF QVGraphicsView::getEffectiveOriginalSize() const
{
    return getTransformWithNoScaling().mapRect(QRectF(QPoint(), getCurrentFileDetails().loadedPixmapSize)).size() / getDpiAdjustment();
}

QRect QVGraphicsView::getContentRect() const
{
    // Avoid using loadedPixmapItem and the active transform because the pixmap may have expensive scaling applied
    // which introduces a rounding error to begin with, and even worse, the error will be magnified if we're in the
    // the process of zooming in and haven't re-applied the expensive scaling yet. If that's the case, callers need
    // to know what the content rect will be once the dust settles rather than what's being temporarily displayed.
    const QRectF loadedPixmapBoundingRect = QRectF(QPoint(), getCurrentFileDetails().loadedPixmapSize);
    const qreal effectiveTransformScale = zoomLevel / appliedDpiAdjustment;
    const QTransform effectiveTransform = getTransformWithNoScaling().scale(effectiveTransformScale, effectiveTransformScale);
    const QRectF contentRect = effectiveTransform.mapRect(loadedPixmapBoundingRect);
    const qreal logicalPixelScale = devicePixelRatioF();
    const auto gvRound = [logicalPixelScale](const qreal value) {
        return roundToCompleteLogicalPixel(qAbs(value), logicalPixelScale) * (value >= 0 ? 1 : -1);
    };
    return QRect(gvRound(contentRect.left()), gvRound(contentRect.top()), gvRound(contentRect.width()), gvRound(contentRect.height()));
}

QRect QVGraphicsView::getUsableViewportRect() const
{
#ifdef COCOA_LOADED
    int obscuredHeight = QVCocoaFunctions::getObscuredHeight(window()->windowHandle());
#else
    int obscuredHeight = 0;
#endif
    QRect rect = viewport()->rect();
    rect.setTop(obscuredHeight);
    return rect;
}

void QVGraphicsView::setTransformScale(qreal value)
{
#ifndef Q_OS_MACOS
    // If fractional display scaling is in use, when attempting to target a given size, the resulting error
    // can be [0,1) unlike the typical [0,0.5] without scaling or integer scaling. This is because the
    // image origin which is always at an integer logical pixel becomes potentially a fractional physical
    // pixel due to the display scaling, adding to the overall error. As a result, tiny rounding errors can
    // cause us to miss the size we were targetting, so increase the scale just a hair to compensate.
    if (value != std::floor(value))
        value *= 1.0 + std::numeric_limits<double>::epsilon();
#endif
    setTransform(getTransformWithNoScaling().scale(value, value));
}

QTransform QVGraphicsView::getTransformWithNoScaling() const
{
    const QTransform t = transform();
    // Only intended to handle combinations of scaling, mirroring, flipping, and rotation
    // in increments of 90 degrees. A seemingly simpler approach would be to scale the
    // transform by the inverse of its scale factor, but the resulting scale factor may
    // not exactly equal 1 due to floating point rounding errors.
    if (t.type() == QTransform::TxRotate)
        return { 0, t.m12() < 0 ? -1.0 : 1.0, t.m21() < 0 ? -1.0 : 1.0, 0, 0, 0 };
    else
        return { t.m11() < 0 ? -1.0 : 1.0, 0, 0, t.m22() < 0 ? -1.0 : 1.0, 0, 0 };
}

qreal QVGraphicsView::getDpiAdjustment() const
{
    return isOneToOnePixelSizingEnabled ? devicePixelRatioF() : 1.0;
}

int QVGraphicsView::roundToCompleteLogicalPixel(const qreal value, const qreal logicalScale)
{
    const int valueRoundedDown = qFloor(value);
    const int valueRoundedUp = valueRoundedDown + 1;
    const int physicalPixelsDrawn = qRound(value * logicalScale);
    const int physicalPixelsShownIfRoundingUp = qRound(valueRoundedUp * logicalScale);
    return physicalPixelsDrawn >= physicalPixelsShownIfRoundingUp ? valueRoundedUp : valueRoundedDown;
}

qreal QVGraphicsView::reverseLogicalPixelRounding(const int value, const qreal logicalScale)
{
    // For a given input value, its physical pixels fall within [value-0.5,value+0.5), so
    // calculate the first physical pixel of the next value (rounding up if between pixels),
    // and the pixel prior to that is the last one within the current value.
    int maxPhysicalPixelForValue = qCeil((value + 0.5) * logicalScale) - 1;
    return maxPhysicalPixelForValue / logicalScale;
}

void QVGraphicsView::handleDpiAdjustmentChange()
{
    if (appliedDpiAdjustment == getDpiAdjustment())
        return;

    removeExpensiveScaling();

    zoomToFit();

    expensiveScaleTimer->start();
}

void QVGraphicsView::handleSmoothScalingChange()
{
    loadedPixmapItem->setTransformationMode(isSmoothScalingRequested() ? Qt::SmoothTransformation : Qt::FastTransformation);

    if (isExpensiveScalingRequested())
        expensiveScaleTimer->start();
    else if (appliedExpensiveScaleZoomLevel != 0.0)
        removeExpensiveScaling();
}

void QVGraphicsView::settingsUpdated()
{
    auto &settingsManager = qvApp->getSettingsManager();

    //smooth scaling
    smoothScalingMode = settingsManager.getInteger("smoothscalingmode");

    //scaling two
    expensiveScalingAboveWindowSize = settingsManager.getBoolean("scalingtwoenabled");

    //smooth scaling limit
    smoothScalingLimit = settingsManager.getBoolean("smoothscalinglimitenabled") ? (settingsManager.getInteger("smoothscalinglimitpercent") / 100.0) : 0;

    //cropmode
    cropMode = settingsManager.getInteger("cropmode");

    //scalefactor
    zoomMultiplier = 1.0 + (settingsManager.getInteger("scalefactor") / 100.0);

    //resize past actual size
    isPastActualSizeEnabled = settingsManager.getBoolean("pastactualsizeenabled");

    //scrolling zoom
    isScrollZoomsEnabled = settingsManager.getBoolean("scrollzoomsenabled");

    //cursor zoom
    isCursorZoomEnabled = settingsManager.getBoolean("cursorzoom");

    //one-to-one pixel sizing
    isOneToOnePixelSizingEnabled = settingsManager.getBoolean("onetoonepixelsizing");

    //loop folders
    isLoopFoldersEnabled = settingsManager.getBoolean("loopfoldersenabled");

    // End of settings variables

    handleSmoothScalingChange();

    handleDpiAdjustmentChange();

    zoomToFit();
}

void QVGraphicsView::closeImage()
{
    imageCore.closeImage();
}

void QVGraphicsView::jumpToNextFrame()
{
    imageCore.jumpToNextFrame();
}

void QVGraphicsView::setPaused(const bool &desiredState)
{
    imageCore.setPaused(desiredState);
}

void QVGraphicsView::setSpeed(const int &desiredSpeed)
{
    imageCore.setSpeed(desiredSpeed);
}

void QVGraphicsView::rotateImage(const int relativeAngle)
{
    const QTransform t = transform();
    const bool isMirroredOrFlipped = t.isRotating() ? (t.m12() < 0 == t.m21() < 0) : (t.m11() < 0 != t.m22() < 0);
    rotate(relativeAngle * (isMirroredOrFlipped ? -1 : 1));
}

void QVGraphicsView::mirrorImage()
{
    const int rotateCorrection = transform().isRotating() ? -1 : 1;
    scale(-1 * rotateCorrection, 1 * rotateCorrection);
}

void QVGraphicsView::flipImage()
{
    const int rotateCorrection = transform().isRotating() ? -1 : 1;
    scale(1 * rotateCorrection, -1 * rotateCorrection);
}
