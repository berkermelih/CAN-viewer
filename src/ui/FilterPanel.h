#ifndef FILTERPANEL_H
#define FILTERPANEL_H

#include "../filter/MessageFilter.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QTableWidgetItem;

/**
 * @brief Mesaj/adres filtre kurallarını düzenleyen panel.
 *
 * Kurallar tek ID, ID aralığı, maske veya bit (belirli bir ID'nin belirli bir
 * veri bitinin 0/1 olması) olarak eklenir ve dahil et / hariç tut olarak
 * işaretlenir. Her değişiklikte FilterChanged yayılır.
 */
class FilterPanel : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Paneli oluşturur.
     * @param parent Üst widget.
     */
    explicit FilterPanel(QWidget *parent = nullptr);

    /// Güncel filtreyi döndürür; nesne panel yaşadığı sürece geçerlidir.
    const MessageFilter &Filter() const;

signals:
    /// Filtre açıldığında/kapandığında veya kurallar değiştiğinde yayılır.
    void FilterChanged();

private:
    /// Panel bileşenlerini oluşturur ve yerleştirir.
    void BuildUi();

    /// Giriş alanlarındaki değerlerden yeni kural oluşturup ekler.
    void AddRuleFromInputs();

    /// Tabloda seçili kuralları siler.
    void RemoveSelectedRules();

    /// Tüm kuralları siler.
    void ClearRules();

    /// Kural tablosunu filtre içeriğinden yeniden doldurur.
    void RefreshTable();

    /// Tablodaki "Etkin" onay kutusu değişikliğini filtreye uygular.
    void HandleItemChanged(QTableWidgetItem *item);

    /// Seçili moda göre ikinci giriş alanının etiketini ve durumunu günceller.
    void UpdateInputState();

    MessageFilter m_filter;
    QCheckBox *m_enabledCheck = nullptr;
    QTableWidget *m_table = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QComboBox *m_actionCombo = nullptr;
    QLineEdit *m_firstEdit = nullptr;
    QLineEdit *m_secondEdit = nullptr;
    QLabel *m_secondLabel = nullptr;
    QFormLayout *m_form = nullptr;
    QWidget *m_bitRow = nullptr;
    QSpinBox *m_byteSpin = nullptr;
    QSpinBox *m_bitSpin = nullptr;
    QComboBox *m_bitValueCombo = nullptr;
    QLabel *m_errorLabel = nullptr;
    bool m_updatingTable = false;
};

#endif // FILTERPANEL_H
