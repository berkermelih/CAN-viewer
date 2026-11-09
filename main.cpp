#include "mainwindow.h"

#include <QApplication>

/**
 * @brief Uygulama giriş noktası; Qt uygulamasını ve ana pencereyi başlatır.
 * @param argc Komut satırı argüman sayısı.
 * @param argv Komut satırı argümanları.
 * @return Olay döngüsünün çıkış kodu.
 */
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Canbus Analyzer"));
    QApplication::setOrganizationName(QStringLiteral("CanbusAnalyzer"));

    MainWindow w;
    w.show();
    return QCoreApplication::exec();
}
