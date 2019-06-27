//
// Created by hengfeng zhuo on 2019-05-27.
//
#include "main.h"

using namespace std;

// 格式转换， 复用/解复用
// FFmpeg操作数据流程： 解复用-->获取流avstream-->读数据包avformat-->释放资源

const char* VIDEO_PATH = "/Users/hengfeng/workspace/C_Cpp/CLion/ffmpeg_learning/resouce/big_buck_bunny.mp4";

const char* AUDIO_OUTPUT_PATH = "/Users/hengfeng/workspace/C_Cpp/CLion/ffmpeg_learning/resouce/out/extra_output.aac";
const char* VIDEO_OUTPUT_PATH = "/Users/hengfeng/workspace/C_Cpp/CLion/ffmpeg_learning/resouce/out/extra_output.264";
const char* FLV_OUTPUT_PATH = "/Users/hengfeng/workspace/C_Cpp/CLion/ffmpeg_learning/resouce/out/extra_output.flv";

#define ADTS_HEADER_LEN  7;

//有的时候当你编码AAC裸流的时候，会遇到写出来的AAC文件并不能在PC和手机上播放，很大的可能就是AAC文件的每一帧里缺少了ADTS头信息文件的包装拼接。只需要加入头文件ADTS即可。一个AAC原始数据块长度是可变的，对原始帧加上ADTS头进行ADTS的封装，就形成了ADTS帧。
//https://blog.csdn.net/tx3344/article/details/7414543
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
 * 打印音视频文件的meta信息
 * @param url
 */
 /*
void dump_media_info(const char* url) {
    int ret;
    AVFormatContext* pFormatContext = NULL;

    // 这个函数是所有ffmpeg都需要首先执行的
    av_register_all();

    // 1. 分配format context
    pFormatContext = avformat_alloc_context();
    if (!pFormatContext) {
        av_log(NULL, AV_LOG_ERROR, "can't alloc the format context!");
        goto __Error;
    }

    // 2. 打开媒体文件
    ret = avformat_open_input(&pFormatContext, url, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open the file %s, the ret is %s \n", url, av_err2str(ret));
        goto __Error;
    }

    // 3. 打印媒体信息
    av_dump_format(pFormatContext, 0, url, 0);

__Error:
    avformat_close_input(&pFormatContext);
    return;
}
*/


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

    //av_dump_format(fmt_ctx, 0, src_url, 0);

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
            //一般的AAC解码器都需要把AAC的ES流打包成ADTS的格式，一般是在AAC ES流前添加7个字节的ADTS header。也就是说你可以吧ADTS这个头看作是AAC的frameheader。
            //ADTS 头中相对有用的信息 采样率、声道数、帧长度
            char adts_header_buf[7];
            adts_header(adts_header_buf, pkt.size);
            fwrite(adts_header_buf, 1, 7, dst_fd);

            len = fwrite(pkt.data, 1, pkt.size, dst_fd);
            if (len != pkt.size) {
                av_log(NULL, AV_LOG_ERROR, "写文件失败 %d %d\n", len, pkt.size);
            }
        }

        av_packet_unref(&pkt);
        av_free(&pkt);
    }

__Error:
    avformat_close_input(&fmt_ctx);
    if (dst_fd) {
        fclose(dst_fd);
    }
    return ret;
}

//============================================================
#ifndef AV_WB32
#   define AV_WB32(p, val) do {                 \
        uint32_t d = (val);                     \
        ((uint8_t*)(p))[3] = (d);               \
        ((uint8_t*)(p))[2] = (d)>>8;            \
        ((uint8_t*)(p))[1] = (d)>>16;           \
        ((uint8_t*)(p))[0] = (d)>>24;           \
    } while(0)
#endif

#ifndef AV_RB16
#   define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])
#endif

static int alloc_and_copy(AVPacket *out,
                          const uint8_t *sps_pps, uint32_t sps_pps_size,
                          const uint8_t *in, uint32_t in_size)
{
    uint32_t offset         = out->size;
    uint8_t nal_header_size = offset ? 3 : 4; // 4 sps PPS个字节00 00 00 01， 如果非sps/PPS 3个字节00 00 01
    int err;

    // 对之前分配的packet进行扩容
    err = av_grow_packet(out, sps_pps_size + in_size + nal_header_size); // nal_header_size就是start code的长度
    if (err < 0)
        return err;

    if (sps_pps) // 如果带有sps pps
        memcpy(out->data + offset, sps_pps, sps_pps_size);
    memcpy(out->data + sps_pps_size + nal_header_size + offset, in, in_size);
    if (!offset) {
        AV_WB32(out->data + sps_pps_size, 1); // 00 00 00 01
    } else {
        (out->data + offset + sps_pps_size)[0] =
        (out->data + offset + sps_pps_size)[1] = 0;
        (out->data + offset + sps_pps_size)[2] = 1;
    }

    return 0;
}

