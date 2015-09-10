
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
#include "libavfilter/avfiltergraph.h" 
#include "libavfilter/buffersink.h"  
#include "libavfilter/buffersrc.h" 


#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "avdevice.lib")
#pragma comment(lib, "avfilter.lib")
#pragma comment(lib, "avfilter.lib")
//#pragma comment(lib, "postproc.lib")
//#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "swscale.lib")
#ifdef __cplusplus
};
#endif
#include "Recorder.h"
#include "CaptureDevices.h"
#include   <time.h>   
#ifdef   WIN32     
#define   LOCALTIME_R(t)     localtime((t))     
#else     
#define   LOCALTIME_R(t)     localtime_r((t),   (struct   tm   *)&tmres)     
#endif   

//#define DRAW_TEXT 1
AVFormatContext	*pFormatCtx_Video = NULL, *pFormatCtx_Audio = NULL, *pFormatCtx_Out = NULL;
AVCodecContext	*pCodecCtx_Video;
AVCodec			*pCodec_Video;
AVFifoBuffer	*fifo_video = NULL;
AVAudioFifo		*fifo_audio = NULL;

AVFilterGraph	*filter_graph;
AVFilterContext *buffersink_ctx;
AVFilterContext *buffersrc_ctx;

static HANDLE gAudioHandle = INVALID_HANDLE_VALUE;
static HANDLE gVideoHandle = INVALID_HANDLE_VALUE;
static HANDLE gRecordHandle= INVALID_HANDLE_VALUE;
int VideoIndex, AudioIndex;
int audioThreadQuit = 0;
int recordThreadQuit = 0;
int videoThreadQuit = 0;
int gYuv420FrameSize = 0;
static VideoInfo gOutVideoInfo;
int64_t cur_pts_v=0,cur_pts_a=0;

static bool bStartRecord = false;
static int init_report_file(const char* filename);
CRITICAL_SECTION AudioSection, VideoSection;

static int FPS = 25;

SwsContext *yuv420p_convert_ctx; //��Դ��ʽת��ΪYUV420P��ʽ
SwsContext *rgb24_convert_ctx;   //��Դ��ʽת��ΪRGB24��ʽ
int frame_size = 0;

uint8_t* pRec_yuv420p_buf = NULL;
uint8_t* pEnc_yuv420p_buf = NULL;
AVFrame *pEncFrame = NULL; //¼���̴߳Ӷ�����ȡ��һ��YUV420P�����ݺ���䵽pEncFrame�У������б���
AVFrame* pRecFrame = NULL; //��Ƶ�ɼ��߳��������ɼ�������Ƶԭʼ����ת����YUV420P��ʽ������֡������Ͷ�ݵ�����.
static bool bCapture = false;


#include <string.h>
#include <stdlib.h>

#define MAX_DEVICES_NUM 10
#define MAX_DEVICES_NAME_SIZE 128
static char pStrDevices[MAX_DEVICES_NUM][MAX_DEVICES_NAME_SIZE]={{}};
static char* pStrings[MAX_DEVICES_NUM];
static Video_Callback g_video_callback = NULL;


DWORD WINAPI VideoCapThreadProc( LPVOID lpParam );
DWORD WINAPI AudioCapThreadProc( LPVOID lpParam );
#define MAX_WIDTH 1080
#define MAX_HEIGHT 720
static uint8_t rgb24_buffer[MAX_WIDTH*MAX_HEIGHT*3];
static int VideoFrameIndex = 0, AudioFrameIndex = 0;
const char *filter_descr = "movie=my_logo.png[wm];[in][wm]overlay=5:5[out]";
//const char *filter_descr="drawtext=fontfile=simfont.ttf:fontcolor=white:shadowcolor=black:text='������Ƶ':x=10:y=10";
//const char *filter_descr="drawtext=fontfile=simfang.ttf: timecode='09\:57\:00\;00': r=30: x=(w-tw)/2: y=h-(2*lh): fontcolor=white: box=1: boxcolor=0x00000000@1";


static void get_log_filename(char* buffer, int size)
{
	struct   tm   *tmNow;     
    time_t   long_time;     
    time(&long_time   );                             /*   Get   time   as   long   integer.   */     
    tmNow   =   LOCALTIME_R(   &long_time   );   /*   Convert   to   local   time.   */    

    _snprintf(buffer,size,"record_%04d%02d%02d%02d%02d%02d.log",tmNow->tm_year+1900,   tmNow->tm_mon   +   1,       
        tmNow->tm_mday,   tmNow->tm_hour,   tmNow->tm_min,   tmNow->tm_sec);     

}
int init_ffmpeg_env()
{
	char log_file[128] = {0,};
	av_register_all();
	avdevice_register_all();
	avfilter_register_all();
	get_log_filename(log_file,128);
	init_report_file(log_file);
	return 0;
}
/*
gbkתutf8
*/
static char *dup_wchar_to_utf8(wchar_t *w)
{
	char *s = NULL;
	int l = WideCharToMultiByte(CP_UTF8, 0, w, -1, 0, 0, 0, 0);
	s = (char *) av_malloc(l);
	if (s)
		WideCharToMultiByte(CP_UTF8, 0, w, -1, s, l, 0, 0);
	return s;
}

