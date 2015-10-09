
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