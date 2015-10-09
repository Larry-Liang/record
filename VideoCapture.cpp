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
#include "VideoCapture.h"
#include "Utils.h"

SwsContext *rgb24_convert_ctx= NULL;   //��Դ��ʽת��ΪRGB24��ʽ

int VideoCap::OpenVideoCapture(AVFormatContext** pFmtCtx, AVCodecContext	** pCodecCtx,const char* psDevName,AVInputFormat *ifmt,const unsigned  int width,
													const unsigned  int height,
													const unsigned  int FrameRate)
{
	int fps = 0;
	int idx = 0;
	AVCodec* pCodec = NULL;
	dshow_dump_params(pFmtCtx,psDevName,ifmt);
	dshow_dump_devices(pFmtCtx,psDevName,ifmt);
	fps = dshow_try_open_devices(pFmtCtx, psDevName, ifmt,width, height, FrameRate);
	if(fps == 0) 
	{
		return -1;
	}
	if(avformat_find_stream_info(*pFmtCtx,NULL)<0)
	{
		av_log(NULL,AV_LOG_ERROR,"Couldn't find stream information.���޷���ȡ��Ƶ����Ϣ��\n");
		return -2;
	}
	VideoIndex = -1;
	for(int i = 0; i < (*pFmtCtx)->nb_streams; i++)
	{
		if ((*pFmtCtx)->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			VideoIndex = i;
			break;
		}
	}
	
	if(VideoIndex == -1)
	{
		av_log(NULL,AV_LOG_ERROR,"Couldn't find video stream information.���޷���ȡ��Ƶ����Ϣ��\n");
		return -3;
	}
	*pCodecCtx = (*pFmtCtx)->streams[VideoIndex]->codec;
	//�����pCodecCtx_Video->codec_id��ʲôʱ���ʼ����? ��dshow.c�е�dshow_add_device�������� codec->codec_id = AV_CODEC_ID_RAWVIDEO;

	pCodec = avcodec_find_decoder((*pCodecCtx)->codec_id);
	if(pCodec == NULL)
	{
		av_log(NULL,AV_LOG_ERROR,"Codec not found.��û���ҵ���������\n");
		return -4;
	}
	if(avcodec_open2((*pCodecCtx), pCodec, NULL) < 0)
	{
		av_log(NULL,AV_LOG_ERROR,"Could not open codec.���޷��򿪽�������\n");
		return -5;
	}
	if( ((*pCodecCtx)->width != 640) || ((*pCodecCtx)->height != 480))
	{
		av_log(NULL,AV_LOG_ERROR,"width=%d,height=%d fmt=%d\r\n",(*pCodecCtx)->width,(*pCodecCtx)->height,(*pCodecCtx)->pix_fmt);
	}
	
	return 0;
}