string GBKToUTF8(const char* strGBK)
{ 
	string tmp;
    int len=MultiByteToWideChar(CP_ACP, 0, (LPCTSTR)strGBK, -1, NULL,0); 
    unsigned short * wszUtf8 = new unsigned short[len+1]; 
    memset(wszUtf8, 0, len * 2 + 2); 
    MultiByteToWideChar(CP_ACP, 0, (LPCTSTR)strGBK, -1, (LPWSTR)wszUtf8, len);
    len = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)wszUtf8, -1, NULL, 0, NULL, NULL);
    char *szUtf8=new char[len + 1]; 
    memset(szUtf8, 0, len + 1); 
    WideCharToMultiByte (CP_UTF8, 0, (LPCWSTR)wszUtf8, -1, (LPSTR)szUtf8, len, NULL,NULL);
	tmp = szUtf8;
	if(szUtf8) delete []szUtf8;
	if(wszUtf8) delete []wszUtf8;
    return tmp; 
}

static bool resetDevciesString(int num)
{
	for(int i = 0; i < num; i++)
	{
		memset(pStrDevices[i],0 , sizeof(MAX_DEVICES_NAME_SIZE));
	} 
	return true;
}
/*
utf8תgbk
*/
std::string UTF8ToGBK(const char* str)
{
     std::string result;
     WCHAR *strSrc;
     TCHAR *szRes;

     //�����ʱ�����Ĵ�С
     int i = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
     strSrc = new WCHAR[i+1];
     MultiByteToWideChar(CP_UTF8, 0, str, -1, strSrc, i);

     //�����ʱ�����Ĵ�С
     i = WideCharToMultiByte(CP_ACP, 0, strSrc, -1, NULL, 0, NULL, NULL);
     szRes = new TCHAR[i+1];
     WideCharToMultiByte(CP_ACP, 0, strSrc, -1, szRes, i, NULL, NULL);

     result = szRes;
     delete []strSrc;
     delete []szRes;

     return result;
}
#if 1
static int init_filters(const char *filters_descr)  
{  
    char args[512];  
    int ret;  
    AVFilter *buffersrc  = avfilter_get_by_name("buffer");  
    AVFilter *buffersink = avfilter_get_by_name("ffbuffersink");  
    AVFilterInOut *outputs = avfilter_inout_alloc();  
    AVFilterInOut *inputs  = avfilter_inout_alloc();  
    enum PixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, PIX_FMT_NONE };  
    AVBufferSinkParams *buffersink_params;  
  
    filter_graph = avfilter_graph_alloc();  
  
    /* buffer video source: the decoded frames from the decoder will be inserted here. */  
	//pFormatCtx_Video->streams[0]->codec->pix_fmt
    _snprintf_s(args, sizeof(args),  
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=1/1",  
			pFormatCtx_Video->streams[0]->codec->width, pFormatCtx_Video->streams[0]->codec->height, AV_PIX_FMT_YUV420P ,  
            pFormatCtx_Video->streams[0]->codec->time_base.num, pFormatCtx_Video->streams[0]->codec->time_base.den,  
            pFormatCtx_Video->streams[0]->codec->sample_aspect_ratio.num, pFormatCtx_Video->streams[0]->codec->sample_aspect_ratio.den);  
  
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",  
                                       args, NULL, filter_graph);  
    if (ret < 0) {  
        av_log(NULL,AV_LOG_ERROR,"Cannot create buffer source\n");  
        return ret;  
    }  
  
    /* buffer video sink: to terminate the filter chain. */  
    buffersink_params = av_buffersink_params_alloc();  
    buffersink_params->pixel_fmts = pix_fmts;  
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",  
                                       NULL, buffersink_params, filter_graph);  
    av_free(buffersink_params);  
    if (ret < 0) {  
        av_log(NULL,AV_LOG_ERROR,"Cannot create buffer sink\n");  
        return ret;  
    }  
  
    /* Endpoints for the filter graph. */  
    outputs->name       = av_strdup("in");  
    outputs->filter_ctx = buffersrc_ctx;  
    outputs->pad_idx    = 0;  
    outputs->next       = NULL;  
  
    inputs->name       = av_strdup("out");  
    inputs->filter_ctx = buffersink_ctx;  
    inputs->pad_idx    = 0;  
    inputs->next       = NULL;  
  
    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,  
                                    &inputs, &outputs, NULL)) < 0)  
        return ret;  
  
    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)  
        return ret;  
    return 0;  
}  
#endif
/*
 ����Ƶ�ɼ��豸
 video: utf8 �������Ƶ�豸�� 
*/
int OpenVideoCapture(const char* psDevName,AVInputFormat *ifmt)
{
	
	//char * psDevName = video;//dup_wchar_to_utf8(L"video=USB ��Ƶ�豸");
	//UTF8ToGBK(psDevName);
	//������ԼӲ����򿪣��������ָ���ɼ�֡��
	AVDictionary *options = NULL;
	char buf[10] = {0,};
	_snprintf_s(buf,10,"%d",FPS);
	av_dict_set(&options, "framerate", buf, NULL);
	av_dict_set(&options, "rtbufsize", "13824k", NULL);

	//av_dict_set(&options, "video_size", "vga", NULL);
	
	//av_dict_set(&options,"offset_x","20",0);
	//The distance from the top edge of the screen or desktop
	//av_dict_set(&options,"offset_y","40",0);
	//Video frame size. The default is to capture the full screen
	av_dict_set(&options,"pix_fmt","rgb24",0);
	if(avformat_open_input(&pFormatCtx_Video, psDevName, ifmt, &options)!=0)
		//if(avformat_open_input(&pFormatCtx_Video, psDevName, ifmt, NULL)!=0)
	{
		av_log(NULL,AV_LOG_ERROR,"Couldn't open input stream.���޷�����Ƶ��������\n");
		return -1;
	}
	//pFormatCtx_Video->streams[0]->codec->time_base.num = 1;
	//pFormatCtx_Video->streams[0]->codec->time_base.den = FPS;
	if(avformat_find_stream_info(pFormatCtx_Video,NULL)<0)
	{
		av_log(NULL,AV_LOG_ERROR,"Couldn't find stream information.���޷���ȡ��Ƶ����Ϣ��\n");
		return -2;
	}
	if (pFormatCtx_Video->streams[0]->codec->codec_type != AVMEDIA_TYPE_VIDEO)
	{
		av_log(NULL,AV_LOG_ERROR,"Couldn't find video stream information.���޷���ȡ��Ƶ����Ϣ��\n");
		return -3;
	}
	pCodecCtx_Video = pFormatCtx_Video->streams[0]->codec;
	//�����pCodecCtx_Video->codec_id��ʲôʱ���ʼ����? ��dshow.c�е�dshow_add_device�������� codec->codec_id = AV_CODEC_ID_RAWVIDEO;

	pCodec_Video = avcodec_find_decoder(pCodecCtx_Video->codec_id);
	if(pCodec_Video == NULL)
	{
		av_log(NULL,AV_LOG_ERROR,"Codec not found.��û���ҵ���������\n");
		return -4;
	}
	if(avcodec_open2(pCodecCtx_Video, pCodec_Video, NULL) < 0)
	{
		av_log(NULL,AV_LOG_ERROR,"Could not open codec.���޷��򿪽�������\n");
		return -5;
	}

	
	
	return 0;
}

