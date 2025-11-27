#include "video_capture.h"
#include <iostream>
#include <cstring> // for memset

VideoCapture::VideoCapture(const std::string &filename)
{
    // 构造函数中不直接抛出异常，依靠 isOpened 判断
    open(filename);
};

VideoCapture::~VideoCapture()
{
    release();
}

bool VideoCapture::open(const std::string &filename)
{
    // [修复] 防止重复 open 导致内存泄露
    release();

    AVDictionary *options = NULL;
    AVStream *st = nullptr;
    
    // 设置参数
    av_dict_set(&options, "buffer_size", "1024000", 0); 
    av_dict_set(&options, "rtsp_transport", "tcp", 0);  
    av_dict_set(&options, "stimeout", "5000000", 0);    
    av_dict_set(&options, "max_delay", "500000", 0);    

    // 分配并初始化一个AVFormatContext
    _fmt_ctx = avformat_alloc_context();
    if (!_fmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate AVFormatContext\n");
        // [修复] 即使分配 context 失败，也要释放 options
        av_dict_free(&options);
        _isOpened = false;
        return false;
    }

    // 设置非阻塞模式
    _fmt_ctx->flags |= AVFMT_FLAG_NONBLOCK;

    int result = avformat_open_input(&_fmt_ctx, filename.c_str(), NULL, &options);
    // [修复] 无论成功失败，options 在 open_input 后都应释放（open_input 不会接管 options 所有权）
    av_dict_free(&options);
    
    if (result < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't open file\n");
        // avformat_open_input 失败时会自动释放 _fmt_ctx 的部分内容，但显式调用 close/free 更安全
        avformat_close_input(&_fmt_ctx); 
        _fmt_ctx = nullptr;
        _isOpened = false;
        return false;
    }

    result = avformat_find_stream_info(_fmt_ctx, NULL);
    if (result < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
        avformat_close_input(&_fmt_ctx);
        _fmt_ctx = nullptr;
        _isOpened = false;
        return false;
    }

    // 打印流信息（可选）
    // for (int i = 0; i < _fmt_ctx->nb_streams; i++) {
    //     av_dump_format(_fmt_ctx, i, filename.c_str(), 0);
    // }

    int height = 0;
    int width = 0;

    for (int i = 0; i < _fmt_ctx->nb_streams; i++) {
        st = _fmt_ctx->streams[i];
        if (st->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            height = st->codecpar->height;
            width = st->codecpar->width;
            break; // 找到第一个视频流即停止
        }
    }

    // 初始化AVPacket
    _pkt = av_packet_alloc();
    if (!_pkt)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot allocate packet\n");
        avformat_close_input(&_fmt_ctx);
        _fmt_ctx = nullptr;
        _isOpened = false;
        return false;
    }
    
    // [修复] 初始化结构体，防止垃圾值
    memset(&jmi_ctx_param, 0, sizeof(jmi_ctx_param));
    jmi_ctx_param.resize_width = width;
    jmi_ctx_param.resize_height = height;

    // [优化] 使用新API替代废弃的 avpicture_get_size
    _size = av_image_get_buffer_size(AV_PIX_FMT_BGR24, width, height, 1);
    // _size = avpicture_get_size(AV_PIX_FMT_BGR24, width, height); // 旧代码
    
    _out_buffer = (uint8_t *)av_malloc(_size);
    if (!_out_buffer) {
        av_log(NULL, AV_LOG_ERROR, "Cannot allocate output buffer\n");
        av_packet_free(&_pkt);
        avformat_close_input(&_fmt_ctx);
        _fmt_ctx = nullptr;
        _isOpened = false;
        return false;
    }

    jmi_ctx_param.coding_type = jmi::NV_VIDEO_CodingH264;
    
    std::string dec_name = "h264_dec_";
    if (filename.size() >= 5) {
        char fifth_last_char = filename[filename.size() - 5];
        dec_name += fifth_last_char;
    } else {
        dec_name += '_';
    }
    // std::cout << dec_name << std::endl;
    
    jmi_ctx_ = jmi::nvjmi_create_decoder(dec_name.data(), &jmi_ctx_param);
    if (!jmi_ctx_) {
        av_log(NULL, AV_LOG_ERROR, "Failed to create NVJMI decoder\n");
        av_freep(&_out_buffer);
        av_packet_free(&_pkt);
        avformat_close_input(&_fmt_ctx);
        _fmt_ctx = nullptr;
        _isOpened = false;
        return false;
    }

    _isOpened = true;
    return true;
}

bool VideoCapture::read(cv::Mat &image) {
    if (!_isOpened) return false;
    
    while (av_read_frame(_fmt_ctx, _pkt) >= 0) {
        // 基础校验
        if (_pkt->size <= 0 || _pkt->data == nullptr) {
            av_packet_unref(_pkt);
            continue;
        }

        // 投递数据包
        nvpacket.payload = _pkt->data;
        nvpacket.payload_size = _pkt->size;
        
        // 如果解码器接收包失败，释放包并继续
        if (jmi::nvjmi_decoder_put_packet(jmi_ctx_, &nvpacket) != 0) {
            av_packet_unref(_pkt);
            continue;
        }

        // 提取帧数据
        int frame_ret;
        bool frame_decoded = false;
        
        // 尝试获取解码后的帧
        while ((frame_ret = jmi::nvjmi_decoder_get_frame_meta(jmi_ctx_, &nvframe_meta)) >= 0) {
            qframe_size = frame_ret;
            
            // 只有成功提取数据才算成功
            if (jmi::nvjmi_decoder_retrieve_frame_data(jmi_ctx_, &nvframe_meta, _out_buffer) >= 0) {
                // 注意：这里创建的 Mat 使用的是 _out_buffer 的内存，没有深拷贝
                image = cv::Mat(nvframe_meta.height, nvframe_meta.width, CV_8UC3, _out_buffer);
                frame_decoded = true;
                break; // 获取到一帧就退出内层循环
            }
        }
        
        av_packet_unref(_pkt); // 无论是否解码成功，都要释放 packet

        if (frame_decoded) {
            return true;
        }
    }
    return false;
}

