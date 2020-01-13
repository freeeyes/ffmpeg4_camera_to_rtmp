#pragma once

extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h" 
#include "libavutil/timestamp.h"
#include "libavutil/rational.h"
#include "libavutil/time.h"
#include "libavutil/pixfmt.h"
#include "libavutil/pixdesc.h"
}
#include "vediolist.h"
#include <iostream>
using namespace std;

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "swscale.lib")

double av_r2d(AVRational r);

void av_error_string_output(const char* funcname, int line, int ret);

void av_free_context(AVFormatContext* ictx, AVFormatContext* octx);

void av_camera_to_rtmp(const char* in_camera, const char* out_url_file, int w, int h);

void av_file_to_rtmp(const char* in_url_file, const char* out_url_file);

void find_win_vedio_device();
