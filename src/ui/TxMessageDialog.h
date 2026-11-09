#ifndef TXMESSAGEDIALOG_H
#define TXMESSAGEDIALOG_H

#include "../models/TxMessageModel.h"

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QSpinBox;

/**
 * @brief Gönderim listesine mesaj ekleme / düzenleme diyaloğu.
 *
 * ID ve veri hex olarak girilir. RTR olmayan çerçevelerde DLC, girilen veri
 * bayt sayısından otomatik belirlenir. Tamam'a basıldığında girdiler doğrulanır.
 */
class TxMessageDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Diyaloğu boş bir mesajla oluşturur.
     * @param parent Üst pencere.
     */
    explicit TxMessageDialog(QWidget *parent = nullptr);

    /// Düzenleme için mevcut bir mesajı alanlara yükler.
    void SetMessage(const TxMessage &message);

    /// Doğrulanmış mesajı döndürür (accept sonrası geçerlidir).
    TxMessage Message() const;

public slots:
    /// Girdileri doğrular; geçerliyse diyaloğu kapatır, değilse hatayı gösterir.
    void accept() override;

private:
    /**
     * @brief Alanlardaki değerleri doğrulayıp mesaja çevirir.
     * @param message Başarılıysa sonuç yazılır.
     * @param errorText Hata durumunda açıklama.
     * @return Girdiler geçerliyse true.
     */
    bool ValidateInputs(TxMessage &message, QString &errorText) const;

    /// RTR seçimine göre DLC ve veri alanlarını etkinleştirir.
    void UpdateRtrState();

    QLineEdit *m_idEdit = nullptr;
    QCheckBox *m_extendedCheck = nullptr;
    QCheckBox *m_rtrCheck = nullptr;
    QSpinBox *m_dlcSpin = nullptr;
    QLineEdit *m_dataEdit = nullptr;
    QSpinBox *m_periodSpin = nullptr;
    QLineEdit *m_commentEdit = nullptr;
    QLabel *m_errorLabel = nullptr;
    TxMessage m_message;
};

#endif // TXMESSAGEDIALOG_H