VideoCapture &VideoCapture::operator>>(cv::Mat &image)
{
    if (!read(image))
        image = cv::Mat();
    return *this;
}

void VideoCapture::release()
{
    if (!_isOpened) { 
        // 即使 _isOpened 为 false，也要确保指针被清理（防止构造函数部分失败的情况）
        // 但为了遵循你的逻辑，这里主要处理已打开的情况
        // 建议移除此 check 或确保构造函数失败时指针已置空
    }
    _isOpened = false; 

    if (_pkt) {
        av_packet_free(&_pkt);
        _pkt = nullptr;
    }
    
    // _fr 和 _frBGR 在 open 中未分配，如果其他地方没用到，这里释放是安全的（av_frame_free处理NULL）
    av_frame_free(&_fr); 
    av_frame_free(&_frBGR);
    _fr = nullptr;
    _frBGR = nullptr;

    if (_fmt_ctx) {
        avformat_close_input(&_fmt_ctx); 
        _fmt_ctx = nullptr;
    }
    
    if (_ctx) {
        avcodec_free_context(&_ctx); 
        _ctx = nullptr;
    }
    
    if (_out_buffer) {
        av_freep(&_out_buffer); 
        _out_buffer = nullptr;
    }
    
    if (jmi_ctx_) {
        jmi::nvjmi_decoder_close(jmi_ctx_);
        jmi::nvjmi_decoder_free_context(&jmi_ctx_);
        jmi_ctx_ = nullptr;
    }
}

// 全局变量部分
const int NUM_VIDEOS = 5;
VideoCapture videos[NUM_VIDEOS]; 
std::map<int,std::string> url_map;
// std::vector<int> emptyFrameCount(NUM_VIDEOS,0); // 未使用
// std::vector<cv::Mat> cam_map(NUM_VIDEOS); // 未使用

static void sleep_ms(unsigned int secs)
{
    struct timeval tval;
    tval.tv_sec=secs/1000;
    tval.tv_usec=(secs*1000)%1000000;
    select(0,NULL,NULL,NULL,&tval);
}

void Init_uri(int i, const char * uri){       
    std::cout << uri << std::endl;
    url_map[i] = uri;
    videos[i].open(uri); // 现在 open 内部会先调用 release，安全
    if(videos[i].isOpened()){
        std::cout<<"Init success"<<std::endl;
    }
    else{
        std::cout<<"Init fail"<< std::endl;
    }
}

bool isConnect(int i){
    if(videos[i].isOpened()){
        return true;
    }
    else{
         std::cout<< "cam: "<< i <<" link fail..."<< std::endl;
        return false;        
    } 
}

void reConnect(int i){
    std::cout<< "cam: "<< i <<" reconnecting..."<< std::endl;
    videos[i].release();
    sleep_ms(500);
    // videos[i].open 会先清理旧资源，防止泄露
    videos[i].open(url_map[i]);
    if(videos[i].isOpened()){
         std::cout<< "cam: "<< i <<" reconnected"<< std::endl;
         sleep_ms(5000);
         return;
    }
    else{
          std::cout<< "cam: "<< i <<" reconnected fail"<< std::endl;
          return;
    }
}

int Getbyte_( int i,int& width, int& height, int& size, unsigned char*& data) {
    if (!videos[i].isOpened()) {
        std::cout << "cam " << i << " link fail" << std::endl;
        return 0;
    }

    cv::Mat frame;
    // 注意：frame.data 指向的是 VideoCapture 内部的 _out_buffer
    if (!videos[i].read(frame) || frame.empty()) {
        videos[i].release();
        return 0;  
    }
    width = frame.cols;
    height = frame.rows;
    size = 30; // 保持原逻辑
    data = frame.data; // 危险：返回后 frame 销毁，但 data 指向 _out_buffer，只要 videos[i] 不读新帧或释放，该指针有效
    return 1;
}

int Getbyte(int i, int& width, int& height, int& size, cv::Mat** returnframe) {
    if (!videos[i].isOpened()) {
        return 0;
    }

    // 使用栈上的 Mat 对象
    cv::Mat frame;
    
    // 使用 cv::VideoCapture 自带的超时机制（如果支持）
    // 或者使用 select/poll 在文件描述符层面实现超时
    bool read_success = videos[i].read(frame);

    if (!read_success || frame.empty()) {

        videos[i].release();
        return 0;
    }

    width = frame.cols;
    height = frame.rows;
    size = 0;
    
    // 先删除旧数据，避免泄漏
    if (*returnframe != nullptr) {
        delete *returnframe;
        *returnframe = nullptr;
    }
    
    // 使用移动语义避免拷贝
    *returnframe = new cv::Mat(std::move(frame));
    
    return 1;
}
