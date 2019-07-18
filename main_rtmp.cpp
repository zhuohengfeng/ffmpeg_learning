//
// Created by hengfeng zhuo on 2019-07-18.
//

#include "main.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace std;
using namespace cv;

const char* RTMP_ADD = "10.88.7.193/live";

int main() {
    // 设置LOG等级
    av_log_set_level(AV_LOG_DEBUG);

    av_log(NULL, AV_LOG_INFO, "Now let's start the rtmp://%s\n\n", RTMP_ADD);

    Mat srcImg = imread("../resource/test.jpeg");
//    imshow("source image", srcImg);
    Mat dstImg;
    blur(srcImg, dstImg, Size(5, 5));
    imshow("dst image", dstImg);
    waitKey(0);

    return 0;
}

