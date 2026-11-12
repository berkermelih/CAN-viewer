#ifndef DOCKTITLEBAR_H
#define DOCKTITLEBAR_H

#include <QRect>
#include <QWidget>

class QDockWidget;
class QLabel;
class QToolButton;

/**
 * @brief Paneller (QDockWidget) için kalıcı başlık çubuğu.
 *
 * Panel oluşturulurken bir kez atanır ve bir daha değiştirilmez; böylece Qt'nin
 * ayrı pencere çerçevesini yeniden kurması ve sürükleme durumunun bozulması önlenir.
 * Sekme grubundaki panelde SetCollapsed(true) ile yüksekliği sıfıra iner (sekme
 * zaten başlık görevi görür); ayrı pencerede her zaman görünür. Başlığın boş
 * kısmındaki fare olayları panele bırakılır: sürükleyerek taşınır / birleştirilir,
 * çift tıklanınca ayrılır veya yerine döner.
 *
 * Sağ üstte: tam pencere / eski boyut (yalnızca ayrı penceredeyken), ayrı pencereye
 * al / ana pencereye yerleştir ve kapat düğmeleri bulunur.
 */
class DockTitleBar : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Başlık çubuğunu oluşturur.
     * @param dock Başlığı gösterilecek panel (üst nesne olarak da kullanılır).
     */
    explicit DockTitleBar(QDockWidget *dock);

    /**
     * @brief Başlığı gizler (yükseklik 0) veya gösterir; düğmelerin durumunu da günceller.
     * @param collapsed true ise başlık yer kaplamaz.
     */
    void SetCollapsed(bool collapsed);

    /// Başlık gizliyse true döndürür.
    bool IsCollapsed() const;

    /// Düğme ikonlarını güncel tema renkleriyle yeniden atar (tema değişince çağrılır).
    void RefreshIcons();

    /// Gizliyken sıfır, değilse başlık yüksekliğinde boyut önerir.
    QSize sizeHint() const override;

    /// sizeHint() ile aynı en küçük boyutu döndürür.
    QSize minimumSizeHint() const override;

protected:
    /// Tema değişiminde düğme ikonlarını yeniler.
    void changeEvent(QEvent *event) override;

private:
    /// Panelin bulunduğu üst düzey pencere ana pencere değilse (ayrı pencere) onu döndürür; değilse nullptr.
    QWidget *SeparateWindow() const;

    /**
     * @brief Ayrı pencereyi bulunduğu ekranın kullanılabilir alanına büyütür veya önceki boyutuna döndürür.
     *
     * Ayrı paneller çerçevesiz araç penceresi olduğundan işletim sisteminin büyütmesi yerine
     * geometri elle ayarlanır; önceki konum ve boyut saklanır.
     */
    void ToggleMaximized();

    /// Pencere ana pencereye geri yerleştiyse büyütme durumunu sıfırlar ve düğmeleri günceller.
    void UpdateButtons();

    QDockWidget *m_dock = nullptr;
    QLabel *m_titleLabel = nullptr;
    QToolButton *m_maximizeButton = nullptr;
    QToolButton *m_floatButton = nullptr;
    QToolButton *m_closeButton = nullptr;
    bool m_collapsed = false;
    bool m_maximized = false;   ///< Ayrı pencere tam pencere boyutuna büyütüldü mü
    QRect m_normalGeometry;     ///< Büyütmeden önceki pencere konumu ve boyutu
    QWidget *m_maximizedWindow = nullptr; ///< Büyütülen pencere (ayrı pencere değişirse durum sıfırlanır)
};

#endif // DOCKTITLEBAR_H
