/*v4l2_example.c*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/time.h>


#include <opencv2/core.hpp>
// #include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

using namespace cv;
using namespace std;

#define TRUE            (1)
#define FALSE           (0)

#define FILE_VIDEO      "/dev/video0"
#define IMAGE           "./img/demo"

// #define IMAGEWIDTH      1280
// #define IMAGEHEIGHT     720

#define IMAGEWIDTH      1920
#define IMAGEHEIGHT     1080

#define FRAME_NUM       10

int fd;
struct v4l2_buffer buf;

typedef struct
{
    void * start;
    unsigned int length;
    long long int timestamp;
} buffer;

buffer* buffers;


int v4l2_init()
{
    struct v4l2_capability cap;
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_format fmt;
    struct v4l2_streamparm stream_para;

    //打开摄像头设备
    if ((fd = open(FILE_VIDEO, O_RDWR)) == -1)
    {
        printf("Error opening V4L interface\n");
        return FALSE;
    }

    //查询设备属性
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1)
    {
        printf("Error opening device %s: unable to query device.\n",FILE_VIDEO);
        return FALSE;
    }
    else
    {
        printf("driver:\t\t%s\n",cap.driver);
        printf("card:\t\t%s\n",cap.card);
        printf("bus_info:\t%s\n",cap.bus_info);
        printf("version:\t%d\n",cap.version);
        printf("capabilities:\t%x\n",cap.capabilities);

        if ((cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == V4L2_CAP_VIDEO_CAPTURE)
        {
            printf("Device %s: supports capture.\n",FILE_VIDEO);
        }

        if ((cap.capabilities & V4L2_CAP_STREAMING) == V4L2_CAP_STREAMING)
        {
            printf("Device %s: supports streaming.\n",FILE_VIDEO);
        }
    }


    //显示所有支持帧格式
    fmtdesc.index=0;
    fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    printf("Support format:\n");
    while(ioctl(fd,VIDIOC_ENUM_FMT,&fmtdesc)!=-1)
    {
        printf("\t%d.%s\n",fmtdesc.index+1,fmtdesc.description);
        fmtdesc.index++;
    }

    //检查是否支持某帧格式
    struct v4l2_format fmt_test;
    fmt_test.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt_test.fmt.pix.pixelformat=V4L2_PIX_FMT_UYVY;
    if(ioctl(fd,VIDIOC_TRY_FMT,&fmt_test)==-1)
    {
        printf("not support format V4L2_PIX_FMT_UYVY!\n");
    }
    else
    {
        printf("support format V4L2_PIX_FMT_UYVY\n");
    }


    //查看及设置当前格式
    printf("set fmt...\n");
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY; //jpg格式
    //fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;//yuv格式

    fmt.fmt.pix.height = IMAGEHEIGHT;
    fmt.fmt.pix.width = IMAGEWIDTH;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    printf("fmt.type:\t\t%d\n",fmt.type);
    printf("pix.pixelformat:\t%c%c%c%c\n",fmt.fmt.pix.pixelformat & 0xFF, (fmt.fmt.pix.pixelformat >> 8) & 0xFF,(fmt.fmt.pix.pixelformat >> 16) & 0xFF, (fmt.fmt.pix.pixelformat >> 24) & 0xFF);
    printf("pix.height:\t\t%d\n",fmt.fmt.pix.height);
    printf("pix.width:\t\t%d\n",fmt.fmt.pix.width);
    printf("pix.field:\t\t%d\n",fmt.fmt.pix.field);
    if(ioctl(fd, VIDIOC_S_FMT, &fmt) == -1)
    {
        printf("Unable to set format\n");
        return FALSE;
    }

    printf("get fmt...\n");
    if(ioctl(fd, VIDIOC_G_FMT, &fmt) == -1)
    {
        printf("Unable to get format\n");
        return FALSE;
    }
    {
        printf("fmt.type:\t\t%d\n",fmt.type);
        printf("pix.pixelformat:\t%c%c%c%c\n",fmt.fmt.pix.pixelformat & 0xFF, (fmt.fmt.pix.pixelformat >> 8) & 0xFF,(fmt.fmt.pix.pixelformat >> 16) & 0xFF, (fmt.fmt.pix.pixelformat >> 24) & 0xFF);
        printf("pix.height:\t\t%d\n",fmt.fmt.pix.height);
        printf("pix.width:\t\t%d\n",fmt.fmt.pix.width);
        printf("pix.field:\t\t%d\n",fmt.fmt.pix.field);
    }

    //设置及查看帧速率，这里只能是30帧，就是1秒采集30张图
    // memset(&stream_para, 0, sizeof(struct v4l2_streamparm));
    // stream_para.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    // stream_para.parm.capture.timeperframe.denominator = 30;
    // stream_para.parm.capture.timeperframe.numerator = 1;

    // if(ioctl(fd, VIDIOC_S_PARM, &stream_para) == -1)
    // {
    //     printf("Unable to set frame rate\n");
    //     return FALSE;
    // }
    // if(ioctl(fd, VIDIOC_G_PARM, &stream_para) == -1)
    // {
    //     printf("Unable to get frame rate\n");
    //     return FALSE;
    // }
    // {
    //     printf("numerator:%d\ndenominator:%d\n",stream_para.parm.capture.timeperframe.numerator,stream_para.parm.capture.timeperframe.denominator);
    // }
    return TRUE;
}



int v4l2_mem_ops()
{
    unsigned int n_buffers = 0;
    struct v4l2_requestbuffers req = {0};

    //申请帧缓冲
    req.count=FRAME_NUM;
    req.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory=V4L2_MEMORY_MMAP;
    if(ioctl(fd,VIDIOC_REQBUFS,&req)==-1)
    {
        printf("request for buffers error\n");
        return FALSE;
    }
    printf("req count %d\n",req.count);
    // 申请用户空间的地址列
    buffers = (buffer*)malloc(req.count*sizeof (*buffers));
    if (!buffers)
    {
        printf ("out of memory!\n");
        return FALSE;
    }

    // 进行内存映射
    for (n_buffers = 0; n_buffers < FRAME_NUM; n_buffers++)
    {
        struct v4l2_buffer buf = {0};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = n_buffers;
        //查询
        if (ioctl (fd, VIDIOC_QUERYBUF, &buf) == -1)
        {
            printf("query buffer error\n");
            return FALSE;
        }

        //映射
        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start = mmap(NULL,buf.length,PROT_READ|PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

        printf("buf addr 0x%llx length %d \n", buffers[n_buffers].start, buffers[n_buffers].length);
        if (buffers[n_buffers].start == MAP_FAILED)
        {
            printf("buffer map error\n");
            return FALSE;
        }
    }
    return TRUE;
}



int v4l2_frame_process()
{
    unsigned int n_buffers;
    enum v4l2_buf_type type;
    char file_name[100];
    char index_str[10];
    long long int extra_time = 0;
    long long int cur_time = 0;
    long long int last_time = 0;

    //入队和开启采集
    for (n_buffers = 0; n_buffers < FRAME_NUM; n_buffers++)
    {
        buf.index = n_buffers;
        ioctl(fd, VIDIOC_QBUF, &buf);
    }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMON, &type);



    //出队，处理，写入yuv文件，入队，循环进行
    int loop = 0;

    while(loop < 10)
    {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv = {0};
        tv.tv_sec = 2;
        int r = select(fd+1, &fds, NULL, NULL, &tv);
        if(-1 == r)
        {
            perror("Waiting for Frame");
            return 1;
        }


        n_buffers = loop;

        //出队
        buf.index = n_buffers;
        ioctl(fd, VIDIOC_DQBUF, &buf);

        //查看采集数据的时间戳之差，单位为微妙
        buffers[n_buffers].timestamp = buf.timestamp.tv_sec*1000000+buf.timestamp.tv_usec;
        cur_time = buffers[n_buffers].timestamp;
        extra_time = cur_time - last_time;
        last_time = cur_time;
        printf("time_deta:%lld\n\n",extra_time);
        printf("buf_len:%d\n",buffers[n_buffers].length);

        Mat imageUYVY(IMAGEHEIGHT,IMAGEWIDTH, CV_8UC2, buffers[n_buffers].start);
        Mat channels[2];
        // cvtColor(image422, imageBGR, COLOR_YUV2RGB_Y422);
        split(imageUYVY, channels);

        imshow("live", channels[1]);
        waitKey(50);

        //处理数据只是简单写入文件，名字以loop的次数和帧缓冲数目有关
        printf("grab image data OK\n");
        memset(file_name,0,sizeof(file_name));
        memset(index_str,0,sizeof(index_str));
        sprintf(index_str,"%d",loop);
        strcpy(file_name,IMAGE);
        strcat(file_name,index_str);
        // strcat(file_name,".jpg");
        strcat(file_name,".yuv");
        FILE *fp2 = fopen(file_name, "wb");
        if(!fp2)
        {
            printf("open %s error\n",file_name);
            return(FALSE);
        }
        fwrite(buffers[n_buffers].start, IMAGEHEIGHT*IMAGEWIDTH*2,1,fp2);
        fclose(fp2);
        printf("save %s OK\n",file_name);

        //入队循环
        ioctl(fd, VIDIOC_QBUF, &buf);

        loop++;
    }
    return TRUE;
}




int v4l2_release()
{
    unsigned int n_buffers;
    enum v4l2_buf_type type;

    //关闭流
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMON, &type);

    //关闭内存映射
    for(n_buffers=0;n_buffers<FRAME_NUM;n_buffers++)
    {
        munmap(buffers[n_buffers].start,buffers[n_buffers].length);
    }

    //释放自己申请的内存
    free(buffers);

    //关闭设备
    close(fd);
    return TRUE;
}




/*int v4l2_video_input_output()
{
    struct v4l2_input input;
    struct v4l2_standard standard;

    //首先获得当前输入的index,注意只是index，要获得具体的信息，就的调用列举操作
    memset (&input,0,sizeof(input));
    if (-1 == ioctl (fd, VIDIOC_G_INPUT, &input.index)) {
        printf("VIDIOC_G_INPUT\n");
        return FALSE;
    }
    //调用列举操作，获得 input.index 对应的输入的具体信息
    if (-1 == ioctl (fd, VIDIOC_ENUMINPUT, &input)) {
        printf("VIDIOC_ENUM_INPUT \n");
        return FALSE;
    }
    printf("Current input %s supports:\n", input.name);


    //列举所有的所支持的 standard，如果 standard.id 与当前 input 的 input.std 有共同的
    //bit flag，意味着当前的输入支持这个 standard,这样将所有驱动所支持的 standard 列举一个
    //遍，就可以找到该输入所支持的所有 standard 了。

    memset(&standard,0,sizeof (standard));
    standard.index = 0;
    while(0 == ioctl(fd, VIDIOC_ENUMSTD, &standard)) {
        if (standard.id & input.std){
            printf ("%s\n", standard.name);
        }
        standard.index++;
    }
    // EINVAL indicates the end of the enumeration, which cannot be empty unless this device falls under the USB exception.

    if (errno != EINVAL || standard.index == 0) {
        printf("VIDIOC_ENUMSTD\n");
        return FALSE;
    }

}*/






int main(int argc, char const *argv[])
{
    printf("begin....\n");

    v4l2_init();
    printf("init....\n");
    sleep(1);

    v4l2_mem_ops();
    printf("malloc....\n");
    sleep(1);

    v4l2_frame_process();
    printf("process....\n");
    sleep(1);

    v4l2_release();
    printf("release\n");
    sleep(2);

    return TRUE;
}
