#pragma once

#include <QAbstractListModel>
#include <QVariantMap>

// A list model over plain maps, so the backend can hand QML rows without one
// bespoke model class per list. Equal-shaped updates change rows in place, which
// keeps ListView selection steady across the periodic refresh.
class RowModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    RowModel(const QStringList &roles, const QString &keyRole, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override { return m_roleNames; }

    void setRows(const QList<QVariantMap> &rows);
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE int indexOf(const QVariant &key) const;

signals:
    void countChanged();

private:
    QStringList m_roles;
    QString m_keyRole;
    QHash<int, QByteArray> m_roleNames;
    QList<QVariantMap> m_rows;
};
