#ifndef MESSAGEFILTER_H
#define MESSAGEFILTER_H

#include "../can/CanFrame.h"

#include <QString>
#include <QVector>

/// Filtre kuralının ID eşleştirme yöntemi.
enum class FilterMode
{
    SingleId, ///< Tek bir ID ile birebir eşleşme
    IdRange,  ///< first..second (dahil) aralığı
    Mask      ///< (id & second) == (first & second)
};

/// Tek bir filtre kuralı.
struct FilterRule
{
    bool enabled = true;                  ///< Kural etkin mi
    bool include = true;                  ///< true: dahil et, false: hariç tut
    FilterMode mode = FilterMode::SingleId;
    quint32 first = 0;                    ///< ID, aralık başı veya maske karşılaştırma değeri
    quint32 second = 0;                   ///< Aralık sonu veya maske
};

/**
 * @brief CAN ID'ye göre dahil et / hariç tut filtre kümesi.
 *
 * Kurallar şöyle değerlendirilir: eşleşen herhangi bir "hariç tut" kuralı
 * çerçeveyi reddeder; en az bir etkin "dahil et" kuralı varsa çerçeve
 * bunlardan en az biriyle eşleşmelidir. Filtre kapalıysa her çerçeve geçer.
 */
class MessageFilter
{
public:
    /// Filtrenin tamamını açar veya kapatır.
    void SetEnabled(bool enabled);

    /// Filtrenin açık olup olmadığını döndürür.
    bool IsEnabled() const;

    /// Yeni bir kural ekler.
    void AddRule(const FilterRule &rule);

    /**
     * @brief Verilen sıradaki kuralı siler.
     * @param row Kural sırası; geçersizse bir şey yapılmaz.
     */
    void RemoveRule(int row);

    /**
     * @brief Verilen sıradaki kuralı etkinleştirir veya devre dışı bırakır.
     * @param row Kural sırası.
     * @param enabled Yeni durum.
     */
    void SetRuleEnabled(int row, bool enabled);

    /// Tüm kuralları siler.
    void ClearRules();

    /// Kural listesini döndürür.
    const QVector<FilterRule> &Rules() const;

    /**
     * @brief Çerçevenin filtreden geçip geçmediğini döndürür.
     * @param frame Değerlendirilecek çerçeve.
     * @return Çerçeve gösterilmeliyse true.
     */
    bool Accepts(const CanFrame &frame) const;

    /**
     * @brief Tek bir kuralın verilen ID ile eşleşip eşleşmediğini döndürür.
     * @param rule Kural.
     * @param id CAN ID.
     */
    static bool RuleMatches(const FilterRule &rule, quint32 id);

    /**
     * @brief Kuralı kullanıcıya gösterilecek kısa metne çevirir.
     * @param rule Kural.
     * @return Örn. "180..1FF" veya "580 / maske 780".
     */
    static QString DescribeRule(const FilterRule &rule);

    /// Kural modunun okunabilir adını döndürür.
    static QString ModeName(FilterMode mode);

private:
    bool m_enabled = false;
    QVector<FilterRule> m_rules;
};

#endif // MESSAGEFILTER_H
