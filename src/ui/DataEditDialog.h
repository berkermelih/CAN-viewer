#ifndef DATAEDITDIALOG_H
#define DATAEDITDIALOG_H

#include <QByteArray>
#include <QDialog>

class QLabel;
class QTableWidget;
class QTableWidgetItem;

/**
 * @brief Mesaj veri baytlarını tablo halinde düzenleme diyaloğu (hex editör).
 *
 * Her satırda 8 bayt ve satırın ASCII karşılığı gösterilir; CAN FD'nin 64
 * baytlık verisi için kullanılır. Veri, ham ikili (.bin) veya hex metin (.txt)
 * dosyasından açılıp aynı biçimlerde kaydedilebilir. Uzunluk diyalog dışında
 * (Tx mesaj diyaloğunda) belirlenir ve burada değişmez.
 */
class DataEditDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief Diyaloğu verilen baytlarla oluşturur.
     * @param data Düzenlenecek veri; uzunluğu tablodaki bayt sayısını belirler.
     * @param parent Üst pencere.
     */
    explicit DataEditDialog(const QByteArray &data, QWidget *parent = nullptr);

    /// Düzenlenmiş veriyi döndürür (uzunluk başlangıçtaki ile aynıdır).
    QByteArray Data() const;

private:
    /// Araç çubuğu, tablo ve alt satırı oluşturur.
    void BuildUi();

    /// Tablo hücrelerini m_data içeriğiyle doldurur.
    void FillTable();

    /**
     * @brief Bir hex hücresi değiştiğinde baytı ve satırın ASCII sütununu günceller.
     * @param item Değişen hücre.
     */
    void HandleItemChanged(QTableWidgetItem *item);

    /**
     * @brief Verilen satırın ASCII sütununu yeniden yazar.
     * @param row Tablo satırı.
     */
    void UpdateAsciiCell(int row);

    /// Dosyadan veri açar (.bin ikili veya .txt hex metin).
    void OpenDataFile();

    /// Veriyi dosyaya kaydeder (.txt ise hex metin, aksi halde ikili).
    void SaveDataFile();

    /// Tüm baytları sıfırlar.
    void ClearData();

    QByteArray m_data;
    QTableWidget *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;
    bool m_updating = false;
};

#endif // DATAEDITDIALOG_H
