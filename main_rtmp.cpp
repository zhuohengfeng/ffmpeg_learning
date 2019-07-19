//
// Created by hengfeng zhuo on 2019-07-18.
//

#include "main.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace std;
using namespace cv;

const char* LOCAL_MP4 = "../resource/test.flv";

const char* RTMP_ADD = "rtmp://10.88.7.193/live";

/**
 * 打印错误信息
 * @param ret
 */
void printError(int ret) {
    char message[1023] = {0};
    av_strerror(ret, message, sizeof(message));
    av_log(NULL, AV_LOG_ERROR, "[Error]===> %s \n\n", message);
}

double r2d(AVRational r) {
    return r.num == 0 || r.den ==0 ? 0.0 : ((double)r.num / r.den);
}


/**
 * 也就是pts反映帧什么时候开始显示，dts反映数据流什么时候开始解码
 * 所谓时间基表示的就是每个刻度是多少秒.如果你是把1秒分成90000份，每一个刻度就是1/90000秒，此时的time_base={1，90000}。
 * pts的值就是占多少个时间刻度（占多少个格子）。它的单位不是秒，而是时间刻度
 *
 * 为什么要有时间基转换。
 * 非压缩时候的数据（即YUV或者其它），在ffmpeg中对应的结构体为AVFrame,它的时间基为AVCodecContext 的time_base ,AVRational{1,25}。
 * 压缩后的数据（对应的结构体为AVPacket）对应的时间基为AVStream的time_base，AVRational{1,90000}。
 *
 * 因为数据状态不同，时间基不一样，所以我们必须转换，在1/25时间刻度下占10格，在1/90000下是占多少格。这就是pts的转换。
 * @return
 */