static bool initPrevVideoFrame(void)
{

}
static bool initOutputVideoFrame(void)
{

}

int OpenVideoCaptureDesktop()
{
	AVInputFormat *ifmt=av_find_input_format("gdigrab");
	//������ԼӲ����򿪣��������ָ���ɼ�֡��
	AVDictionary *options = NULL;
	av_dict_set(&options, "framerate", "15", NULL);
	//av_dict_set(&options,"offset_x","20",0);
	//The distance from the top edge of the screen or desktop
	//av_dict_set(&options,"offset_y","40",0);
	//Video frame size. The default is to capture the full screen
	//av_dict_set(&options,"video_size","320x240",0);
	if(avformat_open_input(&pFormatCtx_Video, "desktop", ifmt, &options)!=0)
	{
		av_log(NULL,AV_LOG_ERROR,"Couldn't open input stream.���޷�����Ƶ��������\n");
		return -1;
	}
	if(avformat_find_stream_info(pFormatCtx_Video,NULL)<0)
	{
		av_log(NULL,AV_LOG_ERROR,"Couldn't find stream information.���޷���ȡ��Ƶ����Ϣ��\n");
		return -1;
	}
	if (pFormatCtx_Video->streams[0]->codec->codec_type != AVMEDIA_TYPE_VIDEO)
	{
		av_log(NULL,AV_LOG_ERROR,"Couldn't find video stream information.���޷���ȡ��Ƶ����Ϣ��\n");
		return -1;
	}
	pCodecCtx_Video = pFormatCtx_Video->streams[0]->codec;
	pCodec_Video = avcodec_find_decoder(pCodecCtx_Video->codec_id);
	if(pCodec_Video == NULL)
	{
		av_log(NULL,AV_LOG_ERROR,"Codec not found.��û���ҵ���������\n");
		return -1;
	}
	if(avcodec_open2(pCodecCtx_Video, pCodec_Video, NULL) < 0)
	{
		av_log(NULL,AV_LOG_ERROR,"Could not open codec.���޷��򿪽�������\n");
		return -1;
	}

	
	//�õ�һ��rgb24
	rgb24_convert_ctx = sws_getContext(pCodecCtx_Video->width, pCodecCtx_Video->height, pCodecCtx_Video->pix_fmt, 
		pCodecCtx_Video->width, pCodecCtx_Video->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL); 

	frame_size = avpicture_get_size(pCodecCtx_Video->pix_fmt, pCodecCtx_Video->width, pCodecCtx_Video->height);
	//����30֡����
	fifo_video = av_fifo_alloc(30 * avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecCtx_Video->width, pCodecCtx_Video->height));
	if(fifo_video == NULL)
	{
		av_log(NULL,AV_LOG_ERROR,"alloc pic fifo failed\r\n");
		return -1;
	}
	return 0;
}



