#include "RxFilterProxyModel.h"

#include "../filter/MessageFilter.h"
#include "RxMessageModel.h"

RxFilterProxyModel::RxFilterProxyModel(const MessageFilter *filter, QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_filter(filter)
{
    setSortRole(RxMessageModel::SortRole);
}

void RxFilterProxyModel::RefreshFilter()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    invalidateFilter();
#endif
}

bool RxFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    Q_UNUSED(sourceParent)
    const auto *model = qobject_cast<const RxMessageModel *>(sourceModel());
    if (model == nullptr || m_filter == nullptr) {
        return true;
    }
    return m_filter->Accepts(model->FrameAt(sourceRow));
}
