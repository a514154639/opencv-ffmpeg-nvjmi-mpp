#pragma once
#include <string>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <memory>
#include <queue>

#ifdef __cplusplus
extern "C"
{
#endif
/*Include ffmpeg header file*/
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
//#include <libavdevice/avdevice.h>

#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>

#ifdef __cplusplus
}
#endif
#include "nvjmi.h"

class VideoCapture
{
public:
    explicit VideoCapture(const std::string &filename);
    ~VideoCapture();
    bool isOpened() { return _isOpened; };
    VideoCapture &operator>>(cv::Mat &image);
    VideoCapture(){};
    bool open(const std::string &filename);
    bool read(cv::Mat &image);
    bool read_bac(cv::Mat &image);
    void release();
    void reconnect(int i); 
    int qframe_size{-1};
    
    

//private:
    
    

private:
    
    mutable bool _isOpened{false};
    AVCodec *_codec{NULL};
    AVCodecContext *_ctx{NULL};
    AVCodecParameters *_origin_par{NULL};
    AVFrame *_fr{NULL};
    AVFrame *_frBGR{NULL};
    AVPacket *_pkt{NULL};
    AVFormatContext *_fmt_ctx{NULL};
    AVRational time_base = {1, AV_TIME_BASE}; 
    std::chrono::system_clock::time_point starttime{}; 
    struct SwsContext *_img_convert_ctx{NULL};
    uint8_t *_out_buffer{NULL};
    uchar * out_data{NULL};
    int _size{-1};
    int _video_stream{-1};
    jmi::nvJmiCtxParam jmi_ctx_param;
    jmi::nvJmiCtx *jmi_ctx_{NULL};
    jmi::nvPacket nvpacket;
    jmi::nvFrameMeta nvframe_meta;
    std::queue<cv::Mat> frame_queue;
    

};

extern "C" void Init_uri(int i, const char * uri);
extern "C" bool isConnect(int i);
extern "C" void reConnect(int i);
extern "C" int Getbyte_( int i,int& width, int& height, int& size, unsigned char*& data);
extern "C" int Getbyte( int i,int& width, int& height, int& size, cv::Mat** returnframe);