int main() {
    // 用来计算发送时间的，开始获取时间戳
    int64_t startTime;

    // 输入格式上下文
    AVFormatContext* pInputFromatContext = NULL;
    // 输出格式上下文
    AVFormatContext* pOutputFormatContext = NULL;

    // 设置LOG等级， 如果FFMpeg打印很多LOG，可以通过这个过滤
    av_log_set_level(AV_LOG_INFO);
    av_log(NULL, AV_LOG_INFO, "Now let's start the %s\n\n", RTMP_ADD);


    // 初始化所有封装muxters和解封装demuxter flv,mp4, mov, mp3等
    // 但是不包含编码器和解码器
    av_register_all();

    // 初始化网络库
    avformat_network_init();

    // 直打开文件
    int ret = avformat_open_input(&pInputFromatContext, LOCAL_MP4, 0, 0);
    if (ret != 0 || pInputFromatContext == NULL) {
        printError(ret);
        goto _ERROR;
    }

    av_log(NULL, AV_LOG_INFO, "打开文件 %s\n", LOCAL_MP4);

    // 获取音频视频流信息
    ret = avformat_find_stream_info(pInputFromatContext, 0);
    if (ret < 0) {
        printError(ret);
        goto _ERROR;
    }

//    av_log(NULL, AV_LOG_INFO, "获取音视频信息:\n");
    // 打印信息，index=0表示打印所有
//    av_dump_format(pInputFromatContext, 0, LOCAL_MP4, 0);


    ///////////// 开始输出到rtmp/ //////////////////
    ret = avformat_alloc_output_context2(&pOutputFormatContext, 0, "flv", RTMP_ADD);
    if (ret < 0 || pOutputFormatContext == NULL) {
        printError(ret);
        goto _ERROR;
    }

    // 配置输出流
    // 遍历输入的AVStream
    for (int i = 0; i < pInputFromatContext->nb_streams; i++) {
        // 因为要输出，所以这里主动创建新的流
        AVStream* out_stream = avformat_new_stream(pOutputFormatContext, pInputFromatContext->streams[i]->codec->codec);
        if (!out_stream) {
            printError(0);
            goto _ERROR;
        }
        // 复制配置信息
        ret = avcodec_parameters_copy(out_stream->codecpar, pInputFromatContext->streams[i]->codecpar);
        out_stream->codec->codec_tag = 0;
    }
//    av_log(NULL, AV_LOG_INFO, "打印输出音视频格式信息:\n");
//    av_dump_format(pOutputFormatContext, 0, RTMP_ADD, 1); // 注意最后一个参数为1表示output

    ///////////// 开始rtmp推流 //////////////////
    // 打开IO，建立连接，打开后会给pb赋值
    av_log(NULL, AV_LOG_INFO, " 打开IO，建立连接\n"); // 这里是不是要打开服务器？？？不然会出错？
    ret = avio_open(&pOutputFormatContext->pb, RTMP_ADD, AVIO_FLAG_WRITE);
    if (ret < 0 || pOutputFormatContext->pb == NULL) {
        av_log(NULL, AV_LOG_ERROR, "打开IO建立连接出错了");
        printError(ret);
        goto _ERROR;
    }

    // 写入头信息
    av_log(NULL, AV_LOG_INFO, "写入头信息\n");
    ret = avformat_write_header(pOutputFormatContext, 0);
    if (ret < 0) {
        printError(ret);
        goto _ERROR;
    }


    // 读取每一帧 推流每一帧数据
    AVPacket pkt;
    startTime = av_gettime();
    av_log(NULL, AV_LOG_INFO, "开始推流...\n"); // startTime = 1563503716865336  从1970年到现在的微秒数
    while(true) {
        // 从输入流中读取帧
        ret = av_read_frame(pInputFromatContext, &pkt); // pkt里面包含了pts和dts
        if (ret != 0) {
            printError(ret);
            break;
        }
        // 读到每一帧数据
        // 打印pts和dts
        av_log(NULL, AV_LOG_INFO, "pts=%d, dts=%d \n", pkt.pts, pkt.dts);

        // 计算转换时间pts, dts--- 因为输入和输出的time base不一致
        // 得到时间基，因为读到的AVPackaget有可能是音频，也有可能是视频，所以这里要用pkt.stream_index
        AVRational inTimebase = pInputFromatContext->streams[pkt.stream_index]->time_base;
        AVRational outTimebase = pOutputFormatContext->streams[pkt.stream_index]->time_base;
        pkt.pts = av_rescale_q_rnd(pkt.pts, inTimebase, outTimebase, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts, inTimebase, outTimebase, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q_rnd(pkt.duration, inTimebase, outTimebase, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));;
        pkt.pos = -1;


        // 延时一下, 避免发送太快 --- 正确的做法是要通过计算
//        if (pkt.stream_index == 0) {
//            av_usleep(30*1000);
//        }

        // 只判断视频
        if (pInputFromatContext->streams[pkt.stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            AVRational timebase = pInputFromatContext->streams[pkt.stream_index]->time_base;

            // 已经过去的时间
            int64_t now = av_gettime() - startTime;
            int64_t dts = 0;
            dts = pkt.dts * (1000 * 1000* r2d(timebase) ); // 微秒
            // 播放太快了
            if (dts > now) {
                av_log(NULL, AV_LOG_INFO, "播放太快了 %d \n", (dts - now));
                av_usleep(dts - now);
            }
        }

        // 开始发送一帧数据, 用这个函数会自动释放avpacket，所以不需要后面那句释放
        ret = av_interleaved_write_frame(pOutputFormatContext, &pkt);
        if (ret < 0) {
            av_log(NULL, AV_LOG_INFO, "发送数据出错啦！！！！");
        }
        // 释放
        //av_packet_unref(&pkt);
    }


_ERROR:
    av_log(NULL, AV_LOG_INFO, "程序执行完成，退出！");

    if (pInputFromatContext != NULL) {
        avformat_close_input(&pInputFromatContext);
        avformat_free_context(pInputFromatContext);
        pInputFromatContext = NULL;
    }
    if (pOutputFormatContext != NULL) {
        avformat_free_context(pOutputFormatContext);
        pOutputFormatContext = NULL;
    }
    return 0;
}

