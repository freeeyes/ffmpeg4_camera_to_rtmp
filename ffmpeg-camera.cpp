// ffmpeg-output.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include "ffmpeg_rtmp.h"
#include <mutex>
#include <thread>
#include <functional>

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
	shared_ptr<CAVFormatInfo> ac_format_info = std::make_shared<CAVFormatInfo>();
	std::vector<CDeviceInfo> vectorDevices;
	find_win_audio_device(vectorDevices);


	if (vectorDevices.size() > 0)
	{
		char szVedioCommand[128] = { '\0' };
		snprintf(szVedioCommand, 128, "audio=%s", vectorDevices[0].vedioname);
		cout << "[Audio]" << szVedioCommand << endl;

		//创建线程执行
		std::thread th_audio([szVedioCommand, ac_format_info] ()
			{ 
				av_audio_to_rtmp(szVedioCommand, 2, 16, 44100, "abc.aac", ac_format_info);
			});

		//阻塞等待输入
		std::cin.get();
		ac_format_info->is_run = false;

		getchar();
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