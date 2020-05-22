
#ifndef	__SHOWVIDEO__H__
#define	__SHOWVIDEO__H__

#include <QtGui>
#include <QLabel>
#include <QImage>
#include <QTimer>
#include <QPixmap>

#include "videodevice.h"

class ShowVideo : public QWidget
{
    Q_OBJECT

private:
    VideoDevice		*myVideoDevice;		/*	videodevice的类对象					*/
    QLabel			*pLabel;			/* 	用于显示视频图片（抓屏到的图片）	*/
    QImage			*pImage;			/* 	图片								*/
    QTimer			*timer;				/* 	定时器								*/
    unsigned char	*pChangedVideoData;	/* 	存放转换后的视频数据				*/
    unsigned char	*pVideoData;		/* 	存放获取的原始视频数据				*/
    int				uDataLength;		/* 	存放原始视频数据帧的长度			*/

    QPixmap pixmap;

public:
    ShowVideo(QWidget *parent = NULL);
    ~ShowVideo();

    int convert_yuv_to_rgb_pixel(int y, int u, int v);
    int convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height);

private slots:
    void paintEvent(QPaintEvent	*);		/*  update 的时候自动调用				*/
};

#endif
