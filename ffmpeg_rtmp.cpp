#include "ffmpeg_rtmp.h"

int init_audio_sample(AVFormatContext* ictx, AVFilterGraph* filter_graph, AVFilterContext*& buffer_sink_ctx, AVFilterContext*& buffer_src_ctx)
{
	//初始化采样率
	char args[512] = {'\0'};
	int ret;
	const AVFilter* abuffersrc = avfilter_get_by_name("abuffer");
	const AVFilter* abuffersink = avfilter_get_by_name("abuffersink");
	AVFilterInOut* outputs = avfilter_inout_alloc();
	AVFilterInOut* inputs = avfilter_inout_alloc();

	auto audioDecoderContext = ictx->streams[0]->codecpar;
	if (!audioDecoderContext->channel_layout)
		audioDecoderContext->channel_layout = av_get_default_channel_layout(audioDecoderContext->channels);

	static const enum AVSampleFormat out_sample_fmts[] = { AV_SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_NONE };
	static const int64_t out_channel_layouts[] = { audioDecoderContext->channel_layout, -1 };
	static const int out_sample_rates[] = { audioDecoderContext->sample_rate , -1 };

	AVRational time_base = ictx->streams[0]->time_base;
	filter_graph = avfilter_graph_alloc();
	filter_graph->nb_threads = 1;

	sprintf_s(args, sizeof(args),
		"time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%I64x",
		time_base.num, time_base.den, audioDecoderContext->sample_rate,
		av_get_sample_fmt_name((AVSampleFormat)audioDecoderContext->format), audioDecoderContext->channel_layout);

	cout << "[init_audio_sample]args=" << args << endl;

	ret = avfilter_graph_create_filter(&buffer_src_ctx, abuffersrc, "in",
		args, NULL, filter_graph);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
		return ret;
	}

	/* buffer audio sink: to terminate the filter chain. */
	ret = avfilter_graph_create_filter(&buffer_sink_ctx, abuffersink, "out",
		NULL, NULL, filter_graph);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
		return ret;
	}

	ret = av_opt_set_int_list(buffer_sink_ctx, "sample_fmts", out_sample_fmts, -1,
		AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
		return ret;
	}

	ret = av_opt_set_int_list(buffer_sink_ctx, "channel_layouts", out_channel_layouts, -1,
		AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
		return ret;
	}

	ret = av_opt_set_int_list(buffer_sink_ctx, "sample_rates", out_sample_rates, -1,
		AV_OPT_SEARCH_CHILDREN);
	if (ret < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
		return ret;
	}

	/* Endpoints for the filter graph. */
	outputs->name = av_strdup("in");
	outputs->filter_ctx = buffer_src_ctx;;
	outputs->pad_idx = 0;
	outputs->next = NULL;

	inputs->name = av_strdup("out");
	inputs->filter_ctx = buffer_sink_ctx;
	inputs->pad_idx = 0;
	inputs->next = NULL;

	if ((ret = avfilter_graph_parse_ptr(filter_graph, "anull",
		&inputs, &outputs, nullptr)) < 0)
		return ret;

	if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
		return ret;

	av_buffersink_set_frame_size(buffer_sink_ctx, 1024);
	return 0;
}

AVFrame* decode_audio(AVPacket* in_packet, 
	AVFrame* src_audio_frame, 
	AVCodecContext* decode_codectx, 
	AVFilterContext* buffer_sink_ctx, 
	AVFilterContext* buffer_src_ctx)
{
	int gotFrame;
	AVFrame* filtFrame = nullptr;

	int ret = avcodec_send_packet(decode_codectx, in_packet);
	if (ret != 0)
	{
		return nullptr;
	}

	while (ret >= 0)
	{
		ret = avcodec_receive_frame(decode_codectx, src_audio_frame);
		if (ret < 0)
		{
			break;
		}

		if (av_buffersrc_add_frame_flags(buffer_src_ctx, src_audio_frame, AV_BUFFERSRC_FLAG_PUSH) < 0) {
			av_log(NULL, AV_LOG_ERROR, "buffe src add frame error!\n");
			return nullptr;
		}

		filtFrame = av_frame_alloc();
		int ret = av_buffersink_get_frame_flags(buffer_sink_ctx, filtFrame, AV_BUFFERSINK_FLAG_NO_REQUEST);
		if (ret < 0)
		{
			av_frame_free(&filtFrame);
			return nullptr;
		}
		return filtFrame;
	}

	return nullptr;
}

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

