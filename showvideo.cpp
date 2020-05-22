
#include "showvideo.h"
#include "videodevice.h"

#include <QHBoxLayout>
#include <QImage>
#include <QPixmap>

ShowVideo::ShowVideo(QWidget *parent):QWidget(parent)
{
    this->myVideoDevice = new VideoDevice;			/* new一个myVideoDevice类对象	*/

    int iRet = this->myVideoDevice->open_device();	/*	打开摄像头设备				*/
    if(-1 == iRet)
    {
        this->myVideoDevice->close_device();		/*	关闭摄像头设备				*/
    }

    iRet = this->myVideoDevice->init_V4L2();		/* 初始化V4L2：初始化设备和mmap */
    if(-1 == iRet)
    {
        this->myVideoDevice->close_device();
    }

    iRet = this->myVideoDevice->start_capturing();	/*	开始采集数据				*/
    if(-1 == iRet)
    {
        this->myVideoDevice->stop_capturing();		/*	停止采集数据				*/
        this->myVideoDevice->close_device();		/*	关闭摄像头					*/
    }

    this->timer = new QTimer(this);
//	connect(this->timer, SIGNAL(timeout()), this, SLOT(update()));
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    this->timer->start(100);

    this->pChangedVideoData = (unsigned char *)malloc(640 * 480 * 3 * sizeof(char));
    if(NULL == this->pChangedVideoData)
    {
        perror("malloc error");
        exit(-1);
    }

    this->pImage = new QImage(this->pChangedVideoData, 640, 480, QImage::Format_RGB888);
    if(NULL == this->pImage)
    {
        perror("malloc pImage error");
        exit(-1);
    }

    pLabel = new QLabel();
    QHBoxLayout *pLayout = new QHBoxLayout();
    pLayout->addWidget(pLabel);
    setLayout(pLayout);
    setWindowTitle(tr("Capture"));

//    this->pLabel->setPixmap(QPixmap::fromImage(*pImage));
}

void ShowVideo::paintEvent(QPaintEvent *)
{
    qDebug()<<"Start update@@@@@@@@@@@@@@@"<<endl;
    /*	获取视频采集的初始数据	*/
    this->myVideoDevice->get_frame((void **)(&(this->pVideoData)), &uDataLength);

    /*	数据转换	*/
    convert_yuv_to_rgb_buffer(this->pVideoData, this->pChangedVideoData, 640, 480);

    /*	加载数据	*/
    this->pImage->loadFromData(this->pChangedVideoData, 640 * 480 * 3 * sizeof(char));

    /*	显示数据	*/
//	this->pLabel->setPixmap(QPixmap::fromImage(*(this->pImage), Qt::AutoColor));
    this->pLabel->setPixmap(QPixmap::fromImage(*pImage));

    /*	处理后的数据帧重新放到队列末尾		*/
    this->myVideoDevice->unget_frame();
}

/* yuv格式转换为rgb格式的算法处理函数 */
int ShowVideo::convert_yuv_to_rgb_pixel(int y, int u, int v)
{
    unsigned	int		pixel32 = 0;
    unsigned	char 	*pixel = (unsigned char *)&pixel32;
    int 				r, g, b;

    r = (int)(y + (1.370705 * (v-128)));
    g = (int)(y - (0.698001 * (v-128)) - (0.337633 * (u-128)));
    b = (int)(y + (1.732446 * (u-128)));

    if(r > 255) r = 255;
    if(g > 255) g = 255;
    if(b > 255) b = 255;
    if(r < 0) r = 0;
    if(g < 0) g = 0;
    if(b < 0) b = 0;

    pixel[0] = r * 220 / 256;
    pixel[1] = g * 220 / 256;
    pixel[2] = b * 220 / 256;

    return pixel32;
}

int ShowVideo::convert_yuv_to_rgb_buffer(unsigned char *yuv, unsigned char *rgb, unsigned int width, unsigned int height)
{
    unsigned int 	in, out = 0;
    unsigned int 	pixel_16;
    unsigned char 	pixel_24[3];
    unsigned int 	pixel32;
    int 			y0, u, y1, v;

    for(in = 0; in < width * height * 2; in += 4)
    {
        pixel_16 = yuv[in + 3] << 24 | yuv[in + 2] << 16 | yuv[in + 1] <<  8 | yuv[in + 0];
        y0 = (pixel_16 & 0x000000ff);
        u  = (pixel_16 & 0x0000ff00) >>  8;
        y1 = (pixel_16 & 0x00ff0000) >> 16;
        v  = (pixel_16 & 0xff000000) >> 24;

        pixel32 = convert_yuv_to_rgb_pixel(y0, u, v);
        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;

        rgb[out++] = pixel_24[0];
        rgb[out++] = pixel_24[1];
        rgb[out++] = pixel_24[2];

        pixel32 = convert_yuv_to_rgb_pixel(y1, u, v);
        pixel_24[0] = (pixel32 & 0x000000ff);
        pixel_24[1] = (pixel32 & 0x0000ff00) >> 8;
        pixel_24[2] = (pixel32 & 0x00ff0000) >> 16;

        rgb[out++] = pixel_24[0];
        rgb[out++] = pixel_24[1];
        rgb[out++] = pixel_24[2];
    }

    return 0;
}
/* yuv格式转换为rgb格式 */

ShowVideo::~ShowVideo()
{
    this->myVideoDevice->stop_capturing();
    this->myVideoDevice->close_device();
}
