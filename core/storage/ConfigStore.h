#pragma once
#include <QObject>
#include <QJsonObject>
#include <QString>

class ConfigStore : public QObject {
    Q_OBJECT
public:
    explicit ConfigStore(QString dataDir, QObject *parent = nullptr);
    QJsonObject data() const { return m_data; }
    void setData(const QJsonObject &obj) { m_data = obj; }
    bool save() const;
    bool load();

private:
    QString m_dataDir;
    QJsonObject m_data;
};
