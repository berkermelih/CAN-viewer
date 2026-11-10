#include "mainwindow.h"
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

    // Kayıtlı tema tercihi (varsayılan: açık)
    const QSettings settings;
    const bool dark = settings.value(QStringLiteral("appearance/theme")).toString() == QLatin1String("dark");
    Theme::Apply(a, dark ? Theme::Mode::Dark : Theme::Mode::Light);

    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
