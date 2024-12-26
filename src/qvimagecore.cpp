#include "qvimagecore.h"
#include "qvapplication.h"
#include "qvwin32functions.h"
#include "qvcocoafunctions.h"
#include "qvlinuxx11functions.h"
#include <cstring>
#include <random>
#include <QMessageBox>
#include <QDir>
#include <QUrl>
#include <QSettings>
#include <QCollator>
#include <QtConcurrent/QtConcurrentRun>
#include <QIcon>
#include <QGuiApplication>
#include <QScreen>

QCache<QString, QVImageCore::ReadData> QVImageCore::pixmapCache;

QVImageCore::QVImageCore(QObject *parent) : QObject(parent)
{
// Set allocation limit to 8 GiB on Qt6
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QImageReader::setAllocationLimit(8192);
#endif

    isLoopFoldersEnabled = true;
    preloadingMode = 1;
    sortMode = 0;
    sortDescending = false;
    allowMimeContentDetection = false;
    colorSpaceConversion = 1;

    randomSortSeed = 0;

    connect(&loadedMovie, &QMovie::updated, this, &QVImageCore::animatedFrameChanged);

    connect(&loadFutureWatcher, &QFutureWatcher<ReadData>::finished, this, [this](){
        loadPixmap(loadFutureWatcher.result());
    });

    largestDimension = 0;
    const auto screenList = QGuiApplication::screens();
    for (auto const &screen : screenList)
    {
        int largerDimension;
        if (screen->size().height() > screen->size().width())
        {
            largerDimension = screen->size().height();
        }
        else
        {
            largerDimension = screen->size().width();
        }

        if (largerDimension > largestDimension)
        {
            largestDimension = largerDimension;
        }
    }

    waitingOnLoad = false;

    // Connect to settings signal
    connect(&qvApp->getSettingsManager(), &SettingsManager::settingsUpdated, this, &QVImageCore::settingsUpdated);
    settingsUpdated();
}

void QVImageCore::loadFile(const QString &fileName, bool isReloading)
{
    if (waitingOnLoad)
    {
        return;
    }

    QString sanitaryFileName = fileName;

    //sanitize file name if necessary
    QUrl sanitaryUrl = QUrl(fileName);
    if (sanitaryUrl.isLocalFile())
        sanitaryFileName = sanitaryUrl.toLocalFile();

    QFileInfo fileInfo(sanitaryFileName);
    sanitaryFileName = fileInfo.absoluteFilePath();

    if (fileInfo.isDir())
    {
        updateFolderInfo(sanitaryFileName);
        if (currentFileDetails.folderFileInfoList.isEmpty())
            closeImage();
        else
            loadFile(currentFileDetails.folderFileInfoList.at(0).absoluteFilePath);
        return;
    }

    // Pause playing movie because it feels better that way
    setPaused(true);

    currentFileDetails.isLoadRequested = true;
    waitingOnLoad = true;

    QColorSpace targetColorSpace = getTargetColorSpace();
    QString cacheKey = getPixmapCacheKey(sanitaryFileName, fileInfo.size(), targetColorSpace);

    //check if cached already before loading the long way
    auto *cachedData = isReloading ? nullptr : QVImageCore::pixmapCache.take(cacheKey);
    if (cachedData != nullptr)
    {
        ReadData readData = *cachedData;
        delete cachedData;
        loadPixmap(readData);
    }
    //or see if the preloader is already working on it
    else if (preloadFilesInProgress.contains(sanitaryFileName))
    {
        waitingOnPreloadFile = sanitaryFileName;
    }
    else
    {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        loadFutureWatcher.setFuture(QtConcurrent::run(this, &QVImageCore::readFile, sanitaryFileName, targetColorSpace));
#else
        loadFutureWatcher.setFuture(QtConcurrent::run(&QVImageCore::readFile, this, sanitaryFileName, targetColorSpace));
#endif
    }
}

