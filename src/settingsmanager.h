#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QVariant>

class SettingsManager : public QObject
{
    Q_OBJECT
public:
    enum class Type
    {
        Boolean,
        Integer,
        Double,
        String
    };
    Q_ENUM(Type)

    struct SSetting {
        QVariant defaultValue;
        QVariant value;
    };

    explicit SettingsManager(QObject *parent = nullptr);

    QString getSystemLanguage() const;

    bool loadTranslation() const;

    void loadSettings();

    const QVariant getSetting(const QString &key, bool defaults = false) const;

    bool getBoolean(const QString &key, bool defaults = false) const;

    int getInteger(const QString &key, bool defaults = false) const;

    double getDouble(const QString &key, bool defaults = false) const;

    const QString getString(const QString &key, bool defaults = false) const;

    const QHash<QString, SSetting> &getSettings() const { return settingsLibrary; }

    bool isDefault(const QString &key) const;

    static void migrateOldSettings();

signals:
    void settingsUpdated();

protected:
    void initializeSettingsLibrary();

private:
    QHash<QString, SSetting> settingsLibrary;

};

#endif // SETTINGSMANAGER_H
