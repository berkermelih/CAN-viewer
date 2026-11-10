#ifndef FRAMEFILTERPROXYMODEL_H
#define FRAMEFILTERPROXYMODEL_H

#include "../can/CanFrame.h"

#include <QSortFilterProxyModel>

#include <functional>

class MessageFilter;

/**
 * @brief Çerçeve tablolarını (Rx, Trace) MessageFilter kurallarına göre süzen proxy model.
 *
 * Kaynak modelden satırdaki çerçeveyi almak için bir erişim fonksiyonu
 * kullanır; böylece farklı modellerle çalışabilir. Filtre değiştiğinde
 * RefreshFilter() çağrılır; kaynak veriler silinmez.
 */
class FrameFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    /// Kaynak satırdaki çerçeveyi döndüren fonksiyon tipi (geçersizse nullptr).
    using FrameAccessor = std::function<const CanFrame *(int sourceRow)>;

    /**
     * @brief Proxy modeli oluşturur.
     * @param filter Uygulanacak filtre; proxy'den uzun yaşamalıdır.
     * @param accessor Kaynak satırdan çerçeveyi döndüren fonksiyon.
     * @param parent Qt sahiplik zinciri için üst nesne.
     */
    FrameFilterProxyModel(const MessageFilter *filter, FrameAccessor accessor, QObject *parent = nullptr);

    /**
     * @brief Filtrenin bu görünüme uygulanıp uygulanmayacağını ayarlar.
     * @param active false ise tüm satırlar gösterilir.
     */
    void SetFilterActive(bool active);

    /// Filtrenin bu görünüme uygulanıp uygulanmadığını döndürür.
    bool IsFilterActive() const;

    /// Filtre kuralları değiştiğinde görünür satırları yeniden hesaplar.
    void RefreshFilter();

protected:
    /// Kaynak satırdaki çerçevenin filtreden geçip geçmediğini döndürür.
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    const MessageFilter *m_filter;
    FrameAccessor m_accessor;
    bool m_active = true;
};

#endif // FRAMEFILTERPROXYMODEL_H
