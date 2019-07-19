//
// Created by hengfeng zhuo on 2019-07-19.
//

#include "main.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace cv;

int main() {

    // 设置LOG等级
    av_log_set_level(AV_LOG_INFO);
    av_log(NULL, AV_LOG_INFO, "Now let's start the game....\n\n");

    Mat srcImg = imread("../resource/test.jpeg");
    // 均值滤波
    Mat dstImg;
    blur(srcImg, dstImg, Size(5, 5));
    imshow("dst image", dstImg);

    waitKey(0);


    return 0;
}
