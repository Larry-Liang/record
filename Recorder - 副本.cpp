
#include "stdafx.h"
#include <stdio.h>
#ifdef	__cplusplus
extern "C"
{
#endif
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavdevice/avdevice.h"
#include "libavutil/audio_fifo.h"
#include "libswresample/swresample.h"
#include "libavfilter/avfiltergraph.h" 
#include "libavfilter/buffersink.h"  
#include "libavfilter/buffersrc.h" 

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avfilter.lib")
#pragma comment(lib, "avfilter.lib")
#pragma comment(lib, "swscale.lib")
#ifdef __cplusplus
};
#endif
#include "Recorder.h"
#include "CaptureDevices.h"
#include "Utils.h"
#include "RateCtrl.h"
#include "VideoCapture.h"
#include "AudioCapture.h"

/*
struct MyVideoChan
{
	AVFormatContext	*pFmtCtx; 
	AVCodecContext	*pCodecCtx;
	AVCodec			*pCodec;
	SwsContext		*sws_ctx;
	int64_t			pts;
	AVFifoBuffer	*fifo_video;
	HANDLE          handle;
	int				index;
};

struct MyAudioChan
{
	AVFormatContext	*pFmtCtx; 
	AVCodecContext	*pCodecCtx;
	AVCodec			*pCodec;
	SwsContext		*sws_ctx;
	int64_t			pts;
	AVAudioFifo		*fifo_audio;
	HANDLE          handle;
};
*/
//MyVideoChan video;
//MyAudioChan audio;

AVFormatContext	*pFormatCtx_Video = NULL, *pFormatCtx_Video2 = NULL,*pFormatCtx_Audio = NULL, *pFormatCtx_Out = NULL;
AVCodecContext	*pCodecCtx_Video;
AVCodecContext	*pCodecCtx_Video2;
AVCodec			*pCodec_Video;
AVFifoBuffer	*fifo_video = NULL;
AVAudioFifo		*fifo_audio = NULL;



static Video_Callback g_video_callback = NULL;
int VideoIndex, AudioIndex;
int VideoFrameIndex=0,AudioFrameIndex=0;
int audioThreadQuit = 0;
int recordThreadQuit = 0;
int videoThreadQuit = 0;

static VideoInfo gOutVideoInfo;
int64_t cur_pts_v=0,cur_pts_a=0;

static bool bStartRecord = false;

CRITICAL_SECTION AudioSection, VideoSection;

static int FPS = 25;

SwsContext *yuv420p_convert_ctx= NULL; //��Դ��ʽת��ΪYUV420P��ʽ
SwsContext *rgb24_convert_ctx= NULL;   //��Դ��ʽת��ΪRGB24��ʽ
SwsContext *rgb24_convert_ctx2= NULL;   //��Դ��ʽת��ΪRGB24��ʽ
struct SwrContext *audio_swr_ctx = NULL;

int frame_size = 0;

int gYuv420FrameSize = 0;
uint8_t* pEnc_yuv420p_buf = NULL;
AVFrame *pEncFrame = NULL; //¼���̴߳Ӷ�����ȡ��һ��YUV420P�����ݺ���䵽pEncFrame�У������б���
AVFrame* pRecFrame = NULL; //��Ƶ�ɼ��߳��������ɼ�������Ƶԭʼ����ת����YUV420P��ʽ������֡������Ͷ�ݵ�����.
AVFrame* pAudioFrame = NULL;
static bool bCapture = false;


#include <string.h>
#include <stdlib.h>


DWORD WINAPI VideoCapThreadProc( LPVOID lpParam );
DWORD WINAPI AudioCapThreadProc( LPVOID lpParam );

int init_ffmpeg_env(HMODULE handle)
{
	
	string path = GetProgramDir(handle);
	string log_cfg = path+"\\record.ini";
	string log_out = path+"\\"+get_log_filename();
	av_register_all();
	avdevice_register_all();
	avfilter_register_all();

	init_report_file(log_cfg,log_out);
	return 0;
}




