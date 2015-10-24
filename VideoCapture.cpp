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
#include "libavutil/pixdesc.h"


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
#include "CaptureDevices.h"
#include "Utils.h"


int VideoCap::OpenVideoCapture(AVFormatContext** pFmtCtx, AVCodecContext	** pCodecCtx,const char* psDevName,int index,AVInputFormat *ifmt,const unsigned  int width,
													const unsigned  int height,
													unsigned  int &FrameRate,const char* fmt)
{
	//int fps = 0;
	int idx = 0;

	AVCodec* pCodec = NULL;
	std::string dshow_path = getDevicePath("video",psDevName);
	//dshow_dump_params(pFmtCtx, dshow_path.c_str(),ifmt);
	//dshow_dump_devices(pFmtCtx,dshow_path.c_str(),ifmt);
	CaptureDevices cap_devs;
	cap_devs.ListCapablities(psDevName,0);
	My_Info info;
	info.pixel_format = AV_PIX_FMT_YUYV422;
	info.fps = FrameRate;
	info.height = height;
	info.width  = width;

	fps = FrameRate;
	if(!cap_devs.GetBestCap(psDevName,AV_PIX_FMT_NONE,width,height,FrameRate,info))
	{
		//return -6;
	}
	
	fps = dshow_try_open_devices(pFmtCtx, dshow_path.c_str(),index, ifmt,info.pixel_format,info.width, info.height, info.fps);
	
	
	if(fps == 0) 
	{
		return -1;
	}
	AVDictionary *options = NULL;
	//av_dict_set_int(&options,"analyzeduration",10000000,0);
	//av_dict_set_int(&options,"probesize",1000000,0);
	//analyzeduration
#if 1
	if(avformat_find_stream_info(*pFmtCtx,NULL)<0)
	{
		av_log(NULL,AV_LOG_ERROR,"Couldn't find stream information.���޷���ȡ��Ƶ����Ϣ��\n");
		return -2;
	}
#endif
	VideoIndex = find_stream_index(*pFmtCtx,AVMEDIA_TYPE_VIDEO);
		
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
	FrameRate = fps;
	return 0;
}

