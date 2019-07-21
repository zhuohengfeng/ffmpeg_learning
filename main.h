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
#include <iostream>
#include <inttypes.h>
#include <string>
#include <exception>

#ifdef __cplusplus             //告诉编译器，这部分代码按C语言的格式进行编译，而不是C++的
extern "C"{
#endif

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavdevice/avdevice.h>
#include <libavformat/version.h>
#include <libavutil/mathematics.h>
#include <libavutil/imgutils.h>

#include <libavutil/time.h>

#ifdef __cplusplus
}
#endif


#include <QtCore/QCoreApplication>
#include <QAudioInput>
#include <QDebug>
#include <QMainWindow>
#include <QListWidget>
#include <QListWidgetItem>
#include <QThread>

#endif /* main_h */
