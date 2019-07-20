//
// Created by hengfeng zhuo on 2019-07-19.
//

#include "main.h"

#include <opencv2/core.hpp> // Mat
#include <opencv2/highgui.hpp> // 测试用的GUI
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

using namespace cv;

const char* RTSP_ADD = "rtsp://184.72.239.149/vod/mp4://BigBuckBunny_175k.mov";

const char* RTMP_ADD = "rtmp://10.88.7.193/live";

int main() {

    // 设置LOG等级
    av_log_set_level(AV_LOG_INFO);
    av_log(NULL, AV_LOG_INFO, "Now let's start the game....\n\n");

// 打开本地图片
//    Mat srcImg = imread("../resource/test.jpeg");
//    // 均值滤波
//    Mat dstImg;
//    blur(srcImg, dstImg, Size(5, 5));
//    imshow("dst image", dstImg);

    // 自定义一张图片
//    Mat mat(1280, 720, CV_8UC3);
//    int es = mat.elemSize();
//    for (int i = 0; i < mat.rows * mat.cols * es; i+=es) {
//        mat.data[i] = 0; //B
//        mat.data[i+1] = 255; //G
//        mat.data[i+2] = 0; //R
//    }
//    namedWindow("custome img");
//    imshow("custome img", mat);


// 读取相机帧数据
    VideoCapture camera;

    // 读取RTSP流，注意，这里打开流会有延时
    bool result = camera.open(RTSP_ADD);
    Mat frame;
    // 像素格式转换上下文
    SwsContext* swsContext = NULL;
    AVCodecContext *codecContext = NULL;

    // rtmp flv封装器
    AVFormatContext* formatContext = NULL;

    if (result) {
        av_log(NULL, AV_LOG_INFO, "打开rtsp成功\n\n");
        try {
            // 注册所有的编解码器
            avcodec_register_all();
            // 注册所有封装器
            av_register_all();
            // 注册所有网络协议
            avformat_network_init();

            int inWidth = camera.get(CAP_PROP_FRAME_WIDTH);
            int inHeight = camera.get(CAP_PROP_FRAME_HEIGHT);
            int fps = camera.get(CAP_PROP_FPS);
            // 初始化格式转换上下文
            swsContext = sws_getCachedContext(swsContext,
                    // 源宽高
                    inWidth, inHeight,
                    // 输入格式BGR
                    AV_PIX_FMT_BGR24,
                    // 输出目标宽，高
                    inWidth, inHeight,
                    // 输出目标的格式
                    AV_PIX_FMT_YUV420P,
                    // 尺寸变化使用算法
                    SWS_BICUBIC,
                    // 可选项
                    0,0,0
                    );
            if (!swsContext) {
                av_log(NULL, AV_LOG_INFO, "sws_getCachedContext 初始失败\n\n");
                goto _Error;
            }

            // 输出的数据结构AvFrame, 里面存YUV数据
            AVFrame *yuv = av_frame_alloc();
            yuv->format = AV_PIX_FMT_YUV420P;
            yuv->width = inWidth;
            yuv->height = inHeight;
            yuv->pts = 0; //必须一致往前增的
            // 真正分配yuv空间
            int ret = av_frame_get_buffer(yuv, 0); //32
            if (ret != 0) {
                av_log(NULL, AV_LOG_INFO, "av_frame_get_buffer 初始失败\n\n");
                goto _Error;
            }


            ///////////////////////////////////////////
            // 1. 找到H264编码器
            AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
            if (!codec) {
                av_log(NULL, AV_LOG_INFO, "avcodec_find_encoder 初始失败\n\n");
                goto _Error;
            }

            // 2. 创建编码器上下文
            codecContext = avcodec_alloc_context3(codec);
            if (!codecContext) {
                av_log(NULL, AV_LOG_INFO, "avcodec_alloc_context3 初始失败\n\n");
                goto _Error;
            }

            // 3. 配置编码器参数
            codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; // 设置全局头文件，对流媒体非必须
            codecContext->codec_id = codec->id;
            codecContext->thread_count = 8;
            // 针对视频才有的参数
            codecContext->bit_rate = 50 * 1024 * 8; // 压缩后每秒视频的bit位大小，这里设置成50k, 根据网络情况设置
            codecContext->width = inWidth;
            codecContext->height = inHeight;
            codecContext->time_base = {1, fps}; // 其实就是一个浮点数，分子，分母分开存放
            codecContext->framerate = {fps, 1};
            codecContext->gop_size = 50; // 多少帧一个关键帧，越大，压缩越大；但是太大了也会有问题，如果关键帧丢失，解码就会出问题，这里可以设置50个关键帧
            codecContext->max_b_frames = 0; //没有b帧，这样pts和dts就一直了
            codecContext->pix_fmt = AV_PIX_FMT_YUV420P; //设置输入的格式

            // 4. 打开编码器上下文
            ret = avcodec_open2(codecContext, codec, 0);
            if (ret != 0) {
                av_log(NULL, AV_LOG_INFO, "avcodec_open2 失败\n\n");
                goto _Error;
            }

            av_log(NULL, AV_LOG_INFO, "创建编码器成功\n\n");

            AVPacket avPacke;
            memset(&avPacke, 0, sizeof(&avPacke));

            int vpts = 0;

            //////////////输出封装器flv和视频流配置/////////////////
            //1. 创建输出封装器上下文
            ret = avformat_alloc_output_context2(&formatContext, 0, "flv", RTMP_ADD);
            if (ret != 0) {
                av_log(NULL, AV_LOG_INFO, "avformat_alloc_output_context2 失败\n\n");
                goto _Error;
            }

            //2. 添加视频流
            AVStream *outStream = avformat_new_stream(formatContext, NULL);
            if (!outStream) {
                av_log(NULL, AV_LOG_INFO, "avformat_new_stream 失败\n\n");
                goto _Error;
            }
            outStream->codecpar->codec_tag = 0;
            // 拷贝参数, 从编码器赋值参数
            avcodec_parameters_from_context(outStream->codecpar, codecContext);
            av_dump_format(formatContext, 0, "", 1);
            // 打开RTMP的输出IO
            ret = avio_open(&formatContext->pb, RTMP_ADD, AVIO_FLAG_WRITE);
            if (ret != 0) {
                av_log(NULL, AV_LOG_INFO, "avio_open 失败\n\n");
                goto _Error;
            }
            // 写入封装头--注意，这个会修改time base
            ret = avformat_write_header(formatContext, NULL);
            if (ret != 0) {
                av_log(NULL, AV_LOG_INFO, "avformat_write_header 失败\n\n");
                goto _Error;
            }


            while(true) {
                // grab只做解码成H264，不做格式转换
                if (!camera.grab()) {
                    continue;
                }
                // h264解码转化rgb
                if (!camera.retrieve(frame)) {
                    continue;
                }
                // 显示窗口
                imshow("show image", frame);
                waitKey(1);//等待1毫米，并刷新

                // rgb to yuv --- 使用格式转换API sws_getCachedContext, 可以做大小和格式转换
                // 输入的数据结构
                uint8_t *indata[AV_NUM_DATA_POINTERS] = {0};
                // rgb数据是交错存放的 bgrbgrbgr....所以只要用到indata[0]
                // 如果是plane的，那存放是分组的，比如 indata[0] bbbbbb, indata[1] gggggg, indata[2] rrrrrrr
                indata[0] = frame.data; //BGR24一个像素有3个字节，24位
                // 一行数据(宽)的字节数
                int inLineSize[AV_NUM_DATA_POINTERS] = {0};
                inLineSize[0] = frame.cols * frame.elemSize(); // 一个像素有3个字节，24位
                // 开始转成YUV
                int h = sws_scale(
                        swsContext,
                        // 源数据
                        indata, inLineSize, 0, frame.rows,
                        // 输出数据
                        yuv->data, yuv->linesize

                );

                //av_log(NULL, AV_LOG_INFO, "得到YUV数据 width=%d, height=%d\n\n", yuv->width, yuv->height);

                //////////////////h264编码//////////////////////////
                // 发送原始数据
                yuv->pts = vpts; // 注意这里要把pts递增，否则会出现non-strictly-monotonic PTS错误
                vpts++;
                ret = avcodec_send_frame(codecContext, yuv);
                if (ret != 0) {
                    continue;
                }
                // 开始接收数据
                ret = avcodec_receive_packet(codecContext, &avPacke);
                if (ret == 0 && avPacke.size > 0) {
                    //av_log(NULL, AV_LOG_INFO, "编码成功得到avPacket  avPacke.size=%d \n\n", avPacke.size);
                }

                ////////////////// 开始推流 //////////////////////////
                av_interleaved_write_frame(formatContext, &avPacke);
            }
        }
        catch (Exception &ex) {
            av_log(NULL, AV_LOG_INFO, "出现异常！！！！！！！！！\n\n");
            std::cerr << ex.what() << std::endl;
        }
    }
    else {
        av_log(NULL, AV_LOG_INFO, "打开rtsp失败\n\n");
    }

_Error:
    if (camera.isOpened()) {
        camera.release();
    }
    if (swsContext) {
        sws_freeContext(swsContext);
        swsContext = NULL;
    }
    if (codecContext) {
        avcodec_free_context(&codecContext);
        codecContext = NULL;
    }
    if (formatContext) {
        avio_close(formatContext->pb);
        formatContext->pb = NULL;
        avformat_free_context(formatContext);
        formatContext = NULL;
    }

    return 0;
}
