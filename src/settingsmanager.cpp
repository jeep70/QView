#include "settingsmanager.h"

#include <QSettings>
#include <QTranslator>
#include <QLocale>
#include <QCoreApplication>
#include <QDir>

#include <QDebug>

SettingsManager::SettingsManager(QObject *parent) : QObject(parent)
{
    initializeSettingsLibrary();
    loadSettings();
    loadTranslation();
}

QString SettingsManager::getSystemLanguage() const
{
    auto entries = QDir(":/i18n/").entryList();
    entries.prepend("qview_en.ts");
    const auto centries = entries;

    const auto languages = QLocale::system().uiLanguages();
    for (auto language : languages)
    {
        language.replace('-', '_');
        const auto countryless = language.left(2);

        for (auto entry : centries)
        {
            entry.remove(0, 6);
            entry.remove(entry.length()-3, 3);

            if (entry == language)
                return language;

            if (entry == countryless)
                return countryless;
        }
    }
    return "en";
}

bool SettingsManager::loadTranslation() const
{
    QString lang = getString("language");
    if (lang == "system")
        lang = getSystemLanguage();

    if (lang == "en")
        return true;

    QTranslator *translator = new QTranslator();
    bool success = translator->load("qview_" + lang + ".qm", QLatin1String(":/i18n"));
    if (success)
    {
        qInfo() << "Loaded translation" << lang;
        QCoreApplication::installTranslator(translator);
    }
    return success;
}

void SettingsManager::loadSettings()
{
    QSettings settings;
    settings.beginGroup("options");
    bool changed = false;

    const auto keys = settingsLibrary.keys();
    for (const auto &key : keys)
    {
         auto &setting = settingsLibrary[key];
         if (setting.value != settings.value(key, setting.defaultValue))
             changed = true;

         setting.value = settings.value(key, setting.defaultValue);
    }

    if (changed)
        emit settingsUpdated();
}

const QVariant SettingsManager::getSetting(const QString &key, bool defaults) const
{
    auto value = settingsLibrary.value(key);

    if (!defaults && !value.value.isNull())
        return value.value;

    if (!value.defaultValue.isNull())
        return value.defaultValue;

    qWarning() << "Error: Invalid settings key: " + key;
    return QVariant();
}

bool SettingsManager::getBoolean(const QString &key, bool defaults) const
{
    auto value = getSetting(key, defaults);

    if (value.canConvert(QMetaType::Bool))
        return value.toBool();

    qWarning() << "Error: Can't convert setting key " + key + " to bool";
    return false;
}

int SettingsManager::getInteger(const QString &key, bool defaults) const
{
    auto value = getSetting(key, defaults);

    if (value.canConvert(QMetaType::Int))
        return value.toInt();

    qWarning() << "Error: Can't convert setting key " + key + " to int";
    return 0;
}

double SettingsManager::getDouble(const QString &key, bool defaults) const
{
    auto value = getSetting(key, defaults);

    if (value.canConvert(QMetaType::Double))
        return value.toDouble();

    qWarning() << "Error: Can't convert setting key " + key + " to double";
    return 0;
}

const QString SettingsManager::getString(const QString &key, bool defaults) const
{
    auto value = getSetting(key, defaults);

    if (value.canConvert(QMetaType::QString))
        return value.toString();

    qWarning() << "Error: Can't convert setting key " + key + " to string";
    return "";
}

bool SettingsManager::isDefault(const QString &key) const
{
    return getSetting(key) == getSetting(key, true);
}

void SettingsManager::initializeSettingsLibrary()
{
    // Window
    settingsLibrary.insert("bgcolorenabled", {true, {}});
    settingsLibrary.insert("bgcolor", {"#212121", {}});
    settingsLibrary.insert("titlebarmode", {1, {}});
    settingsLibrary.insert("windowresizemode", {1, {}});
    settingsLibrary.insert("minwindowresizedpercentage", {20, {}});
    settingsLibrary.insert("maxwindowresizedpercentage", {70, {}});
    settingsLibrary.insert("titlebaralwaysdark", {true, {}});
    settingsLibrary.insert("quitonlastwindow", {false, {}});
    settingsLibrary.insert("menubarenabled", {false, {}});
    settingsLibrary.insert("fullscreendetails", {false, {}});
    // Image
    settingsLibrary.insert("filteringenabled", {true, {}});
    settingsLibrary.insert("scalingenabled", {true, {}});
    settingsLibrary.insert("scalingtwoenabled", {true, {}});
    settingsLibrary.insert("scalefactor", {25, {}});
    settingsLibrary.insert("scrollzoomsenabled", {true, {}});
    settingsLibrary.insert("cursorzoom", {true, {}});
#ifdef Q_OS_MACOS
    // Usually not desired due to the way macOS does DPI scaling
    settingsLibrary.insert("onetoonepixelsizing", {false, {}});
#else
    settingsLibrary.insert("onetoonepixelsizing", {true, {}});
#endif
    settingsLibrary.insert("cropmode", {0, {}});
    settingsLibrary.insert("pastactualsizeenabled", {true, {}});
    settingsLibrary.insert("colorspaceconversion", {1, {}});
    // Miscellaneous
    settingsLibrary.insert("language", {"system", {}});
    settingsLibrary.insert("sortmode", {0, {}});
    settingsLibrary.insert("sortdescending", {false, {}});
    settingsLibrary.insert("preloadingmode", {1, {}});
    settingsLibrary.insert("loopfoldersenabled", {true, {}});
    settingsLibrary.insert("slideshowreversed", {false, {}});
    settingsLibrary.insert("slideshowtimer", {5, {}});
    settingsLibrary.insert("afterdelete", {2, {}});
    settingsLibrary.insert("askdelete", {true, {}});
    settingsLibrary.insert("allowmimecontentdetection", {false, {}});
    settingsLibrary.insert("saverecents", {true, {}});
    settingsLibrary.insert("updatenotifications", {false, {}});
    settingsLibrary.insert("skiphidden", {true, {}});
}
