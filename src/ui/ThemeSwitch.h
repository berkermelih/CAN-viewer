#ifndef THEMESWITCH_H
#define THEMESWITCH_H

#include <QAbstractButton>

class QPainter;
class QVariantAnimation;

/**
 * @brief Açık / koyu tema için kayan anahtar (toggle switch).
 *
 * İşaretsizken topuz solda ve içinde güneş, işaretliyken sağda ve içinde ay
 * çizilir; geçiş kısa bir animasyonla yapılır. Renkler Theme paletinden
 * çizim anında okunur. Durum değişince QAbstractButton::toggled yayılır.
 */
class ThemeSwitch : public QAbstractButton
{
    Q_OBJECT

public:
    /**
     * @brief Anahtarı oluşturur (checkable).
     * @param parent Üst widget.
     */
    explicit ThemeSwitch(QWidget *parent = nullptr);

    /// Önerilen boyutu döndürür.
    QSize sizeHint() const override;

protected:
    /// Ray, topuz ve güneş/ay simgesini çizer.
    void paintEvent(QPaintEvent *event) override;

    /// Fare üzerindeyken vurgulu çizim için yeniden çizer.
    void enterEvent(QEnterEvent *event) override;

    /// Fare ayrılınca normal çizime döner.
    void leaveEvent(QEvent *event) override;

private:
    /**
     * @brief Topuzu yeni duruma doğru kaydırır.
     * @param checked Hedef durum.
     */
    void AnimateTo(bool checked);

    /**
     * @brief Verilen dairenin içine güneş simgesi çizer.
     * @param painter Çizici.
     * @param center Merkez.
     * @param radius Topuz yarıçapı.
     */
    static void DrawSun(QPainter &painter, const QPointF &center, qreal radius);

    /**
     * @brief Verilen dairenin içine hilal (ay) simgesi çizer.
     * @param painter Çizici.
     * @param center Merkez.
     * @param radius Topuz yarıçapı.
     * @param background Hilali oymak için topuz rengi.
     */
    static void DrawMoon(QPainter &painter, const QPointF &center, qreal radius, const QColor &background);

    qreal m_position = 0.0; ///< 0: sol (açık), 1: sağ (koyu)
    bool m_hovered = false;
    QVariantAnimation *m_animation = nullptr;
};

#endif // THEMESWITCH_H
