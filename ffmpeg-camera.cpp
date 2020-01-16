// ffmpeg-output.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include "ffmpeg_rtmp.h"
//#include "ffmpeg_rtmp_old.h"

void Camera_Vedio_test()
{
	//Integrated Camera
	std::vector<CDeviceInfo> vediodevicelist;
	find_win_vedio_device(vediodevicelist);

	if (vediodevicelist.size() > 0)
	{
		char szVedioCommand[128] = { '\0' };
		snprintf(szVedioCommand, 128, "video=%s", vediodevicelist[0].vedioname);
		cout << "[Vedio]" << szVedioCommand << endl;
		av_camera_to_rtmp(szVedioCommand, RTMP_URL, 320, 240);
		//av_camera_to_rtmp(szVedioCommand, "example.mp4", 320, 240);
	}
	else
	{
		cout << "[Vedio]no find camera." << endl;
	}
}

void Device_Audio_test()
{
	std::vector<CDeviceInfo> vectorDevices;
	find_win_audio_device(vectorDevices);

	if (vectorDevices.size() > 0)
	{
		char szVedioCommand[128] = { '\0' };
		snprintf(szVedioCommand, 128, "audio=%s", vectorDevices[0].vedioname);
		cout << "[Audio]" << szVedioCommand << endl;
		av_audio_to_rtmp(szVedioCommand, RTMP_URL);
	}
	else
	{
		cout << "[Audio]no find camera." << endl;
	}
}


int main()
{
	//Camera_Vedio_test();
	Device_Audio_test();

	getchar();
    return 0;
}