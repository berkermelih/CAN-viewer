# CAN-viewer (Canbus Analyzer)

PCAN-View benzeri, Qt 6 ile yazılmış bir CAN bus görüntüleyici ve CANopen aracı.

## Özellikler

- PEAK PCAN-USB (PCAN-Basic) ve donanımsız test için sanal (loopback) kanal
- Alınan mesajlar tablosu: ID, tip, DLC, veri, periyot, sayaç, zaman ve otomatik CANopen yorumu
- Gönderim listesi: elle veya periyodik gönderim
- Filtre: tek ID, ID aralığı, maske; dahil et / hariç tut
- EDS dosyası açma ve nesne sözlüğü ağacı
- CANopen: SDO okuma/yazma (expedited + segmented), NMT komutları, SYNC, PDO çözme (EDS varsayılan eşlemeleri)

## Gereksinimler

- Qt 6 (Widgets), CMake 3.16+, GCC (Windows'ta Qt ile gelen MinGW-w64)
- PCAN donanımı için PEAK-System **PCAN-Basic** sürücüsü (`PCANBasic.dll`)

## Derleme

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.0/mingw_64
cmake --build build -j
```

Qt Creator ile `CMakeLists.txt` açılarak da derlenebilir.

## Bilinen sınırlamalar

CAN FD, hata sayaçları/hata çerçeveleri, SDO blok aktarımı ve trace görünümü henüz yok.
