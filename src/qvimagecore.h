#ifndef QVIMAGECORE_H
#define QVIMAGECORE_H

#include <QObject>
#include <QImageReader>
#include <QPixmap>
#include <QMovie>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QTimer>
#include <QCache>
#include <QElapsedTimer>

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
#include <QColorSpace>
#else
typedef QString QColorSpace;
#endif

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
        qint64 lastCreated;
        qint64 size;
        QString mimeType;
    };

    struct ErrorData
    {
        bool hasError = false;
        int errorNum = 0;
        QString errorString;
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
        QElapsedTimer timeSinceLoaded;
        ErrorData errorData;

        void updateLoadedIndexInFolder();
    };

    struct ReadData
    {
        QPixmap pixmap;
        QString absoluteFilePath;
        qint64 fileSize;
        QSize imageSize;
        QColorSpace targetColorSpace;
        ErrorData errorData;
    };

    explicit QVImageCore(QObject *parent = nullptr);

    void loadFile(const QString &fileName, bool isReloading = false);
    ReadData readFile(const QString &fileName, const QColorSpace &targetColorSpace);
    void loadPixmap(const ReadData &readData);
    void closeImage();
    QList<CompatibleFile> getCompatibleFiles(const QString &dirPath) const;
    void updateFolderInfo(QString dirPath = QString());
    void requestCaching();
    void requestCachingFile(const QString &filePath, const QColorSpace &targetColorSpace);
    void addToCache(const ReadData &readImageAndFileInfo);
    static QString getPixmapCacheKey(const QString &absoluteFilePath, const qint64 &fileSize, const QColorSpace &targetColorSpace);
    QColorSpace getTargetColorSpace() const;
    QColorSpace detectDisplayColorSpace() const;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && QT_VERSION < QT_VERSION_CHECK(6, 7, 2)
    static bool removeTinyDataTagsFromIccProfile(QByteArray &profile);
#endif

    void settingsUpdated();

    void jumpToNextFrame();
    void setPaused(bool desiredState);
    void setSpeed(int desiredSpeed);

    QPixmap scaleExpensively(const QSizeF desiredSize);

    //returned const reference is read-only
    const QPixmap& getLoadedPixmap() const {return loadedPixmap; }
    const QMovie& getLoadedMovie() const {return loadedMovie; }
    const FileDetails& getCurrentFileDetails() const {return currentFileDetails; }

signals:
    void animatedFrameChanged(QRect rect);

    void fileChanged();

protected:
    void loadEmptyPixmap();
    FileDetails getEmptyFileDetails();

private:
    QPixmap loadedPixmap;
    QMovie loadedMovie;

    FileDetails currentFileDetails;

    QFutureWatcher<ReadData> loadFutureWatcher;

    bool isLoopFoldersEnabled;
    int preloadingMode;
    int sortMode;
    bool sortDescending;
    bool allowMimeContentDetection;
    int colorSpaceConversion;

    static QCache<QString, ReadData> pixmapCache;

    QPair<QString, int> lastDirInfo;
    unsigned randomSortSeed;

    QStringList lastFilesPreloaded;
    QStringList preloadFilesInProgress;
    QString waitingOnPreloadFile;

    int largestDimension;

    bool waitingOnLoad;
};

#endif // QVIMAGECORE_H