QVImageCore::ReadData QVImageCore::readFile(const QString &fileName, const QColorSpace &targetColorSpace)
{
    QImageReader imageReader;
    imageReader.setAutoTransform(true);

    imageReader.setFileName(fileName);

    QImage readImage;
    if (imageReader.format() == "svg" || imageReader.format() == "svgz")
    {
        // Render vectors into a high resolution
        QIcon icon;
        icon.addFile(fileName);
        readImage = icon.pixmap(largestDimension).toImage();
        readImage.setDevicePixelRatio(1.0);
        // If this fails, try reading the normal way so that a proper error message is given
        if (readImage.isNull())
            readImage = imageReader.read();
    }
    else
    {
        readImage = imageReader.read();
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && QT_VERSION < QT_VERSION_CHECK(6, 7, 2)
    // Work around Qt ICC profile parsing bug
    if (!readImage.colorSpace().isValid() && !readImage.colorSpace().iccProfile().isEmpty())
    {
        QByteArray profileData = readImage.colorSpace().iccProfile();
        if (removeTinyDataTagsFromIccProfile(profileData))
            readImage.setColorSpace(QColorSpace::fromIccProfile(profileData));
    }
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    // Assume image is sRGB if it doesn't specify
    if (!readImage.colorSpace().isValid())
        readImage.setColorSpace(QColorSpace::SRgb);

    // Convert image color space if we have a target that's different
    if (targetColorSpace.isValid() && readImage.colorSpace() != targetColorSpace)
        readImage.convertToColorSpace(targetColorSpace);
#endif

    QPixmap readPixmap = QPixmap::fromImage(readImage);
    QFileInfo fileInfo(fileName);

    ReadData readData = {
        readPixmap,
        fileInfo.absoluteFilePath(),
        fileInfo.size(),
        imageReader.size(),
        targetColorSpace,
        {}
    };

    if (readPixmap.isNull())
    {
        readData.errorData = {
            true,
            imageReader.error(),
            imageReader.errorString()
        };
    }

    return readData;
}

void QVImageCore::loadPixmap(const ReadData &readData)
{
    if (readData.errorData.hasError)
    {
        currentFileDetails = getEmptyFileDetails();
        currentFileDetails.errorData = readData.errorData;
    }
    else
    {
        currentFileDetails.errorData = {};
    }

    // Do this first so we can keep folder info even when loading errored files
    currentFileDetails.fileInfo = QFileInfo(readData.absoluteFilePath);
    currentFileDetails.updateLoadedIndexInFolder();
    if (currentFileDetails.loadedIndexInFolder == -1)
        updateFolderInfo();

    // Reset mechanism to avoid stalling while loading
    waitingOnLoad = false;

    if (currentFileDetails.errorData.hasError)
    {
        loadEmptyPixmap();
        return;
    }

    loadedPixmap = readData.pixmap;

    // Set file details
    currentFileDetails.isPixmapLoaded = true;
    currentFileDetails.baseImageSize = readData.imageSize;
    currentFileDetails.loadedPixmapSize = loadedPixmap.size();
    if (currentFileDetails.baseImageSize == QSize(-1, -1))
    {
        qInfo() << "QImageReader::size gave an invalid size for " + currentFileDetails.fileInfo.fileName() + ", using size from loaded pixmap";
        currentFileDetails.baseImageSize = currentFileDetails.loadedPixmapSize;
    }

    addToCache(readData);

    // Animation detection
    loadedMovie.setFormat("");
    loadedMovie.stop();
    loadedMovie.setFileName(currentFileDetails.fileInfo.absoluteFilePath());

    // APNG workaround
    if (loadedMovie.format() == "png")
    {
        loadedMovie.setFormat("apng");
        loadedMovie.setFileName(currentFileDetails.fileInfo.absoluteFilePath());
    }

    if (loadedMovie.isValid() && loadedMovie.frameCount() != 1)
        loadedMovie.start();

    currentFileDetails.isMovieLoaded = loadedMovie.state() == QMovie::Running;

    if (!currentFileDetails.isMovieLoaded)
        if (auto device = loadedMovie.device())
            device->close();

    currentFileDetails.timeSinceLoaded.start();

    emit fileChanged();

    requestCaching();
}

void QVImageCore::closeImage()
{
    currentFileDetails = getEmptyFileDetails();
    loadEmptyPixmap();
}

void QVImageCore::loadEmptyPixmap()
{
    loadedPixmap = QPixmap();
    loadedMovie.stop();
    loadedMovie.setFileName("");

    emit fileChanged();
}

QVImageCore::FileDetails QVImageCore::getEmptyFileDetails()
{
    return {
        QFileInfo(),
        currentFileDetails.folderFileInfoList,
        currentFileDetails.loadedIndexInFolder,
        false,
        false,
        false,
        QSize(),
        QSize(),
        QElapsedTimer(),
        {}
    };
}

// All file logic, sorting, etc should be moved to a different class or file
QList<QVImageCore::CompatibleFile> QVImageCore::getCompatibleFiles(const QString &dirPath) const
{
    QList<CompatibleFile> fileList;

    QMimeDatabase mimeDb;
    const auto &extensions = qvApp->getFileExtensionList();
    const auto &mimeTypes = qvApp->getMimeTypeNameList();

    QMimeDatabase::MatchMode mimeMatchMode = allowMimeContentDetection ? QMimeDatabase::MatchDefault : QMimeDatabase::MatchExtension;

    // skip hidden files if user wants to
    QDir::Filters filters = QDir::Files;

    auto &settingsManager = qvApp->getSettingsManager();
    if (!settingsManager.getBoolean("skiphidden"))
        filters |= QDir::Hidden;

    const QFileInfoList currentFolder = QDir(dirPath).entryInfoList(filters, QDir::Unsorted);
    for (const QFileInfo &fileInfo : currentFolder)
    {
        bool matched = false;
        const QString absoluteFilePath = fileInfo.absoluteFilePath();
        const QString fileName = fileInfo.fileName();
        for (const QString &extension : extensions)
        {
            if (fileName.endsWith(extension, Qt::CaseInsensitive))
            {
                matched = true;
                break;
            }
        }
        QString mimeType;
        if (!matched || sortMode == 4)
        {
            mimeType = mimeDb.mimeTypeForFile(absoluteFilePath, mimeMatchMode).name();
            matched |= mimeTypes.contains(mimeType);
        }

        // ignore macOS ._ metadata files
        if (fileName.startsWith("._"))
        {
            matched = false;
        }

        if (matched)
        {
            fileList.append({
                absoluteFilePath,
                fileName,
                sortMode == 1 ? fileInfo.lastModified().toMSecsSinceEpoch() : 0,
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
                sortMode == 2 ? fileInfo.birthTime().toMSecsSinceEpoch() : 0,
#else
                sortMode == 2 ? fileInfo.created().toMSecsSinceEpoch() : 0,
#endif
                sortMode == 3 ? fileInfo.size() : 0,
                sortMode == 4 ? mimeType : QString()
            });
        }
    }

    return fileList;
}

void QVImageCore::updateFolderInfo(QString dirPath)
{
    if (dirPath.isEmpty())
    {
        dirPath = currentFileDetails.fileInfo.path();

        // No directory specified and a file is not already loaded from which we can infer one
        if (dirPath.isEmpty())
            return;
    }

    currentFileDetails.folderFileInfoList = getCompatibleFiles(dirPath);

    QPair<QString, int> dirInfo = {dirPath,
                                         currentFileDetails.folderFileInfoList.count()};
    // If the current folder changed since the last image, assign a new seed for random sorting
    if (lastDirInfo != dirInfo)
    {
        randomSortSeed = std::chrono::system_clock::now().time_since_epoch().count();
    }
    lastDirInfo = dirInfo;

    // Sorting

    if (sortMode == 0) // Natural sorting
    {
        QCollator collator;
        collator.setNumericMode(true);
        std::sort(currentFileDetails.folderFileInfoList.begin(),
                  currentFileDetails.folderFileInfoList.end(),
                  [&collator, this](const CompatibleFile &file1, const CompatibleFile &file2)
        {
            if (sortDescending)
                return collator.compare(file1.fileName, file2.fileName) > 0;
            else
                return collator.compare(file1.fileName, file2.fileName) < 0;
        });
    }
    else if (sortMode == 1) // date modified
    {
        std::sort(currentFileDetails.folderFileInfoList.begin(),
                  currentFileDetails.folderFileInfoList.end(),
                  [this](const CompatibleFile &file1, const CompatibleFile &file2)
        {
            if (sortDescending)
                return file1.lastModified < file2.lastModified;
            else
                return file1.lastModified > file2.lastModified;
        });
    }
    else if (sortMode == 2) // date created
    {
        std::sort(currentFileDetails.folderFileInfoList.begin(),
                  currentFileDetails.folderFileInfoList.end(),
                  [this](const CompatibleFile &file1, const CompatibleFile &file2)
        {
            if (sortDescending)
                return file1.lastCreated < file2.lastCreated;
            else
                return file1.lastCreated > file2.lastCreated;
        });

    }
    else if (sortMode == 3) // size
    {
        std::sort(currentFileDetails.folderFileInfoList.begin(),
                  currentFileDetails.folderFileInfoList.end(),
                  [this](const CompatibleFile &file1, const CompatibleFile &file2)
        {
            if (sortDescending)
                return file1.size < file2.size;
            else
                return file1.size > file2.size;
        });
    }
    else if (sortMode == 4) // type
    {
        QCollator collator;
        std::sort(currentFileDetails.folderFileInfoList.begin(),
                  currentFileDetails.folderFileInfoList.end(),
                  [&collator, this](const CompatibleFile &file1, const CompatibleFile &file2)
        {
            if (sortDescending)
                return collator.compare(file1.mimeType, file2.mimeType) > 0;
            else
                return collator.compare(file1.mimeType, file2.mimeType) < 0;
        });
    }
    else if (sortMode == 5) // Random
    {
        std::shuffle(currentFileDetails.folderFileInfoList.begin(), currentFileDetails.folderFileInfoList.end(), std::default_random_engine(randomSortSeed));
    }

    // Set current file index variable
    currentFileDetails.updateLoadedIndexInFolder();
}

void QVImageCore::requestCaching()
{
    if (preloadingMode == 0)
    {
        QVImageCore::pixmapCache.clear();
        return;
    }

    QColorSpace targetColorSpace = getTargetColorSpace();

    int preloadingDistance = 1;

    if (preloadingMode > 1)
        preloadingDistance = 4;

    QStringList filesToPreload;
    for (int i = currentFileDetails.loadedIndexInFolder-preloadingDistance; i <= currentFileDetails.loadedIndexInFolder+preloadingDistance; i++)
    {
        int index = i;

        // Don't try to cache the currently loaded image
        if (index == currentFileDetails.loadedIndexInFolder)
            continue;

        //keep within index range
        if (isLoopFoldersEnabled)
        {
            if (index > currentFileDetails.folderFileInfoList.length()-1)
                index = index-(currentFileDetails.folderFileInfoList.length());
            else if (index < 0)
                index = index+(currentFileDetails.folderFileInfoList.length());
        }

        //if still out of range after looping, just cancel the cache for this index
        if (index > currentFileDetails.folderFileInfoList.length()-1 || index < 0 || currentFileDetails.folderFileInfoList.isEmpty())
            continue;


        QString filePath = currentFileDetails.folderFileInfoList[index].absoluteFilePath;
        filesToPreload.append(filePath);

        requestCachingFile(filePath, targetColorSpace);
    }
    lastFilesPreloaded = filesToPreload;

}

void QVImageCore::requestCachingFile(const QString &filePath, const QColorSpace &targetColorSpace)
{
    QFile imgFile(filePath);
    QString cacheKey = getPixmapCacheKey(filePath, imgFile.size(), targetColorSpace);

    //check if image is already loaded or requested
    if (QVImageCore::pixmapCache.contains(cacheKey) || lastFilesPreloaded.contains(filePath))
        return;

    if (imgFile.size()/1024 > QVImageCore::pixmapCache.maxCost()/2)
        return;

    preloadFilesInProgress.append(filePath);

    auto *cacheFutureWatcher = new QFutureWatcher<ReadData>();
    connect(cacheFutureWatcher, &QFutureWatcher<ReadData>::finished, this, [cacheFutureWatcher, this](){
        const ReadData readData = cacheFutureWatcher->result();
        if (waitingOnPreloadFile == readData.absoluteFilePath)
        {
            loadPixmap(readData);
            waitingOnPreloadFile = QString();
        }
        else
        {
            addToCache(readData);
        }
        preloadFilesInProgress.removeAll(readData.absoluteFilePath);
        cacheFutureWatcher->deleteLater();
    });

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    cacheFutureWatcher->setFuture(QtConcurrent::run(this, &QVImageCore::readFile, filePath, targetColorSpace));
#else
    cacheFutureWatcher->setFuture(QtConcurrent::run(&QVImageCore::readFile, this, filePath, targetColorSpace));
#endif
}

void QVImageCore::addToCache(const ReadData &readData)
{
    if (readData.pixmap.isNull())
        return;

    QString cacheKey = getPixmapCacheKey(readData.absoluteFilePath, readData.fileSize, readData.targetColorSpace);
    qint64 pixmapMemoryBytes = static_cast<qint64>(readData.pixmap.width()) * readData.pixmap.height() * readData.pixmap.depth() / 8;

    QVImageCore::pixmapCache.insert(cacheKey, new ReadData(readData), qMax(pixmapMemoryBytes / 1024, 1LL));
}

QString QVImageCore::getPixmapCacheKey(const QString &absoluteFilePath, const qint64 &fileSize, const QColorSpace &targetColorSpace)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QString targetColorSpaceHash = QCryptographicHash::hash(targetColorSpace.iccProfile(), QCryptographicHash::Md5).toHex();
#else
    QString targetColorSpaceHash = "";
#endif
    return absoluteFilePath + "\n" + QString::number(fileSize) + "\n" + targetColorSpaceHash;
}

