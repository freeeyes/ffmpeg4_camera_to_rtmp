// ffmpeg-output.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include "ffmpeg_rtmp.h"
//#include "ffmpeg_rtmp_old.h"

int main()
{
	//Integrated Camera
	find_win_vedio_device();
	av_camera_to_rtmp("video=Integrated Camera", "rtmp://127.0.0.1:1735/live/12345", 320, 240);
	//av_camera_to_rtmp("video=Integrated Camera", "example.mp4", 320, 240);
	//captureFrame();
	getchar();
    return 0;
}