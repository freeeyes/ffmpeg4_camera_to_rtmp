// ffmpeg-output.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include "ffmpeg_rtmp.h"
//#include "ffmpeg_rtmp_old.h"

int main()
{
	//Integrated Camera
	std::vector<CVedioDevice> vediodevicelist;
	find_win_vedio_device(vediodevicelist);

	if (vediodevicelist.size() > 0)
	{
		char szVedioCommand[128] = { '\0' };
		snprintf(szVedioCommand, 128, "video=%s", vediodevicelist[0].vedioname);
		cout << "[main]" << szVedioCommand << endl;
		av_camera_to_rtmp(szVedioCommand, "rtmp://192.145.179.70:20011/live/12345", 320, 240);
		//av_camera_to_rtmp(szVedioCommand, "example.mp4", 320, 240);
	}
	else
	{
		cout << "[main]no find camera." << endl;
	}
	getchar();
    return 0;
}