#include "ffmpeg_rtmp.h"

double av_r2d(AVRational r)
{
	if (r.num == 0 || r.den == 0)
	{
		return 0.0;
	}
	else
	{
		return (double)r.num / r.den;
	}
}

void av_error_string_output(const char* funcname, int line, int ret)
{
	char av_error[1024] = { '\0' };
	av_strerror(ret, av_error, 1024);
	cout << "[av_error_string_output](" << funcname << ":" << line << ")" << av_error << endl;
}

void av_free_context(AVFormatContext* ictx, AVFormatContext* octx)
{
	if (nullptr != ictx)
	{
		avformat_close_input(&ictx);
	}

	if (nullptr != octx)
	{
		avformat_free_context(octx);
	}
}

void av_camera_to_rtmp(const char* in_camera, const char* out_url_file, int w, int h)
{
	int ret = 0;

	unsigned char* src_data[AV_NUM_DATA_POINTERS];
	unsigned char* dst_data[AV_NUM_DATA_POINTERS];
	int src_linesize[AV_NUM_DATA_POINTERS];
	int dst_linesize[AV_NUM_DATA_POINTERS];

	//init device
	avdevice_register_all();
	//init av net
	avformat_network_init();

	char szVedioSize[20] = { '\0' };
	snprintf(szVedioSize, 20, "%dx%d", w, h);

	AVFormatContext* ictx = nullptr;  //input context
	AVFormatContext* octx = nullptr;  //output context

	AVDictionary* ioptions = nullptr;
	av_dict_set(&ioptions, "video_size", szVedioSize, 0);
	av_dict_set(&ioptions, "pixel_format", av_get_pix_fmt_name(AV_PIX_FMT_YUYV422), 0);
	av_dict_set(&ioptions, "framerate", "30", 0);

	string strACS = (string)in_camera;
	string strUTF8 = ASCII2UTF_8(strACS);

	AVInputFormat* ifmt = av_find_input_format("dshow");
	ret = avformat_open_input(&ictx, strUTF8.c_str(), ifmt, &ioptions);
	if (0 != ret)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	av_dict_free(&ioptions);

	cout << "[av_file_to_rtmp]open camera OK." << endl;

	ret = avformat_find_stream_info(ictx, nullptr);
	if (0 != ret)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	av_dump_format(ictx, 0, in_camera, 0);

	int stream_index = -1;
	stream_index = av_find_best_stream(ictx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	if (-1 == stream_index) {
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	//编码器
	AVCodec* encodec = nullptr;
	encodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!encodec) {
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	AVCodecContext* encodec_ctx = nullptr;
	encodec_ctx = avcodec_alloc_context3(encodec);
	if (!encodec_ctx)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}
	encodec_ctx->codec_id = encodec->id;
	encodec_ctx->bit_rate = 400000;
	encodec_ctx->width = ictx->streams[stream_index]->codecpar->width;
	encodec_ctx->height = ictx->streams[stream_index]->codecpar->height;
	encodec_ctx->time_base.num = 1;
	encodec_ctx->time_base.den = 30;
	encodec_ctx->framerate.num = 30;
	encodec_ctx->framerate.den = 1;
	encodec_ctx->gop_size = 10;
	encodec_ctx->max_b_frames = 0;
	encodec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	encodec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	/* Some formats want stream headers to be separate. */
	//if (octx->oformat->flags & AVFMT_GLOBALHEADER)
	//	encodec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	AVDictionary* param = NULL;
	av_dict_set(&param, "preset", "superfast", 0);
	av_dict_set(&param, "tune", "zerolatency", 0);
	av_dict_set(&param, "profile", "main", 0);

	if (0 > avcodec_open2(encodec_ctx, encodec, &param)) {
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	av_dict_free(&param);

	ret = avformat_alloc_output_context2(&octx, 0, "flv", out_url_file);
	if (!octx)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	//traversal input
	AVStream* output_stream = avformat_new_stream(octx, encodec_ctx->codec);
	if (nullptr == output_stream)
	{
		av_error_string_output(__FUNCTION__, __LINE__, 0);
		av_free_context(ictx, octx);
		return;
	}

	ret = avcodec_parameters_from_context(output_stream->codecpar, encodec_ctx);
	if (0 != ret)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	//av_stream_set_r_frame_rate(output_stream, av_make_q(1, fps));
	ret = avio_open(&octx->pb, out_url_file, AVIO_FLAG_WRITE);
	if (ret != 0)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		avio_close(octx->pb);
		av_free_context(ictx, octx);
		return;
	}

	av_dump_format(octx, 0, out_url_file, 1);

	ret = avformat_write_header(octx, NULL);
	if (0 > ret)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		avio_close(octx->pb);
		av_free_context(ictx, octx);
		return;
	}

	AVPacket packet;
	av_init_packet(&packet);
	packet.data = NULL;
	packet.size = 0;

	struct SwsContext* sws_ctx = sws_getContext(ictx->streams[stream_index]->codecpar->width,
		ictx->streams[stream_index]->codecpar->height,
		(AVPixelFormat)ictx->streams[stream_index]->codecpar->format,
		w,
		h,
		AV_PIX_FMT_YUV420P,
		SWS_BILINEAR, NULL, NULL, NULL);
	int src_bufsize = av_image_alloc(src_data, src_linesize,
		ictx->streams[stream_index]->codecpar->width,
		ictx->streams[stream_index]->codecpar->height,
		(AVPixelFormat)ictx->streams[stream_index]->codecpar->format,
		16);
	int dst_bufsize = av_image_alloc(dst_data, dst_linesize, w, h, AV_PIX_FMT_YUV420P, 1);

	AVFrame* outFrame = av_frame_alloc();
	unsigned char* picture_buf = (uint8_t*)av_malloc(dst_bufsize);
	av_image_fill_arrays(outFrame->data,
		outFrame->linesize,
		picture_buf,
		encodec_ctx->pix_fmt,
		encodec_ctx->width,
		encodec_ctx->height,
		1);
	outFrame->format = encodec_ctx->pix_fmt;
	outFrame->width = encodec_ctx->width;
	outFrame->height = encodec_ctx->height;

	int y_size = encodec_ctx->width * encodec_ctx->height;
	AVPacket outpkt;
	av_new_packet(&outpkt, dst_bufsize);

	int loop = 0;
	int got_picture = -1;
	int delayedFrame = 0;
	while (1) 
	{
		ret = av_read_frame(ictx, &packet);
		if (0 > ret)
		{
			break;
		}

		if (packet.stream_index == stream_index) {

			memcpy(src_data[0], packet.data, packet.size);
			sws_scale(sws_ctx, 
				src_data,
				src_linesize, 
				0,
				ictx->streams[stream_index]->codecpar->height,
				dst_data, 
				dst_linesize);
			outFrame->data[0] = dst_data[0];
			outFrame->data[1] = dst_data[0] + y_size;
			outFrame->data[2] = dst_data[0] + y_size * 5 / 4;
			outFrame->pts = loop;
			loop++;

			ret = avcodec_send_frame(encodec_ctx, outFrame);
			if (ret < 0)
				continue;
			ret = avcodec_receive_packet(encodec_ctx, &outpkt);

			if (0 == ret) 
			{
				outpkt.stream_index = output_stream->index;
				AVRational itime = ictx->streams[packet.stream_index]->time_base;
				AVRational otime = octx->streams[packet.stream_index]->time_base;

				outpkt.pts = av_rescale_q_rnd(packet.pts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
				outpkt.dts = av_rescale_q_rnd(packet.dts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
				outpkt.duration = av_rescale_q_rnd(packet.duration, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
				outpkt.pos = -1;

				ret = av_interleaved_write_frame(octx, &outpkt);
				cout << "output frame " << loop - delayedFrame << endl;
			}
			else {
				delayedFrame++;
				cout << "output no frame" << endl;;
			}
		}
		av_packet_unref(&packet);
	}
	
	av_free(picture_buf);
	av_freep(&src_data[0]);
	av_freep(&dst_data[0]);
	av_free_context(ictx, octx);
	cout << "[av_camera_to_rtmp]test ffmpeg to rtmp ok." << endl;
}

void av_audio_to_rtmp(const char* in_Audio, int channels, int samplesize, int samplerate, const char* out_url_file)
{
	int ret = 0;
	int audio_stream_index = -1;
	int samplebyte = 2;

	//init device
	avdevice_register_all();
	//init av net
	avformat_network_init();

	AVFormatContext* ictx = nullptr;  //input context
	AVFormatContext* octx = nullptr;  //output context

	AVDictionary* ioptions = nullptr;
	av_dict_set_int(&ioptions, "sample_rate", samplerate, 0);
	av_dict_set_int(&ioptions, "sample_size", samplesize, 0);
	av_dict_set_int(&ioptions, "channels", channels, 0);

	string strACS = (string)in_Audio;
	string strUTF8 = ASCII2UTF_8(strACS);

	AVInputFormat* ifmt = av_find_input_format("dshow");
	ret = avformat_open_input(&ictx, strUTF8.c_str(), ifmt, &ioptions);
	if (0 != ret)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}
	av_dict_free(&ioptions);

	av_dump_format(ictx, 0, in_Audio, 0);

	for (unsigned int i = 0; i < ictx->nb_streams; i++)
	{
		if (ictx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			audio_stream_index = i;
			break;
		}
	}
	if (-1 == audio_stream_index)
	{
		cout << "[av_audio_to_rtmp]no find audio stream." << endl;
		av_free_context(ictx, octx);
		return;
	}

	cout << "[av_audio_to_rtmp]fmt=" << av_get_sample_fmt_name((AVSampleFormat)ictx->streams[audio_stream_index]->codecpar->format) << endl;

	//Re-sampling
	AVSampleFormat outSampleFmt = AVSampleFormat::AV_SAMPLE_FMT_FLTP;
	SwrContext* asc = nullptr;
	asc = swr_alloc_set_opts(asc,
		av_get_default_channel_layout(channels), outSampleFmt, samplerate,//输出格式
		av_get_default_channel_layout(channels), (AVSampleFormat)ictx->streams[audio_stream_index]->codecpar->format, samplerate, 0, 0);//输入格式
	if (!asc)
	{
		cout << "[av_audio_to_rtmp]swr_alloc_set_opts error." << endl;
		av_free_context(ictx, octx);
		return;
	}

	ret = swr_init(asc);
	if (ret != 0)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	AVFrame* pcm = av_frame_alloc();
	pcm->format = outSampleFmt;
	pcm->channels = channels;
	pcm->channel_layout = av_get_default_channel_layout(channels);
	pcm->nb_samples = 1024; //一帧音频一通道的采用数量
	ret = av_frame_get_buffer(pcm, 0);
	if (ret != 0)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (!codec)
	{
		cout << "[av_audio_to_rtmp]avcodec_find_encoder AV_CODEC_ID_AAC failed!" << endl;
		av_free_context(ictx, octx);
		return;
	}

	AVCodecContext* ac = avcodec_alloc_context3(codec);
	if (!ac)
	{
		cout << "[av_audio_to_rtmp]avcodec_alloc_context3 failed!" << endl;
		av_free_context(ictx, octx);
		return;
	}

	ac->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	ac->thread_count = 8;
	ac->bit_rate = 40000;
	ac->sample_rate = samplerate;
	ac->sample_fmt = AV_SAMPLE_FMT_FLTP;
	ac->channels = channels;
	ac->channel_layout = av_get_default_channel_layout(channels);

	AVDictionary* ooptions = nullptr;
	av_dict_set_int(&ooptions, "sample_rate", samplerate, 0);
	av_dict_set_int(&ooptions, "sample_size", samplesize, 0);
	av_dict_set_int(&ooptions, "channels", channels, 0);

	ret = avcodec_open2(ac, 0, &ooptions);
	if (ret != 0)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	av_dict_free(&ooptions);

	ret = avformat_alloc_output_context2(&octx, 0, "flv", out_url_file);
	if (!octx)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	AVStream* output_stream = avformat_new_stream(octx, nullptr);
	if (nullptr == output_stream)
	{
		av_error_string_output(__FUNCTION__, __LINE__, 0);
		av_free_context(ictx, octx);
		return;
	}

	output_stream->codecpar->codec_tag = 0;
	ret = avcodec_parameters_from_context(output_stream->codecpar, ac);
	if (0 != ret)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	av_dump_format(octx, 0, out_url_file, 1);

	ret = avio_open(&octx->pb, out_url_file, AVIO_FLAG_WRITE);
	if (ret != 0)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		avio_close(octx->pb);
		av_free_context(ictx, octx);
		return;
	}

	ret = avformat_write_header(octx, NULL);
	if (0 > ret)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		avio_close(octx->pb);
		av_free_context(ictx, octx);
		return;
	}

	int frameindex = 0;
	int64_t apts = 0;
	AVPacket inpkt = { 0 };
	AVPacket outpkt = { 0 };

	while (1)
	{
		ret = av_read_frame(ictx, &inpkt);
		if (0 != ret)
		{
			break;
		}

		frameindex++;
		//cout << "frameindex=" << frameindex << endl;

		if (0 >= inpkt.size)
		{
			continue;
		}

		const uint8_t* indata[AV_NUM_DATA_POINTERS] = { 0 };
		indata[0] = (uint8_t*)inpkt.data;
		int len = swr_convert(asc, pcm->data, pcm->nb_samples, //输出参数，输出存储地址和样本数量
			indata, pcm->nb_samples
		);

		pcm->pts = apts;
		apts += av_rescale_q(pcm->nb_samples, { 1, samplerate }, ac->time_base);

		int ret = avcodec_send_frame(ac, pcm);
		if (ret != 0)
		{
			continue;
		}

		ret = avcodec_receive_packet(ac, &outpkt);
		if (ret != 0)
		{
			continue;
		}

		//推流
		outpkt.pts = av_rescale_q(outpkt.pts, ac->time_base, output_stream->time_base);
		outpkt.dts = av_rescale_q(outpkt.dts, ac->time_base, output_stream->time_base);
		outpkt.duration = av_rescale_q(outpkt.duration, ac->time_base, output_stream->time_base);
		ret = av_interleaved_write_frame(octx, &outpkt);
		if (ret == 0)
		{
			cout << "#" << flush;
		}
	}

	av_write_trailer(octx);
	avio_close(octx->pb);
	av_free_context(ictx, octx);
	cout << "[av_audio_to_rtmp]test ffmpeg to rtmp ok." << endl;
}

