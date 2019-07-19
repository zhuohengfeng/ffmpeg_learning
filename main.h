//
//  main.h
//  io_format
//
//  Created by hengfeng zhuo on 2019/2/12.
//  Copyright © 2019 hengfeng zhuo. All rights reserved.
//

#ifndef main_h
#define main_h

#include <stdio.h>
#include <istream>
#include <inttypes.h>

#ifdef __cplusplus             //告诉编译器，这部分代码按C语言的格式进行编译，而不是C++的
extern "C"{
#endif

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavformat/version.h>
#include <libavutil/mathematics.h>

#include <libavutil/time.h>

#ifdef __cplusplus
}
#endif



#endif /* main_h */
