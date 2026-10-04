#include "rowmodel.h"

RowModel::RowModel(const QStringList &roles, const QString &keyRole, QObject *parent)
    : QAbstractListModel(parent), m_roles(roles), m_keyRole(keyRole) {
    for (int i = 0; i < roles.size(); ++i)
        m_roleNames.insert(Qt::UserRole + 1 + i, roles.at(i).toUtf8());
}

int RowModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_rows.size(); }

QVariant RowModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const int i = role - Qt::UserRole - 1;
    return i >= 0 && i < m_roles.size() ? m_rows.at(index.row()).value(m_roles.at(i)) : QVariant();
}

void RowModel::setRows(const QList<QVariantMap> &rows) {
    bool sameKeys = rows.size() == m_rows.size();
    for (int i = 0; sameKeys && i < rows.size(); ++i)
        sameKeys = rows.at(i).value(m_keyRole) == m_rows.at(i).value(m_keyRole);

    if (sameKeys) {
        for (int i = 0; i < rows.size(); ++i) {
            if (rows.at(i) == m_rows.at(i))
                continue;
            m_rows[i] = rows.at(i);
            emit dataChanged(index(i), index(i));
        }
        return;
    }
    const int before = m_rows.size();
    beginResetModel();
    m_rows = rows;
    endResetModel();
    if (before != rows.size())
        emit countChanged();
}

QVariantMap RowModel::get(int row) const { return row >= 0 && row < m_rows.size() ? m_rows.at(row) : QVariantMap(); }

int RowModel::indexOf(const QVariant &key) const {
    for (int i = 0; i < m_rows.size(); ++i)
        if (m_rows.at(i).value(m_keyRole) == key)
            return i;
    return -1;
}