bool VideoCap::SetCallBackAttr(int width, int height, AVPixelFormat format,Video_Callback pCbFunc)
{
	if(sws_ctx!=NULL)
	{
		sws_freeContext(sws_ctx);
		sws_ctx = NULL;
	}
	if(pCodecContext->pix_fmt==-1)
	{
		pCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
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
AVFrame* VideoCap::GrabFrame()
{
	AVPacket packet;
	static AVFrame	*pFrame = NULL;
	int got_picture = 0;
	if(pFrame == NULL)
	{
		pFrame = av_frame_alloc();
	}
	av_init_packet(&packet);
	if (av_read_frame(pFormatContext, &packet) < 0)
	{
		return NULL;
	}
	if(packet.stream_index == VideoIndex)
	{
			//������Ƶ�� 
		if (avcodec_decode_video2(pCodecContext, pFrame, &got_picture, &packet) < 0)
		{
			av_log(NULL,AV_LOG_ERROR,"Camera[%d] Decode Error.\n",channel);
			return NULL;
		}
		if (got_picture)
		{				
			sws_scale(record_sws_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, 
							pFrame->height, pRecFrame->data, pRecFrame->linesize);
			DWORD tick = GetTickCount();
			pRecFrame->pts = tick;
			return pRecFrame;
		}
	}
	return NULL;
}
/*
���߳����豸���򿪺󣬾ͱ�������һֱ���豸���رպ��߳��˳���
��¼��ֹͣ�󣬸��߳̽����ɼ���Ƶ���ص������ǲ����������ݵ���Ƶ������.
*/
void VideoCap::Run( )
{
	AVPacket packet;
	int got_picture = 0;
	int discard = 15;
	bCapture = true;
	int ret = 0;
	total_fps = 0;
	int64_t old_pts = 0;
	DWORD old_time  = GetTickCount();
	DWORD start_time = old_time;
	AVFrame	*pFrame; //��Ŵ�����ͷ�����õ���һ֡����.�������Ӧ�þ���YUYV422���߶ȺͿ����Ԥ���ĸ߶ȺͿ��.
	evt_ready.set();
	pFrame = av_frame_alloc();//����һ��֡�����ڽ���������Ƶ��.
	pts = 0;
	test_fps = 0;
	lost_fps = 0;
	//����һ��Frame�������RGB24��ʽ��Ԥ����Ƶ.
	AVFrame* pPreviewFrame = alloc_picture(AV_PIX_FMT_BGR24,pCodecContext->width, pCodecContext->height,16);

	av_init_packet(&packet);
	av_log(NULL,AV_LOG_DEBUG,"camera[%d] start capture thread\r\n ",channel);
	while(bCapture)
	{
		packet.data = NULL;
		packet.size = 0;
		
		ret = av_read_frame(pFormatContext, &packet);
		//������ͷ��ȡһ �����ݣ�������δ����
		
		if (ret < 0) {
			//av_usleep(10000);
			continue;
		}
		total_fps++;
#if 0
		if( ( packet.pts - old_pts ) < 1000/fps)
		{
			lost_fps++;
			av_free_packet(&packet);
			continue;
		}
#else
		int wait = (old_time+1000/fps) - GetTickCount();
		total_fps++;
		if(wait > 0)
		{
			lost_fps++;
			//if(channel == 0)
			//av_free_packet(&packet);
			//continue;
		}
#endif
		test_fps++;
		old_time = GetTickCount();
		if(packet.stream_index == VideoIndex)
		{
			//������Ƶ�� 
			if (avcodec_decode_video2(pCodecContext, pFrame, &got_picture, &packet) < 0)
			{
				av_log(NULL,AV_LOG_ERROR,"Camera[%d] Decode Error.\n",channel);
				continue;
			}
			if (got_picture)
			{
				
				//ת����RGB24

				if(pCallback)
				{
					//�ص���Ƶ����.

					sws_scale(sws_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, 
							pCodecContext->height, pPreviewFrame->data, pPreviewFrame->linesize);
					pCallback(channel,pPreviewFrame->data[0], pCodecContext->width ,pCodecContext->height);
					if( (pCodecContext->width!=640) || (pCodecContext->height!=480))
					{
						av_log(NULL,AV_LOG_INFO,"width=%d,height=%d\r\n",pCodecContext->width,pCodecContext->height);
					}

				}
				
				if(bStartRecord && fifo_video) //���������¼�񣬲�������Ƶ���ݵ�¼�����.
				{
					//ת����Ƶ֡Ϊ¼����Ƶ��ʽ YUYV422->YUV420P
					//picture is yuv420 data.
					pts++;

					//av_log(NULL,AV_LOG_ERROR,"pts%d=%d\r\n",channel,pts);
					sws_scale(record_sws_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, 
							pFrame->height, pRecFrame->data, pRecFrame->linesize);
#if 1
					if (av_fifo_space(fifo_video) >= nSampleSize)
					{
						//lost = 0;
						EnterCriticalSection(&section);	
						DWORD tick = GetTickCount();
						av_fifo_generic_write(fifo_video, &tick, sizeof(tick), NULL);
						av_fifo_generic_write(fifo_video, pRecFrame->data[0], y_size , NULL);
						av_fifo_generic_write(fifo_video, pRecFrame->data[1], y_size/4, NULL);
						av_fifo_generic_write(fifo_video, pRecFrame->data[2], y_size/4, NULL);
						
						LeaveCriticalSection(&section);
	
					}
					else
					{
						
						av_log(NULL,AV_LOG_ERROR,"%d lost%d\r\n",channel,lost++);
					}
#else
					EnterCriticalSection(&section);	
					av_frame_copy(pRecFrame2,pRecFrame);
					LeaveCriticalSection(&section);
#endif
				}
				
			}
		}
		//�ͷŲɼ�����һ������.
		av_free_packet(&packet);
		
	}
	int ticks = GetTickCount();
	av_log(NULL,AV_LOG_ERROR,"time=%d,total_frame=%d,lost_frame=%d,test_frame=%d,need_frame=%d\r\n",ticks-start_time,total_fps,lost_fps,test_fps,(ticks-start_time)*fps/1000);
	av_log(NULL,AV_LOG_ERROR,"camera[%d] stop capture thread test_fps=%d fps=%d \r\n ",channel,1000*test_fps/(ticks-start_time),fps);
	if(pFrame)
		av_frame_free(&pFrame);

	if(pPreviewFrame)
		av_frame_free(&pPreviewFrame);
	
	if(pFormatContext)
		avformat_close_input(&pFormatContext);
	pFormatContext = NULL;

	if(fifo_video)
	{
		av_fifo_free(fifo_video);
		fifo_video = 0;
	}
	evt_quit.set();
	bQuit = 1;

	
}

int VideoCap::OpenPreview(const char* psDevName,int index,AVInputFormat *ifmt,const unsigned  int width,
													const unsigned  int height,
													unsigned  int &FrameRate,AVPixelFormat cap_format,AVPixelFormat format, Video_Callback pCbFunc)
{
	int ret = 0;
	if(bCapture) 
	{
		//����ͷ�Ѿ�������.
		return ERR_RECORD_OK;
	}
	bQuit	 = false;
	pCallback = pCbFunc;
	const char* name = av_get_pix_fmt_name(cap_format);
	ret = OpenVideoCapture(&pFormatContext,&pCodecContext,psDevName,index,ifmt,width,height,FrameRate,name);
	if(ret != ERR_RECORD_OK) 
	{
		return ERR_RECORD_VIDEO_OPEN;
	}
	if(!SetCallBackAttr(width,height,format,pCbFunc))
	{
		av_log(NULL,AV_LOG_ERROR,"SetCallBackAttr failed\r\n");
		return ERR_RECORD_VIDEO_OPEN;
	}
	Start();
	return ret;
}
int VideoCap::Start()
{
	if(bCapture) return ERR_RECORD_OK; 
	HANDLE handle = CreateThread( NULL, 0, VideoCapThreadProc, this, 0, NULL);
	if(handle == NULL)
	{
		av_log(NULL,AV_LOG_ERROR,"CreateThread VideoCapThreadProc failed\r\n");
		return ERR_RECORD_VIDEO_OPEN;
	}
	//evt_ready.wait(1000);
	if(!evt_ready.wait(1000))return ERR_RECORD_VIDEO_OPEN;

	return ERR_RECORD_OK;
}
int VideoCap::Stop()
{
	return ERR_RECORD_OK;
}
VideoCap::VideoCap(int chan):
	channel(chan)
{
	pFormatContext = NULL;
	pCodecContext = NULL;
	bCapture = false;
	VideoIndex = 0;
	bQuit = false;
	bStartRecord = false;
	pCallback = NULL;
	fifo_video = NULL;
	sws_ctx = NULL;
	lost = 0;
	InitializeCriticalSection(&section);
}
VideoCap::~VideoCap()
{
	bCapture = false;
}
int VideoCap::Close()
{
	if(bCapture == false) 
	{
		return ERR_RECORD_OK;
	}
	
	bCapture = false;
	if(!evt_quit.wait(1000))return ERR_RECORD_VIDEO_OPEN;
	//evt_quit.wait(1000);
	return ERR_RECORD_OK;
	//�ȴ��߳̽���.
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
AVFrame* VideoCap::GetLastSample()
{
	bool find = false;
	AVFrame* frame = NULL;

	frame = GetSample();

	if(frame != NULL) return frame;

	return pRecFrame2;
}
AVFrame* VideoCap::PeekSample()
{
	bool ok = true;
	
	if(fifo_video == NULL) ok = false;
	if(av_fifo_size(fifo_video) < nSampleSize) ok = false;
	
	if(!ok){
		//av_log(NULL,AV_LOG_ERROR,"vts%d=%d-\r\n",channel,vts++);
		return NULL;
	}
	//av_log(NULL,AV_LOG_ERROR,"vts%d=%d\r\n",channel,vts++);
	DWORD tick = 0;
	EnterCriticalSection(&section);
	uint8_t* offset = av_fifo_peek2(fifo_video,0);

	memcpy(&tick, offset, sizeof(DWORD));
	offset+=sizeof(DWORD);
	memcpy(pRecFrame2->data[0], offset, y_size);
	offset+=y_size;
	memcpy(pRecFrame2->data[1], offset, y_size/4);
	offset+=y_size/4;
	memcpy(pRecFrame2->data[2], offset, y_size/4);
	offset+=y_size/4;
	
	LeaveCriticalSection(&section);

	pRecFrame2->pts = tick;
	return pRecFrame2;
}
AVFrame* VideoCap::GetAudioMatchFrame(DWORD audio_timestamp)
{
	DWORD timestamp2;
	AVFrame* frame = NULL;
	bool first = true;
	AVFrame* prev_frame = NULL;
	if(!GetTimeStamp(timestamp2))
	{
		//�Լ���û�����ݰ���������һ��
		return pRecFrame2;
	}

	do{
		frame = GetSample();
		if(frame != NULL)
		{
			prev_frame = frame;
			if( audio_timestamp <= frame->pts ) //�ҵ�ʱ���ƥ���֡
			{
				break;
			}
			else //�Լ���ʱ��cС����һ·ʱ���
			{
				
			}
		}
		else
		{
			frame = prev_frame;
			break;
		}
		first = false;
		
	}while( (frame!=NULL));

	if(frame != NULL) return frame;

	return pRecFrame2;

}
AVFrame* VideoCap::GetMatchFrame(DWORD timestamp)
{
	bool find = false;
	bool first = true;
	AVFrame* frame = NULL;
	AVFrame* prev_frame = NULL;
	DWORD timestamp2;
	if(!GetTimeStamp(timestamp2))
	{
		//�Լ���û�����ݰ���������һ��
		return pRecFrame2;
	}
	if(timestamp < timestamp2)
	{
		frame = PeekSample();
		return pRecFrame2;
	}

	do{
		frame = GetSample();
		if(frame != NULL)
		{
			prev_frame = frame;
			if( timestamp <= frame->pts ) //�ҵ�ʱ���ƥ���֡
			{
				break;
			}
			else //�Լ���ʱ��cС����һ·ʱ���
			{
				
			}
		}
		else
		{
			frame = prev_frame;
			break;
		}
		first = false;
		
	}while( (frame!=NULL));

	if(frame != NULL) return frame;

	return pRecFrame2;
}
bool VideoCap::GetTimeStamp(DWORD &timeStamp)
{
	if(fifo_video == NULL) return false;
	if(av_fifo_size(fifo_video) < nSampleSize) return false;
	EnterCriticalSection(&section);
	uint8_t* offset = av_fifo_peek2(fifo_video,0);
	timeStamp = *((DWORD*)offset);
	LeaveCriticalSection(&section);
	
	return true;
}
AVFrame* VideoCap::GetSample()
{
	bool ok = true;
	
	if(fifo_video == NULL) ok = false;
	if(av_fifo_size(fifo_video) < nSampleSize) ok = false;
	
	if(!ok){
		//av_log(NULL,AV_LOG_ERROR,"vts%d=%d-\r\n",channel,vts++);
		return NULL;
	}
	//av_log(NULL,AV_LOG_ERROR,"vts%d=%d\r\n",channel,vts++);
	DWORD tick = 0;
	EnterCriticalSection(&section);
	av_fifo_generic_read(fifo_video, &tick, sizeof(tick), NULL);
	av_fifo_generic_read(fifo_video, pRecFrame2->data[0], y_size, NULL); //�Ӷ����ж�ȡһ֡YUV420P������֡,֡�Ĵ�СԤ�����.
	av_fifo_generic_read(fifo_video, pRecFrame2->data[1], y_size/4, NULL); 
	av_fifo_generic_read(fifo_video, pRecFrame2->data[2], y_size/4, NULL); 
	
	LeaveCriticalSection(&section);


	/*
				
	pEncFrame->format = pFmtContext->streams[VideoIndex]->codec->pix_fmt;
	pEncFrame->width  = pFmtContext->streams[VideoIndex]->codec->width;
	pEncFrame->height =	pFmtContext->streams[VideoIndex]->codec->height;
	*/
	pRecFrame2->pts = tick;
	return pRecFrame2;
}
int VideoCap::StartRecord(AVPixelFormat format, int width, int height)
{
	_fmt = format;
	_width = width;
	_height = height;
	 
	record_sws_ctx = sws_getContext(pCodecContext->width, pCodecContext->height,  pCodecContext->pix_fmt, 
		width, height, _fmt, SWS_POINT, NULL, NULL, NULL); 

	if(record_sws_ctx == NULL)
	{
		return -1;
	}

	nSampleSize = avpicture_get_size(format, width, height);
	//����30֡Ŀ��֡��С��video��fifo
	fifo_video = av_fifo_alloc(300 * (nSampleSize+sizeof(DWORD)));
	if(fifo_video == NULL)
	{
		av_log(NULL,AV_LOG_ERROR,"alloc pic fifo failed\r\n");
		return -8;
	}
	
	pRecFrame = alloc_picture(format,width,height,16);//AV_PIX_FMT_YUV420P
	pRecFrame2= alloc_picture(format,width,height,16);//AV_PIX_FMT_YUV420P

	y_size = width*height;
	bStartRecord = true;
	vts = 0;
	pts = 0;
	return 0;
}
int VideoCap::StopRecord()
{
	bStartRecord = false;
	return 0;
}
