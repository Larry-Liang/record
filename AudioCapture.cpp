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
#include <Windows.h>

#include "AudioCapture.h"
#include "Utils.h"

int OpenAudioCapture(AVFormatContext** pFmtCtx, const char * psDevName, AVInputFormat *ifmt)
{

	//��Direct Show�ķ�ʽ���豸������ ���뷽ʽ ��������ʽ������
	//char * psDevName = dup_wchar_to_utf8(L"audio=��˷� (Realtek High Definition Au");
	//char * psDevName = dup_wchar_to_utf8(L"audio=��˷� (2- USB Audio Device)");
	if (avformat_open_input(pFmtCtx, psDevName, ifmt,NULL) < 0)
	{
		av_log(NULL,AV_LOG_ERROR,"Couldn't open input stream.���޷�����Ƶ��������\n");
		return -1;
	}

	if(avformat_find_stream_info(*pFmtCtx,NULL)<0)  
		return -2; 
	
	if((*pFmtCtx)->streams[0]->codec->codec_type != AVMEDIA_TYPE_AUDIO)
	{
		av_log(NULL,AV_LOG_ERROR,"Couldn't find video stream information.���޷���ȡ��Ƶ����Ϣ��\n");
		return -3;
	}

	AVCodec *tmpCodec = avcodec_find_decoder((*pFmtCtx)->streams[0]->codec->codec_id);
	if(0 > avcodec_open2((*pFmtCtx)->streams[0]->codec, tmpCodec, NULL))
	{
		av_log(NULL,AV_LOG_ERROR,"can not find or open audio decoder!\n");
		return -4;
	}

	

	return 0;
}

DWORD WINAPI AudioCapThreadProc( LPVOID lpParam )
{
	if(lpParam != NULL)
	{
		AudioCap* vc = (AudioCap*)lpParam;
		vc->Run();
	}
	return 0;
}

AudioCap::AudioCap()
{
	pFormatContext = NULL;
	pCodecContext= NULL;
	bCapture= true;
	bQuit=false;
	bStartRecord = false;
	fifo_audio = NULL;
	InitializeCriticalSection(&section);
	sws_ctx=NULL;
}
int AudioCap::Open(const char * psDevName, AVInputFormat *ifmt)
{
	int ret = 0;
	bQuit	 = false;
	bCapture = true;
	
	ret = OpenAudioCapture(&pFormatContext,psDevName,ifmt);
	
	CreateThread( NULL, 0, AudioCapThreadProc, this, 0, NULL);

	return ret;
}

int AudioCap::Close()
{
	bCapture = false;
	//�ȴ��߳̽���.
	return 0;
}
AVCodecContext* AudioCap::GetCodecContext()
{
	return pFormatContext->streams[0]->codec;
}
void AudioCap::Run( )
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
		if(av_read_frame(pFormatContext,&pkt) < 0)
		{
			continue;
		}
		//�����ָ���ĸ�ʽ��pkt  fixme
		if (avcodec_decode_audio4(pFormatContext->streams[0]->codec, frame, &gotframe, &pkt) < 0)
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
			fifo_audio = av_audio_fifo_alloc(pFormatContext->streams[0]->codec->sample_fmt, 
				pFormatContext->streams[0]->codec->channels, 30 * frame->nb_samples);
		}
		if(bStartRecord)
		{
			int buf_space = av_audio_fifo_space(fifo_audio);
			if (av_audio_fifo_space(fifo_audio) >= frame->nb_samples)
			{

				av_log(NULL,AV_LOG_PANIC,"************write audio fifo\r\n");
			

				//��Ƶ������¼�����.
				EnterCriticalSection(&section);
				av_audio_fifo_write(fifo_audio, (void **)frame->data, frame->nb_samples);
				LeaveCriticalSection(&section);
				
				
			}
		}
		
		
		
	}
	av_frame_free(&frame);
	if(pFormatContext)
		avformat_close_input(&pFormatContext);
	pFormatContext = NULL;

	if(fifo_audio)
	{
		av_audio_fifo_free(fifo_audio);
		fifo_audio = NULL;
	}

	bQuit = true;
	av_log(NULL,AV_LOG_ERROR,"video thread exit\r\n");

}
bool AudioCap::StartRecord()
{
	bStartRecord = true;
	return true;
}
int AudioCap::StopRecord()
{
	bStartRecord = false;
	return 0;
}
int AudioCap::SimpleSize()
{

	return av_audio_fifo_size(fifo_audio);	
}
int AudioCap::GetSample(void **data, int nb_samples)
{
	int nRead = 0;
	EnterCriticalSection(&section);
			//����Ƶfifo�ж�ȡ��������Ҫ����������.
	nRead = av_audio_fifo_read(fifo_audio, data, 
		nb_samples);
	LeaveCriticalSection(&section);

	return nRead;
}