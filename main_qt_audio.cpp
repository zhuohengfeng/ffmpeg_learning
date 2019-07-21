//
// Created by hengfeng zhuo on 2019-07-20.
//
#include "main.h"

char *outUrl = "rtmp://192.168.1.44/live";

int main(int argc, char* argv[]) {
    int ret = 0;

    // 设置LOG等级
    QCoreApplication a(argc, argv);

    // 注册所有的封装器
    av_register_all();

    // 注册所有网络协议
    avformat_network_init();


    // 这里设置QT的录制声音参数
    int sampleRate = 44100; // 采样率
    int channels = 2;  // 通道
    int sampleBytes = 2; // 位数
    AVSampleFormat  inSampleFmt = AV_SAMPLE_FMT_S16;
    AVSampleFormat  outSampleFmt = AV_SAMPLE_FMT_FLTP;

    /// 1. QT音频开始录制
    QAudioFormat fmt;
    fmt.setSampleRate(sampleRate); //采样率
    fmt.setChannelCount(channels); //通道
    fmt.setSampleSize(sampleBytes*8); //采样位数
    fmt.setCodec("audio/pcm");
    fmt.setByteOrder(QAudioFormat::LittleEndian);
    fmt.setSampleType(QAudioFormat::UnSignedInt);
    QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();

    if (!info.isFormatSupported(fmt)) {
        qDebug() << "Audio Format不支持";
        fmt = info.nearestFormat(fmt);
    }

    // 开始录制音频
    qDebug() << "Audio Format设置完成，开始录音";
    QAudioInput *input = new QAudioInput(fmt);
    // 这里已经开始采集了
    QIODevice *io = input->start();

    /// 2. 音频重采样，上下文初始化---- 具体的重采样要在后面读到帧后再处理
    // 因为我们设置的是16位，但是编码AAC要去FLTP格式，所以我们要做重采样
    SwrContext *asc = NULL;
    asc = swr_alloc_set_opts(asc,
            // 输出格式
            av_get_default_channel_layout(channels),
            outSampleFmt,
            sampleRate,
            // 输入格式
            av_get_default_channel_layout(channels),
            inSampleFmt,
            sampleRate,
            0, 0
            );
    if (!asc) {
        qDebug() << "swr_alloc_set_opts 错误";
        goto _Error;
    }
    ret = swr_init(asc);
    if (ret !=0 ) {
        qDebug() << "swr_init 错误";
        goto _Error;
    }
    else {
        /// 3. 音频重采样输出空间分配
        qDebug() << "Audio 开始重采样....";
        // 重采样的还是原始数据，所以放到一个avframe中
        AVFrame* pcmAvFrame = av_frame_alloc();
        pcmAvFrame->format = outSampleFmt; // FLTP
        pcmAvFrame->channels = channels;
        pcmAvFrame->channel_layout = av_get_default_channel_layout(channels); // 通道布局
        pcmAvFrame->nb_samples = 1024; //一帧音频一通道的采用数量
        // 分配pcm avframe实际的空间
        ret = av_frame_get_buffer(pcmAvFrame, 0);
        if (ret !=0 ) {
            qDebug() << "av_frame_get_buffer 错误";
            goto _Error;
        }

        ///4 初始化音频编码器
        AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        if (!codec){
            qDebug() << "avcodec_find_encoder AV_CODEC_ID_AAC failed!";
            goto _Error;
        }
        // 音频编码器上下文
        AVCodecContext *codecContext = avcodec_alloc_context3(codec);
        if (!codecContext)
        {
            qDebug() << "avcodec_alloc_context3 AV_CODEC_ID_AAC failed!";
            goto _Error;
        }

        codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        codecContext->thread_count = 8;
        codecContext->bit_rate = 40000;
        codecContext->sample_rate = sampleRate;
        codecContext->sample_fmt = AV_SAMPLE_FMT_FLTP;
        codecContext->channels = channels;
        codecContext->channel_layout = av_get_default_channel_layout(channels);

        //打开音频编码器
        ret = avcodec_open2(codecContext, 0, 0);
        if (ret != 0)
        {
            qDebug() << "avcodec_open2 failed!";
            goto _Error;
        }

        /// 5. 输出封装器和音频流配置
        // 创建输出封装器上下文flv
        AVFormatContext *formatContext = NULL;
        ret = avformat_alloc_output_context2(&formatContext, 0, "flv", outUrl);
        if (ret != 0)
        {
            qDebug() << "avformat_alloc_output_context2 failed!";
            goto _Error;
        }

        // 添加音频流
        AVStream *audioStream = avformat_new_stream(formatContext,NULL);
        audioStream->codecpar->codec_tag = 0;
        // 从编码器中复制参数
        avcodec_parameters_from_context(audioStream->codecpar, codecContext);
        av_dump_format(formatContext, 0, outUrl, 1);

        ///6. 打开rtmp 的网络输出IO
        ret = avio_open(&formatContext->pb, outUrl, AVIO_FLAG_WRITE);
        if (ret != 0)
        {
            qDebug() << "avio_open failed!";
            goto _Error;
        }


        /**
            Mux 主要分为 三步操作：
                avformat_write_header ： 写文件头
                av_write_frame/av_interleaved_write_frame： 写packet
                av_write_trailer ： 写文件尾，对于流文件（RTMP）可以不需要
         */
        //写入Mux封装头---用于写视频文件头
        ret = avformat_write_header(formatContext, NULL);
        if (ret != 0)
        {
            qDebug() << "avformat_write_header failed!";
            goto _Error;
        }

        // 一次读取的一帧音频的字节数
        int readSize = pcmAvFrame->nb_samples * channels * sampleBytes; // 1024 * 2 * 2 = 4096个字节
        char *buf = new char[readSize]; // 每次读的字节数，QT中读
        int apts = 0;
        AVPacket avPacket = {0};
        while(true) {
            // QT读取录音
            if (input->bytesReady() < readSize) {
                // 如果数据不满4096，则等待1毫秒
                QThread::msleep(1);
                continue;
            }
            int size = 0;
            while (size != readSize) {
                int len = io->read(buf+size, readSize - size);
                if (len < 0) {
                    break;
                }
                size += len;
            }

            if (size != readSize) {
                continue;
            }

            // 已经读一帧源数据PCM
            // 重采样源数据,得到重采样数据
            const uint8_t *indata[AV_NUM_DATA_POINTERS] = {0};
            indata[0] = (uint8_t *)buf;
            // 重采样的数据就放在这个AvFrame的data中
            int len = swr_convert(asc, pcmAvFrame->data, pcmAvFrame->nb_samples , // 输出参数和样本数量
                        indata, pcmAvFrame->nb_samples);
            qDebug() << "重采样了字节: " << len;

            // pts运算
            // nb_sapmle / sample_reate = 一帧音频的秒数sec
            // pts = sec * timebase.den
            pcmAvFrame->pts = apts; // pts需要递增的
            // pts递增，av_rescale_q函数用于time_base之间"转换"
            // 这个函数的作用是计算a * bq / cq，来把时间戳从一个时基调整到另外一个时基。
            // 在进行时基转换的时候，我们应该首选这个函数，因为它可以避免溢出的情况发生。
            apts += av_rescale_q(pcmAvFrame->nb_samples, {1, sampleRate}, codecContext->time_base);

            // 发送给编码器
            ret = avcodec_send_frame(codecContext, pcmAvFrame);
            if (ret != 0) {
                continue;
            }

            // 接收编码后的数据，注意这里的是编码后的，所以是avPacket
            av_packet_unref(&avPacket);
            ret = avcodec_receive_packet(codecContext, &avPacket);
            if (ret != 0) {
                continue;
            }
            qDebug() << "得到编码后的数据包: " << sizeof(avPacket.data);

            //推流
            avPacket.pts = av_rescale_q(avPacket.pts, codecContext->time_base, codecContext->time_base);
            avPacket.dts = av_rescale_q(avPacket.dts, codecContext->time_base, codecContext->time_base);
            avPacket.duration = av_rescale_q(avPacket.duration, codecContext->time_base, codecContext->time_base);
            ret = av_interleaved_write_frame(formatContext, &avPacket); // 写入封装格式上下文
            if (ret == 0)
            {
                std::cout << "#" << flush;
            }
        }

        delete buf;
    }



_Error:

    return a.exec();
}
