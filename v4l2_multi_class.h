#pragma once
#include <string>
#include <thread>
#include <opencv2/opencv.hpp>

class CaptureChannel{
public:


    enum io_method {
            IO_METHOD_READ,
            IO_METHOD_MMAP,
            IO_METHOD_USERPTR,
    };

    struct buffer {
            void   *start;
            size_t  length;
    };


    CaptureChannel(int dev_index);
    ~CaptureChannel();
    int Init(int memory_mode, int width, int height);
    void UnInit();
    void StartCapture();
    void StopCapture();

private:
    std::string m_strDevname;
    enum io_method   m_io = IO_METHOD_MMAP;
    int              m_fd = -1;
    struct buffer *m_ptrBuffers;
    bool m_interrupt = true;
    int m_buffers = 10;
    int m_width;
    int m_height;
    std::thread* m_thread;



    void Display(void* p);
    void ProcLoop();
    void InitRead(unsigned int buffer_size);
    void InitMmap(void);
    void InitUserp(unsigned int buffer_size);
    int ReadFrame();
    void OpenDevice();
    void CloseDevice();
    void errno_exit(const char *s);

    void Save();
}
