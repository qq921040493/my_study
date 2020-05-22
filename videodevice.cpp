#include "videodevice.h"

#include <QDebug>

VideoDevice::VideoDevice()
{
    this->pBuffers = NULL;			/***内存映射的结构体指针***/
    this->fd = -1;					/***打开的设备描述符***/
    this->uCount = 4;				/***申请的帧缓存数据***/
    this->uIndex = -1;				/***表示第几帧的下标***/
}

VideoDevice::~VideoDevice()
{

}

/**********************************************************************************
 * V4L2 的操作流程：
1. 打开摄像头设备文件， int fd = open("/dev/video0", O_RDWR);

2. 取得设备的capability, 看设备有什么功能，比如是否有视频输入，是否有视频输出，
   是否支持图片捕捉等。struct capabilty cap; capabilities 常用值V4L2_CAP_VIDEO_CAPTURE
   表示是否支持图像捕捉，设置:VIDIOC_QUERYCAP

3. 设置视频额制式和帧格式，制式包括（PAL,NTSC）PAL:表示亚洲视频标准格式
    NTSC:表示欧洲视频标准格式；帧的格式包括图片的宽度和高度等
    设置:VIDIOC_QUERYSTD, VIDIOC_S_STD, VIDIOC_S_FMT, struct v4l2_std_id,
    struct v4l2_format

4. 向内核空间申请帧缓存， 一般不超过5个，最好在2~5个之间。struct v4l2_requestbuffers

5. 将申请到的帧缓存映射到用户空间，这样就可以在内存直接操作采集到的帧数据。 mmap()

6. 将申请到的帧缓存全部入队列，循环队列存放采集到色帧数据
    设置：VIDIOC_QBUF, struct v4l2_buffer

7. 开始视频采集
    设置：VIDIOC_STREAMON

8. 出队列取得采集到的帧数据，取得原始采集到的数据
    设置：VIDIOC_DQBUF

9. 将缓存的帧数据重新入队列到队尾，这样就可以循环采集帧数据
    设置：VIDIOC_QBUF

10. 停止视频采集
    设置：VIDIOC_STREAMOFF

11. 关闭视频设备	close(fd);
***********************************************************************************/

int VideoDevice::open_device()		/***打开摄像头设备文件***/
{
    this->fd = open(DEVICE_NAME, O_RDWR, 0);
    if(-1 == this->fd)
    {
        perror("open error");
        return	-1;
    }
    qDebug()<<"Open device success"<<endl;

    return	0;
}

int VideoDevice::close_device()		/***关闭摄像头设备文件***/
{
    if(-1 ==close(fd))
    {
        return	-1;
    }

    return	0;
}