void av_audio_to_rtmp(const char* in_Audio, int channels, int samplesize, int samplerate, const char* out_url_file, shared_ptr<CAVFormatInfo> av_format_info)
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
	AVFilterGraph* filter_graph = nullptr;
	AVFilterContext* buffer_sink_ctx = nullptr;
	AVFilterContext* buffer_src_ctx = nullptr;

	//设置解码器
	AVDictionary* ioptions = nullptr;
	av_dict_set_int(&ioptions, "audio_buffer_size", 20, 0);

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

	ret = avformat_find_stream_info(ictx, NULL);
	if (0 != ret)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	av_format_info->ictx   = ictx;
	av_format_info->is_run = true;

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

	AVCodec* decode_codec = avcodec_find_decoder(ictx->streams[audio_stream_index]->codecpar->codec_id);
	if (decode_codec == NULL)
	{
		cout << "[av_audio_to_rtmp]no find avcodec_find_decoder stream." << endl;
		av_free_context(ictx, octx);
		return;
	}

	AVCodecContext* decode_codectx = avcodec_alloc_context3(decode_codec);
	if (decode_codectx == nullptr)
	{
		ret = AVERROR(ENOMEM);
		return;
	}

	avcodec_parameters_to_context(decode_codectx, ictx->streams[audio_stream_index]->codecpar);

	//打开解码器
	if (0 > avcodec_open2(decode_codectx, decode_codec, NULL))
	{
		printf("failed open decoder\n");
		return;
	}

	av_dump_format(ictx, 0, in_Audio, 0);

	cout << "[av_audio_to_rtmp]fmt=" << av_get_sample_fmt_name((AVSampleFormat)ictx->streams[audio_stream_index]->codecpar->format) << endl;
	cout << "[av_audio_to_rtmp]bit_rate=" << ictx->streams[audio_stream_index]->codecpar->bit_rate << endl;
	cout << "[av_audio_to_rtmp]sample_rate=" << ictx->streams[audio_stream_index]->codecpar->sample_rate << endl;

	//设置编码器
	AVCodec*  encode_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
	if (encode_codec == nullptr)
	{
		ret = AVERROR(EINVAL);
		av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
		return;
	}
	AVCodecContext* encode_codectx = avcodec_alloc_context3(encode_codec);
	if (encode_codectx == nullptr)
	{
		ret = AVERROR(ENOMEM);
		return;
	}

	encode_codectx->codec = encode_codec;
	encode_codectx->sample_rate = 48000;
	encode_codectx->channel_layout = 3;
	encode_codectx->channels = 2;
	encode_codectx->sample_fmt = AVSampleFormat::AV_SAMPLE_FMT_FLTP;
	encode_codectx->codec_tag = 0;
	encode_codectx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

	//打开编码器
	ret = avcodec_open2(encode_codectx, encode_codec, nullptr);
	if (0 != ret)
	{
		ret = AVERROR(EINVAL);
		av_log(nullptr, AV_LOG_ERROR, "%s %d : %ld\n", __FILE__, __LINE__, ret);
		return;
	}

	//设置格式化输出流
	ret = avformat_alloc_output_context2(&octx, 0, 0, out_url_file);
	if (!octx)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	AVStream* output_stream = avformat_new_stream(octx, encode_codec);
	if (nullptr == output_stream)
	{
		av_error_string_output(__FUNCTION__, __LINE__, 0);
		av_free_context(ictx, octx);
		return;
	}

	ret = avcodec_parameters_from_context(output_stream->codecpar, encode_codectx);
	if (0 != ret)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	ret = avio_open2(&octx->pb, out_url_file, AVIO_FLAG_WRITE, nullptr, nullptr);
	if (0 != ret)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		av_free_context(ictx, octx);
		return;
	}

	av_dump_format(octx, 0, out_url_file, 1);

	//重采样初始化
	init_audio_sample(ictx, filter_graph, buffer_sink_ctx, buffer_src_ctx);

	ret = avformat_write_header(octx, NULL);
	if (0 > ret)
	{
		av_error_string_output(__FUNCTION__, __LINE__, ret);
		avio_close(octx->pb);
		av_free_context(ictx, octx);
		return;
	}

	//开始转换
	int loop = 1;
	int delayedFrame = 0;
	int audio_count = 0;
	AVPacket in_packet;
	av_init_packet(&in_packet);
	in_packet.data = NULL;
	in_packet.size = 0;
	AVPacket out_packet;
	av_init_packet(&out_packet);
	out_packet.data = NULL;
	out_packet.size = 0;

	AVFrame* pSrcAudioFrame = av_frame_alloc();
	int got_frame = 0;

	int out_packet_size = 0;

	while (1)
	{
		if (false == av_format_info->is_run)
		{
			cout << "[av_audio_to_rtmp]is end." << endl;
			break;
		}

		ret = av_read_frame(ictx, &in_packet);
		if (0 != ret)
		{
			break;
		}

		loop++;
		cout << "frameindex=" << loop << ", pk.size=" << in_packet.size << endl;

		if (0 >= in_packet.size)
		{
			continue;
		}

		AVFrame* filter_frame = decode_audio(&in_packet, pSrcAudioFrame, decode_codectx, buffer_sink_ctx, buffer_src_ctx);

		if (nullptr != filter_frame)
		{
			//avcodec_encode_audio2(encode_codectx, &out_packet, filter_frame, &got_frame);
			ret = avcodec_send_frame(encode_codectx, filter_frame);
			if (ret < 0)
			{
				av_log(NULL, AV_LOG_ERROR, "avcodec_send_frame error.\n");
				break;
			}

			ret = avcodec_receive_packet(encode_codectx, &out_packet);

			/*
			auto streamTimeBase = octx->streams[out_packet.stream_index]->time_base.den;
			auto codecTimeBase = octx->streams[out_packet.stream_index]->codecpar->time_base.den;
			out_packet.pts = out_packet.dts = (1024 * streamTimeBase * audio_count) / codecTimeBase;
			audio_count++;

			auto inputStream = ictx->streams[out_packet.stream_index];
			auto outputStream = octx->streams[out_packet.stream_index];
			av_packet_rescale_ts(&out_packet, inputStream->time_base, outputStream->time_base);
			*/

			out_packet.stream_index = output_stream->index;
			AVRational itime = ictx->streams[out_packet.stream_index]->time_base;
			AVRational otime = octx->streams[out_packet.stream_index]->time_base;

			out_packet.pts = av_rescale_q_rnd(in_packet.pts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			out_packet.dts = av_rescale_q_rnd(in_packet.dts, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			out_packet.duration = av_rescale_q_rnd(in_packet.duration, itime, otime, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			out_packet.pos = -1;

			out_packet_size += out_packet.size;

			av_interleaved_write_frame(octx, &out_packet);
			av_packet_unref(&out_packet);
		}

		av_packet_unref(&in_packet);
	}

	av_write_trailer(octx);
	avio_close(octx->pb);
	av_free_context(ictx, octx);
	cout << "[av_audio_to_rtmp]ffmpeg audi0 to rtmp ok(" << out_packet_size << ")" << endl;
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