int h264_extradata_to_annexb(const uint8_t *codec_extradata, const int codec_extradata_size, AVPacket *out_extradata, int padding)
{
    uint16_t unit_size;
    uint64_t total_size                 = 0;
    uint8_t *out                        = NULL, unit_nb, sps_done = 0,
            sps_seen                   = 0, pps_seen = 0, sps_offset = 0, pps_offset = 0;
    const uint8_t *extradata            = codec_extradata + 4; // 跳过前4个无用数据
    static const uint8_t nalu_header[4] = { 0, 0, 0, 1 };
    int length_size = (*extradata++ & 0x3) + 1; // retrieve length coded size, 用于指示表示编码数据长度所需字节数

    sps_offset = pps_offset = -1;

    /* retrieve sps and pps unit(s) */
    unit_nb = *extradata++ & 0x1f; /* number of sps unit(s) */  // SPS / PPS个数
    if (!unit_nb) {
        goto pps;
    }else {
        sps_offset = 0;
        sps_seen = 1;
    }

    while (unit_nb--) {
        int err;

        unit_size   = AV_RB16(extradata);
        total_size += unit_size + 4;
        if (total_size > INT_MAX - padding) {
            av_log(NULL, AV_LOG_ERROR,
                   "Too big extradata size, corrupted stream or invalid MP4/AVCC bitstream\n");
            av_free(out);
            return AVERROR(EINVAL);
        }
        if (extradata + 2 + unit_size > codec_extradata + codec_extradata_size) {
            av_log(NULL, AV_LOG_ERROR, "Packet header is not contained in global extradata, "
                                       "corrupted stream or invalid MP4/AVCC bitstream\n");
            av_free(out);
            return AVERROR(EINVAL);
        }
        if ((err = av_reallocp(&out, total_size + padding)) < 0)
            return err;
        memcpy(out + total_size - unit_size - 4, nalu_header, 4);
        memcpy(out + total_size - unit_size, extradata + 2, unit_size);
        extradata += 2 + unit_size;
        pps:
        if (!unit_nb && !sps_done++) {
            unit_nb = *extradata++; /* number of pps unit(s) */
            if (unit_nb) {
                pps_offset = total_size;
                pps_seen = 1;
            }
        }
    }

    if (out)
        memset(out + total_size, 0, padding);

    // 拿到了sps pps
    if (!sps_seen)
        av_log(NULL, AV_LOG_WARNING,
               "Warning: SPS NALU missing or invalid. "
               "The resulting stream may not play.\n");

    if (!pps_seen)
        av_log(NULL, AV_LOG_WARNING,
               "Warning: PPS NALU missing or invalid. "
               "The resulting stream may not play.\n");

    out_extradata->data      = out;
    out_extradata->size      = total_size;

    return length_size;
}


/**
 * 设置start code和设置sps/pps
 * @param fmt_ctx
 * @param in  传入的bao
 * @param dst_fd
 * @return
 */
