#ifndef RXFILTERPROXYMODEL_H
#define RXFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>

class MessageFilter;

/**
 * @brief Rx tablosunu MessageFilter kurallarına göre süzen ve sıralayan proxy model.
 *
 * Kaynak model RxMessageModel olmalıdır. Filtre değiştiğinde RefreshFilter()
 * çağrılarak görünüm yeniden hesaplanır; kaynak veriler silinmez.
 */
class RxFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    /**
     * @brief Proxy modeli oluşturur.
     * @param filter Uygulanacak filtre; proxy'den uzun yaşamalıdır.
     * @param parent Qt sahiplik zinciri için üst nesne.
     */
    explicit RxFilterProxyModel(const MessageFilter *filter, QObject *parent = nullptr);

    /// Filtre kuralları değiştiğinde görünür satırları yeniden hesaplar.
    void RefreshFilter();

protected:
    /// Kaynak satırdaki çerçevenin filtreden geçip geçmediğini döndürür.
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    const MessageFilter *m_filter;
};

#endif // RXFILTERPROXYMODEL_H