QColorSpace QVImageCore::getTargetColorSpace() const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    return
        colorSpaceConversion == 1 ? detectDisplayColorSpace() :
        colorSpaceConversion == 2 ? QColorSpace::SRgb :
        colorSpaceConversion == 3 ? QColorSpace::DisplayP3 :
        QColorSpace();
#else
    return {};
#endif
}

QColorSpace QVImageCore::detectDisplayColorSpace() const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QWindow *window = static_cast<QWidget*>(parent())->window()->windowHandle();

    QByteArray profileData;
#ifdef WIN32_LOADED
    profileData = QVWin32Functions::getIccProfileForWindow(window);
#endif
#ifdef COCOA_LOADED
    profileData = QVCocoaFunctions::getIccProfileForWindow(window);
#endif
#ifdef X11_LOADED
    profileData = QVLinuxX11Functions::getIccProfileForWindow(window);
#endif

    if (!profileData.isEmpty())
    {
        QColorSpace colorSpace = QColorSpace::fromIccProfile(profileData);
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 2)
        if (!colorSpace.isValid() && removeTinyDataTagsFromIccProfile(profileData))
            colorSpace = QColorSpace::fromIccProfile(profileData);
#endif
        return colorSpace;
    }
#endif

    return {};
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && QT_VERSION < QT_VERSION_CHECK(6, 7, 2)
// Workaround for QTBUG-125241
bool QVImageCore::removeTinyDataTagsFromIccProfile(QByteArray &profile)
{
    const int offsetTagCount = 128;
    const qsizetype length = profile.length();
    qsizetype offset = offsetTagCount;
    char *data = profile.data();
    bool foundTinyData = false;
    // read tag count
    if (length - offset < 4)
        return false;
    quint32 tagCount = qFromBigEndian<quint32>(data + offset);
    offset += 4;
    // so we don't have to worry about overflows
    if (tagCount > 99999)
        return false;
    // loop through tags
    if (length - offset < qsizetype(tagCount * 12))
        return false;
    while (tagCount)
    {
        tagCount -= 1;
        const quint32 dataSize = qFromBigEndian<quint32>(data + offset + 8);
        if (dataSize >= 12)
        {
            // this tag is fine
            offset += 12;
            continue;
        }
        // qt will fail on this tag, remove it
        foundTinyData = true;
        if (tagCount)
        {
            // shift subsequent tags back
            std::memmove(data + offset, data + offset + 12, tagCount * 12);
        }
        // zero fill gap at end
        std::memset(data + offset + (tagCount * 12), 0, 12);
        // decrement tag count
        qToBigEndian(qFromBigEndian<quint32>(data + offsetTagCount) - 1, data + offsetTagCount);
    }
    return foundTinyData;
}
#endif

