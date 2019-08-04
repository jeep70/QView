﻿#ifndef QVIMAGECORE_H
#define QVIMAGECORE_H

#include <QObject>
#include <QImageReader>
#include <QPixmap>
#include <QMovie>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QTimer>
#include <QCache>

class QVImageCore : public QObject
{
    Q_OBJECT

public:
    enum class scaleMode
    {
       normal,
       width,
       height
    };
    Q_ENUM(scaleMode)

    struct QVFileDetails
    {
        QFileInfo fileInfo;
        bool isPixmapLoaded;
        bool isMovieLoaded;
        QFileInfoList folder;
        int folderIndex;
        QSize imageSize;
        QSize loadedPixmapSize;
    };

    struct QVImageAndFileInfo
    {
        QImage readImage;
        QFileInfo readFileInfo;
    };

    explicit QVImageCore(QObject *parent = nullptr);

    void loadFile(const QString &fileName);
    QVImageAndFileInfo readFile(const QString &fileName);
    void postRead(const QVImageAndFileInfo &readImageAndFileInfo);
    void postLoad();
    void updateFolderInfo();
    void requestCaching();
    void requestCachingFile(const QString &filePath);
    void addToCache(const QVImageAndFileInfo &readImageAndFileInfo);

    void loadSettings();

    void jumpToNextFrame();
    void setPaused(bool desiredState);
    void setSpeed(int desiredSpeed);

    void rotateImage(int rotation);
    QImage matchCurrentRotation(const QImage &imageToRotate);
    QPixmap matchCurrentRotation(const QPixmap &pixmapToRotate);

    QPixmap scaleExpensively(const int desiredWidth, const int desiredHeight, const scaleMode mode = scaleMode::normal);
    QPixmap scaleExpensively(const QSize desiredSize, const scaleMode mode = scaleMode::normal);

    //returned const reference is read-only
    const QPixmap& getLoadedPixmap() const {return loadedPixmap; }
    const QMovie& getLoadedMovie() const {return loadedMovie; }
    const QVFileDetails& getCurrentFileDetails() const {return currentFileDetails; }
    int getCurrentRotation() const {return currentRotation; }

    void setDevicePixelRatio(qreal scaleFactor);

signals:
    void animatedFrameChanged(QRect rect);

    void fileInfoUpdated();

    void updateLoadedPixmapItem();

    void fileRead();

    void readError(const QString &errorString, const QString fileName);

private:
    const QStringList filterList = (QStringList() << "*.bmp" << "*.cur" << "*.gif" << "*.icns" << "*.ico" << "*.jp2" << "*.jpeg" << "*.jpe" << "*.jpg" << "*.mng" << "*.pbm" << "*.pgm" << "*.png" << "*.ppm" << "*.svg" << "*.svgz" << "*.tif" << "*.tiff" << "*.wbmp" << "*.webp" << "*.xbm" << "*.xpm");

    QPixmap loadedPixmap;
    QMovie loadedMovie;
    QImageReader imageReader;

    QVFileDetails currentFileDetails;
    QVFileDetails lastFileDetails;
    int currentRotation;

    qreal devicePixelRatio;

    QFutureWatcher<QVImageAndFileInfo> loadFutureWatcher;

    bool justLoadedFromCache;

    bool isLoopFoldersEnabled;
    int preloadingMode;
    int sortMode;
    bool sortAscending;


    QStringList lastFilesPreloaded;

    QTimer *fileChangeRateTimer;

    int largestDimension;
};

#endif // QVIMAGECORE_H
