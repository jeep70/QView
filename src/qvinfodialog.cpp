#include <QCoreApplication>
#include <QDebug>
#include <fstream>  // For file stream
#include <TinyEXIF.h>
#include "qvinfodialog.h"
#include "ui_qvinfodialog.h"
#include <QDateTime>
#include <QMimeDatabase>
#include <QTimer>

static int getGcd (int a, int b) {
    return (b == 0) ? a : getGcd(b, a%b);
}

QVInfoDialog::QVInfoDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::QVInfoDialog)
{
    ui->setupUi(this);
    setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint | Qt::CustomizeWindowHint));
    setFixedSize(0, 0);

    width = 0;
    height = 0;
    frameCount = 0;
}

QVInfoDialog::~QVInfoDialog()
{
    delete ui;
}

void QVInfoDialog::setInfo(const QFileInfo &value, const int &value2, const int &value3, const int &value4)
{
    selectedFileInfo = value;
    width = value2;
    height = value3;
    frameCount = value4;

    // If the dialog is visible, it means we've just navigated to a new image. Instead of running
    // updateInfo immediately, add it to the event queue. This is a workaround for a (Windows-specific?)
    // delay when calling adjustSize on the window if the font contains certain characters (e.g. Chinese)
    // the first time that happens for a given font. At least on Windows, by making the work happen later
    // in the event loop, it allows the main window to repaint first, giving the appearance of better
    // responsiveness. If the dialog is not visible, however, it means we're preparing to display for an
    // image already opened. In this case there is no urgency to repaint the main window, and we need to
    // process the updates here synchronously to avoid the caller showing the dialog before it's ready
    // (i.e. to avoid showing outdated info or placeholder text).
    if (isVisible())
        QTimer::singleShot(0, this, &QVInfoDialog::updateInfo);
    else
        updateInfo();
}

QString readExifData(const QString &imagePath) {
    // Open the file as a binary input stream
    std::ifstream file(imagePath.toStdString(), std::ifstream::binary);

    // Check if the file is open
    if (!file.is_open()) {
        return "Error: Unable to open image file!";
    }

    // Create an EXIFInfo object and read the EXIF data
    TinyEXIF::EXIFInfo exifInfo(file);

    // QString to accumulate the output
    QString exifOutput;

    // Check if EXIF data is present
    if (exifInfo.Fields) {
        exifOutput += "Camera Make: " + QString::fromStdString(exifInfo.Make) + "\n";
        exifOutput += "Camera Model: " + QString::fromStdString(exifInfo.Model) + "\n";
        exifOutput += "Focal Length: " + QString::number(exifInfo.FocalLength) + " mm\n";
        exifOutput += "Exposure Time: " + QString::number(exifInfo.ExposureTime) + " s\n";
        exifOutput += "F-Number: " + QString::number(exifInfo.FNumber) + "\n";
        exifOutput += "ISO: " + QString::number(exifInfo.ISOSpeedRatings) + "\n";
        exifOutput += "Date Taken: " + QString::fromStdString(exifInfo.DateTimeOriginal) + "\n";
        exifOutput += "Latitude: " + QString::number(exifInfo.GeoLocation.Latitude) + "\n";
        exifOutput += "Longitude: " + QString::number(exifInfo.GeoLocation.Longitude) + "\n";
    } else {
        exifOutput = "No EXIF data found.";
    }

    return exifOutput;  // Return the accumulated EXIF data as a QString
}


void QVInfoDialog::updateInfo()
{
    QLocale locale = QLocale::system();
    QMimeDatabase mimedb;
    QMimeType mime = mimedb.mimeTypeForFile(selectedFileInfo.absoluteFilePath(), QMimeDatabase::MatchContent);
    //this is just math to figure the megapixels and then round it to the tenths place
    const double megapixels = static_cast<double>(qRound(((static_cast<double>((width*height))))/1000000 * 10 + 0.5)) / 10 ;

    QString exifData = readExifData(selectedFileInfo.absoluteFilePath());

    ui->nameLabel->setText(selectedFileInfo.fileName());
    ui->typeLabel->setText(mime.name());
    ui->locationLabel->setText(selectedFileInfo.path());
    ui->sizeLabel->setText(tr("%1 (%2 bytes)").arg(formatBytes(selectedFileInfo.size()), locale.toString(selectedFileInfo.size())));
    ui->modifiedLabel->setText(selectedFileInfo.lastModified().toString(locale.dateTimeFormat()));
    ui->dimensionsLabel->setText(tr("%1 x %2 (%3 MP)").arg(QString::number(width), QString::number(height), QString::number(megapixels)));
    ui->miscLabel->setText(exifData);

    int gcd = getGcd(width,height);
    if (gcd != 0)
        ui->ratioLabel->setText(QString::number(width/gcd) + ":" + QString::number(height/gcd));
    if (frameCount != 0)
    {
        ui->framesLabel2->show();
        ui->framesLabel->show();
        ui->framesLabel->setText(QString::number(frameCount));
    }
    else
    {
        ui->framesLabel2->hide();
        ui->framesLabel->hide();
    }
    window()->adjustSize();
}