void QVImageCore::jumpToNextFrame()
{
    if (currentFileDetails.isMovieLoaded)
        loadedMovie.jumpToNextFrame();
}

void QVImageCore::setPaused(bool desiredState)
{
    if (currentFileDetails.isMovieLoaded)
        loadedMovie.setPaused(desiredState);
}

void QVImageCore::setSpeed(int desiredSpeed)
{
    if (desiredSpeed < 0)
        desiredSpeed = 0;

    if (desiredSpeed > 1000)
        desiredSpeed = 1000;

    if (currentFileDetails.isMovieLoaded)
        loadedMovie.setSpeed(desiredSpeed);
}

QPixmap QVImageCore::scaleExpensively(const QSizeF desiredSize)
{
    if (!currentFileDetails.isPixmapLoaded)
        return QPixmap();

    // Get the current frame of the animation if this is an animation
    QPixmap relevantPixmap;
    if (!currentFileDetails.isMovieLoaded)
    {
        relevantPixmap = loadedPixmap;
    }
    else
    {
        relevantPixmap = loadedMovie.currentPixmap();
    }

    // If we are really close to the original size, just return the original
    if (abs(desiredSize.width() - relevantPixmap.width()) < 1 &&
        abs(desiredSize.height() - relevantPixmap.height()) < 1)
    {
        return relevantPixmap;
    }

    QSize size = desiredSize.toSize();
    size.rwidth() = qMax(size.width(), 1);
    size.rheight() = qMax(size.height(), 1);

    return relevantPixmap.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}


