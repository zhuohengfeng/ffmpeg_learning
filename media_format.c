//
//  media_format.c
//  io_format
//
//  Created by hengfeng zhuo on 2019/2/12.
//  Copyright © 2019 hengfeng zhuo. All rights reserved.
//

#include "main.h"

// 格式转换， 复用/解复用
// FFmpeg操作数据流程： 解复用-->获取流-->读数据包-->释放资源



#define ADTS_HEADER_LEN  7;

//有的时候当你编码AAC裸流的时候，会遇到写出来的AAC文件并不能在PC和手机上播放，很大的可能就是AAC文件的每一帧里缺少了ADTS头信息文件的包装拼接。只需要加入头文件ADTS即可。一个AAC原始数据块长度是可变的，对原始帧加上ADTS头进行ADTS的封装，就形成了ADTS帧。
void adts_header(char *szAdtsHeader, int dataLen){
    
    int audio_object_type = 2;
    int sampling_frequency_index = 7;
    int channel_config = 2;
    
    int adtsLen = dataLen + 7;
    
    szAdtsHeader[0] = 0xff;         //syncword:0xfff                          高8bits
    szAdtsHeader[1] = 0xf0;         //syncword:0xfff                          低4bits
    szAdtsHeader[1] |= (0 << 3);    //MPEG Version:0 for MPEG-4,1 for MPEG-2  1bit
    szAdtsHeader[1] |= (0 << 1);    //Layer:0                                 2bits
    szAdtsHeader[1] |= 1;           //protection absent:1                     1bit
    
    szAdtsHeader[2] = (audio_object_type - 1)<<6;            //profile:audio_object_type - 1                      2bits
    szAdtsHeader[2] |= (sampling_frequency_index & 0x0f)<<2; //sampling frequency index:sampling_frequency_index  4bits
    szAdtsHeader[2] |= (0 << 1);                             //private bit:0                                      1bit
    szAdtsHeader[2] |= (channel_config & 0x04)>>2;           //channel configuration:channel_config               高1bit
    
    szAdtsHeader[3] = (channel_config & 0x03)<<6;     //channel configuration:channel_config      低2bits
    szAdtsHeader[3] |= (0 << 5);                      //original：0                               1bit
    szAdtsHeader[3] |= (0 << 4);                      //home：0                                   1bit
    szAdtsHeader[3] |= (0 << 3);                      //copyright id bit：0                       1bit
    szAdtsHeader[3] |= (0 << 2);                      //copyright id start：0                     1bit
    szAdtsHeader[3] |= ((adtsLen & 0x1800) >> 11);           //frame length：value   高2bits
    
    szAdtsHeader[4] = (uint8_t)((adtsLen & 0x7f8) >> 3);     //frame length:value    中间8bits
    szAdtsHeader[5] = (uint8_t)((adtsLen & 0x7) << 5);       //frame length:value    低3bits
    szAdtsHeader[5] |= 0x1f;                                 //buffer fullness:0x7ff 高5bits
    szAdtsHeader[6] = 0xfc;
}

/**
 这里说的音视频信息就是META信息，比如格式，帧数，采样率等等
 */
int media_print_info(const char* media_url) {
    int ret = 0;
    av_log(NULL, AV_LOG_INFO, "打印多媒体文件：%s\n", media_url);
    
    // 格式上下文
    AVFormatContext* ctx = NULL;
    
    // 这个函数是所有ffmpeg都需要首先执行的
    av_register_all();
    
    // 打开媒体文件
    ret = avformat_open_input(&ctx, media_url, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open the file %s, the ret is %s \n", media_url, av_err2str(ret));
        goto __Error;
    }
    
    av_dump_format(ctx, 0, media_url, 0);
    
__Error:
    avformat_close_input(&ctx);
    
    return ret;
}

// 抽取音频流
int media_extra_audio(const char* src_url, const char* dst_url) {
    int ret = 0;
    FILE* dst_fd = NULL;
    int audio_index = -1;
    AVPacket pkt;
    int len = 0;
    
    av_log(NULL, AV_LOG_INFO, "抽取多媒体文件音频：%s\n", src_url);
    
    // 格式上下文
    AVFormatContext* fmt_ctx = NULL;
    
    // 这个函数是所有ffmpeg都需要首先执行的
    av_register_all();
    
    // 打开媒体文件
    ret = avformat_open_input(&fmt_ctx, src_url, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open the file %s, the ret is %s \n", src_url, av_err2str(ret));
        goto __Error;
    }
    
    av_dump_format(fmt_ctx, 0, src_url, 0);
    
    // 从原文件获取流
    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't av_find_best_stream file %s, the ret is %s \n", src_url, av_err2str(ret));
        goto __Error;
    }
    
    audio_index = ret;
    av_log(NULL, AV_LOG_INFO, "音频流index = %d\n", audio_index);
    
    // 打开目标文件(抽取出来的视频文件)
    dst_fd = fopen(dst_url, "wb");
    if (!dst_fd) {
        av_log(NULL, AV_LOG_ERROR, "Can't open dst file %s \n", dst_url);
        goto __Error;
    }
    
    // 开始读取音频帧，然后打包
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    
    while(av_read_frame(fmt_ctx, &pkt) >= 0) {
        // 音频流
        if (pkt.stream_index == audio_index) {
            
            char adts_header_buf[7];
            adts_header(adts_header_buf, pkt.size);
            fwrite(adts_header_buf, 1, 7, dst_fd);
            
            len = fwrite(pkt.data, 1, pkt.size, dst_fd);
            if (len != pkt.size) {
                av_log(NULL, AV_LOG_ERROR, "写文件失败 %d %d\n", len, pkt.size);
            }
        }
        
        av_packet_unref(&pkt);
    }
    
__Error:
    avformat_close_input(&fmt_ctx);
    if (dst_fd) {
        fclose(dst_fd);
    }
    return ret;
}

// 抽取视频流
int media_extra_video(const char* src_url, const char* dst_url) {
    return 0;
}
