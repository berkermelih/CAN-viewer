# CLAUDE.md — Canbus Analyzer

Bu dosya, Claude Code'un bu projede çalışırken uyması gereken kuralları içerir.

## Proje Tanımı

**Canbus Analyzer**, Qt Creator ile geliştirilen, PCAN-View benzeri bir CAN bus görüntüleyicidir:

- CAN mesajlarını canlı izleme (ID, DLC, veri, periyot, sayaç, zaman damgası).
- Mesaj ve adres (CAN ID / ID aralığı / maske) filtreleme.
- EDS (Electronic Data Sheet) dosyalarını açma ve nesne sözlüğünü (Object Dictionary) gösterme.
- EDS içeriğini kullanarak CANopen üzerinden veri gönderme ve alma (SDO, PDO, NMT, Heartbeat/EMCY).

Proje ilerleyen aşamalarda özelleştirilecektir; kapsam dışı özellikler sorulmadan eklenmemelidir.

## Teknoloji ve Uyumluluk (zorunlu)

| Konu        | Kural |
|-------------|-------|
| Dil         | **C++17**. C++20 ve sonrası özellikler kullanılmaz (`concepts`, `ranges`, `std::span`, `std::format`, `<=>` vb.). |
| Framework   | **Qt 6** (Widgets, Network). Qt 5'te kalmış/kaldırılmış API'ler (`QRegExp`, `QTextCodec`, `QLinkedList` vb.) kullanılmaz. |
| Derleme     | **CMake**, Qt Creator CMake proje akışı korunur. |
| Derleyici   | **GCC** (Windows'ta Qt ile gelen MinGW-w64 GCC). `-Wall -Wextra -pedantic` ile uyarısız derlenmeli. |
| IDE         | Qt Creator |
| CAN donanımı| **PEAK PCAN** (PCAN-USB vb.), PCAN-Basic sürücüsü üzerinden. |

- Mevcut `CMakeLists.txt` (C++17, Qt6 öncelikli) projenin temelidir; kullanıcı istemedikçe yapısı değiştirilmez. Yeni kaynak dosyalar eklenirken yalnızca `PROJECT_SOURCES` listesi ve gerekli `find_package`/`target_link_libraries` satırları güncellenir.
- Yeni bir bağımlılık eklenmeden önce Qt 6 + C++17 + GCC uyumluluğu doğrulanmalı ve kullanıcıya sorulmalıdır.

### Derleme

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=<Qt6 kurulum yolu>/mingw_64
cmake --build build -j
```

## CAN Donanımı: PCAN

- CAN erişimi PEAK'in **PCAN-Basic** kütüphanesi (`PCANBasic.dll`) ile yapılır. Kütüphane çalışma zamanında `QLibrary` ile yüklenir (`src/can/PcanInterface`); PEAK başlık dosyaları ve Qt Serial Bus modülü gerekmez. Kullanılan API sabitleri/yapıları `PcanInterface` içinde PCANBasic.h ile birebir tanımlıdır.
- DLL bulunamadığında uygulama açık bir hata mesajı gösterir.
- Mevcut: bağlı PCAN kanallarının otomatik algılanması (`PCAN_ATTACHED_CHANNELS`, eski sürücüde USBBUS1..16 yoklaması; algılanana kadar "Port seç" görünür, bağlı değilken 2 sn'de bir yeniden taranır), CAN CC bit hızı (5 kbit/s – 1 Mbit/s), **CAN FD** (`CAN_InitializeFD/ReadFD/WriteFD`, 80 MHz saat; nominal 125 kbit/s – 1 Mbit/s, veri 1 – 10 Mbit/s, BRS), bağlan/kes, bus durumu (OK / Light / Warning / Passive / Off). Alım kuyruğu 1 ms zamanlayıcı ile bloklamadan okunur.
- **CAN XL desteklenmez** (PCAN-Basic API'si sunmaz); Tx diyaloğunda seçenek görünür ama devre dışıdır.
- Henüz yok: hata sayaçları, hata çerçevelerinin gösterimi, CANopen FD (CiA 1301) yorumu.
- Donanım katmanı bir arayüz (`ICanInterface`, `CanBusSettings`) arkasında tutulur. **Sanal (loopback)** arayüz yalnızca test içindir, port listesinde gösterilmez. PCAN dışında arayüz eklenmesi kullanıcıya sorulur.

## Kodlama Kuralları

- **Fonksiyon adları PascalCase**: `OpenEdsFile`, `ApplyMessageFilter`, `SendSdoWrite`.
  Sınıf/tür adları da PascalCase. Üye değişkenler `m_` önekli camelCase (`m_rxModel`).
  Qt'nin override edilen sanal fonksiyonları (`paintEvent`, `data`, `rowCount` vb.) Qt'deki adıyla kalır.
- **Her fonksiyonun önünde ne işe yaradığını açıklayan yorum** bulunmalıdır (başlık dosyasında bildirimde, gerekirse kaynakta tanımda). Parametreler, dönüş değeri ve önemli yan etkiler belirtilir. Doxygen uyumlu `///` veya `/** */` kullanılır:

```cpp
/**
 * @brief Verilen yoldaki EDS dosyasını okuyup nesne sözlüğünü doldurur.
 * @param filePath Açılacak .eds dosyasının tam yolu.
 * @return Başarılıysa nesne sözlüğü, hata durumunda std::nullopt.
 */
std::optional<ObjectDictionary> OpenEdsFile(const QString &filePath);
```

- Bellek yönetimi: Qt parent-child sahipliği, `std::unique_ptr` / `std::make_unique`.
- `nullptr`, `override`, `explicit`, `enum class`, `std::optional` gibi modern C++17 araçları tercih edilir.
- Sinyal/slot bağlantılarında fonksiyon işaretçili `connect` sözdizimi kullanılır.
- UI iş parçacığı bloke edilmez; CAN okuma olay tabanlı (`framesReceived` sinyali) veya ayrı iş parçacığında yapılır.
- İstekle ilgisiz değişiklik yapılmaz; mevcut düzen korunur.

## Mimari (hedef)

Katmanlar birbirinden ayrı tutulur; CANopen mantığı donanımdan bağımsız olmalıdır.

```
src/
  can/        ICanInterface soyutlaması, PcanInterface (PCANBasic.dll / QLibrary), VirtualCanInterface (loopback)
  canopen/    NMT, SDO, PDO, Heartbeat, EMCY — yalnızca CAN frame'leri üzerinde çalışır
  eds/        EDS/DCF (INI tabanlı, CiA 306) ayrıştırıcı, ObjectDictionary modeli ve DcfWriter
  update/     UpdateChecker (GitHub Releases, Qt Network)
  filter/     ID / aralık / maske / veri filtreleri
  ui/         Paneller (CanOpenPanel, FilterPanel), diyaloglar (TxMessageDialog, DataEditDialog), ThemeSwitch ve Theme (renk paleti, QSS, ikonlar)
  models/     RxMessageModel, TxMessageModel, TxListFile (.cantx), TraceModel, FrameFilterProxyModel (Rx ve Trace için ortak filtre)
resources/    Qt kaynakları (resources.qrc; çizgi ikonları `currentColor` ile, Theme::Icon tema rengine boyar)
docs/         Doxygen yapılandırması ve dokümantasyon (Docker ile üretilecek)
tests/        Qt Test birim testleri (EDS ayrıştırma, filtreler, SDO kodlama)
```

- `main.cpp` ve `mainwindow.*` (Qt Creator şablonu) proje kökünde kalır; diğer kaynaklar `src/` altındadır ve CMake'te `PROJECT_SOURCES` listesine eklenir. `src/` içi include'lar göreli yoldur (`../can/CanFrame.h`).
- Doğrulanmamış protokol davranışı varmış gibi sunulmaz; desteklenmeyen özellikler arayüzde açıkça belirtilir.

## İşlevsel Kapsam

- Rx tablosu: ID, tip (Std/Ext/RTR), DLC, veri (hex), periyot, sayaç, son zaman; verisi değişen satırlar kısa süre vurgulanır.
- Trace: tüm Rx/Tx çerçevelerinin kronolojik kaydı (No, zaman, Δt, yön, ID, veri, CANopen yorumu); duraklat, temizle, isteğe bağlı filtre, CSV dışa aktarma; en fazla 100 000 kayıt. Zaman, Rx ve Tx için ortak uygulama saatidir (donanım zaman damgası değil).
- Tx tablosu: elle / periyodik mesaj gönderme. Mesaj diyaloğu PCAN-View düzenindedir (CAN CC / FD / XL, ID, uzunluk, bayt kutuları, "..." ile hex editör, periyot, duraklat, Extended/RTR/BRS, açıklama). Liste `.cantx` (JSON, `TxListFile`) olarak kaydedilir/açılır; son kullanılanlar menüsü ve kaydedilmemiş değişiklik uyarısı vardır.
- Sağ tık menüleri: Tx (yeni, düzenle, gönder, kes/kopyala/yapıştır, sil, tümünü temizle, ID/veri biçimi, listeyi aç/kaydet), Rx (kopyala, satırı sil, temizle, biçim), Trace (kopyala, duraklat, filtre, temizle, CSV, biçim). ID hex/dec, veri hex/dec/ASCII gösterilebilir.
- Paneller (Mesajlar, Trace, CANopen/EDS, Filtre) `QDockWidget`'tır: sekmeden sürüklenip ayrılabilir veya ekran bölünebilir; Görünüm → Pencere düzeni ile sekmeli / üç bölmeli düzen seçilir, düzen QSettings'te saklanır.
- Filtreler: tek ID, ID aralığı, maske, **bit** (belirli ID'nin belirli bayt/bitinin 0/1 olması), dahil et/hariç tut.
- EDS / **DCF**: dosya açma (.eds, .dcf; DCF'te ParameterValue "DCF değeri" sütununda, [DeviceComissioning] NodeID hedef düğüme uygulanır), nesne ağacı (index/subindex, ad, tip, erişim, varsayılan değer), seçili nesneye SDO okuma/yazma. Ağaç CiA 301 alanlarına göre başlıklara ayrılır: Dosya/cihaz bilgisi, Veri tipleri (0001–0FFF), Temel — Communication (1000–1FFF), Vendor specific (2000–5FFF), Device specific (6000–9FFF), Diğer (A000–FFFF). Index/ad ile arama yapılabilir.
- Parametre seti: rw parametreler cihazdan sırayla okunup DCF olarak kaydedilir (`DcfWriter`, kaynak EDS metni korunur, CompactSubObj nesnelerine değer yazılmaz); kayıtlı DCF cihaza CiA 301 sırasıyla yazılır (PDO devre dışı → eşleme sub0=0/girdiler/sub0=n → diğerleri → COB-ID son değer → isteğe bağlı 1010h:01 "save"). Art arda 3 SDO zaman aşımında işlem durur.
- Güncelleme denetimi (Yardım menüsü): GitHub Releases `releases/latest` etiketi `src/AppInfo.h` içindeki `Version` ile karşılaştırılır (Qt Network). Yayın etiketi `v<Version>` biçiminde olmalıdır.

## Arayüz Teması

- Renkler `src/ui/Theme` içindeki tokenlardan gelir (Claude paleti: fildişi zemin, sıcak gri metin, terracotta vurgu `#D97757`); açık ve koyu mod vardır, tercih QSettings'te saklanır. Sağ üstte ay/güneş simgeli kayan anahtar (`ThemeSwitch`) ve Görünüm menüsü aynı ayarı değiştirir.
- Widget'larda sabit renk/`setStyleSheet` kullanılmaz; bunun yerine stil sayfasındaki roller kullanılır: `objectName` = `SectionTitle`, `MutedText`, `ErrorText`, `CounterBadge`, `BrandLabel`, `PrimaryAction`; birincil düğmeler için `setProperty("primary", true)`.
- Modellerdeki renkler (ör. vurgu) setter ile verilir; modeller `ui/` katmanına bağımlı olmaz.
- CANopen: Node ID seçimi, NMT komutları, PDO eşlemelerinin EDS'ten çözülmesi.
- Mesajlar, adresler ve EDS içeriği için açık doğrulama ve hata gösterimi.

## Dokümantasyon

- Kod içi dokümantasyon Doxygen formatındadır (yukarıdaki yorum kuralı).
- Dokümantasyon üretimi **Docker** ile yapılacak (ör. Doxygen + Graphviz içeren bir imaj, `docs/Dockerfile`).
  Ayrıntılı iş akışı ileride belirlenip bu bölüme eklenecek; o zamana kadar Docker yapılandırması sorulmadan eklenmez.

## MCP Sunucuları

MCP sunucuları https://github.com/punkpeye/awesome-mcp-servers listesinden seçilecektir.
Henüz hiçbir MCP bağlantısı yapılmamıştır. Eklemeden önce kaynak, bakım durumu ve istenen izinler incelenmeli ve kullanıcı onayı alınmalıdır.

Listeden ön incelemede çıkan adaylar (doğrulanmadı, sadece başlangıç noktası):

| Amaç | Aday |
|------|------|
| GitHub depo gezinme | `Ryan0204/github-repo-mcp` |
| Kod indeksleme + git | `srclight/srclight` |
| clangd tabanlı kod arama | `JSungMin/vs-token-safer` |
| Hata ayıklama (LLDB) | `stass/lldb-mcp` |
| Docker işlemleri (doküman imajı) | `AIops-tools/Container-Host-AIops` |
| Seri port (gerekirse) | `adancurusul/serial-mcp-server` |

Not: Listede CAN / CANopen için bir MCP sunucusu bulunamadı; bu işlevsellik uygulamanın kendisinde geliştirilecektir.
