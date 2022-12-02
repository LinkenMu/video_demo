/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 *
 *      This program is provided with the V4L2 API
 * see https://linuxtv.org/docs.php for more information
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#include <opencv2/core.hpp>
// #include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include "v4l2_multi.h"

#include <iostream>

using namespace cv;
using namespace std;


#define CLEAR(x) memset(&(x), 0, sizeof(x))
 void CaptureChannel::errno_exit(const char *s)
{
        fprintf(stderr, "%s %s error %d, %s\\n", m_strDevname.data(), s, errno, strerror(errno));
        exit(EXIT_FAILURE);
}

 int xioctl(int fh, int request, void *arg)
{
        int r;

        do {
                r = ioctl(fh, request, arg);
        } while (-1 == r && EINTR == errno);

        return r;
}




CaptureChannel::CaptureChannel(int dev_index)
{

    m_strDevname = "/dev/video" + to_string(dev_index);
    m_interrupt = true;
    m_fd = -1;

}

CaptureChannel::~CaptureChannel()
{

}




void CaptureChannel::Display(void* p)
{

        Mat imageUYVY(m_height,m_width, CV_8UC2, (void* )p);
        Mat channels[2];
        // cvtColor(image422, imageBGR, COLOR_YUV2RGB_Y422);
        split(imageUYVY, channels);
        Mat resize_image;
        resize(channels[1], resize_image, Size(),0.2,0.2);
        namedWindow(m_strDevname.data(), 0);
        imshow(m_strDevname.data(), resize_image);
        waitKey(30);
}

int CaptureChannel::ReadFrame(void)
{
        struct v4l2_buffer buf;
        unsigned int i;

        switch (m_io) {
        case IO_METHOD_READ:
                if (-1 == read(m_fd, m_ptrBuffers[0].start, m_ptrBuffers[0].length)) {
                        switch (errno) {
                        case EAGAIN:
                                return 0;

                        case EIO:
                                /* Could ignore EIO, see spec. */

                                /* fall through */

                        default:
                                errno_exit("read");
                        }
                }

                Display(m_ptrBuffers[0].start);
                break;

        case IO_METHOD_MMAP:
                CLEAR(buf);

                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;

                if (-1 == xioctl(m_fd, VIDIOC_DQBUF, &buf)) {
                        switch (errno) {
                        case EAGAIN:
                                return 0;

                        case EIO:
                                /* Could ignore EIO, see spec. */

                                /* fall through */

                        default:
                                errno_exit("VIDIOC_DQBUF");
                        }
                }

                assert(buf.index < m_buffers);
                printf("%s: %d.%3d ms \n",m_strDevname.data(), buf.timestamp.tv_sec,buf.timestamp.tv_usec/1000);
                // Display(m_ptrBuffers[buf.index].start);

                if (-1 == xioctl(m_fd, VIDIOC_QBUF, &buf))
                        errno_exit("VIDIOC_QBUF");
                break;

        case IO_METHOD_USERPTR:
                CLEAR(buf);

                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_USERPTR;

                if (-1 == xioctl(m_fd, VIDIOC_DQBUF, &buf)) {
                        switch (errno) {
                        case EAGAIN:
                                return 0;

                        case EIO:
                                /* Could ignore EIO, see spec. */

                                /* fall through */

                        default:
                                errno_exit("VIDIOC_DQBUF");
                        }
                }

                for (i = 0; i < m_buffers; ++i)
                        if (buf.m.userptr == (unsigned long)m_ptrBuffers[i].start
                            && buf.length == m_ptrBuffers[i].length)
                                break;

                assert(i < m_buffers);

                Display((void *)buf.m.userptr);

                if (-1 == xioctl(m_fd, VIDIOC_QBUF, &buf))
                        errno_exit("VIDIOC_QBUF");
                break;
        }

        return 1;
}

void CaptureChannel::ProcLoop(void)
{
        while (!m_interrupt) {
                for (;;) {
                        fd_set fds;
                        struct timeval tv;
                        int r;

                        FD_ZERO(&fds);
                        FD_SET(m_fd, &fds);

                        /* Timeout. */
                        tv.tv_sec = 2;
                        tv.tv_usec = 0;

                        r = select(m_fd + 1, &fds, NULL, NULL, &tv);

                        if (-1 == r) {
                                if (EINTR == errno)
                                        continue;
                                errno_exit("select");
                        }

                        if (0 == r) {
                                fprintf(stderr, "select timeout\\n");
                                exit(EXIT_FAILURE);
                        }

                        if (ReadFrame())
                                break;
                        /* EAGAIN - continue select loop. */
                }
        }
        cout << m_strDevname << " loop  exit !!!" <<endl;
}