/*
¼���ʱ��Ż�ָ�������Ⱥ͸߶ȣ����ʣ���������,֡��
���б������Ĳ���(gop�ȣ�
��������ļ�����ز���.
��Ҫ�Ƿ�������ļ���FormatContext������2����������������ز������򿪶��ڵı�����.
*/
int OpenOutPut(const char* outFileName,VideoInfo* pVideoInfo, AudioInfo* pAudioInfo,SubTitleInfo* pSubTitle)
{
	AVStream *pVideoStream = NULL, *pAudioStream = NULL;
	if(pFormatCtx_Video == NULL || pFormatCtx_Audio == NULL)
	{
		av_log(NULL,AV_LOG_ERROR,"please opendevices first\r\n");
		return -1;
	}
	

	//Ϊ����ļ�����FormatContext
	avformat_alloc_output_context2(&pFormatCtx_Out, NULL, NULL, outFileName); //����������ú�pFormatCtx_Out->oformat�оͲ³�����Ŀ�������
	//������Ƶ����������Ƶ��������ʼ��.
	if (pFormatCtx_Video->streams[0]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
	{
		VideoIndex = 0;
		//Ϊ����ļ��½�һ����Ƶ��,�����ɹ���pFormatCtx_Out�е�streams��Ա���Ѿ����ΪpVideoStream��.
		pVideoStream = avformat_new_stream(pFormatCtx_Out, NULL);

		if (!pVideoStream)
		{
			av_log(NULL,AV_LOG_ERROR,"can not new video stream for output!\n");
			avformat_free_context(pFormatCtx_Out);
			return -2;
		}

		//set codec context param
#ifdef H264_ENC
		pVideoStream->codec->codec  = avcodec_find_encoder(AV_CODEC_ID_H264); //��Ƶ���ı�����ΪMPEG4
#else
		if(pFormatCtx_Out->oformat->video_codec == AV_CODEC_ID_H264)
		{
			pFormatCtx_Out->oformat->video_codec = AV_CODEC_ID_MPEG4;
		}
		pVideoStream->codec->codec =  avcodec_find_encoder(pFormatCtx_Out->oformat->video_codec);
		//pVideoStream->codec->codec  = avcodec_find_encoder(AV_CODEC_ID_FLV1); //��Ƶ���ı�����ΪMPEG4
#endif
		
		
		//open encoder
		if (!pVideoStream->codec->codec)
		{
			av_log(NULL,AV_LOG_ERROR,"can not find the encoder!\n");
			return -3;
		}

		pVideoStream->codec->height = pVideoInfo->height; //����ļ���Ƶ���ĸ߶�
		pVideoStream->codec->width  = pVideoInfo->width;  //����ļ���Ƶ���Ŀ��
		AVRational ar;
		ar.den = 15;
		ar.num = 1;
		pVideoStream->codec->time_base = pFormatCtx_Video->streams[0]->codec->time_base; //����ļ���Ƶ���ĸ߶Ⱥ������ļ���ʱ��һ��.
		pVideoStream->time_base = pFormatCtx_Video->streams[0]->codec->time_base;
		pVideoStream->codec->sample_aspect_ratio = pFormatCtx_Video->streams[0]->codec->sample_aspect_ratio;
		// take first format from list of supported formats
		//�ɲ鿴ff_mpeg4_encoder��ָ���ĵ�һ�����ظ�ʽ������ǲ���MPEG4�����ʱ��������Ƶ֡�����ظ�ʽ��������YUV420P
		pVideoStream->codec->pix_fmt = pFormatCtx_Out->streams[VideoIndex]->codec->codec->pix_fmts[0]; //���ظ�ʽ������MPEG4֧�ֵĵ�һ����ʽ
		
		//CBR_Set(pVideoStream->codec, pVideoInfo->bitrate); //���ù̶�����
		VBR_Set(pVideoStream->codec, pVideoInfo->bitrate, 2*pVideoInfo->bitrate  , 0);
		
		if (pFormatCtx_Out->oformat->flags & AVFMT_GLOBALHEADER)
			pVideoStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		AVDictionary *options = NULL;
		av_dict_set(&options, "bufsize", "5120k", NULL);//�������ָ���������Ĳ���
		//������ļ�����Ƶ������
#ifdef H264_ENC
		pVideoStream->codec->me_range = 16;
        pVideoStream->codec->max_qdiff = 4;
        pVideoStream->codec->qmin = 10;
        pVideoStream->codec->qmax = 51;
        pVideoStream->codec->qcompress = 0.6;
#endif
		//pVideoStream->rc_lookahead=0;//�����Ͳ����ӳٱ������������
		//av_opt_set(pVideoStream->codec->priv_data, "preset", "slow", 0);
		if ((avcodec_open2(pVideoStream->codec, pVideoStream->codec->codec, &options)) < 0)
		{
			av_log(NULL,AV_LOG_ERROR,"can not open the encoder\n");
			return -4;
		}
	}
	//������Ƶ����������Ƶ��������ʼ��.
	if(pFormatCtx_Audio->streams[0]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
	{
		AVCodecContext *pOutputCodecCtx = NULL;
		AudioIndex = 1;
		pAudioStream = avformat_new_stream(pFormatCtx_Out, NULL);
		//��avformat_alloc_output_context2 �о��ҵ��� pFormatCtx_Out->oformat�������ļ��ĺ�׺��ƥ�䵽��AVOutputFormat ff_mp4_muxer
		//    .audio_codec       = AV_CODEC_ID_AAC 
		
		//pAudioStream->codec->codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
		pAudioStream->codec->codec = avcodec_find_encoder(pFormatCtx_Out->oformat->audio_codec);

		pOutputCodecCtx = pAudioStream->codec;

		pOutputCodecCtx->sample_fmt = pOutputCodecCtx->codec->sample_fmts?pOutputCodecCtx->codec->sample_fmts[0]:AV_SAMPLE_FMT_FLTP; //����Ĳ����ʵ��ڲɼ��Ĳ�����
		pOutputCodecCtx->bit_rate = 64000;//pAudioInfo->bitrate;
		pOutputCodecCtx->sample_rate = 44100;
		if (pOutputCodecCtx->codec->supported_samplerates) {
            pOutputCodecCtx->sample_rate = pOutputCodecCtx->codec->supported_samplerates[0];
            for (int i = 0; pOutputCodecCtx->codec->supported_samplerates[i]; i++) {
                if (pOutputCodecCtx->codec->supported_samplerates[i] == 44100)
                    pOutputCodecCtx->sample_rate = 44100;
            }
        }

		//pOutputCodecCtx->channel_layout = pFormatCtx_Audio->streams[0]->codec->channel_layout;
		pOutputCodecCtx->channels = av_get_channel_layout_nb_channels(pOutputCodecCtx->channel_layout);//�����ͨ�����ڲɼ���ͨ����
		pOutputCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
		if (pOutputCodecCtx->codec->channel_layouts) {
            pOutputCodecCtx->channel_layout = pOutputCodecCtx->codec->channel_layouts[0];
            for (int i = 0; pOutputCodecCtx->codec->channel_layouts[i]; i++) {
                if (pOutputCodecCtx->codec->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                    pOutputCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
            }
        }
	
		pOutputCodecCtx->channels        = av_get_channel_layout_nb_channels(pOutputCodecCtx->channel_layout);
       
		AVRational time_base={1, pAudioStream->codec->sample_rate};
		pAudioStream->time_base = time_base; //����
		
		pOutputCodecCtx->codec_tag = 0;  
		//mpeg4    .flags             = AVFMT_GLOBALHEADER | AVFMT_ALLOW_FLUSH | AVFMT_TS_NEGATIVE,
		if (pFormatCtx_Out->oformat->flags & AVFMT_GLOBALHEADER)  
			pOutputCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
		//������ļ��ı�����.
		if (avcodec_open2(pOutputCodecCtx, pOutputCodecCtx->codec, 0) < 0)
		{
			//��������ʧ�ܣ��˳�����
			av_log(NULL,AV_LOG_ERROR,"can not open output codec!\n");
			return -5;
		}
		pAudioFrame = alloc_audio_frame(pAudioStream->codec->sample_fmt,pAudioStream->codec->channel_layout,pAudioStream->codec->sample_rate,pAudioStream->codec->frame_size);
		audio_swr_ctx = swr_alloc();
        if (!audio_swr_ctx) {
            av_log(NULL,AV_LOG_ERROR, "Could not allocate resampler context\n");
            return -9;
        }
		av_opt_set_int       (audio_swr_ctx, "in_channel_count",   pFormatCtx_Audio->streams[0]->codec->channels,       0);
        av_opt_set_int       (audio_swr_ctx, "in_sample_rate",     pFormatCtx_Audio->streams[0]->codec->sample_rate,    0);
		av_opt_set_sample_fmt(audio_swr_ctx, "in_sample_fmt",      pFormatCtx_Audio->streams[0]->codec->sample_fmt, 0);
        av_opt_set_int       (audio_swr_ctx, "out_channel_count",  pAudioStream->codec->channels,       0);
        av_opt_set_int       (audio_swr_ctx, "out_sample_rate",    pAudioStream->codec->sample_rate,    0);
        av_opt_set_sample_fmt(audio_swr_ctx, "out_sample_fmt",     pAudioStream->codec->sample_fmt,     0);

        /* initialize the resampling context */
        if ((swr_init(audio_swr_ctx)) < 0) {
            av_log(NULL,AV_LOG_ERROR, "Failed to initialize the resampling context\n");
            return -10;
        }
	}
	//���ļ�
	if (!(pFormatCtx_Out->oformat->flags & AVFMT_NOFILE))
	{
		if(avio_open(&pFormatCtx_Out->pb, outFileName, AVIO_FLAG_WRITE) < 0)
		{
			av_log(NULL,AV_LOG_ERROR,"can not open output file handle!\n");
			return -6;
		}
	}
	//д���ļ�ͷ
	if(avformat_write_header(pFormatCtx_Out, NULL) < 0)
	{
		av_log(NULL,AV_LOG_ERROR,"can not write the header of the output file!\n");
		return -7;
	}
	

	//pVideoStream->codec->time_base = pFormatCtx_Video->streams[0]->codec->time_base;
	//�������¼����Ƶ�ֱ��ʴ�С�ͱ��������ʽ����һ��AVFrame����������Ӧ����������
	pEncFrame  = av_frame_alloc();
	//����¼���һ֡��Ƶ�Ĵ�С
	gYuv420FrameSize = avpicture_get_size(pFormatCtx_Out->streams[VideoIndex]->codec->pix_fmt, 
		pFormatCtx_Out->streams[VideoIndex]->codec->width, pFormatCtx_Out->streams[VideoIndex]->codec->height);
	pEnc_yuv420p_buf = new uint8_t[gYuv420FrameSize];

	avpicture_fill((AVPicture *)pEncFrame, pEnc_yuv420p_buf, 
		pFormatCtx_Out->streams[VideoIndex]->codec->pix_fmt, 
		pFormatCtx_Out->streams[VideoIndex]->codec->width, 
		pFormatCtx_Out->streams[VideoIndex]->codec->height);

	yuv420p_convert_ctx = sws_getContext(pCodecCtx_Video->width, pCodecCtx_Video->height,  pCodecCtx_Video->pix_fmt, 
		pVideoInfo->width, pVideoInfo->height, AV_PIX_FMT_YUV420P, SWS_POINT, NULL, NULL, NULL); 
	//��ȡĿ��֡�Ĵ�С.
	frame_size = avpicture_get_size(pFormatCtx_Out->streams[VideoIndex]->codec->pix_fmt, pVideoInfo->width, pVideoInfo->height);
	//����30֡Ŀ��֡��С��video��fifo
	fifo_video = av_fifo_alloc(30 * frame_size);
	if(fifo_video == NULL)
	{
		av_log(NULL,AV_LOG_ERROR,"alloc pic fifo failed\r\n");
		return -8;
	}

	pRecFrame = alloc_picture(AV_PIX_FMT_YUV420P,pVideoInfo->width,pVideoInfo->height,16);

	return 0;
}

int _av_compare_ts(int64_t ts_a, AVRational tb_a, int64_t ts_b, AVRational tb_b)
{
    int64_t a = tb_a.num * (int64_t)tb_b.den;
    int64_t b = tb_b.num * (int64_t)tb_a.den;
	int64_t c = 0;
    if ((FFABS(ts_a)|a|FFABS(ts_b)|b) <= INT_MAX)
        return (ts_a*a > ts_b*b) - (ts_a*a < ts_b*b);
	c = av_rescale_rnd(ts_a, a, b, AV_ROUND_DOWN);
    if (c < ts_b)
        return -1;
	c = av_rescale_rnd(ts_b, b, a, AV_ROUND_DOWN);
    if (c < ts_a)
        return 1;
    return 0;
}


//#define ENABLE_YUVFILE 1
//#define RGB_DEBUG 1
//#define YUV_DEBUG 1
FILE *fp_yuv = NULL;

/*
���߳����豸���򿪺󣬾ͱ�������һֱ���豸���رպ��߳��˳���
��¼��ֹͣ�󣬸��߳̽����ɼ���Ƶ���ص������ǲ����������ݵ���Ƶ������.
*/
DWORD WINAPI VideoCapThreadProc( LPVOID lpParam )
{
	AVPacket packet;/* = (AVPacket *)av_malloc(sizeof(AVPacket))*/;
	int got_picture = 0;
	AVFrame	*pFrame; //��Ŵ�����ͷ�����õ���һ֡����.�������Ӧ�þ���YUYV422���߶ȺͿ����Ԥ���ĸ߶ȺͿ��.
#ifdef RGB_DEBUG
	FILE *output = NULL;
	output = fopen("tmp.rgb24", "wb+");
#endif

#ifdef YUV_DEBUG
	
	FILE* yuvout = fopen("tmp.yuv", "wb+");
#endif
#if ENABLE_YUVFILE
	fp_yuv = fopen("tmp.rgb24", "wb+");
#endif
	pFrame = av_frame_alloc();//����һ��֡�����ڽ���������Ƶ��.
	AVFrame *filt_frame = av_frame_alloc();
	AVFrame* pPreviewFrame = av_frame_alloc(); //����һ��֡�����ڴ��Ԥ����Ƶ����,Ŀ����RGB24

	int nRGB24size = avpicture_get_size(AV_PIX_FMT_RGB24, 
		pCodecCtx_Video->width, pCodecCtx_Video->height);
	uint8_t* video_cap_buf = new uint8_t[nRGB24size];

	avpicture_fill((AVPicture *)pPreviewFrame, video_cap_buf, 
		AV_PIX_FMT_RGB24, 
		pCodecCtx_Video->width, 
		pCodecCtx_Video->height);


	av_init_packet(&packet);
	
	while(bCapture)
	{
		packet.data = NULL;
		packet.size = 0;
		
		//������ͷ��ȡһ �����ݣ�������δ����
		if (av_read_frame(pFormatCtx_Video, &packet) < 0)
		{
			continue;
		}
		if(packet.stream_index == 0)
		{
			//������Ƶ�� 
			if (avcodec_decode_video2(pCodecCtx_Video, pFrame, &got_picture, &packet) < 0)
			{
				av_log(NULL,AV_LOG_ERROR,"Decode Error.\n");
				continue;
			}
			if (got_picture)
			{
				
#ifdef ENABLE_FILTER
				int ret = 0;
				if(bStartRecord && fifo_video)
				{
					//ֱ��ͨ��filter������ͷ��ԭʼ����ת����RGB24��ʽ���˲��������ľ���RGB24.
					ret = push_filter(pFrame,filt_frame);
				}
				
#endif

				//ת����RGB24

				if(g_video_callback)
				{
					//�ص���Ƶ����.
#ifdef ENABLE_FILTER
					if(bStartRecord && ret==0)
						g_video_callback(filt_frame->data[0], pCodecCtx_Video->width ,pCodecCtx_Video->height);
					else
					{
						sws_scale(rgb24_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, 
							pCodecCtx_Video->height, pPreviewFrame->data, pPreviewFrame->linesize);
						g_video_callback(pPreviewFrame->data[0], pCodecCtx_Video->width ,pCodecCtx_Video->height);
					}
#else	
				
					sws_scale(rgb24_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, 
							pCodecCtx_Video->height, pPreviewFrame->data, pPreviewFrame->linesize);
						g_video_callback(pPreviewFrame->data[0], pCodecCtx_Video->width ,pCodecCtx_Video->height);
					if( (pCodecCtx_Video->width!=640) || (pCodecCtx_Video->height!=480))
					{
						av_log(NULL,AV_LOG_INFO,"width=%d,height=%d\r\n",pCodecCtx_Video->width,pCodecCtx_Video->height);
					}
					
#endif
						
					
#ifdef RGB_DEBUG
				fwrite(pPreviewFrame->data[0],nRGB24size,1,output);    
#endif
				}
	
				//bStartRecord�����־��λ��fifo_video���ܻ�û�д����ɹ�.
				if(bStartRecord && fifo_video) //���������¼�񣬲�������Ƶ���ݵ�¼�����.
				{
					//ת����Ƶ֡Ϊ¼����Ƶ��ʽ YUYV422->YUV420P
					//picture is yuv420 data.
					int y_size = gOutVideoInfo.width*gOutVideoInfo.height;
					if (av_fifo_space(fifo_video) >= gYuv420FrameSize)
					{
#ifdef ENABLE_FILTER
						//������3��������Դ��Ƶ�ĸ߶ȣ���ԭ��������Ŀ����Ƶ�ĸ߶ȣ����£���Ƶ���°벿����.
						sws_scale(yuv420p_convert_ctx, (const uint8_t* const*)filt_frame->data, filt_frame->linesize, 0, 
							filt_frame->height, pRecFrame->data, pRecFrame->linesize);
#else
						sws_scale(yuv420p_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, 
							pFrame->height, pRecFrame->data, pRecFrame->linesize);
#endif

#ifdef YUV_DEBUG
				fwrite(pRecFrame->data[0],y_size,1,yuvout);  
				fwrite(pRecFrame->data[1],y_size/4,1,yuvout); 
				fwrite(pRecFrame->data[2],y_size/4,1,yuvout); 
#endif
						EnterCriticalSection(&VideoSection);	
					
						av_fifo_generic_write(fifo_video, pRecFrame->data[0], y_size, NULL);
						av_fifo_generic_write(fifo_video, pRecFrame->data[1], y_size/4, NULL);
						av_fifo_generic_write(fifo_video, pRecFrame->data[2], y_size/4, NULL);
						
						
						LeaveCriticalSection(&VideoSection);
						
					}
				}
#ifdef ENABLE_FILTER
				if(bStartRecord && ret == 0)
				{
					av_frame_unref(filt_frame);
				}
#endif
				
			}
		}
		//�ͷŲɼ�����һ������.
		av_free_packet(&packet);
		
	}
	if(pFrame)
	av_frame_free(&pFrame);
	if(pRecFrame)
	av_frame_free(&pRecFrame);
	if(pPreviewFrame)
	av_frame_free(&pPreviewFrame);
	if(filt_frame)
	av_frame_free(&pPreviewFrame);
	if(video_cap_buf)
	{
		delete []video_cap_buf;
	}
	videoThreadQuit = 1;
	
	av_log(NULL,AV_LOG_INFO,"video thread exit\r\n");
	return 0;
}

DWORD WINAPI AudioCapThreadProc( LPVOID lpParam )
{
	AVPacket pkt;
	AVFrame *frame;
	frame = av_frame_alloc();
	int gotframe;
	while(bCapture)// �˳���־
	{
		pkt.data = NULL;
		pkt.size = 0;
	
		//����Ƶ�豸�ж�ȡһ֡��Ƶ����
		if(av_read_frame(pFormatCtx_Audio,&pkt) < 0)
		{
			continue;
		}
		//�����ָ���ĸ�ʽ��pkt  fixme
		if (avcodec_decode_audio4(pFormatCtx_Audio->streams[0]->codec, frame, &gotframe, &pkt) < 0)
		{
			//����ʧ�ܺ��˳����̣߳�������Ҫ�޸�
			av_frame_free(&frame);
			av_log(NULL,AV_LOG_ERROR,"can not decoder a frame");
			break;
		}
		av_free_packet(&pkt);

		if (!gotframe)
		{
			continue;//û�л�ȡ�����ݣ�������һ��
		}
		//����audio��fifo.
		if (NULL == fifo_audio)
		{
			fifo_audio = av_audio_fifo_alloc(pFormatCtx_Audio->streams[0]->codec->sample_fmt, 
				pFormatCtx_Audio->streams[0]->codec->channels, 30 * frame->nb_samples);
		}
		if(bStartRecord)
		{
			int buf_space = av_audio_fifo_space(fifo_audio);
			if (av_audio_fifo_space(fifo_audio) >= frame->nb_samples)
			{

				av_log(NULL,AV_LOG_PANIC,"************write audio fifo\r\n");
			

				//��Ƶ������¼�����.
				EnterCriticalSection(&AudioSection);
				av_audio_fifo_write(fifo_audio, (void **)frame->data, frame->nb_samples);
				LeaveCriticalSection(&AudioSection);
				
				
			}
		}
		
		
		
	}
	av_frame_free(&frame);
	audioThreadQuit = 1;
	av_log(NULL,AV_LOG_ERROR,"video thread exit\r\n");

	return 0;
}
//¼���߳�.
DWORD WINAPI RecordThreadProc( LPVOID lpParam )
{
	
	bStartRecord = true;
	cur_pts_v = cur_pts_a = 0; //��λ����Ƶ��pts
	VideoFrameIndex = AudioFrameIndex = 0; //��λ����Ƶ��֡��.
	while(bStartRecord) //������¼���־���Ž���¼�񣬷����˳��߳�
	{
		//����Ƶ�̶߳��Ѿ������˶��к󣬾Ϳ�ʼȡ����
		if (fifo_audio && fifo_video)
		{
			int sizeAudio = av_audio_fifo_size(fifo_audio);
			int sizeVideo = av_fifo_size(fifo_video);
			//��������д��ͽ���ѭ��
			if (av_audio_fifo_size(fifo_audio) <= pFormatCtx_Out->streams[AudioIndex]->codec->frame_size && 
				av_fifo_size(fifo_video) <= frame_size && !bCapture)
			{
				break;
			}
		}
		
		if(_av_compare_ts(cur_pts_v, pFormatCtx_Out->streams[VideoIndex]->codec->time_base, 
			cur_pts_a,pFormatCtx_Out->streams[AudioIndex]->codec->time_base) <= 0)
		{
			//av_log(NULL,AV_LOG_PANIC,"************write video\r\n");
			//read data from fifo
			if (av_fifo_size(fifo_video) < frame_size && !bCapture)
			{
				//cur_pts_v = 0x7fffffffffffffff;
			}
			if(av_fifo_size(fifo_video) >= gYuv420FrameSize)
			{
				EnterCriticalSection(&VideoSection);
				av_fifo_generic_read(fifo_video, pEnc_yuv420p_buf, gYuv420FrameSize, NULL); //�Ӷ����ж�ȡһ֡YUV420P������֡,֡�Ĵ�СԤ�����.
				LeaveCriticalSection(&VideoSection);
				//����ȡ����������䵽AVFrame��.
				avpicture_fill((AVPicture *)pEncFrame, pEnc_yuv420p_buf, 
					pFormatCtx_Out->streams[VideoIndex]->codec->pix_fmt, 
					pFormatCtx_Out->streams[VideoIndex]->codec->width, 
					pFormatCtx_Out->streams[VideoIndex]->codec->height);
				
				//pts = n * (��1 / timbase��/ fps); ����pts,����֮ǰ����pts
				pEncFrame->pts = cur_pts_v++;// * ((pFormatCtx_Video->streams[0]->time_base.den / pFormatCtx_Video->streams[0]->time_base.num) / FPS);
			
				pEncFrame->format = pFormatCtx_Out->streams[VideoIndex]->codec->pix_fmt;
				pEncFrame->width = pFormatCtx_Out->streams[VideoIndex]->codec->width;
				pEncFrame->height=	pFormatCtx_Out->streams[VideoIndex]->codec->height;
				int got_picture = 0;
				AVPacket pkt;
				av_init_packet(&pkt);
				
				pkt.data = NULL;
				pkt.size = 0;
				//����һ֡��Ƶ.
				int ret = avcodec_encode_video2(pFormatCtx_Out->streams[VideoIndex]->codec, &pkt, pEncFrame, &got_picture);
				if(ret < 0)
				{
					//�������,������֡
					continue;
				}
				
				if (got_picture==1)
				{
 					pkt.stream_index = VideoIndex;
					//�������İ���Pts��dts��ת��������ļ���ָ����ʱ�� .�ڱ����Ϳ��Եó�����pts��dts.
				
					av_packet_rescale_ts(&pkt, pFormatCtx_Out->streams[VideoIndex]->codec->time_base, pFormatCtx_Out->streams[VideoIndex]->time_base);
				
					//д��һ��packet.
					ret = av_interleaved_write_frame(pFormatCtx_Out, &pkt);

					av_free_packet(&pkt);
				}
				
			}
		}
		else
		{
			
			if (NULL == fifo_audio)
			{
				continue;//��δ��ʼ��fifo
			}
			if (av_audio_fifo_size(fifo_audio) < pFormatCtx_Out->streams[AudioIndex]->codec->frame_size && !bCapture)
			{
				//cur_pts_a = 0x7fffffffffffffff;
			}
			if(av_audio_fifo_size(fifo_audio) >= 
				(pFormatCtx_Out->streams[AudioIndex]->codec->frame_size > 0 ? pFormatCtx_Out->streams[AudioIndex]->codec->frame_size : 1024))
			{

				AVFrame *frame;
				AVFrame *frame2;
				frame = alloc_audio_frame(pFormatCtx_Audio->streams[0]->codec->sample_fmt,\
					pFormatCtx_Out->streams[1]->codec->channel_layout,\
					pFormatCtx_Out->streams[1]->codec->sample_rate,\
					pFormatCtx_Out->streams[1]->codec->frame_size);
				frame2 = frame;
			

				EnterCriticalSection(&AudioSection);
				//����Ƶfifo�ж�ȡ��������Ҫ����������.
				av_audio_fifo_read(fifo_audio, (void **)frame->data, 
					pFormatCtx_Out->streams[1]->codec->frame_size);
				LeaveCriticalSection(&AudioSection);

				if (pFormatCtx_Out->streams[AudioIndex]->codec->sample_fmt != pFormatCtx_Audio->streams[0]->codec->sample_fmt 
					|| pFormatCtx_Out->streams[AudioIndex]->codec->channels != pFormatCtx_Audio->streams[0]->codec->channels 
					|| pFormatCtx_Out->streams[AudioIndex]->codec->sample_rate != pFormatCtx_Audio->streams[0]->codec->sample_rate)
				{
						int dst_nb_samples;
					//���������������Ƶ��ʽ��һ�� ��Ҫ�ز�����������һ���ľ�û��
					  dst_nb_samples = av_rescale_rnd(swr_get_delay(audio_swr_ctx, pFormatCtx_Out->streams[AudioIndex]->codec->sample_rate) + frame->nb_samples,
                                            pFormatCtx_Out->streams[AudioIndex]->codec->sample_rate, pFormatCtx_Out->streams[AudioIndex]->codec->sample_rate, AV_ROUND_UP);
					  //av_assert0(dst_nb_samples == frame->nb_samples);

					  int ret = swr_convert(audio_swr_ctx,
                              pAudioFrame->data, dst_nb_samples,
                              (const uint8_t **)frame->data, frame->nb_samples);
					  frame = pAudioFrame;
				}

				AVPacket pkt_out;
				av_init_packet(&pkt_out);
				int got_picture = -1;
				pkt_out.data = NULL;
				pkt_out.size = 0;

				frame->pts = cur_pts_a;
				cur_pts_a+=pFormatCtx_Out->streams[AudioIndex]->codec->frame_size;
				
				//cur_pts_a=AudioFrameIndex;
				if (avcodec_encode_audio2(pFormatCtx_Out->streams[AudioIndex]->codec, &pkt_out, frame, &got_picture) < 0)
				{
					av_log(NULL,AV_LOG_ERROR,"can not decoder a frame");
				}
				if(frame2)
					av_frame_free(&frame2);
				if (got_picture) 
				{
					pkt_out.stream_index = AudioIndex; //ǧ��Ҫ�ǵü���仰������ᵼ��û����Ƶ��.
					av_packet_rescale_ts(&pkt_out, pFormatCtx_Out->streams[AudioIndex]->codec->time_base, pFormatCtx_Out->streams[AudioIndex]->time_base);
				
					int ret = av_interleaved_write_frame(pFormatCtx_Out, &pkt_out);
					if(ret == 0)
					{
						//av_log(NULL,AV_LOG_PANIC,"write audio ok\r\n");
					}
					else
					{
						av_log(NULL,AV_LOG_PANIC,"write audio failed\r\n");
					}
					
					av_free_packet(&pkt_out);
				}
				else
				{
					//av_log(NULL,AV_LOG_PANIC,"xxxxxxxxxxxxxwrite audio file failed\r\n");
				}
				
			}
		}
	}
	if(pEnc_yuv420p_buf)
	delete[] pEnc_yuv420p_buf;

	if(pEncFrame)
	{
		av_frame_free(&pEncFrame);
	}
	av_fifo_free(fifo_video);
	fifo_video = 0;

	av_write_trailer(pFormatCtx_Out);

	avio_close(pFormatCtx_Out->pb);
	avformat_free_context(pFormatCtx_Out);

	
	recordThreadQuit = 1;
	av_log(NULL,AV_LOG_INFO,"app  exit\r\n");
	
	return 0;
}
int  CloseDevices()
{
	if(bCapture == false)
	{
		printf("�豸�Ѿ����ر��� ");
		return 0;
	}
	//��ֹͣ¼��
	CloudWalk_RecordStop();
	//��ֹͣ�ɼ��߳�
	bCapture = false;
	while(!audioThreadQuit || !videoThreadQuit)
	{
		printf("wait audio and video thread quit\r\n");
		Sleep(10);
	}
	if (pFormatCtx_Video != NULL)
	{
		avformat_close_input(&pFormatCtx_Video);
		pFormatCtx_Video = NULL;
	}
	if (pFormatCtx_Audio != NULL)
	{
		avformat_close_input(&pFormatCtx_Audio);
		pFormatCtx_Audio = NULL;
	}
	if(fifo_audio)
	{
		av_audio_fifo_free(fifo_audio);
		fifo_audio = NULL;
	}
	return 0;
}
int  SDK_CallMode CloudWalk_CloseDevices(void)
{
	return 	CloseDevices();
}
/*
ֹͣ¼��д��ʣ���¼������.
*/
int  SDK_CallMode CloudWalk_RecordStop (void)
{
	av_log(NULL,AV_LOG_DEBUG, "CloudWalk_RecordStop\r\n");
	//ֹͣ¼�񣬹�����Ƶ�ɼ��̣߳�Ȼ���ֹ��������Ƶ���ݵ�¼�����.
	if(false == bStartRecord)
	{
		return ERR_RECORD_OK;
	}
	bStartRecord = false;
	
	
	while(!recordThreadQuit)
	{
		av_log(NULL,AV_LOG_ERROR,"RecordStop wait quit\r\n");
		Sleep(10);
	}
#ifdef ENABLE_FILTER
	uninit_filter();
#endif
	return ERR_RECORD_OK;
}

/*
����¼��ָ���洢Ŀ¼��ָ����Ƶ���ӵ�����.
����ӿڿ��ܻᷴ�����ã����Ƿ������õ����
*/
CLOUDWALKFACESDK_API  int  SDK_CallMode CloudWalk_RecordStart (const char* filePath,VideoInfo* pVideoInfo, AudioInfo* pAudioInfo,SubTitleInfo* pSubTitle)
{
	av_log(NULL,AV_LOG_DEBUG, "CloudWalk_RecordStart\r\n");
	if(bStartRecord)
	{
		av_log(NULL,AV_LOG_ERROR,"record has been started already!\r\n");
		return 0;
	}
	if(!bCapture)
	{
		av_log(NULL,AV_LOG_ERROR,"not open devices!\r\n");
		return ERR_RECORD_NOT_OPEN_DEVS;
	}
	pVideoInfo->width  =  FFALIGN(pVideoInfo->width,  16);
	pVideoInfo->height =  FFALIGN(pVideoInfo->height, 16);
	gOutVideoInfo = *pVideoInfo;
	if (OpenOutPut(filePath,pVideoInfo,pAudioInfo,pSubTitle) < 0)
	{
		av_log(NULL,AV_LOG_ERROR,"open output file failed\r\n");
		return ERR_RECORD_OPEN_FILE;
	}
#ifdef ENABLE_FILTER
	char filter_descr[256] = {0,};
	char text_buf[256] = {0,};
	//"drawtext=fontfile=simfang.ttf:fontcolor=red:fontsize=32:shadowcolor=black:text='hello':x=10:y=10"
	GBKToUTF8_V2(pSubTitle->text,text_buf,256);
	_snprintf_s(filter_descr,256,"drawtext=fontfile=%s:fontcolor=%s:fontsize=%d:shadowcolor=black:text='%s':x=%d:y=%d",pSubTitle->fontname,pSubTitle->fontcolor,pSubTitle->fontsize,text_buf,pSubTitle->x,pSubTitle->y);
	if(init_filters(filter_descr) < 0)
	{
		av_log(NULL,AV_LOG_ERROR,"init_filters failed\r\n");
		return ERR_RECORD_OPEN_FILTER;
	}
#endif
	//֪ͨ����Ƶ�ɼ��߳̿�ʼ��������Ƶ���ݵ�¼�����.
	
	CreateThread( NULL, 0, RecordThreadProc, 0, 0, NULL);
	
	//����������ļ��Ĳ���������¼��
	return ERR_RECORD_OK;
}


int  SDK_CallMode   CloudWalk_OpenDevices(
													const char* pVideoDevice,
													const char* pVideoDevice2,
													const char* pAudioDevice,
													const unsigned  int width,
													const unsigned  int height,
													const unsigned  int FrameRate,
													int sampleRateInHz,
													int channelConfig,
													Video_Callback video_callback)
{

	
	

	audioThreadQuit = 0;
	videoThreadQuit = 0;
	recordThreadQuit= 0;
	FPS = FrameRate;
	av_log(NULL,AV_LOG_ERROR,"CloudWalk_OpenDevices vidoe=%s,audio=%s width=%d height=%d framerate=%d\r\n",\
			pVideoDevice,pAudioDevice,width,height,FrameRate);
	AVInputFormat *pDShowInputFmt = av_find_input_format("dshow");
	if(pDShowInputFmt == NULL)
	{
		av_log(NULL,AV_LOG_ERROR,"open dshow failed\r\n");
		return ERR_RECORD_DSHOW_OPEN;
	}
	
	if (OpenVideoCapture(&pFormatCtx_Video, &pCodecCtx_Video,getDevicePath("video",pVideoDevice).c_str() ,pDShowInputFmt,width,height,FrameRate) < 0)
	{
		av_log(NULL,AV_LOG_ERROR,"open video failed\r\n");
		return ERR_RECORD_VIDEO_OPEN;
	}
	
	if (OpenVideoCapture(&pFormatCtx_Video2,&pCodecCtx_Video2,getDevicePath("video",pVideoDevice2).c_str() ,pDShowInputFmt,width,height,FrameRate) < 0)
	{
		av_log(NULL,AV_LOG_ERROR,"open video failed\r\n");
		return ERR_RECORD_VIDEO_OPEN;
	}
	
	if (OpenAudioCapture(&pFormatCtx_Audio, getDevicePath("audio",pAudioDevice).c_str(),pDShowInputFmt) < 0)
	{
		av_log(NULL,AV_LOG_ERROR,"open audio failed\r\n");
		return ERR_RECORD_AUDIO_OPEN;
	}

	//�źų�ʼ��Ϊ���źţ�Ȼ���źŲ�������Ҫ�ֹ���λ�������ź�һֱ��Ч.
	
	InitializeCriticalSection(&VideoSection);
	InitializeCriticalSection(&AudioSection);

	//������Ҫת����Ŀ���ʽΪRGB24, �ߴ����Ԥ��ͼ��Ĵ�С.
	rgb24_convert_ctx = sws_getContext(pCodecCtx_Video->width, pCodecCtx_Video->height, pCodecCtx_Video->pix_fmt, 
		pCodecCtx_Video->width, pCodecCtx_Video->height, AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL); 
	
	rgb24_convert_ctx2 = sws_getContext(pCodecCtx_Video2->width, pCodecCtx_Video2->height, pCodecCtx_Video2->pix_fmt, 
		pCodecCtx_Video2->width, pCodecCtx_Video2->height, AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL); 

	//start cap screen thread
	CreateThread( NULL, 0, VideoCapThreadProc, 0, 0, NULL);
	//start cap audio thread
	CreateThread( NULL, 0, AudioCapThreadProc, 0, 0, NULL);
	
	
	bCapture = true;
	g_video_callback = video_callback; 
	return 0;

}

//������dllж�ص�ʱ����ô˺���������ᵼ�³����޷��˳�������ͷû���ٴ�����.
void FreeAllRes()
{
	//ֱ��ж��dll������û�е��ùرղɼ��̣߳�������Ϊ�߳��������Ѿ���ǿ��ɱ���ˣ�����û��ִ�е�audioThreadQuit=1
	audioThreadQuit=videoThreadQuit=1;
	CloseDevices();
}
#define MAX_DEVICES_NUM 10
#define MAX_DEVICES_NAME_SIZE 128
static char pStrDevices[MAX_DEVICES_NUM][MAX_DEVICES_NAME_SIZE]={{}};
static char* pStrings[MAX_DEVICES_NUM];

bool resetDevciesString(int num)
{
	for(int i = 0; i < num; i++)
	{
		memset(pStrDevices[i],0 , sizeof(MAX_DEVICES_NAME_SIZE));
	} 
	return true;
}
/*
�г����������е�����Ƶ�豸
*/
char** SDK_CallMode CloudWalk_ListDevices(int  devType, int* devCount)
{
	CaptureDevices *capDev = new CaptureDevices();
	vector<wstring> videoDevices, audioDevices;

	//�����ȡ�����ַ���������GBK����Ŀ��ַ�.
	capDev->GetVideoDevices(&videoDevices);
	capDev->GetAudioDevices(&audioDevices);

	delete capDev;
	
	for(int i = 0;  i < MAX_DEVICES_NUM;i++)
	{
		pStrings[i] = &pStrDevices[i][0];
	}
	resetDevciesString(MAX_DEVICES_NUM);
	if(devType == 0)
	{
		for(int i = 0; i < videoDevices.size(); i++)
		{
			std::string str = ws2s(videoDevices[i]);
			strcpy(pStrDevices[i],str.c_str());
			av_log(NULL,AV_LOG_INFO,"video[%d]=%s\r\n",i+1,str.c_str());
		}
		*devCount =  videoDevices.size();
	}
	else
	{
		for(int i = 0; i < audioDevices.size(); i++)
		{
			std::string str = ws2s(audioDevices[i]);
			strcpy(pStrDevices[i],str.c_str());
			av_log(NULL,AV_LOG_INFO,"audio[%d]=%s\r\n",i+1,str.c_str());
		}
		*devCount =  audioDevices.size();
	}
	return pStrings;

}

extern int muxing_func(int argc, char **argv);
int  SDK_CallMode CloudWalk_Muxing(int argc, char **argv)
{
	return muxing_func(argc, argv);
}