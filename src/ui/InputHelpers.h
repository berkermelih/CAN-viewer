#ifndef INPUTHELPERS_H
#define INPUTHELPERS_H

#include <QObject>
#include <QRegularExpressionValidator>

/**
 * @brief Girilen metni otomatik olarak büyük harfe çeviren düzenli ifade doğrulayıcısı.
 *
 * Hex alanlarında (ID, veri baytları) kullanılır: kullanıcı küçük harf yazsa da
 * alana büyük harf olarak girilir; ardından metin düzenli ifadeye göre denetlenir.
 */
class UpperCaseValidator : public QRegularExpressionValidator
{
    Q_OBJECT

public:
    /**
     * @brief Doğrulayıcıyı oluşturur.
     * @param pattern Büyük harfe çevrilmiş metnin uyması gereken ifade.
     * @param parent Qt sahiplik zinciri için üst nesne.
     */
    UpperCaseValidator(const QRegularExpression &pattern, QObject *parent = nullptr);

    /**
     * @brief Metni büyük harfe çevirir ve düzenli ifadeye göre denetler.
     * @param input Düzenlenen metin; yerinde büyük harfe çevrilir.
     * @param position İmleç konumu (değişmez).
     * @return Kabul / ara durum / geçersiz.
     */
    State validate(QString &input, int &position) const override;
};

/**
 * @brief Açılır kutuların (QComboBox) ve sayı kutularının (QSpinBox vb.) fare tekerleğiyle
 *        değişmesini engelleyen olay süzgeci.
 *
 * Uygulama geneline (QApplication) kurulur. Tekerlek bu kutuların üzerinde çevrilirse
 * değer değişmez; olay üst widget'a bırakılır, böylece kaydırılabilir alanlar kaymaya
 * devam eder. Açılır kutuda değer liste açılarak, sayı kutusunda yazarak veya oklarla seçilir.
 */
class WheelLockFilter : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Süzgeci oluşturur.
     * @param parent Qt sahiplik zinciri için üst nesne (genellikle QApplication).
     */
    explicit WheelLockFilter(QObject *parent = nullptr);

protected:
    /**
     * @brief Kapalı açılır kutulara ve sayı kutularına gelen tekerlek olaylarını yutar, üst widget'a iletilmesini sağlar.
     * @param watched Olayın hedefi.
     * @param event Olay.
     * @return Olay açılır kutuya ulaşmayacaksa true.
     */
    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // INPUTHELPERS_H
