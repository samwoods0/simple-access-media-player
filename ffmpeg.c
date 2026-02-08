#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define AUDIO_BUFFER_SIZE 192000
double pause_start_time = 0.0;
double paused_duration = 0.0;
double get_audio_clock(double audio_start_time, double audio_clock_base, int audio_playing)
{
 if(!audio_playing) return audio_clock_base;
 double now = av_gettime() / 1000000.0;
 return audio_clock_base + (now - audio_start_time);
}

void display_controls()
{
	//display progress bar
	//	width = codec_ctx->width-10px?
	//	
	//display play/pause button
	//display volume control - slider, precise
	//display subtitle options
}

int main()
{
	
	const char *url = "media_files/The Land Before Time.mp4";
	AVFormatContext *format_ctx  = NULL;
	
	AVCodecContext *codec_ctx = NULL;	
	const AVCodec *codec = NULL;
	
	AVCodecContext *audio_codec_ctx = NULL;
	const AVCodec *audio_codec = NULL;

	avformat_open_input(&format_ctx, url, NULL, NULL);
	 	
	int video_stream = 0;
	int audio_stream = 0;

	for(int i = 0; i < format_ctx->nb_streams; i++)
	{
		if(format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			video_stream = i;
		}
		else if(format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			audio_stream = i;
		}

		//printf("Stream Type: %s\n", av_get_media_type_string(format_ctx->streams[i]->codecpar->codec_type));
	}
	const uint64_t media_duration = format_ctx->streams[video_stream]->duration;
	double  media_dur_minutes = ((double) media_duration * av_q2d(format_ctx->streams[video_stream]->time_base))/60.0;
	double  media_dur_seconds =((double) ((int)(media_dur_minutes * 100.0) % 100)/100) * 60;
	
	printf("Duration: Min:%d Sec:%d\n",(int)media_dur_minutes,(int) media_dur_seconds);

	codec = avcodec_find_decoder(format_ctx->streams[video_stream]->codecpar->codec_id);
	codec_ctx = avcodec_alloc_context3(codec);
	avcodec_parameters_to_context(codec_ctx, format_ctx->streams[video_stream]->codecpar);
	avcodec_open2(codec_ctx, codec, NULL);

	audio_codec = avcodec_find_decoder(format_ctx->streams[audio_stream]->codecpar->codec_id);
	audio_codec_ctx = avcodec_alloc_context3(audio_codec);
	avcodec_parameters_to_context(audio_codec_ctx, format_ctx->streams[audio_stream]->codecpar);
	avcodec_open2(audio_codec_ctx, audio_codec, NULL);

	printf("Width: %d, Height: %d\n", codec_ctx->width, codec_ctx->height);
	
	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);
	SDL_Window *window = SDL_CreateWindow("Media Player",SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, codec_ctx->width, codec_ctx->height,SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE );
	SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

	SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV,SDL_TEXTUREACCESS_STREAMING, codec_ctx->width, codec_ctx->height);
	
	SDL_AudioSpec wanted, obtained;
	wanted.freq = 44100;
	wanted.format = AUDIO_S16SYS;
	wanted.channels = 2;
	wanted.samples = 1024;
	wanted.callback = NULL;
	wanted.userdata = NULL;

	SDL_AudioDeviceID audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
	SDL_PauseAudioDevice(audio_dev, 0);

	AVChannelLayout in_layout, out_layout;
	av_channel_layout_copy(&in_layout, &audio_codec_ctx->ch_layout);
	av_channel_layout_default(&out_layout, 2); //stereo
	
	SwrContext *swr = NULL;

	int swr_ctx_ret = swr_alloc_set_opts2(&swr, &out_layout, AV_SAMPLE_FMT_S16, obtained.freq, &in_layout, audio_codec_ctx->sample_fmt, audio_codec_ctx->sample_rate, 0, NULL);
	
	swr_init(swr);
	uint8_t *audio_buff = (uint8_t *)av_malloc(AUDIO_BUFFER_SIZE);

	AVFrame *frame = av_frame_alloc();
	AVFrame *audio_frame = av_frame_alloc();
	AVPacket packet;

	SDL_Event event;
	int quit = 0;
	int paused = 0;
	int fullscreen = 0;
	
	double audio_start_time = 0.0;
	double audio_clock_base = 0.0;
	int audio_playing = 0;

	while(!quit)
	{	
		while(SDL_PollEvent(&event))
		{
			if(event.type == SDL_QUIT) quit = 1;
			if(event.type == SDL_KEYDOWN)
			{
				if(event.key.keysym.sym == SDLK_SPACE)
				{
					paused = !paused;
					if(paused)
					{
						pause_start_time = av_gettime() / 1000000.0;
						SDL_PauseAudioDevice(audio_dev, 1);
					}
					else
					{
						double now = av_gettime() / 1000000.0;
						paused_duration += (now - pause_start_time);
						audio_start_time += (now - pause_start_time);
						SDL_PauseAudioDevice(audio_dev, 0);
					}
				}
			}
			else if(event.key.keysym.sym == SDLK_f)
			{
				fullscreen = !fullscreen;
				SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
			}
		}	

		if(paused)
		{
			SDL_Delay(10);
			continue;
		}

		av_read_frame(format_ctx, &packet);
		
		if(packet.stream_index == video_stream)
		{	
			
			avcodec_send_packet(codec_ctx, &packet);

			while (avcodec_receive_frame(codec_ctx, frame) == 0)
			{			
				double pts = frame->best_effort_timestamp * av_q2d(format_ctx->streams[video_stream]->time_base);

				double audio_clock = get_audio_clock(audio_start_time, audio_clock_base, audio_playing);

				double delay = pts - audio_clock;
				if(delay > 0)
				{
					SDL_Delay((int)(delay*1000));
				}	
				SDL_UpdateYUVTexture(texture, NULL, frame->data[0], frame->linesize[0], frame->data[1], frame->linesize[1], frame->data[2], frame->linesize[2]);
				SDL_RenderClear(renderer);
				SDL_RenderCopy(renderer, texture, NULL, NULL);
				SDL_RenderPresent(renderer);
			}			
		}
		if(packet.stream_index == audio_stream)
		{
			avcodec_send_packet(audio_codec_ctx, &packet);
	
			while(avcodec_receive_frame(audio_codec_ctx, audio_frame) == 0)
			{
				int out_samples = swr_convert(swr, &audio_buff, AUDIO_BUFFER_SIZE / 4, (const uint8_t **)audio_frame->data, audio_frame->nb_samples);
				int out_size = out_samples * 2 * 2;

				SDL_QueueAudio(audio_dev, audio_buff, out_size);			
				if(!audio_playing && SDL_GetAudioDeviceStatus(audio_dev) == SDL_AUDIO_PLAYING)
				{
					audio_start_time = av_gettime() / 1000000.0;
					audio_clock_base = 0.0;
					audio_playing = 1;
				}	
				
			}
		}
	
		av_packet_unref(&packet);
	}

	av_free(audio_buff);
	swr_free(&swr);
	avcodec_free_context(&codec_ctx);
	avcodec_free_context(&audio_codec_ctx);
	avformat_close_input(&format_ctx);
	av_frame_free(&frame);
	av_frame_free(&audio_frame);

	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
