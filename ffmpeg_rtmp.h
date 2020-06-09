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
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
#include "libavutil/audio_fifo.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
}
#include "vediolist.h"
#include "CSwr.h"
#include <iostream>
using namespace std;

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "avfilter.lib")

static const  char* RTMP_URL = "rtmp://192.144.179.70:20011/live/12345";

#define AV_CODEC_FLAG_GLOBAL_HEADER (1 << 22)
#define CODEC_FLAG_GLOBAL_HEADER AV_CODEC_FLAG_GLOBAL_HEADER
#define AVFMT_RAWPICTURE 0x0020

class CDeviceInfo
{
public:
	char vedioname[128] = {'\0'};
	char vediodesc[256] = {'\0'};
};

class CAVFormatInfo
{
public:
	AVFormatContext* ictx = nullptr;   //ÊäÈëÔ´
	bool is_run = false;
};

int init_audio_sample(AVFormatContext* ictx, AVFilterGraph* filter_graph, AVFilterContext*& buffer_sink_ctx, AVFilterContext*& buffer_src_ctx);

AVFrame* decode_audio(AVPacket* in_packet, AVFrame* src_audio_frame, AVCodecContext* decode_codectx, AVFilterContext* buffer_sink_ctx, AVFilterContext* buffer_src_ctx);

double av_r2d(AVRational r);

void av_error_string_output(const char* funcname, int line, int ret);

void av_free_context(AVFormatContext* ictx, AVFormatContext* octx);

void av_camera_to_rtmp(const char* in_camera, const char* out_url_file, int w, int h);

void av_audio_to_rtmp(const char* in_Audio, int channels, int samplesize, int samplerate, const char* out_url_file, shared_ptr<CAVFormatInfo> av_format_info);

void av_file_to_rtmp(const char* in_url_file, const char* out_url_file);

void find_win_vedio_device(std::vector<CDeviceInfo>& vediodevicelist);

void find_win_audio_device(std::vector<CDeviceInfo>& vediodevicelist);
