/********************************************************************
	created:	2012/04/24
	created:	24:4:2012   16:29
	filename: 	scs-sdk\include\sdk.h
	file path:	scs-sdk\include
	file base:	sdk
	file ext:	h
	author:		 
	
	purpose:	
*********************************************************************/

#ifndef CLOUDWALK_SDK_H
#define CLOUDWALK_SDK_H

#include <windows.h>

#ifdef COULDWALKRECORDER_EXPORTS
#define CLOUDWALKFACESDK_API __declspec(dllexport)
#else
#define CLOUDWALKFACESDK_API __declspec(dllimport)
#endif



#ifdef _WIN32
#define SDK_CallMode WINAPI
#define SDK_CallBack CALLBACK
#else
#define SDK_CallMode
#define SDK_CallBack
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
¼��ģ�鷵�ش�����
*/
enum SDK_RECORD_ERR{
	ERR_RECORD_OK=0, //�ɹ�.
	ERR_RECORD_DSHOW_OPEN=900, //DirectShow��ʧ��
	ERR_RECORD_VIDEO_OPEN, //��Ƶ�豸��ʧ��
	ERR_RECORD_AUDIO_OPEN, //��Ƶ�豸��ʧ��
	ERR_RECORD_NOT_OPEN_DEVS,	//δ������Ƶ�豸,¼������ʧ��.
	ERR_RECORD_OPEN_FILE,	//����¼���ļ�ʧ��.
	ERR_RECORD_OPEN_FILTER,
};

/**
����: �г�����֧�ֵ���������ͷ����Ƶ�ɼ��豸�б�
����������
	DevType[in]: ��ȡ�豸���� 0:����ͷ 1:��˷�
	devCount[in/out]: �����豸�ĸ���
	����ֵ:
	����GBK����������豸���Ƶ��ַ������飬����ĸ���ΪdevCount�ĸ�����

*/

CLOUDWALKFACESDK_API  char** SDK_CallMode CloudWalk_ListDevices(int  devType, int* devCount);


/**
��Ƶ�ص��ӿ�
����������
	rgb24�� rgb24��ʽ��һ֡ͼ�񻺳�����
	width:  ͼ��Ŀ��
	height:  ͼ��ĸ߶�  

*/
typedef  int (*Video_Callback)(unsigned char* rgb24, int width, int height);

/**
����: ������Ƶ�豸��ע��ص�����
����������
	pVideoDevice: ��Ƶ�豸��
	pAudioDevice: ��Ƶ�豸��
	width:ͼ��Ŀ��
	height:ͼ��ĸ߶�
	FrameRate������ͷ�ɼ���֡��
	FrameBitRate: ѹ�������Ƶ����
	sampleRateInHz����Ƶ����Ƶ��
	channelConfig����Ƶͨ����
	_callback: �ɼ���һ֡��Ƶ��ص��ú���������RGB24��ʽ����Ƶ���ݡ��ص������ڲ�Ӧ�þ��췵�أ���Ҫ�����������Ӱ�쵽��Ƶ�ɼ���
	����ֵ:
	///0-�ɹ�����0  Ϊ�������

*/
CLOUDWALKFACESDK_API  int  SDK_CallMode   CloudWalk_OpenDevices(
													const char* pVideoDevice,
													const char* pAudioDevice,
													const  unsigned  int width,
													const unsigned  int height,
													const unsigned  int FrameRate,
													int sampleRateInHz,
													int channelConfig,
													Video_Callback video_callback);

/**
����: ����¼��
����������
filePath�� ������ļ�·�����ļ���
pText: ��Ƶ���ӵ�����
����ֵ:
///0-�ɹ�����0  Ϊ�������

*/

/*
¼�������Ƶ����.
*/
enum VideoEncType
{
	ENC_H264=0,
	ENC_MPEG4
};
typedef struct _VideoInfo
{
	int width,height;	//��Ƶ�ĸ߶ȺͿ��,���û�а�16�ֽڶ��룬�ڲ���ǿ�ƶ���.
	int fps;		//��Ƶ��֡��
	int bitrate;	//��Ƶ������

}VideoInfo;

typedef struct _AudioInfo
{
	int channle; //��Ƶͨ����
	int bitrate; //���������
}AudioInfo;
#define MAX_SUBTITLE_SIZE 128
typedef struct _SubTitleInfo
{
	int  x,y; // ��Ļ��λ��
	const char *text; //��Ļ������
	const char *fontname; //��������. [simfang.ttf]
	const char *fontcolor; //������ɫ[red,white,black]
	int  fontsize; //����Ĵ�С.
}SubTitleInfo;
typedef enum{

}REC_ERR;


CLOUDWALKFACESDK_API  int  SDK_CallMode CloudWalk_RecordStart (const char* filePath,VideoInfo* pVideoInfo, AudioInfo* pAudioInfo,SubTitleInfo* pSubTitle);


/**
����: ֹͣ¼��(ֹͣ¼��󣬱��浽�ļ��У��ر�¼���ļ�)

����������
����ֵ:
///0-�ɹ�����0  Ϊ�������

*/
CLOUDWALKFACESDK_API  int  SDK_CallMode CloudWalk_RecordStop (void);

/**
����: �ر��豸,�ر��豸���ͷ�������Դ��


����������
����ֵ:
///0-�ɹ�����0  Ϊ�������

*/

CLOUDWALKFACESDK_API  int  SDK_CallMode CloudWalk_CloseDevices(void);


CLOUDWALKFACESDK_API  int  SDK_CallMode CloudWalk_Muxing(int argc, char **argv);
#ifdef __cplusplus
};
#endif


#endif  // SCSSDK_SDK_H

