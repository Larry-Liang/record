
#include "stdafx.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
#include "RecordMux.h"
static int FPS = 25;
RecordMux recMux;

#define MAX_DEVICES_NUM 10
#define MAX_DEVICES_NAME_SIZE 128
static char pStrDevices[MAX_DEVICES_NUM][MAX_DEVICES_NAME_SIZE]={{}};
static char* pStrings[MAX_DEVICES_NUM];


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
	//��ֹͣ¼��
	CloudWalk_RecordStop();
	//��ֹͣ�ɼ��߳�
	recMux.Close();

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

	recMux.Stop();
	return ERR_RECORD_OK;
}

/*
����¼��ָ���洢Ŀ¼��ָ����Ƶ���ӵ�����.
����ӿڿ��ܻᷴ�����ã����Ƿ������õ����
*/
CLOUDWALKFACESDK_API  int  SDK_CallMode CloudWalk_RecordStart (const char* filePath,VideoInfo* pVideoInfo, AudioInfo* pAudioInfo,SubTitleInfo* pSubTitle)
{
	av_log(NULL,AV_LOG_DEBUG, "CloudWalk_RecordStart\r\n");
	
	pVideoInfo->width  =  FFALIGN(pVideoInfo->width,  16);
	pVideoInfo->height =  FFALIGN(pVideoInfo->height, 16);
	
	recMux.Start(filePath,pVideoInfo,pAudioInfo,pSubTitle);
	//����������ļ��Ĳ���������¼��
	return ERR_RECORD_OK;
}
CLOUDWALKFACESDK_API  int  SDK_CallMode   CloudWalk_OpenDevices2(
													int video1,
													int video2,
													int audio1,
													const  unsigned  int width,
													const unsigned  int height,
													const unsigned  int FrameRate,
													int sampleRateInHz,
													int channelConfig,
													Video_Callback video_callback)
{
	int count = 0;
	int ret = 0;
	//unsigned int fps = FrameRate;
	av_log(NULL,AV_LOG_ERROR,"CloudWalk_OpenDevices video1=%d,video2=%d,audio=%d width=%d height=%d framerate=%d\r\n",\
			video1,video2,audio1,width,height,FrameRate);

	char** pVideos = CloudWalk_ListDevices(0,&count);
	if(count <= 0) return ERR_RECORD_VIDEO_INDEX;
	if(count < video1)return ERR_RECORD_VIDEO_INDEX;
	if(count < video2)return ERR_RECORD_VIDEO_INDEX;
	if(video1 == video2) return ERR_RECORD_VIDEO_INDEX;

	
	if(strcmp(pVideos[video1],pVideos[video2]) !=0 )
	{
		std::string str_video1 = pVideos[video1];
		std::string str_video2 = pVideos[video2];
		char** pAudios = CloudWalk_ListDevices(1,&count);
		if(count <= 0) return ERR_RECORD_AUDIO_INDEX;
		if(count < audio1)return ERR_RECORD_AUDIO_INDEX;
		return CloudWalk_OpenDevices(str_video1.c_str(),str_video2.c_str(),pAudios[audio1],width,height,FrameRate,sampleRateInHz,channelConfig,video_callback);
	}

	if(video1 > video2)
	{
		video1 = 1;
		video2 = 0;
	}
	else
	{
		video1 = 0;
		video2 = 1;
	}
	unsigned int fps = FrameRate;

	ret = recMux.OpenCamera(pVideos[video1],video1,width,height,fps,AV_PIX_FMT_BGR24, video_callback);
	if(ret != ERR_RECORD_OK)
	{
		av_log(NULL,AV_LOG_ERROR,"OpenCamera failed\r\n");
		return ret;
	}
#if 1
	ret = recMux.OpenCamera2(pVideos[video2],video2,width,height,fps,AV_PIX_FMT_BGR24, video_callback);
	if(ret != ERR_RECORD_OK)
	{
		av_log(NULL,AV_LOG_ERROR,"OpenCamera failed\r\n");
		return ret;
	}
#endif
	char** pAudios = CloudWalk_ListDevices(1,&count);
	if(count <= 0) return ERR_RECORD_AUDIO_INDEX;
	if(count < audio1)return ERR_RECORD_AUDIO_INDEX;

	ret = recMux.OpenAudio(getDevicePath("audio",pAudios[audio1]).c_str());
	if(ret != ERR_RECORD_OK)
	{
		av_log(NULL,AV_LOG_ERROR,"OpenAudio failed\r\n");
		return ret;
	}
	///recMux.StartCap();
	return 0;
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

	int ret = 0;
	unsigned int fps = FrameRate;
	av_log(NULL,AV_LOG_ERROR,"CloudWalk_OpenDevices vidoe=%s,audio=%s width=%d height=%d framerate=%d\r\n",\
			pVideoDevice,pAudioDevice,width,height,FrameRate);
	//����Ƶ�豸Ҫ���ڴ���Ƶ�豸��ǰ�棬�������Ҽ���ĵ�������Ƶ�豸��򲻿�.
	ret = recMux.OpenAudio(pAudioDevice);
	if(ret != ERR_RECORD_OK)
	{
		av_log(NULL,AV_LOG_ERROR,"OpenAudio failed\r\n");
		return ERR_RECORD_AUDIO_OPEN;
	}
	ret = recMux.OpenCamera(pVideoDevice,-1,width,height,fps,AV_PIX_FMT_BGR24, video_callback);
	if(ret != ERR_RECORD_OK)
	{
		av_log(NULL,AV_LOG_ERROR,"OpenCamera failed\r\n");
		return ret;
	}
#if 1
	ret = recMux.OpenCamera2(pVideoDevice2,-1,width,height,fps,AV_PIX_FMT_BGR24, video_callback);
	if(ret != ERR_RECORD_OK)
	{
		av_log(NULL,AV_LOG_ERROR,"OpenCamera failed\r\n");
		return ret;
	}
#endif
	
	//recMux.StartCap();

	return 0;

}

//������dllж�ص�ʱ����ô˺���������ᵼ�³����޷��˳�������ͷû���ٴ�����.
void FreeAllRes()
{
	//ֱ��ж��dll������û�е��ùرղɼ��̣߳�������Ϊ�߳��������Ѿ���ǿ��ɱ���ˣ�����û��ִ�е�audioThreadQuit=1

	CloseDevices();
}

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