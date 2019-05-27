//
// Created by hengfeng zhuo on 2019-05-27.
//
#include "main.h"

using namespace std;

int av_io_move(const char *url_src, const char *url_dst){
    int ret = 0;
    ret = avpriv_io_move(url_src, url_dst);
    av_log(NULL, AV_LOG_DEBUG, "move the file %s to file %s, the ret is %s \n", url_src, url_dst, av_err2str(ret));

    return ret;
}

int av_io_delete(const char *url_src){
    int ret = 0;
    ret = avpriv_io_delete(url_src);
    av_log(NULL, AV_LOG_DEBUG, "delete the file %s, the ret is %s \n", url_src, av_err2str(ret));
    return ret;
}

/**
 * 遍历url_src的文件信息
 * @param url_src
 * @return
 */
int av_io_scan(const char *url_src){
    int ret = 0;

    AVIODirContext* ctx = NULL;
    AVIODirEntry* entry = NULL;

    // 打开目录, 获取到上下文ctx, 注意打印语句av_err2str()
    ret = avio_open_dir(&ctx, url_src, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open the dir %s, the ret is %s \n", url_src, av_err2str(ret));
        goto __Error;
    }

    // 开始遍历
    while(1) {
        // 读取目录信息
        ret = avio_read_dir(ctx, &entry);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Can't read the dir %s, the ret is %s \n", url_src, av_err2str(ret));
            goto __Error;
        }
        // 没有更多
        if (!entry) {
            break;
        }
        av_log(NULL, AV_LOG_INFO, "%12" PRId64 " %s \n", entry->size, entry->name);

        avio_free_directory_entry(&entry);
    }


    __Error:
    avio_close_dir(&ctx);

    return ret;
}


int main() {

    // 设置LOG等级
    av_log_set_level(AV_LOG_DEBUG);

    av_io_scan(".");

    av_log(NULL, AV_LOG_INFO, "Now let's start the game....\n\n");

    return 0;
}