void av_file_to_rtmp(const char* in_url_file, const char* out_url_file)
{
	int ret = 0;
	//init device
	avdevice_register_all();
	//init av net
	avformat_network_init();

	AVFormatContext* ictx = nullptr;  //input context
	AVFormatContext* octx = nullptr;  //output context

	ret = avformat_open_input(&ictx, in_url_file, nullptr, nullptr);
	if (0 != ret)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	cout << "[av_file_to_rtmp]open file OK." << endl;

	ret = avformat_find_stream_info(ictx, nullptr);
	if (0 != ret)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	//av_dump_format(ictx, 0, in_url_file, 0);

	ret = avformat_alloc_output_context2(&octx, 0, "flv", out_url_file);

	if (!octx)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	//traversal input
	for (unsigned int i = 0; i < ictx->nb_streams; i++)
	{
		AVCodec* avcodec = avcodec_find_decoder(ictx->streams[i]->codecpar->codec_id);
		AVStream* output_stream = avformat_new_stream(octx, avcodec);
		if (output_stream)
		{
			AVCodecContext* codec_ctx = avcodec_alloc_context3(avcodec);
			ret = avcodec_parameters_to_context(codec_ctx, ictx->streams[i]->codecpar);
			if (0 != ret)
			{
				av_error_string_output(__FUNCTION__, __LINE__, ret);
				av_free_context(ictx, octx);
				return;
			}

			codec_ctx->codec_tag = 0;
			if (octx->oformat->flags & AVFMT_GLOBALHEADER)
			{
				codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
			}

			ret = avcodec_parameters_from_context(output_stream->codecpar, codec_ctx);
			if (0 != ret)
			{
				av_error_string_output(__FUNCTION__, __LINE__, ret);
				av_free_context(ictx, octx);
				return;
			}
		}
	}

	av_dump_format(octx, 0, out_url_file, 1);

	//rtmp output
	ret = avio_open(&octx->pb, out_url_file, AVIO_FLAG_WRITE);
	if (!octx->pb)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	ret = avformat_write_header(octx, nullptr);
	if (ret < 0)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	cout << "[av_file_to_rtmp]avformat_write_header(" << ret << ")" << endl;

	AVPacket pkt;
	long long starttime = av_gettime();

	for (;;)
	{
		ret = av_read_frame(ictx, &pkt);
		if (0 != ret)
		{
			break;
		}

		if (0 >= pkt.size)
		{
			continue;
		}

		//cout << pkt.pts << " " << flush;

		//convert pts dts
		AVRational itime = ictx->streams[pkt.stream_index]->time_base;
		AVRational otime = octx->streams[pkt.stream_index]->time_base;

		pkt.pts = av_rescale_q_rnd(pkt.pts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q_rnd(pkt.duration, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.pos = -1;

		/*
		if (ictx->streams[pkt.stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			AVRational tb = ictx->streams[pkt.stream_index]->time_base;

			unsigned int now = av_gettime() - starttime;
			unsigned int dts = 0;
			dts = pkt.dts * (1000 * 1000 * av_r2d(tb));
			if (dts > now)
			{
				av_usleep((unsigned int)(dts - now));
			}
		}
		*/

		ret = av_interleaved_write_frame(octx, &pkt);
		if (ret < 0)
		{
			av_error_string_output(__FUNCTION__, __LINE__, ret);
			av_free_context(ictx, octx);
			return;
		}
	}

	av_free_context(ictx, octx);
	cout << "[av_file_to_rtmp]test ffmpeg to rtmp ok." << endl;
}

void find_win_vedio_device(std::vector<CDeviceInfo>& vediodevicelist)
{
	//获取摄像头信息
	vediodevicelist.clear();

	std::vector<TDeviceName> vectorDevices;
	GetAudioVideoInputDevices(vectorDevices, CLSID_VideoInputDeviceCategory);

	for (TDeviceName& vediodeviceinfo : vectorDevices)
	{
		CDeviceInfo vediodevice;
		snprintf(vediodevice.vedioname, 128, "%s", WCharToChar(vediodeviceinfo.FriendlyName));
		snprintf(vediodevice.vediodesc, 256, "%s", WCharToChar(vediodeviceinfo.MonikerName));
		vediodevicelist.push_back(vediodevice);
	}
}

void find_win_audio_device(std::vector<CDeviceInfo>& vediodevicelist)
{
	//获取音频信息
	vediodevicelist.clear();

	std::vector<TDeviceName> audioDevices;
	GetAudioVideoInputDevices(audioDevices, CLSID_AudioInputDeviceCategory);

	for (TDeviceName& audiodeviceinfo : audioDevices)
	{
		CDeviceInfo audiodevice;
		snprintf(audiodevice.vedioname, 128, "%s", WCharToChar(audiodeviceinfo.FriendlyName));
		snprintf(audiodevice.vediodesc, 256, "%s", WCharToChar(audiodeviceinfo.MonikerName));
		vediodevicelist.push_back(audiodevice);
	}
}