int h264_mp4toannexb(AVFormatContext *fmt_ctx, AVPacket *in, FILE *dst_fd)
{

    AVPacket *out = NULL;
    AVPacket spspps_pkt;

    int len;
    uint8_t unit_type;
    int32_t nal_size;
    uint32_t cumul_size    = 0;
    const uint8_t *buf;
    const uint8_t *buf_end;
    int            buf_size;
    int ret = 0, i;

    out = av_packet_alloc();

    buf      = in->data;
    buf_size = in->size;
    buf_end  = in->data + in->size; // 数据结尾地址

    do {
        ret= AVERROR(EINVAL);
        if (buf + 4 /*s->length_size*/ > buf_end)
            goto fail;

        // AVPackage有可能包含一帧或者多帧，每一帧开头4个字节表示帧的大小，这里nal_size表示获取的一帧大小
        for (nal_size = 0, i = 0; i<4/*s->length_size*/; i++)
            nal_size = (nal_size << 8) | buf[i];

        buf += 4; /*s->length_size;*/
        unit_type = *buf & 0x1f;  // NAL单元的类型，比如关键帧等

        if (nal_size > buf_end - buf || nal_size < 0)
            goto fail;

        /*
        if (unit_type == 7)
            s->idr_sps_seen = s->new_idr = 1;
        else if (unit_type == 8) {
            s->idr_pps_seen = s->new_idr = 1;
            */
        /* if SPS has not been seen yet, prepend the AVCC one to PPS */
        /*
        if (!s->idr_sps_seen) {
            if (s->sps_offset == -1)
                av_log(ctx, AV_LOG_WARNING, "SPS not present in the stream, nor in AVCC, stream may be unreadable\n");
            else {
                if ((ret = alloc_and_copy(out,
                                     ctx->par_out->extradata + s->sps_offset,
                                     s->pps_offset != -1 ? s->pps_offset : ctx->par_out->extradata_size - s->sps_offset,
                                     buf, nal_size)) < 0)
                    goto fail;
                s->idr_sps_seen = 1;
                goto next_nal;
            }
        }
    }
    */

        /* if this is a new IDR picture following an IDR picture, reset the idr flag.
         * Just check first_mb_in_slice to be 0 as this is the simplest solution.
         * This could be checking idr_pic_id instead, but would complexify the parsing. */
        /*
        if (!s->new_idr && unit_type == 5 && (buf[1] & 0x80))
            s->new_idr = 1;

        */
        /* prepend only to the first type 5 NAL unit of an IDR picture, if no sps/pps are already present */
        if (/*s->new_idr && */unit_type == 5 /*&& !s->idr_sps_seen && !s->idr_pps_seen*/) { // unit_type = 5表示一个关键帧

            // 从扩展数据中获取pps和sps
            h264_extradata_to_annexb( fmt_ctx->streams[in->stream_index]->codec->extradata,
                                      fmt_ctx->streams[in->stream_index]->codec->extradata_size,
                                      &spspps_pkt,
                                      AV_INPUT_BUFFER_PADDING_SIZE);
            // 增加特征码
            if ((ret=alloc_and_copy(out,
                                    spspps_pkt.data, spspps_pkt.size,
                                    buf, nal_size)) < 0)
                goto fail;
            /*s->new_idr = 0;*/
            /* if only SPS has been seen, also insert PPS */
        }
            /*else if (s->new_idr && unit_type == 5 && s->idr_sps_seen && !s->idr_pps_seen) {
                if (s->pps_offset == -1) {
                    av_log(ctx, AV_LOG_WARNING, "PPS not present in the stream, nor in AVCC, stream may be unreadable\n");
                    if ((ret = alloc_and_copy(out, NULL, 0, buf, nal_size)) < 0)
                        goto fail;
                } else if ((ret = alloc_and_copy(out,
                                            ctx->par_out->extradata + s->pps_offset, ctx->par_out->extradata_size - s->pps_offset,
                                            buf, nal_size)) < 0)
                    goto fail;
            }*/ else {
            if ((ret=alloc_and_copy(out, NULL, 0, buf, nal_size)) < 0)
                goto fail;
            /*
            if (!s->new_idr && unit_type == 1) {
                s->new_idr = 1;
                s->idr_sps_seen = 0;
                s->idr_pps_seen = 0;
            }
            */
        }

        // 写到目标文件中
        len = fwrite( out->data, 1, out->size, dst_fd);
        if(len != out->size){
            av_log(NULL, AV_LOG_DEBUG, "warning, length of writed data isn't equal pkt.size(%d, %d)\n",
                   len,
                   out->size);
        }
        fflush(dst_fd);

        next_nal:
        buf        += nal_size;
        cumul_size += nal_size + 4;//s->length_size;
    } while (cumul_size < buf_size);

    /*
    ret = av_packet_copy_props(out, in);
    if (ret < 0)
        goto fail;

    */
    fail:
    av_packet_free(&out);

    return ret;
}



// 抽取视频流
int media_extra_video(const char* src_url, const char* dst_url) {
    int ret = 0;
    FILE* dst_fd = NULL;
    int video_index = -1;
    AVPacket pkt;
    int len = 0;

    av_log(NULL, AV_LOG_INFO, "抽取多媒体文件视频：%s\n", src_url);

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

    //av_dump_format(fmt_ctx, 0, src_url, 0);

    // 从原文件获取流
    ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't av_find_best_stream file %s, the ret is %s \n", src_url, av_err2str(ret));
        goto __Error;
    }

    video_index = ret;
    av_log(NULL, AV_LOG_INFO, "视频流index = %d \n", video_index);

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
        if (pkt.stream_index == video_index) {
            h264_mp4toannexb(fmt_ctx, &pkt, dst_fd);
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

//============================================================

int conver_mp4_to_flv(const char* src_url, const char* dst_url) {

}




int main() {

    // 设置LOG等级
    av_log_set_level(AV_LOG_DEBUG);

    av_log(NULL, AV_LOG_INFO, "Now let's start the format game....\n\n");

    // 打印媒体信息
    // dump_media_info(VIDEO_PATH);

    // 抽取音频
    // media_extra_audio(VIDEO_PATH, AUDIO_OUTPUT_PATH);

    // 抽取视频
    media_extra_video(VIDEO_PATH, VIDEO_OUTPUT_PATH);

    // 格式转换,mp4转成flv
    conver_mp4_to_flv(VIDEO_PATH, FLV_OUTPUT_PATH);

    return 0;
}
