#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

/*
 * TODO List:
 * Data read
 * save raw data
 * use windows named pipes
 * data analysis
 * logs
 *
 */

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "EMGReader_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w;
    w.show();
    return a.exec();
}
