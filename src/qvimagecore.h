﻿#ifndef QVIMAGECORE_H
#define QVIMAGECORE_H

#include <QObject>
#include <QImageReader>
#include <QPixmap>
#include <QMovie>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
#include <QColorSpace>
#endif
#include <QFileInfo>
#include <QFutureWatcher>
#include <QTimer>
#include <QCache>

class QVImageCore : public QObject
{
    Q_OBJECT

public:
    struct CompatibleFile
    {
        QString absoluteFilePath;
        QString fileName;

        // Only populated if needed for sorting
        qint64 lastModified;
        qint64 size;
        QString mimeType;
    };

    struct FileDetails
    {
        QFileInfo fileInfo;
        QList<CompatibleFile> folderFileInfoList;
        int loadedIndexInFolder = -1;
        bool isLoadRequested = false;
        bool isPixmapLoaded = false;
        bool isMovieLoaded = false;
        QSize baseImageSize;
        QSize loadedPixmapSize;

        void updateLoadedIndexInFolder();
    };

    struct ReadData
    {
        QPixmap pixmap;
        QFileInfo fileInfo;
        QSize size;
    };

    explicit QVImageCore(QObject *parent = nullptr);

    void loadFile(const QString &fileName);
    ReadData readFile(const QString &fileName, bool forCache);
    void loadPixmap(const ReadData &readData, bool fromCache);
    void closeImage();
    QList<CompatibleFile> getCompatibleFiles();
    void updateFolderInfo();
    void requestCaching();
    void requestCachingFile(const QString &filePath);
    void addToCache(const ReadData &readImageAndFileInfo);

    void settingsUpdated();

    void jumpToNextFrame();
    void setPaused(bool desiredState);
    void setSpeed(int desiredSpeed);

    void rotateImage(int rotation);
    QImage matchCurrentRotation(const QImage &imageToRotate);
    QPixmap matchCurrentRotation(const QPixmap &pixmapToRotate);

    QPixmap scaleExpensively(const int desiredWidth, const int desiredHeight);
    QPixmap scaleExpensively(const QSizeF desiredSize);

    //returned const reference is read-only
    const QPixmap& getLoadedPixmap() const {return loadedPixmap; }
    const QMovie& getLoadedMovie() const {return loadedMovie; }
    const FileDetails& getCurrentFileDetails() const {return currentFileDetails; }
    int getCurrentRotation() const {return currentRotation; }

signals:
    void animatedFrameChanged(QRect rect);

    void updateLoadedPixmapItem();

    void fileChanged();

    void readError(int errorNum, const QString &errorString, const QString &fileName);

private:
    QPixmap loadedPixmap;
    QMovie loadedMovie;

    FileDetails currentFileDetails;
    int currentRotation;

    QFutureWatcher<ReadData> loadFutureWatcher;

    bool isLoopFoldersEnabled;
    int preloadingMode;
    int sortMode;
    bool sortDescending;
    bool showHiddenFiles;
    bool allowMimeContentDetection;
    int colorSpaceConversion;

    QPair<QString, uint> lastDirInfo;
    unsigned randomSortSeed;

    QStringList lastFilesPreloaded;

    int largestDimension;

    bool waitingOnLoad;
};

#endif // QVIMAGECORE_H