bool VideoCap::SetCallBackAttr(int width, int height, AVPixelFormat format,Video_Callback pCbFunc)
{
	if(sws_ctx!=NULL)
	{
		sws_freeContext(sws_ctx);
		sws_ctx = NULL;
	}
	//������Ҫת����Ŀ���ʽΪBGR24, �ߴ����Ԥ��ͼ��Ĵ�С.
	sws_ctx = sws_getContext(pCodecContext->width, pCodecContext->height, pCodecContext->pix_fmt, 
		width, height, format, SWS_BICUBIC, NULL, NULL, NULL);

	return (sws_ctx!=NULL);
}
DWORD WINAPI VideoCapThreadProc( LPVOID lpParam )
{
	if(lpParam != NULL)
	{
		VideoCap* vc = (VideoCap*)lpParam;
		vc->Run();
	}
	return 0;
}
/*
���߳����豸���򿪺󣬾ͱ�������һֱ���豸���رպ��߳��˳���
��¼��ֹͣ�󣬸��߳̽����ɼ���Ƶ���ص������ǲ����������ݵ���Ƶ������.
*/
void VideoCap::Run( )
{
	AVPacket packet;
	int got_picture = 0;
	AVFrame	*pFrame; //��Ŵ�����ͷ�����õ���һ֡����.�������Ӧ�þ���YUYV422���߶ȺͿ����Ԥ���ĸ߶ȺͿ��.

	pFrame = av_frame_alloc();//����һ��֡�����ڽ���������Ƶ��.
	
	AVFrame* pPreviewFrame = av_frame_alloc(); //����һ��֡�����ڴ��Ԥ����Ƶ����,Ŀ����RGB24

	int nRGB24size = avpicture_get_size(AV_PIX_FMT_RGB24, 
		pCodecContext->width, pCodecContext->height);

	uint8_t* video_cap_buf = new uint8_t[nRGB24size];

	avpicture_fill((AVPicture *)pPreviewFrame, video_cap_buf, 
		AV_PIX_FMT_RGB24, 
		pCodecContext->width, 
		pCodecContext->height);


	av_init_packet(&packet);
	
	while(bCapture)
	{
		packet.data = NULL;
		packet.size = 0;
		
		//������ͷ��ȡһ �����ݣ�������δ����
		if (av_read_frame(pFormatContext, &packet) < 0)
		{
			continue;
		}
		if(packet.stream_index == 0)
		{
			//������Ƶ�� 
			if (avcodec_decode_video2(pCodecContext, pFrame, &got_picture, &packet) < 0)
			{
				av_log(NULL,AV_LOG_ERROR,"Decode Error.\n");
				continue;
			}
			if (got_picture)
			{
				
				//ת����RGB24

				if(pCallback)
				{
					//�ص���Ƶ����.

					sws_scale(rgb24_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, 
							pCodecContext->height, pPreviewFrame->data, pPreviewFrame->linesize);
					pCallback(pPreviewFrame->data[0], pCodecContext->width ,pCodecContext->height);
					if( (pCodecContext->width!=640) || (pCodecContext->height!=480))
					{
						av_log(NULL,AV_LOG_INFO,"width=%d,height=%d\r\n",pCodecContext->width,pCodecContext->height);
					}

				}
	
				

				
			}
		}
		//�ͷŲɼ�����һ������.
		av_free_packet(&packet);
		
	}
	if(pFrame)
	av_frame_free(&pFrame);

	if(pPreviewFrame)
	av_frame_free(&pPreviewFrame);
	
	if(video_cap_buf)
	{
		delete []video_cap_buf;
	}
	bQuit = 1;
	
	av_log(NULL,AV_LOG_INFO,"video thread exit\r\n");
	
}

int VideoCap::OpenPreview(const char* psDevName,AVInputFormat *ifmt,const unsigned  int width,
													const unsigned  int height,
													const unsigned  int FrameRate,AVPixelFormat format, Video_Callback pCbFunc)
{
	int ret = 0;
	bQuit	 = false;
	bCapture = true;
	ret = OpenVideoCapture(&pFormatContext,&pCodecContext,psDevName,ifmt,width,height,FrameRate);
	SetCallBackAttr(width,height,format,pCbFunc);
	CreateThread( NULL, 0, VideoCapThreadProc, this, 0, NULL);

	return ret;
}

VideoCap::VideoCap()
{
	pFormatContext = NULL;
	pCodecContext = NULL;
	bCapture = false;
	VideoIndex = 0;
	bQuit = false;
	pCallback = NULL;

	sws_ctx = NULL;
}
int VideoCap::Close()
{
	bCapture = 1;
	return 0;
}
AVRational VideoCap::GetVideoTimeBase()
{
	return pFormatContext->streams[VideoIndex]->codec->time_base;
}
AVRational VideoCap::Get_aspect_ratio()
{
	return pFormatContext->streams[VideoIndex]->codec->sample_aspect_ratio;
}
int VideoCap::GetWidth()
{
	return pCodecContext->width;
}
int VideoCap::GetHeight()
{
	return pCodecContext->height;
}
AVPixelFormat VideoCap::GetFormat()
{
	return pCodecContext->pix_fmt;
}