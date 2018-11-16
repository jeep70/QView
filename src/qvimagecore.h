﻿#ifndef QVIMAGECORE_H
#define QVIMAGECORE_H

#include <QObject>
#include <QImageReader>
#include <QPixmap>
#include <QMovie>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QPixmapCache>

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
    };

    struct QVImageAndFileInfo
    {
        QImage readImage;
        QFileInfo readFileInfo;
    };

    explicit QVImageCore(QObject *parent = nullptr);

    void loadFile(const QString &fileName);
    QVImageAndFileInfo readFile(const QString &fileName);
    void postLoad();
    void updateFolderInfo();
    void addIndexToCache(const int &index);
    void addToCache(const QVImageAndFileInfo loadedImageAndFileInfo);

    void loadSettings();

    void jumpToNextFrame();
    void setPaused(bool desiredState);
    void setSpeed(int desiredSpeed);

    const QPixmap scaleExpensively(const int desiredWidth, const int desiredHeight, const scaleMode mode = scaleMode::normal);
    const QPixmap scaleExpensively(const QSize desiredSize, const scaleMode mode = scaleMode::normal);

    //returned const reference is read-only
    const QPixmap& getLoadedPixmap() const {return loadedPixmap; }
    const QMovie& getLoadedMovie() const {return loadedMovie; }
    const QVFileDetails& getCurrentFileDetails() const {return currentFileDetails; }


signals:
    void animatedFrameChanged(QRect rect);

    void fileInfoUpdated();

    void fileRead(QString string);

    void readError(const QString &errorString, const QString &fileName);

public slots:
    void processFile(int index);

private:
    const QStringList filterList = (QStringList() << "*.bmp" << "*.cur" << "*.gif" << "*.icns" << "*.ico" << "*.jp2" << "*.jpeg" << "*.jpe" << "*.jpg" << "*.mng" << "*.pbm" << "*.pgm" << "*.png" << "*.ppm" << "*.svg" << "*.svgz" << "*.tif" << "*.tiff" << "*.wbmp" << "*.webp" << "*.xbm" << "*.xpm");

    QPixmap loadedPixmap;
    QMovie loadedMovie;
    QImageReader imageReader;

    QVFileDetails currentFileDetails;
    QVFileDetails lastFileDetails;

    QFutureWatcher<QVImageAndFileInfo> loadFutureWatcher;

    bool vetoFutureWatcher;
    QFutureWatcher<QVImageAndFileInfo> cacheFutureWatcher;
    QFutureWatcher<QVImageAndFileInfo> cacheFutureWatcher2;
    QPixmapCache pixmapCache;

    bool isLoopFoldersEnabled;
};

#endif // QVIMAGECORE_H
