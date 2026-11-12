#include "mainwindow.h"
#include "src/ui/InputHelpers.h"
#include "src/ui/Localization.h"
#include "src/ui/Theme.h"

#include <QApplication>
#include <QSettings>

/**
 * @brief Uygulama giriş noktası; temayı uygular, Qt uygulamasını ve ana pencereyi başlatır.
 * @param argc Komut satırı argüman sayısı.
 * @param argv Komut satırı argümanları.
 * @return Olay döngüsünün çıkış kodu.
 */
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Canbus Analyzer"));
    QApplication::setOrganizationName(QStringLiteral("CanbusAnalyzer"));
    QApplication::setWindowIcon(Theme::CreateAppIcon());

    // Onay pencerelerindeki standart düğmeler (Kaydet, İptal, Evet, Hayır ...) Türkçe gösterilir.
    InstallTurkishTranslations(a);

    // Kayıtlı tema tercihi (varsayılan: açık)
    const QSettings settings;
    const bool dark = settings.value(QStringLiteral("appearance/theme")).toString() == QLatin1String("dark");
    Theme::Apply(a, dark ? Theme::Mode::Dark : Theme::Mode::Light);

    // Açılır kutular ve sayı kutuları (Node ID vb.) fare tekerleğiyle fark edilmeden değişmesin.
    a.installEventFilter(new WheelLockFilter(&a));

    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