int VideoDevice::init_device()
{
    /***查询V4L2设备驱动的功能，例如设备驱动名，总线信息，版本信息***/
    struct v4l2_capability cap;		/***V4L2驱动的功能***/
    memset(&cap, 0, sizeof(cap));

    if(-1 == ioctl(this->fd, VIDIOC_QUERYCAP, &cap))	/***填充cap里面的成员***/
    {
        perror("ioctl VIDIOC_QUERYCAP error");
        return	-1;
    }

    if(! (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        return	-1;				/***查询V4L2是否支持图像捕捉功能***/
    }

    if(! (cap.capabilities & V4L2_CAP_STREAMING))
    {
        return	-1;				/***查询V4L2是支持数据流类型***/
    }

        /***打印V4L2设备的相关信息***/
    printf("Capability infrmations:\n");
    printf("diver : %s \n", cap.driver);
    printf("card : %s \n", cap.card);
    printf("bus_info : %s \n", cap.bus_info);
    printf("version : %08x \n", cap.version);
    printf("capabilities : %08x\n", cap.capabilities);

    /***设置视频捕捉的格式，例如设置视频图像数据的长、宽，图像格式(GPEG,YUYV,RGB)***/
    struct v4l2_format fmt;							/***设置获取视频格式***/
    memset(&fmt, 0, sizeof(fmt));

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;			/***视频数据流类型，永远都是这个类型***/
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;	/***视频数据的格式：GPEG,YUYV,RGB***/
    fmt.fmt.pix.width = 640;						/***视频图像的宽***/
    fmt.fmt.pix.height = 480;						/***视频图像的长***/
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;		/***设施视频的区域***/

    if(-1 == ioctl(this->fd, VIDIOC_S_FMT, &fmt))	/***设置视频捕捉格式***/
    {
        perror("ioctl VIDIOC_S_FMT error");
        return	-1;
    }
    qDebug()<<"Init device success"<<endl;

    return	0;
}

/***********************************************************
 * 	想内核空间申请帧缓存，视频采集数据之后，数据帧存放在内核空间，用户是不能访问的
既然要对采集的数据进行处理，则要把内核的数据帧放到用户空间来，则我们要将其映射到
用户空间。
    先填充好v4l2_requestbuffer的一些域，如uCount(缓存数据帧的个数)等，通过操作命令
VIDIOC_REQBUFS，根据v4l2_requestbuff的要求向内核申请帧缓存。
    有了帧缓存之后，我们申请相同个数和相同长度的用户空间，把内核的帧缓存映射到用户
空间中来，这样，应用程序就可以通过访问用户空间的地址来间接访问内核空间。因为前面
申请的帧缓存不能直接返回一个首地址，所以要借助于v4l2_buffer这个结构体来获取内核缓存
中的每一帧图像数据
************************************************************/

int VideoDevice::init_mmap()
{
    /***向内核申请帧缓存***/
    struct v4l2_requestbuffers reqbuf;			/*	向内核申请帧缓存	*/
    memset(&reqbuf, 0, sizeof(reqbuf));

    reqbuf.count = this->uCount;				/*	缓存数量--->  数据帧的个数	*/
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;	/*	数据帧流类型	*/
    reqbuf.memory = V4L2_MEMORY_MMAP;			/*	内存映射的方式采集数据	*/

    /*	VIDIOC_REQBUFS表示分配内存，调用ioctl是配置生效*/
    if(-1 == ioctl(this->fd, VIDIOC_REQBUFS, &reqbuf))
    {
        perror("ioctl VIDIOC_REQBUFS error");
        return	-1;
    }

    if((reqbuf.count < 2) || (reqbuf.count > 5))
    {
        return	-1;			/*	保证帧缓存的数量不易太大也不易太小*/
    }

    /*	在用户空间申请count个VideoBuffer个数据类型的空间，分别保存每个数据帧的首地址
        和长度	pStart, uLength;
    */
    this->pBuffers = (VideoBuffer *)calloc(reqbuf.count, sizeof(VideoBuffer));
    if(NULL == this->pBuffers)
    {
        perror("calloc error");
        return	-1;
    }

    unsigned int numBufs = 0;
    for(numBufs = 0; numBufs < reqbuf.count; numBufs ++)	/*	映射所有的帧缓存	*/
    {
        struct v4l2_buffer buf;	/*	驱动中的一帧数据，保存内核中帧缓存的相关信息的结构体*/
        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;		/*	数据流类型	*/
        buf.memory = V4L2_MEMORY_MMAP;				/*	内存映射	*/
        buf.index = numBufs;						/*	帧号		*/

        if(-1 == ioctl(this->fd, VIDIOC_QUERYBUF, &buf))	/*	获取相应的帧缓存信息	*/
        {
            perror("ioctl VIDIOC_QUERYBUF error");
            return	-1;
        }

        pBuffers[numBufs].uLength = buf.length;		/*	用户空间的长度	*/
        pBuffers[numBufs].pStart = (unsigned char *)mmap(NULL, buf.length, PROT_READ |
        PROT_WRITE, MAP_SHARED, fd, buf.m.offset);	/*	映射内存	*/
        if(NULL == pBuffers[numBufs].pStart)
        {
            perror("mmap error");
            return	-1;
        }
    }
    qDebug()<<"Init mmap success."<<endl;

    return	0;
}

int VideoDevice::init_V4L2()		/*	初始化V4L2	*/
{
    if(-1 == init_device())			/*	初始化设备	*/
    {
        return	-1;
    }

    if(-1 == init_mmap())			/*	初始化内存	*/
    {
        return	-1;
    }
    qDebug()<<"Init V4L2 success"<<endl;

    return	0;
}

int VideoDevice::start_capturing()		/*	 开始采集数据	*/
{
    unsigned int i = 0;
    for(i = 0; i < this->uCount; i ++)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        /*	将申请到的帧数据入队列，存储采集到的数据	*/
        if(-1 == ioctl(fd, VIDIOC_QBUF, &buf))
        {
            perror("ioctl VIDIOC_QBUF error");
            return	-1;
        }
    }

    /*
        启动摄像头捕捉图像，利用v4l2_buf_type枚举类型将其设置为v4l2_BUF_TYPE_CAPTURE类型
        然后使用VIDEIOC_STREAMON操作命令，根据v4l2_buf_type的值开始捕捉视频
    */

    enum v4l2_buf_type type;							/*	开始视频采集	*/
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == ioctl(this->fd, VIDIOC_STREAMON, &type))	/*	数据流类型		*/
    {
        perror("ioctl VIDIOC_STREAMON error");			/*	开始采集数据	*/
        return	-1;
    }
    qDebug()<<"Start capturing!!!!!!!!!!!!!"<<endl;

    return	0;
}

int VideoDevice::stop_capturing()
{
    enum v4l2_buf_type	type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == ioctl(this->fd, VIDIOC_STREAMOFF, &type))
    {
        perror("ioctl VIDIOC_STREAMON error");
        return	-1;
    }

    return	0;
}

int VideoDevice::get_frame(void **frame_buf, int *len)
{
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if(-1 == ioctl(this->fd, VIDIOC_DQBUF, &buf))
    {
        perror("ioctl VIDIOC_DQBUF error");
        return	-1;
    }				/*	从缓存队列中去得一帧数据	*/

    *frame_buf = this->pBuffers[buf.index].pStart;
    *len = this->pBuffers[buf.index].uLength;
    this->uIndex = buf.index;

    qDebug()<<"get frame_buf"<<endl;
    return	0;
}

int VideoDevice::unget_frame()
{
    if(-1 != this->uIndex)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = this->uIndex;

        if(-1 == ioctl(this->fd, VIDIOC_QBUF, &buf))
        {
            return	-1;		/*	把处理后的数据帧放回缓存队列，好循环存放采集的数据帧	*/
        }

        return	0;
    }

    return	-1;
}
