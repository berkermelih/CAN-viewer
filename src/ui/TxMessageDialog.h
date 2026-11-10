#ifndef TXMESSAGEDIALOG_H
#define TXMESSAGEDIALOG_H

#include "../models/TxMessageModel.h"

#include <QByteArray>
#include <QDialog>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QWidget;

/**
 * @brief Gönderim listesine mesaj ekleme / düzenleme diyaloğu (PCAN-View düzeni).
 *
 * Üstte çerçeve biçimi (CAN CC / CAN FD / CAN XL) seçilir. ID hex olarak,
 * uzunluk açılır listeden girilir; 8 bayta kadar veri tek tek bayt kutularında,
 * daha uzun CAN FD verisi "..." ile açılan hex editörde düzenlenir. Periyot,
 * duraklatma, mesaj tipi (Extended, RTR, BRS) ve açıklama da bu diyalogdan
 * ayarlanır. CAN XL, PCAN-Basic tarafından desteklenmediği için seçilemez.
 */
class TxMessageDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Diyaloğu boş bir CAN CC mesajıyla oluşturur.
     * @param parent Üst pencere.
     */
    explicit TxMessageDialog(QWidget *parent = nullptr);

    /// Düzenleme için mevcut bir mesajı alanlara yükler.
    void SetMessage(const TxMessage &message);

    /**
     * @brief Bağlı donanımın CAN FD desteğine göre "CAN FD" seçeneğini etkinleştirir.
     *
     * Destek yoksa seçenek pasifleşir; FD olarak kaydedilmiş bir mesaj düzenleniyorsa
     * seçenek işaretli ama pasif kalır ve kullanıcı yalnızca CAN CC'ye geçebilir.
     * @param supported Donanım CAN FD destekliyorsa true.
     * @param reason Pasifken gösterilecek açıklama.
     */
    void SetFdSupported(bool supported, const QString &reason);

    /// Doğrulanmış mesajı döndürür (accept sonrası geçerlidir).
    TxMessage Message() const;

public slots:
    /// Girdileri doğrular; geçerliyse diyaloğu kapatır, değilse hatayı gösterir.
    void accept() override;

private:
    /// Diyalog bileşenlerini oluşturur ve yerleştirir.
    void BuildUi();

    /**
     * @brief Bayt kutuları satırını oluşturur (her kutunun altında bayt numarası).
     * @return Kutuları içeren widget.
     */
    QWidget *BuildByteEditors();

    /**
     * @brief Uzunluk listesini seçili çerçeve biçimine göre doldurur.
     * @param preferredLength Seçilmeye çalışılacak uzunluk; geçerli değilse bir üstteki seçilir.
     */
    void FillLengthCombo(int preferredLength);

    /// Seçili uzunluk.
    int CurrentLength() const;

    /// Biçim, uzunluk ve RTR seçimine göre alanları etkinleştirir / gizler.
    void UpdateFieldStates();

    /// Bayt kutularını ve uzun veri önizlemesini m_data içeriğiyle günceller.
    void RefreshDataFields();

    /**
     * @brief Bir bayt kutusu düzenlendiğinde veriyi günceller; iki hane girilince sonraki kutuya geçer.
     * @param position Bayt numarası.
     * @param text Kutudaki metin.
     */
    void HandleByteEdited(int position, const QString &text);

    /// Veri baytlarını hex editörde (DataEditDialog) düzenler.
    void EditDataInEditor();

    /// ID 0x7FF'den büyük girildiğinde Extended seçeneğini otomatik işaretler.
    void HandleIdEdited(const QString &text);

    /**
     * @brief Alanlardaki değerleri doğrulayıp mesaja çevirir.
     * @param message Başarılıysa sonuç yazılır.
     * @param errorText Hata durumunda açıklama.
     * @return Girdiler geçerliyse true.
     */
    bool ValidateInputs(TxMessage &message, QString &errorText) const;

    QRadioButton *m_ccRadio = nullptr;
    QRadioButton *m_fdRadio = nullptr;
    QRadioButton *m_xlRadio = nullptr;
    QLabel *m_formatHint = nullptr;
    bool m_fdSupported = true;
    QString m_fdUnsupportedReason;
    QLineEdit *m_idEdit = nullptr;
    QComboBox *m_lengthCombo = nullptr;
    QWidget *m_byteRow = nullptr;
    QVector<QLineEdit *> m_byteEdits;
    QLineEdit *m_longDataPreview = nullptr;
    QPushButton *m_editorButton = nullptr;
    QSpinBox *m_cycleSpin = nullptr;
    QCheckBox *m_pausedCheck = nullptr;
    QCheckBox *m_extendedCheck = nullptr;
    QCheckBox *m_rtrCheck = nullptr;
    QCheckBox *m_brsCheck = nullptr;
    QLineEdit *m_commentEdit = nullptr;
    QLabel *m_errorLabel = nullptr;

    QByteArray m_data; ///< 64 baytlık veri tamponu; geçerli kısım seçili uzunluk kadardır
    TxMessage m_message;
};

#endif // TXMESSAGEDIALOG_H
