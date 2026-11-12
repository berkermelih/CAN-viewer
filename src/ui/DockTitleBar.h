#ifndef DOCKTITLEBAR_H
#define DOCKTITLEBAR_H

#include <QPoint>
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
 * kısmındaki fare olayları panele bırakılır: sürükleyerek taşınır / ana pencereye
 * yerleştirilir, çift tıklanınca ayrılır veya yerine döner.
 *
 * Sağ üstte: tam pencere / eski boyut (yalnızca ayrı penceredeyken), ayrı pencereye
 * al / ana pencereye yerleştir ve kapat düğmeleri bulunur. Ayrı penceredeyken
 * "ana pencereye yerleştir" düğmesi ve başlığa çift tıklama DockRequested() sinyalini
 * yayar; yerleştirme ana pencere tarafından yapılır.
 *
 * Tam pencere durumu pencerenin özelliklerinde saklanır ve pencere taşınır/boyutlanırsa
 * kendiliğinden geçersiz olur. Tam penceredeyken sürükleme başlayınca ve yerleştirmeden
 * önce pencere eski boyutuna döner.
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

signals:
    /// Ayrı penceredeki panelin ana pencereye yerleştirilmesi istendi (düğme veya başlığa çift tıklama).
    void DockRequested();

protected:
    /// Tema değişiminde düğme ikonlarını yeniler.
    void changeEvent(QEvent *event) override;

    /**
     * @brief Ayrı penceredeyken başlığa çift tıklamayı DockRequested() sinyaline çevirir.
     *
     * Ana penceredeyken olay panele bırakılır (Qt paneli ayrı pencereye alır).
     * @param event Fare olayı.
     */
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    /**
     * @brief Tam penceredeki ayrı panelde sürükleme beklemeye başlar; diğer durumlarda olayı panele bırakır.
     * @param event Fare olayı.
     */
    void mousePressEvent(QMouseEvent *event) override;

    /**
     * @brief Tam penceredeki panel sürüklenmeye başlayınca eski boyutuna döndürüp sürüklemeyi Qt'ye devreder.
     * @param event Fare olayı.
     */
    void mouseMoveEvent(QMouseEvent *event) override;

    /**
     * @brief Sürüklenmeden bırakılan basmayı sonlandırır (boyut değişmez).
     * @param event Fare olayı.
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    /// Panelin bulunduğu üst düzey pencere ana pencere değilse (ayrı pencere) onu döndürür; değilse nullptr.
    QWidget *SeparateWindow() const;

    /**
     * @brief Pencere tam pencere boyutundaysa true döndürür.
     *
     * Saklanan büyütülmüş geometri pencerenin güncel geometrisiyle aynı olmalıdır; pencere
     * sonradan taşındıysa veya boyutlandıysa durum geçersiz sayılır.
     * @param window Denetlenecek üst düzey pencere.
     */
    static bool IsWindowMaximized(const QWidget *window);

    /**
     * @brief Ayrı pencereyi bulunduğu ekranın kullanılabilir alanına büyütür veya önceki boyutuna döndürür.
     *
     * Ayrı paneller çerçevesiz araç penceresi olduğundan işletim sisteminin büyütmesi yerine
     * geometri elle ayarlanır; önceki konum ve boyut pencerenin özelliklerinde saklanır.
     */
    void ToggleMaximized();

    /**
     * @brief Tam penceredeki pencereyi önceki boyutuna, imleç başlıkta kalacak şekilde döndürür.
     * @param window Ayrı pencere.
     * @param cursor İmlecin ekran konumu.
     */
    static void RestoreForDrag(QWidget *window, const QPoint &cursor);

    /**
     * @brief Ayrı pencere tam penceredeyse önceki boyutuna döndürür.
     *
     * Ana pencereye yerleştirmeden önce çağrılır; aksi halde Qt panelin ayrı pencere boyutunu
     * ekran boyutu olarak hatırlar ve panel sonra yeniden ayrılınca ekranı kaplar.
     */
    void RestoreIfMaximized();

    /**
     * @brief Pencere bulunduğu ekranın kullanılabilir alanının büyük kısmını (%90+) kaplıyorsa true döndürür.
     * @param window Denetlenecek üst düzey pencere.
     */
    static bool CoversScreen(const QWidget *window);

    /**
     * @brief Yeni ayrılan panel ekranı kaplıyorsa (Qt eski tam pencere boyutunu hatırladıysa) makul boyuta getirir.
     *
     * Panel ayrı pencereye geçince olay döngüsünden çağrılır; fare basılıyken (sürüklemede) veya
     * tam pencere bilerek seçildiyse bir şey yapmaz.
     */
    void FitFloatingWindow();

    /// Düğmelerin görünürlüğünü, ipuçlarını ve ikonlarını güncel duruma göre ayarlar.
    void UpdateButtons();

    QDockWidget *m_dock = nullptr;
    QLabel *m_titleLabel = nullptr;
    QToolButton *m_maximizeButton = nullptr;
    QToolButton *m_floatButton = nullptr;
    QToolButton *m_closeButton = nullptr;
    bool m_collapsed = false;
    bool m_maximizedPressed = false; ///< Tam penceredeki panelin başlığına basıldı (sürükleme bekleniyor)
    QPoint m_pressGlobal;            ///< Basılan ekran konumu
};

#endif // DOCKTITLEBAR_H