int OpenAudioCapture(const char * psDevName, AVInputFormat *ifmt)
{

	
	//��Direct Show�ķ�ʽ���豸������ ���뷽ʽ ��������ʽ������
	//char * psDevName = dup_wchar_to_utf8(L"audio=��˷� (Realtek High Definition Au");
	//char * psDevName = dup_wchar_to_utf8(L"audio=��˷� (2- USB Audio Device)");
	if (avformat_open_input(&pFormatCtx_Audio, psDevName, ifmt,NULL) < 0)
	{
		av_log(NULL,AV_LOG_ERROR,"Couldn't open input stream.���޷�����Ƶ��������\n");
		return -1;
	}

	if(avformat_find_stream_info(pFormatCtx_Audio,NULL)<0)  
		return -2; 
	
	if(pFormatCtx_Audio->streams[0]->codec->codec_type != AVMEDIA_TYPE_AUDIO)
	{
		av_log(NULL,AV_LOG_ERROR,"Couldn't find video stream information.���޷���ȡ��Ƶ����Ϣ��\n");
		return -3;
	}

	AVCodec *tmpCodec = avcodec_find_decoder(pFormatCtx_Audio->streams[0]->codec->codec_id);
	if(0 > avcodec_open2(pFormatCtx_Audio->streams[0]->codec, tmpCodec, NULL))
	{
		av_log(NULL,AV_LOG_ERROR,"can not find or open audio decoder!\n");
		return -4;
	}

	

	return 0;
}
/*
 �̶���������,������С���ƵıȽ�׼ȷ.
 CBR (Constant Bit Rate)
There is no native CBR mode, but you can "simulate" a constant bit rate setting by tuning the parameters of ABR, like
ffmpeg -i input -c:v libx264 -b:v 4000k -minrate 4000k -maxrate 4000k -bufsize 1835k out.m2v
in this example, -bufsize is the "rate control buffer" so it will enforce your requested "average" (4000k in this case) across each 1835k worth of video. So basically it is assumed that the receiver/end player will buffer that much data so it's ok to fluctuate within that much.
Of course, if it's all just empty/black frames then it will still serve less than that many bits/s (but it will raise the quality level as much as it can, up to the crf level).

ֻ����bit_rate��ƽ�����ʣ���һ���ܿ���ס

c->bit_rate = 400000;
c->rc_max_rate = 400000;
c->rc_min_rate = 400000;
*/
void CBR_Set(AVCodecContext *c, long br)
{
	c->bit_rate = br;
	c->rc_min_rate =br;
	c->rc_max_rate = br;
	c->bit_rate_tolerance = br;
	c->rc_buffer_size=br;
	c->rc_initial_buffer_occupancy = c->rc_buffer_size*3/4;
	//c->rc_buffer_aggressivity= (float)1.0;
	//c->rc_initial_cplx= 0.5;
}
/*
 �ɱ���������,������С���ƵĲ�̫׼ȷ.
 ffmpeg.x264 doc : https://trac.ffmpeg.org/wiki/Encode/H.264
*/
void VBR_Set(AVCodecContext *c, long br, long max, long min)
{
	c->flags |= CODEC_FLAG_QSCALE;
    c->rc_min_rate  = min;
    c->rc_max_rate  = max;
    c->bit_rate		= br; //�������������

	/*
	c->flags |= CODEC_FLAG_QSCALE;
	c->rc_min_rate =min;
	c->rc_max_rate = max; 
	c->bit_rate = br;
	*/
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
		
		pVideoStream->codec->time_base = pFormatCtx_Video->streams[0]->codec->time_base; //����ļ���Ƶ���ĸ߶Ⱥ������ļ���ʱ��һ��.
		pVideoStream->time_base = pFormatCtx_Video->streams[0]->codec->time_base;
		pVideoStream->codec->sample_aspect_ratio = pFormatCtx_Video->streams[0]->codec->sample_aspect_ratio;
		// take first format from list of supported formats
		//�ɲ鿴ff_mpeg4_encoder��ָ���ĵ�һ�����ظ�ʽ������ǲ���MPEG4�����ʱ��������Ƶ֡�����ظ�ʽ��������YUV420P
		pVideoStream->codec->pix_fmt = pFormatCtx_Out->streams[VideoIndex]->codec->codec->pix_fmts[0]; //���ظ�ʽ������MPEG4֧�ֵĵ�һ����ʽ
		
		CBR_Set(pVideoStream->codec, pVideoInfo->bitrate); //���ù̶�����
		//VBR_Set(pVideoStream->codec, pVideoInfo->bitrate, 2*pVideoInfo->bitrate  , pVideoInfo->bitrate/2);
		
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

		pOutputCodecCtx->sample_rate = pFormatCtx_Audio->streams[0]->codec->sample_rate; //����Ĳ����ʵ��ڲɼ��Ĳ�����
		pOutputCodecCtx->channel_layout = pFormatCtx_Out->streams[0]->codec->channel_layout;
		pOutputCodecCtx->channels = av_get_channel_layout_nb_channels(pAudioStream->codec->channel_layout);//�����ͨ�����ڲɼ���ͨ����
		if(pOutputCodecCtx->channel_layout == 0)
		{
			pOutputCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
			pOutputCodecCtx->channels = av_get_channel_layout_nb_channels(pOutputCodecCtx->channel_layout);
		}
		pOutputCodecCtx->sample_fmt = pAudioStream->codec->codec->sample_fmts[0];
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
	//AVRational time_base={1, pAudioStream->codec->sample_rate};
	//pAudioStream->time_base = time_base;
	pVideoStream->codec->time_base = pFormatCtx_Video->streams[0]->codec->time_base;
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

	//������Ҫת����Ŀ���ʽΪYUV420P
	yuv420p_convert_ctx = sws_getContext(pCodecCtx_Video->width, pCodecCtx_Video->height, pCodecCtx_Video->pix_fmt, 
		pVideoInfo->width, pVideoInfo->height, AV_PIX_FMT_YUV420P, SWS_POINT, NULL, NULL, NULL); 

	//yuv420p_convert_ctx = sws_getContext(pCodecCtx_Video->width, pCodecCtx_Video->height, AV_PIX_FMT_RGB24, 
	//	pVideoInfo->width, pVideoInfo->height, AV_PIX_FMT_YUV420P, SWS_POINT, NULL, NULL, NULL); 
	//��ȡĿ��֡�Ĵ�С.
	frame_size = avpicture_get_size(pFormatCtx_Out->streams[VideoIndex]->codec->pix_fmt, pVideoInfo->width, pVideoInfo->height);
	//����30֡Ŀ��֡��С��video��fifo
	fifo_video = av_fifo_alloc(30 * frame_size);
	if(fifo_video == NULL)
	{
		av_log(NULL,AV_LOG_ERROR,"alloc pic fifo failed\r\n");
		return -8;
	}

	pRecFrame = av_frame_alloc(); //����һ��֡�����ڴ��¼����Ƶ����,Ŀ����YUV420P
	//int size = avpicture_get_size(AV_PIX_FMT_YUV420P, 
	//	pVideoInfo->width, pVideoInfo->height);
	pRec_yuv420p_buf = new uint8_t[gYuv420FrameSize];
	avpicture_fill((AVPicture *)pRecFrame, pRec_yuv420p_buf, 
		AV_PIX_FMT_YUV420P, 
		pVideoInfo->width, 
		pVideoInfo->height);

	
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
std::string ws2s(const std::wstring& ws)
{
    std::string curLocale = setlocale(LC_ALL, NULL);        // curLocale = "C";
    setlocale(LC_ALL, "chs");
    const wchar_t* _Source = ws.c_str();
    size_t _Dsize = 2 * ws.size() + 1;
    char *_Dest = new char[_Dsize];
    memset(_Dest,0,_Dsize);
    wcstombs(_Dest,_Source,_Dsize);
    std::string result = _Dest;
    delete []_Dest;
    setlocale(LC_ALL, curLocale.c_str());
    return result;
}

std::wstring s2ws(const std::string& s)
{
    setlocale(LC_ALL, "chs"); 
    const char* _Source = s.c_str();
    size_t _Dsize = s.size() + 1;
    wchar_t *_Dest = new wchar_t[_Dsize];
    wmemset(_Dest, 0, _Dsize);
    mbstowcs(_Dest,_Source,_Dsize);
    std::wstring result = _Dest;
    delete []_Dest;
    setlocale(LC_ALL, "C");
    return result;
}


static void YUV420p_to_RGB24(unsigned char *yuv420[3], unsigned char *rgb24, int width, int height) 
{
//  int begin = GetTickCount();
	int R,G,B,Y,U,V;
	int x,y;
	int nWidth = width>>1; //ɫ���źſ��
	for (y=0;y<height;y++)
	{
	for (x=0;x<width;x++)
	{
	Y = *(yuv420[0] + y*width + x);
	U = *(yuv420[1] + ((y>>1)*nWidth) + (x>>1));
	V = *(yuv420[2] + ((y>>1)*nWidth) + (x>>1));
	R = Y + 1.402*(V-128);
	G = Y - 0.34414*(U-128) - 0.71414*(V-128);
	B = Y + 1.772*(U-128);

	//��ֹԽ��
	if (R>255)R=255;
	if (R<0)R=0;
	if (G>255)G=255;
	if (G<0)G=0;
	if (B>255)B=255;
	if (B<0)B=0;
   
	*(rgb24 + ((height-y-1)*width + x)*3) = B;
	*(rgb24 + ((height-y-1)*width + x)*3 + 1) = G;
	*(rgb24 + ((height-y-1)*width + x)*3 + 2) = R;
  }
 }
}
//#define ENABLE_YUVFILE 1
//#define RGB_DEBUG 1
//#define YUV_DEBUG 1
FILE *fp_yuv = NULL;
#ifdef DRAW_TEXT
static int push_filter(AVFrame	*pFrame,AVFrame* pFilterFrame)
{
	int ret = 0;
	//AVFilterBufferRef *picref = NULL;
	//AVFrame *filt_frame = av_frame_alloc();

	pFrame->pts = av_frame_get_best_effort_timestamp(pFrame);
	 /* push the decoded frame into the filtergraph */
	   /* push the decoded frame into the filtergraph */
    if (av_buffersrc_add_frame_flags(buffersrc_ctx, pFrame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
        return -1;
    }
#if 0
    if (av_buffersrc_add_frame(buffersrc_ctx, pFrame) < 0) {
        av_log(NULL,AV_LOG_ERROR,( "Error while feeding the filtergraph\n");
        return NULL;
    }
#endif
	 while (1) {
        ret = av_buffersink_get_frame(buffersink_ctx, pFilterFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
			return -2;
		}
        
        if (ret < 0)
		{
			return ret;	
		}

		return 0;


    }

	 /*
	while (1) {
		int ret = av_buffersink_get_buffer_ref(buffersink_ctx, &picref, 0);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
		{
			ret = -2;
			break;
		}
		if (ret < 0)
		{
			break;		
		}
		
		if (picref) {
	#if ENABLE_YUVFILE
			int y_size=picref->video->w*picref->video->h;
			fwrite(picref->data[0],1,y_size,fp_yuv);     //Y
			fwrite(picref->data[1],1,y_size/4,fp_yuv);   //U
			fwrite(picref->data[2],1,y_size/4,fp_yuv);   //V
			
	#endif
			return picref;
			//avfilter_unref_bufferp(&picref);	//free picref;
		}

	}
	*/
	//if(ret < 0) return NULL;
	

}
#endif

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
	//int height = pFormatCtx_Out->streams[VideoIndex]->codec->height;
	//int width  = pFormatCtx_Out->streams[VideoIndex]->codec->width;
	
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
				//ת����Ŀ���ʽYUV420P,����picture���AVFrame. �����������ͷ��YUYV422ת����YUV420P
				if(pFrame->format != AV_PIX_FMT_RGB24)
					sws_scale(rgb24_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, 
						pCodecCtx_Video->height, pPreviewFrame->data, pPreviewFrame->linesize);
				
#ifdef DRAW_TEXT
				pPreviewFrame->width = 640;
				pPreviewFrame->height = 480;
				pPreviewFrame->format = AV_PIX_FMT_RGB24;
				
				//int ret = push_filter(pPreviewFrame,filt_frame);
				
#endif
				
				
				
				//ת����RGB24
				//YUV420p_to_RGB24(pic_video->data,rgb24_buffer, width ,height);
				if(g_video_callback)
				{
					//�ص���Ƶ����.
#ifdef DRAW_TEXT
					//if(ret == 0)
						g_video_callback(pPreviewFrame->data[0], pCodecCtx_Video->width ,pCodecCtx_Video->height);
						
			
					
#else
					
					if(pFrame->format != AV_PIX_FMT_RGB24)
						g_video_callback(pPreviewFrame->data[0], pCodecCtx_Video->width ,pCodecCtx_Video->height);
					else 
						g_video_callback(pFrame->data[0], pCodecCtx_Video->width ,pCodecCtx_Video->height);
#endif
						
					
#ifdef RGB_DEBUG
				fwrite(pPreviewFrame->data[0],nRGB24size,1,output);    
#endif
				}
				#ifdef DRAW_TEXT
				//if(ret == 0)
				//{
				//	av_frame_unref(filt_frame);
				//}
#endif
				//bStartRecord�����־��λ��fifo_video���ܻ�û�д����ɹ�.
				if(bStartRecord && fifo_video) //���������¼�񣬲�������Ƶ���ݵ�¼�����.
				{
					//ת����Ƶ֡Ϊ¼����Ƶ��ʽ YUYV422->YUV420P
					//picture is yuv420 data.
					int y_size = gOutVideoInfo.width*gOutVideoInfo.height;
					if (av_fifo_space(fifo_video) >= gYuv420FrameSize)
					{
						sws_scale(yuv420p_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, 
								pFormatCtx_Out->streams[VideoIndex]->codec->height, pRecFrame->data, pRecFrame->linesize);
						//pRecFrame->width = 640;
						//pRecFrame->height = 480;
						//pRecFrame->format = AV_PIX_FMT_YUV420P;
				
						//int ret = push_filter(pRecFrame,filt_frame);
						//sws_scale(yuv420p_convert_ctx, (const uint8_t* const*)pPreviewFrame->data, pPreviewFrame->linesize, 0, 
						//		pFormatCtx_Out->streams[VideoIndex]->codec->height, pRecFrame->data, pRecFrame->linesize);
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
						//if(ret == 0)
						//{
						//	av_frame_unref(filt_frame);
						//}
					}
				}
				
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
		if(!bStartRecord) // ¼��δ��ʼ��˯��.
		{
			//WaitForSingleObject(gAudioHandle,INFINITE);
		}
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
	SetEvent(gAudioHandle); //������Ƶ�ɼ�.
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
		
		if(_av_compare_ts(cur_pts_v, pFormatCtx_Out->streams[VideoIndex]->time_base, 
			cur_pts_a,pFormatCtx_Out->streams[AudioIndex]->time_base) <= 0)
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
				pEncFrame->pts = VideoFrameIndex * ((pFormatCtx_Video->streams[0]->time_base.den / pFormatCtx_Video->streams[0]->time_base.num) / FPS);
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
					pkt.pts = av_rescale_q_rnd(pkt.pts, pFormatCtx_Video->streams[0]->time_base, 
						pFormatCtx_Out->streams[VideoIndex]->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));  
					pkt.dts = av_rescale_q_rnd(pkt.dts,  pFormatCtx_Video->streams[0]->time_base, 
						pFormatCtx_Out->streams[VideoIndex]->time_base, (AVRounding)(AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));  

					pkt.duration = ((pFormatCtx_Out->streams[0]->time_base.den / pFormatCtx_Out->streams[0]->time_base.num) / FPS);

					cur_pts_v = pkt.pts;
					//д��һ��packet.
					ret = av_interleaved_write_frame(pFormatCtx_Out, &pkt);

					av_free_packet(&pkt);
				}
				VideoFrameIndex++;
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
				frame = av_frame_alloc();
				frame->nb_samples = pFormatCtx_Out->streams[AudioIndex]->codec->frame_size>0 ? pFormatCtx_Out->streams[AudioIndex]->codec->frame_size: 1024;
				frame->channel_layout = pFormatCtx_Out->streams[AudioIndex]->codec->channel_layout;
				frame->format = pFormatCtx_Out->streams[AudioIndex]->codec->sample_fmt;
				frame->sample_rate = pFormatCtx_Out->streams[AudioIndex]->codec->sample_rate;
				av_frame_get_buffer(frame, 0);

				EnterCriticalSection(&AudioSection);
				av_audio_fifo_read(fifo_audio, (void **)frame->data, 
					(pFormatCtx_Out->streams[AudioIndex]->codec->frame_size > 0 ? pFormatCtx_Out->streams[AudioIndex]->codec->frame_size : 1024));
				LeaveCriticalSection(&AudioSection);

				if (pFormatCtx_Out->streams[0]->codec->sample_fmt != pFormatCtx_Audio->streams[AudioIndex]->codec->sample_fmt 
					|| pFormatCtx_Out->streams[0]->codec->channels != pFormatCtx_Audio->streams[AudioIndex]->codec->channels 
					|| pFormatCtx_Out->streams[0]->codec->sample_rate != pFormatCtx_Audio->streams[AudioIndex]->codec->sample_rate)
				{
					//���������������Ƶ��ʽ��һ�� ��Ҫ�ز�����������һ���ľ�û��
				}

				AVPacket pkt_out;
				av_init_packet(&pkt_out);
				int got_picture = -1;
				pkt_out.data = NULL;
				pkt_out.size = 0;

				frame->pts = AudioFrameIndex * pFormatCtx_Out->streams[AudioIndex]->codec->frame_size;
				if (avcodec_encode_audio2(pFormatCtx_Out->streams[AudioIndex]->codec, &pkt_out, frame, &got_picture) < 0)
				{
					av_log(NULL,AV_LOG_ERROR,"can not decoder a frame");
				}
				av_frame_free(&frame);
				if (got_picture) 
				{
					pkt_out.stream_index = AudioIndex;
					pkt_out.pts = AudioFrameIndex * pFormatCtx_Out->streams[AudioIndex]->codec->frame_size;
					pkt_out.dts = AudioFrameIndex * pFormatCtx_Out->streams[AudioIndex]->codec->frame_size;
					pkt_out.duration = pFormatCtx_Out->streams[AudioIndex]->codec->frame_size;

					cur_pts_a = pkt_out.pts;
					
					int ret = av_interleaved_write_frame(pFormatCtx_Out, &pkt_out);
					
					av_free_packet(&pkt_out);
				}
				else
				{
					//av_log(NULL,AV_LOG_PANIC,"xxxxxxxxxxxxxwrite audio file failed\r\n");
				}
				AudioFrameIndex++;
			}
		}
	}
	if(pEnc_yuv420p_buf)
	delete[] pEnc_yuv420p_buf;
	if(pRec_yuv420p_buf)
	delete[] pRec_yuv420p_buf;
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
	
	ResetEvent(gAudioHandle);
	while(!recordThreadQuit)
	{
		av_log(NULL,AV_LOG_ERROR,"RecordStop wait quit\r\n");
		Sleep(10);
	}
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
	gOutVideoInfo = *pVideoInfo;
	if (OpenOutPut(filePath,pVideoInfo,pAudioInfo,pSubTitle) < 0)
	{
		av_log(NULL,AV_LOG_ERROR,"open output file failed\r\n");
		return ERR_RECORD_OPEN_FILE;
	}
	
	//֪ͨ����Ƶ�ɼ��߳̿�ʼ��������Ƶ���ݵ�¼�����.
	//bStartRecord = true;
	//start record thread
	
	CreateThread( NULL, 0, RecordThreadProc, 0, 0, NULL);
	//SetEvent(gVideoHandle);
	
	//SetEvent(gRecordHandle);
	//����������ļ��Ĳ���������¼��
	return ERR_RECORD_OK;
}
static string getDevicePath(const char* pDevType, const char* pDevName)
{
	char tmpbuf[256] = {0,};
	_snprintf(tmpbuf,256,"%s=%s",pDevType,pDevName);
	return GBKToUTF8(tmpbuf);
}
static FILE *report_file;
static int report_file_level = AV_LOG_DEBUG;
#define va_copy(dst, src) ((dst) = (src))
static void log_callback_report(void *ptr, int level, const char *fmt, va_list vl)
{
    va_list vl2;
    char line[1024];
    static int print_prefix = 1;

    va_copy(vl2, vl);
    av_log_default_callback(ptr, level, fmt, vl);
    av_log_format_line(ptr, level, fmt, vl2, line, sizeof(line), &print_prefix);
    va_end(vl2);
    if (report_file_level >= level) {
        fputs(line, report_file);
        fflush(report_file);
    }
}
struct log_def{
	const char* desc;
	int level;
};
static struct log_def loglist[] = {
	{"quiet",AV_LOG_QUIET},
	{"debug",AV_LOG_DEBUG},
	{"verbose",AV_LOG_VERBOSE},
	{"info",AV_LOG_INFO},
	{"warning",AV_LOG_WARNING},
	{"error",AV_LOG_ERROR},
	{"fatal",AV_LOG_FATAL},
	{"panic",AV_LOG_PANIC},
	
};
#define ARRAY_SIZE(a,b) (sizeof(a)/sizeof(b))
static int parse_log_level(const char* logdesc)
{
	
	for(int i = 0; i < ARRAY_SIZE(loglist,struct log_def);i++)
	{
		if(!strcmp(logdesc, loglist[i].desc))
		{
			return loglist[i].level;
		}
	}
	return AV_LOG_QUIET;
}
static int init_report_file(const char* filename)
{
	char buf[32] = {0,};
	const char* inifile="c:\\record.ini";
	char ini_path[MAX_PATH];
	
	if(GetCurrentDirectory(MAX_PATH, ini_path) < 0) return 0;
	strcat(ini_path,"\\record.ini");

	int num = GetPrivateProfileString("log","enable","0", buf,sizeof(buf), ini_path);
	if(num < 0) return 0;
	if( 0 != strcmp(buf,"1"))
	{
		return 0;
	}
	memset(buf,0,32);
	num = GetPrivateProfileString("log","level","quiet", buf,sizeof(buf), ini_path);
	
	report_file_level = parse_log_level(buf);
	
	report_file = fopen(filename, "w");
    if (!report_file) {
        int ret = AVERROR(errno);
        av_log(NULL, AV_LOG_ERROR, "Failed to open report \"%s\": %s\n",
               filename, strerror(errno));
        return 1;
    }
	av_log_set_callback(log_callback_report);
	av_log(NULL,AV_LOG_ERROR,"¼��ģ����سɹ�");
	return 0;
}


