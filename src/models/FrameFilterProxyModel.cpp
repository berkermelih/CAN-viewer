#include "FrameFilterProxyModel.h"

#include "../filter/MessageFilter.h"

#include <utility>

FrameFilterProxyModel::FrameFilterProxyModel(const MessageFilter *filter, FrameAccessor accessor, QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_filter(filter)
    , m_accessor(std::move(accessor))
{
}

void FrameFilterProxyModel::SetFilterActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    RefreshFilter();
}

bool FrameFilterProxyModel::IsFilterActive() const
{
    return m_active;
}

void FrameFilterProxyModel::RefreshFilter()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    invalidateFilter();
#endif
}

bool FrameFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    Q_UNUSED(sourceParent)
    if (!m_active || m_filter == nullptr || !m_accessor) {
        return true;
    }
    const CanFrame *frame = m_accessor(sourceRow);
    return frame == nullptr || m_filter->Accepts(*frame);
}
