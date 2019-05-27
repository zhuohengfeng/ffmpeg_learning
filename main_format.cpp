//
// Created by hengfeng zhuo on 2019-05-27.
//
#include "main.h"

using namespace std;


// 格式转换， 复用/解复用
// FFmpeg操作数据流程： 解复用-->获取流avstream-->读数据包avformat-->释放资源


/**
 * 打印音视频文件的meta信息
 * @param url
 */
void dump_media_info(const char* url) {

}












int main() {

    // 设置LOG等级
    av_log_set_level(AV_LOG_DEBUG);

    av_log(NULL, AV_LOG_INFO, "Now let's start the format game....\n\n");







    return 0;
}