int  SDK_CallMode   CloudWalk_OpenDevices(
													const char* pVideoDevice,
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
	
	AVInputFormat *pDShowInputFmt = av_find_input_format("dshow");
	if(pDShowInputFmt == NULL)
	{
		av_log(NULL,AV_LOG_ERROR,"open dshow failed\r\n");
		return ERR_RECORD_DSHOW_OPEN;
	}
	
	if (OpenVideoCapture(getDevicePath("video",pVideoDevice).c_str() ,pDShowInputFmt) < 0)
	{
		av_log(NULL,AV_LOG_ERROR,"open video failed\r\n");
		return ERR_RECORD_VIDEO_OPEN;
	}
	if (OpenAudioCapture(getDevicePath("audio",pAudioDevice).c_str(),pDShowInputFmt) < 0)
	{
		av_log(NULL,AV_LOG_ERROR,"open audio failed\r\n");
		return ERR_RECORD_AUDIO_OPEN;
	}
#ifdef DRAW_TEXT
	if(init_filters(filter_descr) < 0)
	{
		av_log(NULL,AV_LOG_ERROR,("init_filters failed\r\n");
		return -4;
	}
#endif
	//�źų�ʼ��Ϊ���źţ�Ȼ���źŲ�������Ҫ�ֹ���λ�������ź�һֱ��Ч.
	gAudioHandle = CreateEvent(NULL,TRUE,FALSE,NULL);
	gVideoHandle = CreateEvent(NULL,TRUE,FALSE,NULL);
	gRecordHandle = CreateEvent(NULL,TRUE,FALSE,NULL);
	InitializeCriticalSection(&VideoSection);
	InitializeCriticalSection(&AudioSection);

	//������Ҫת����Ŀ���ʽΪRGB24, �ߴ����Ԥ��ͼ��Ĵ�С.
	rgb24_convert_ctx = sws_getContext(pCodecCtx_Video->width, pCodecCtx_Video->height, pCodecCtx_Video->pix_fmt, 
		pCodecCtx_Video->width, pCodecCtx_Video->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL); 
	
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
			//str.copy(pStrDevices[i],str.size());
			av_log(NULL,AV_LOG_INFO,"video[%d]=%s\r\n",i+1,str.c_str());
		}
		*devCount =  videoDevices.size();
	}
	else
	{
		for(int i = 0; i < audioDevices.size(); i++)
		{
			std::string str = ws2s(audioDevices[i]);
			//str.copy(pStrDevices[i],str.size());
			strcpy(pStrDevices[i],str.c_str());
			av_log(NULL,AV_LOG_INFO,"audio[%d]=%s\r\n",i+1,str.c_str());
		}
		*devCount =  audioDevices.size();
	}
	return pStrings;
	//return (char**)&pStrDevices[0][0];

}
int my_Video_Callback(unsigned char* rgb24, int width, int height)
{
	return 0;
}
#ifndef COULDWALKRECORDER_EXPORTS
int _tmain(int argc, _TCHAR* argv[])
{
	int num = 0;
	std::string sVideoDev, sAudioDev;
	char** pDev = CloudWalk_ListDevices(0,&num);
	for(int i = 0 ; i < num ; i++)
	{
		printf("video[%d]=%s\r\n",i+1, pDev[i]);
	}
	sVideoDev = pDev[0];
	pDev = CloudWalk_ListDevices(1,&num);
	for(int i = 0 ; i < num ; i++)
	{
		printf("audio[%d]=%s\r\n",i+1, pDev[i]);
	}
	sAudioDev = pDev[0];
	CloudWalk_OpenDevices(sVideoDev.c_str(), sAudioDev.c_str(), 640,480,25,44100,2,my_Video_Callback);
	VideoInfo v;
	AudioInfo a;
	v.width = 640;
	v.height = 480;
	v.bitrate = 1024*1024;
	v.fps = 10;
	a.bitrate = 192000;
	a.channle = 2;
	//Sleep(10000);
	CloudWalk_RecordStart("hello1.mp4",&v,&a,NULL);
	Sleep(5000);
	//CloudWalk_RecordStop();
	//CloudWalk_RecordStart("hello2.mp4",&v,&a,NULL);
	//Sleep(30000);
	//CloudWalk_RecordStop();
	//getchar();
	CloudWalk_CloseDevices();

	return 0;
}
#endif