void QVImageCore::settingsUpdated()
{
    auto &settingsManager = qvApp->getSettingsManager();

    //loop folders
    isLoopFoldersEnabled = settingsManager.getBoolean("loopfoldersenabled");

    //preloading mode
    preloadingMode = settingsManager.getInteger("preloadingmode");
    switch (preloadingMode) {
    case 1:
    {
        QVImageCore::pixmapCache.setMaxCost(204800);
        break;
    }
    case 2:
    {
        QVImageCore::pixmapCache.setMaxCost(819200);
        break;
    }
    }

    //sort mode
    sortMode = settingsManager.getInteger("sortmode");

    //sort ascending
    sortDescending = settingsManager.getBoolean("sortdescending");

    //allow mime content detection
    allowMimeContentDetection = settingsManager.getBoolean("allowmimecontentdetection");

    //update folder info to reflect new settings (e.g. sort order)
    updateFolderInfo();

    bool changedImagePreprocessing = false;

    //colorspaceconversion
    if (colorSpaceConversion != settingsManager.getInteger("colorspaceconversion"))
    {
        colorSpaceConversion = settingsManager.getInteger("colorspaceconversion");
        changedImagePreprocessing = true;
    }

    if (changedImagePreprocessing && currentFileDetails.isPixmapLoaded)
        loadFile(currentFileDetails.fileInfo.absoluteFilePath());
}

void QVImageCore::FileDetails::updateLoadedIndexInFolder()
{
    const QString targetPath = fileInfo.absoluteFilePath().normalized(QString::NormalizationForm_D);
    for (int i = 0; i < folderFileInfoList.length(); i++)
    {
        // Compare absoluteFilePath first because it's way faster, but double-check with
        // QFileInfo::operator== because it respects file system case sensitivity rules
        QString candidatePath = folderFileInfoList[i].absoluteFilePath.normalized(QString::NormalizationForm_D);
        if (candidatePath.compare(targetPath, Qt::CaseInsensitive) == 0 &&
            QFileInfo(folderFileInfoList[i].absoluteFilePath) == fileInfo)
        {
            loadedIndexInFolder = i;
            return;
        }
    }
    loadedIndexInFolder = -1;
}
