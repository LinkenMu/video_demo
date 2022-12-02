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
#include <vector>

using namespace cv;

int main(int argc, char* argv[])
{
        std::vector<CaptureChannel*>  array;
        CaptureChannel* channel_0 = new CaptureChannel(0);
        channel_0->Init(CaptureChannel::IO_METHOD_MMAP, 1920,1080);
        array.push_back(channel_0);

        CaptureChannel* channel_1 = new CaptureChannel(1);
        channel_1->Init(CaptureChannel::IO_METHOD_MMAP, 1920,1080);
        array.push_back(channel_1);

        CaptureChannel* channel_2 = new CaptureChannel(2);
        channel_2->Init(CaptureChannel::IO_METHOD_MMAP, 1280,720);
        array.push_back(channel_2);

        CaptureChannel* channel_3 = new CaptureChannel(3);
        channel_3->Init(CaptureChannel::IO_METHOD_MMAP, 1280,720);
        array.push_back(channel_3);

        CaptureChannel* channel_6 = new CaptureChannel(6);
        channel_6->Init(CaptureChannel::IO_METHOD_MMAP, 1280,720);
        array.push_back(channel_6);

        CaptureChannel* channel_7 = new CaptureChannel(7);
        channel_7->Init(CaptureChannel::IO_METHOD_MMAP, 1280,720);
        array.push_back(channel_7);
        // array.emplace_back(0);
        // array[0].Init(CaptureChannel::IO_METHOD_MMAP, 1920,1080);
        // array.emplace_back(2);
        // array[1].Init(CaptureChannel::IO_METHOD_MMAP, 1280,720);
        // array.emplace_back(3);
        // array[2].Init(CaptureChannel::IO_METHOD_MMAP, 1280,720);
        // array.emplace_back(4);
        // array[3].Init(CaptureChannel::IO_METHOD_MMAP, 1280,720);

        for(auto iter: array)
        {
                iter->StartCapture();
        }
        sleep(100);

        for(auto iter: array)
        {
                iter->StopCapture();
                iter->UnInit();
        }


        fprintf(stderr, "\\n");
        return 0;
}