void CaptureChannel::StopCapture(void)
{
        enum v4l2_buf_type type;
        m_interrupt = true;
        if(m_thread->joinable())
        {
            m_thread->join();
        }

        switch (m_io) {
        case IO_METHOD_READ:
                /* Nothing to do. */
                break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (-1 == xioctl(m_fd, VIDIOC_STREAMOFF, &type))
                        errno_exit("VIDIOC_STREAMOFF");
                break;
        }


}

void CaptureChannel::StartCapture(void)
{
        unsigned int i;
        enum v4l2_buf_type type;

        switch (m_io) {
        case IO_METHOD_READ:
                /* Nothing to do. */
                break;

        case IO_METHOD_MMAP:
                for (i = 0; i < m_buffers; ++i) {
                        struct v4l2_buffer buf;

                        CLEAR(buf);
                        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                        buf.memory = V4L2_MEMORY_MMAP;
                        buf.index = i;

                        if (-1 == xioctl(m_fd, VIDIOC_QBUF, &buf))
                                errno_exit("VIDIOC_QBUF");
                }
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (-1 == xioctl(m_fd, VIDIOC_STREAMON, &type))
                        errno_exit("VIDIOC_STREAMON");
                break;

        case IO_METHOD_USERPTR:
                for (i = 0; i < m_buffers; ++i) {
                        struct v4l2_buffer buf;

                        CLEAR(buf);
                        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                        buf.memory = V4L2_MEMORY_USERPTR;
                        buf.index = i;
                        buf.m.userptr = (unsigned long)m_ptrBuffers[i].start;
                        buf.length = m_ptrBuffers[i].length;

                        if (-1 == xioctl(m_fd, VIDIOC_QBUF, &buf))
                                errno_exit("VIDIOC_QBUF");
                }
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (-1 == xioctl(m_fd, VIDIOC_STREAMON, &type))
                        errno_exit("VIDIOC_STREAMON");
                break;
        }

        m_interrupt = false;
        m_thread = new thread(&CaptureChannel::ProcLoop, this);
}


 void CaptureChannel::InitRead(unsigned int buffer_size)
{
        m_ptrBuffers = calloc(1, sizeof(*m_ptrBuffers));

        if (!m_ptrBuffers) {
                fprintf(stderr, "Out of memory\\n");
                exit(EXIT_FAILURE);
        }

        m_ptrBuffers[0].length = buffer_size;
        m_ptrBuffers[0].start = malloc(buffer_size);

        if (!m_ptrBuffers[0].start) {
                fprintf(stderr, "Out of memory\\n");
                exit(EXIT_FAILURE);
        }
}

void CaptureChannel::InitMmap(void)
{
        struct v4l2_requestbuffers req;
        int n_buffers;
        CLEAR(req);

        req.count = m_buffers;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (-1 == xioctl(m_fd, VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s does not support "
                                 "memory mappingn", m_strDevname.data());
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_REQBUFS");
                }
        }

        if (req.count < 2) {
                fprintf(stderr, "Insufficient buffer memory on %s\\n",
                         m_strDevname.data());
                exit(EXIT_FAILURE);
        }

        m_ptrBuffers = calloc(req.count, sizeof(*m_ptrBuffers));

        if (!m_ptrBuffers) {
                fprintf(stderr, "Out of memory\\n");
                exit(EXIT_FAILURE);
        }

        for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
                struct v4l2_buffer buf;

                CLEAR(buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = n_buffers;

                if (-1 == xioctl(m_fd, VIDIOC_QUERYBUF, &buf))
                        errno_exit("VIDIOC_QUERYBUF");
                printf(" buf length %u \n", buf.length);
                m_ptrBuffers[n_buffers].length = buf.length;
                m_ptrBuffers[n_buffers].start =
                        mmap(NULL /* start anywhere */,
                              buf.length,
                              PROT_READ | PROT_WRITE /* required */,
                              MAP_SHARED /* recommended */,
                              m_fd, buf.m.offset);

                if (MAP_FAILED == m_ptrBuffers[n_buffers].start)
                        errno_exit("mmap");
        }
}

 void CaptureChannel::InitUserp(unsigned int buffer_size)
{
        struct v4l2_requestbuffers req;
        int n_buffers;
        CLEAR(req);

        req.count  = m_buffers;
        req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_USERPTR;

        if (-1 == xioctl(m_fd, VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s does not support "
                                 "user pointer i/on", m_strDevname.data());
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_REQBUFS");
                }
        }

        m_ptrBuffers = calloc(m_buffers, sizeof(*m_ptrBuffers));

        if (!m_ptrBuffers) {
                fprintf(stderr, "Out of memory\\n");
                exit(EXIT_FAILURE);
        }

        for (n_buffers = 0; n_buffers < m_buffers; ++n_buffers) {
                m_ptrBuffers[n_buffers].length = buffer_size;
                m_ptrBuffers[n_buffers].start = malloc(buffer_size);

                if (!m_ptrBuffers[n_buffers].start) {
                        fprintf(stderr, "Out of memory\\n");
                        exit(EXIT_FAILURE);
                }
        }
}

