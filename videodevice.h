
#ifndef	__VIDEODEVICE__H__
#define	__VIDEODEVICE__H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>

#include <linux/videodev2.h>
#include <stdint.h>
#include <asm/types.h>

#define	DEVICE_NAME		"/dev/video0"

class	VideoDevice
{
private:
    typedef struct VideoBuffer
    {
        unsigned char 	*pStart;
        unsigned int 	uLength;
    }VideoBuffer;

    VideoBuffer		*pBuffers;
    int				fd;
    unsigned int 	uCount = 4;
    int				uIndex;

public:
    VideoDevice();
    ~VideoDevice();

    int open_device();
    int close_device();

    int init_device();
    int init_mmap();
    int init_V4L2();

    int start_capturing();
    int stop_capturing();

    int get_frame(void **, int *);
    int unget_frame();
};

#endif
