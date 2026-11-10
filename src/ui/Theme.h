#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QFont>
#include <QIcon>

class QApplication;
class QWidget;

/**
 * @brief Uygulama genelindeki görsel tema (Claude renk paleti).
 *
 * Krem/fildişi zemin, sıcak gri metin ve terracotta vurgu rengi kullanılır.
 * Açık ve koyu iki mod vardır. Tema; Fusion stili, QPalette ve uygulama
 * geneli stil sayfası (QSS) ile uygulanır. Modeller renkleri Colors() ile
 * çizim anında okuduğu için tema değişimi anında yansır.
 */
namespace Theme {

/// Tema modu.
enum class Mode
{
    Light,
    Dark
};

/// Temanın renk tokenları.
struct Palette
{
    QColor window;        ///< Sayfa zemini
    QColor surface;       ///< Araç çubuğu, başlık, grup kutusu, durum çubuğu
    QColor base;          ///< Tablo ve giriş alanı zemini
    QColor alternateBase; ///< Tablo alternatif satır zemini
    QColor border;        ///< Çizgi ve kenarlıklar
    QColor text;          ///< Birincil metin
    QColor textSecondary; ///< İkincil metin (başlıklar, etiketler)
    QColor textMuted;     ///< Silik metin (ipuçları, devre dışı)
    QColor accent;        ///< Vurgu (terracotta)
    QColor accentHover;   ///< Vurgu üzerine gelindiğinde
    QColor accentPressed; ///< Vurgu basılıyken
    QColor accentSoft;    ///< Seçim ve hover için yumuşak vurgu zemini
    QColor onAccent;      ///< Vurgu zemini üzerindeki metin
    QColor success;       ///< Bağlı / başarılı durum
    QColor danger;        ///< Hata / bağlantı kes
    QColor warning;       ///< Uyarı
};

/**
 * @brief Temayı uygulamaya uygular (stil, palet, yazı tipi, stil sayfası).
 * @param app Uygulama nesnesi.
 * @param mode Açık veya koyu mod.
 */
void Apply(QApplication &app, Mode mode);

/// Şu an uygulanmış tema modunu döndürür.
Mode CurrentMode();

/// Şu an uygulanmış temanın renklerini döndürür.
const Palette &Colors();

/// Tablolar ve hex veriler için eş aralıklı yazı tipini döndürür.
QFont MonospaceFont();

/// Uygulama simgesini (vurgu renginde yuvarlatılmış kare) çizerek döndürür.
QIcon CreateAppIcon();

/**
 * @brief Kaynaklardaki çizgi ikonunu (":/icons/<ad>.svg") tema rengiyle boyayıp döndürür.
 *
 * İkonlar önbelleğe alınır; tema değişince önbellek temizlenir, bu yüzden
 * tema değişiminden sonra ikonlar yeniden atanmalıdır.
 * @param name İkon adı (ör. "copy", "delete", "send").
 * @param color Çizgi rengi; geçersizse ikincil metin rengi kullanılır.
 * @return İkon; dosya bulunamazsa boş ikon.
 */
QIcon Icon(const QString &name, const QColor &color = QColor());

/**
 * @brief Dinamik bir özellik değiştiğinde widget'ın stilini yeniden hesaplatır.
 * @param widget Stili yenilenecek widget; nullptr ise bir şey yapılmaz.
 */
void Repolish(QWidget *widget);

/**
 * @brief Renkli durum noktası ve metinden oluşan zengin metin üretir.
 * @param color Noktanın rengi.
 * @param text Noktanın yanındaki metin.
 */
QString StatusDotText(const QColor &color, const QString &text);

} // namespace Theme

#endif // THEME_H