int CaptureChannel::Init(int memory_mode, int width, int height)
{
        struct v4l2_capability cap;
        struct v4l2_cropcap cropcap;
        struct v4l2_crop crop;
        struct v4l2_format fmt;
        unsigned int min;
        m_io = memory_mode;
        m_width = width;
        m_height = height;

        OpenDevice();

        if (-1 == xioctl(m_fd, VIDIOC_QUERYCAP, &cap)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s is no V4L2 device\\n",
                                 m_strDevname.data());
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_QUERYCAP");
                }
        }

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
                fprintf(stderr, "%s is no video capture device\\n",
                         m_strDevname.data());
                exit(EXIT_FAILURE);
        }

        switch (m_io) {
        case IO_METHOD_READ:
                if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
                        fprintf(stderr, "%s does not support read i/o\\n",
                                 m_strDevname.data());
                        exit(EXIT_FAILURE);
                }
                break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
                if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                        fprintf(stderr, "%s does not support streaming i/o\\n",
                                 m_strDevname.data());
                        exit(EXIT_FAILURE);
                }
                break;
        }


        /* Select video input, video standard and tune here. */


        CLEAR(cropcap);

        cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (0 == xioctl(m_fd, VIDIOC_CROPCAP, &cropcap)) {
                crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                crop.c = cropcap.defrect; /* reset to default */

                if (-1 == xioctl(m_fd, VIDIOC_S_CROP, &crop)) {
                        switch (errno) {
                        case EINVAL:
                                /* Cropping not supported. */
                                break;
                        default:
                                /* Errors ignored. */
                                break;
                        }
                }
        } else {
                /* Errors ignored. */
        }


        CLEAR(fmt);

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (m_height > 0) {
                fmt.fmt.pix.width       = m_width;
                fmt.fmt.pix.height      = m_height;
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_UYVY;
                fmt.fmt.pix.field       = V4L2_FIELD_ANY;

                if (-1 == xioctl(m_fd, VIDIOC_S_FMT, &fmt))
                        errno_exit("VIDIOC_S_FMT");

                /* Note VIDIOC_S_FMT may change width and height. */
        } else {
                /* Preserve original settings as set by v4l2-ctl for example */
                if (-1 == xioctl(m_fd, VIDIOC_G_FMT, &fmt))
                        errno_exit("VIDIOC_G_FMT");
        }

        /* Buggy driver paranoia. */
        min = fmt.fmt.pix.width * 2;
        if (fmt.fmt.pix.bytesperline < min)
                fmt.fmt.pix.bytesperline = min;
        min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
        if (fmt.fmt.pix.sizeimage < min)
                fmt.fmt.pix.sizeimage = min;

        switch (m_io) {
        case IO_METHOD_READ:
                InitRead(fmt.fmt.pix.sizeimage);
                break;

        case IO_METHOD_MMAP:
                InitMmap();
                break;

        case IO_METHOD_USERPTR:
                InitUserp(fmt.fmt.pix.sizeimage);
                break;
        }

        return 0;
}



void CaptureChannel::UnInit(void)
{
        unsigned int i;

        switch (m_io) {
        case IO_METHOD_READ:
                free(m_ptrBuffers[0].start);
                break;

        case IO_METHOD_MMAP:
                for (i = 0; i < m_buffers; ++i)
                        if (-1 == munmap(m_ptrBuffers[i].start, m_ptrBuffers[i].length))
                                errno_exit("munmap");
                break;

        case IO_METHOD_USERPTR:
                for (i = 0; i < m_buffers; ++i)
                        free(m_ptrBuffers[i].start);
                break;
        }

        free(m_ptrBuffers);
        CloseDevice();
}

 void CaptureChannel::CloseDevice(void)
{
        if (-1 == close(m_fd))
                errno_exit("close");

        m_fd = -1;
}

 void CaptureChannel::OpenDevice(void)
{
        struct stat st;

        if (-1 == stat(m_strDevname.data(), &st)) {
                fprintf(stderr, "Cannot identify '%s': %d, %s\\n",
                         m_strDevname.data(), errno, strerror(errno));
                exit(EXIT_FAILURE);
        }

        if (!S_ISCHR(st.st_mode)) {
                fprintf(stderr, "%s is no devicen", m_strDevname.data());
                exit(EXIT_FAILURE);
        }

        m_fd = open(m_strDevname.data(), O_RDWR /* required */ | O_NONBLOCK, 0);

        if (-1 == m_fd) {
                fprintf(stderr, "Cannot open '%s': %d, %s\\n",
                         m_strDevname.data(), errno, strerror(errno));
                exit(EXIT_FAILURE);
        }
}

