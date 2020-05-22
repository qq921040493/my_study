#include <QtGui>
#include <QTextCodec>
#include <QApplication>
#include "showvideo.h"

int main(int argc, char *argv[])
{
    QApplication	app(argc, argv);
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("gb 18030"));

    ShowVideo	*myShowVideo;
    myShowVideo = new ShowVideo;
    myShowVideo->resize(640, 480);
    myShowVideo->show();

    return	app.exec();
}
