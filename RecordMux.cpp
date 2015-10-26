#ifdef	__cplusplus
extern "C"
{
#endif
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavdevice/avdevice.h"
#include "libavutil/time.h"
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

#include "Utils.h"
#include "Recorder.h"
#include "RecordMux.h"
#include "VideoCapture.h"
#include "AudioCapture.h"
#include "RateCtrl.h"

static AVFrame* pAudioFrame = NULL;
static AVFrame* pEncFrame = NULL;
static AVFrame* pRecFrame = NULL;

SwrContext *audio_swr_ctx = NULL;

MyFile file1;
MyFile file2;
/*
¼���ʱ��Ż�ָ�������Ⱥ͸߶ȣ����ʣ���������,֡��
���б������Ĳ���(gop�ȣ�
��������ļ�����ز���.
��Ҫ�Ƿ�������ļ���FormatContext������2����������������ز������򿪶��ڵı�����.
*/
int RecordMux::OpenOutPut(const char* outFileName,VideoInfo* pVideoInfo, AudioInfo* pAudioInfo,SubTitleInfo* pSubTitle)
{
	AVStream *pVideoStream = NULL, *pAudioStream = NULL;

	//Ϊ����ļ�����FormatContext
	avformat_alloc_output_context2(&pFmtContext, NULL, NULL, outFileName); //����������ú�pFormatCtx_Out->oformat�оͲ³�����Ŀ�������
	//������Ƶ����������Ƶ��������ʼ��.
	//if (pFormatCtx_Video->streams[0]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
	{
		VideoIndex = 0;
		//Ϊ����ļ��½�һ����Ƶ��,�����ɹ���pFormatCtx_Out�е�streams��Ա���Ѿ����ΪpVideoStream��.
		pVideoStream = avformat_new_stream(pFmtContext, NULL);

		if (!pVideoStream)
		{
			av_log(NULL,AV_LOG_ERROR,"can not new video stream for output!\n");
			avformat_free_context(pFmtContext);
			return -2;
		}

		//set codec context param
#ifdef H264_ENC
		pVideoStream->codec->codec  = avcodec_find_encoder(AV_CODEC_ID_H264); //��Ƶ���ı�����ΪMPEG4
#else
		if(pFmtContext->oformat->video_codec == AV_CODEC_ID_H264)
		{
			pFmtContext->oformat->video_codec = AV_CODEC_ID_MPEG4;
		}
		pVideoStream->codec->codec =  avcodec_find_encoder(pFmtContext->oformat->video_codec);
		
#endif

		//open encoder
		if (!pVideoStream->codec->codec)
		{
			av_log(NULL,AV_LOG_ERROR,"can not find the encoder!\n");
			return -3;
		}

		pVideoStream->codec->height = pVideoInfo->height; //����ļ���Ƶ���ĸ߶�
		pVideoStream->codec->width  = pVideoCaps.size()*pVideoInfo->width;  //����ļ���Ƶ���Ŀ��
		//pVideoStream->codec->width  = pVideoInfo->width; 
		
		//¼���֡��������ڲɼ���֡�ʣ���¼����Ƶ�����ظ�����Ƶ֡
		//¼���֡��������ڲɼ���֡�ʣ���¼����Ƶ�лᶪ���ɼ���֡�ʣ��������ֶ������Ǿ��Ȼ��ģ�����ɼ�15fps��¼��10fps�������һ���֣�¼��10֡������5֡.
		pVideoStream->codec->time_base = pVideoCaps[0]->GetVideoTimeBase(); //����ļ���Ƶ���ĸ߶Ⱥ������ļ���ʱ��һ��.
		pVideoStream->time_base = pVideoCaps[0]->GetVideoTimeBase();
		pVideoStream->codec->sample_aspect_ratio = pVideoCaps[0]->Get_aspect_ratio();
		// take first format from list of supported formats
		//�ɲ鿴ff_mpeg4_encoder��ָ���ĵ�һ�����ظ�ʽ������ǲ���MPEG4�����ʱ��������Ƶ֡�����ظ�ʽ��������YUV420P
		pVideoStream->codec->pix_fmt = pFmtContext->streams[VideoIndex]->codec->codec->pix_fmts[0]; //���ظ�ʽ������MPEG4֧�ֵĵ�һ����ʽ
		
		//CBR_Set(pVideoStream->codec, pVideoInfo->bitrate); //���ù̶�����
		VBR_Set(pVideoStream->codec, pVideoInfo->bitrate, 2*pVideoInfo->bitrate  , 0);
		
		if (pFmtContext->oformat->flags & AVFMT_GLOBALHEADER)
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
	//if(pFormatCtx_Audio->streams[0]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
	{
		AVCodecContext *pOutputCodecCtx = NULL;
		AudioIndex = 1;
		pAudioStream = avformat_new_stream(pFmtContext, NULL);
		//��avformat_alloc_output_context2 �о��ҵ��� pFormatCtx_Out->oformat�������ļ��ĺ�׺��ƥ�䵽��AVOutputFormat ff_mp4_muxer
		//    .audio_codec       = AV_CODEC_ID_AAC 
		
		//pAudioStream->codec->codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
		pAudioStream->codec->codec = avcodec_find_encoder(pFmtContext->oformat->audio_codec);

		pOutputCodecCtx = pAudioStream->codec;

		int i = 0;
		bool find = false;
		if(pOutputCodecCtx->codec->sample_fmts)
		{
			pOutputCodecCtx->sample_fmt = pOutputCodecCtx->codec->sample_fmts[0];
			while(pOutputCodecCtx->codec->sample_fmts[i++] != -1)
			{
				if(pOutputCodecCtx->codec->sample_fmts[i] == pAudioCap->GetCodecContext()->sample_fmt )
				{
					pOutputCodecCtx->sample_fmt = pOutputCodecCtx->codec->sample_fmts[i];
					break;
				}
			}
		}
		else
		{
			pOutputCodecCtx->sample_fmt = AV_SAMPLE_FMT_FLTP;
		}
		pOutputCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16P;
		pOutputCodecCtx->bit_rate = 192000;//pAudioInfo->bitrate;
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
				//if (pOutputCodecCtx->codec->channel_layouts[i] == pAudioCap->GetCodecContext()->channel_layout)
				//	pOutputCodecCtx->channel_layout = pAudioCap->GetCodecContext()->channel_layout;
            }
        }
	
		pOutputCodecCtx->channels        = av_get_channel_layout_nb_channels(pOutputCodecCtx->channel_layout);
       
		AVRational time_base={1, pAudioStream->codec->sample_rate};
		pAudioStream->time_base = time_base; //����
		
		pOutputCodecCtx->codec_tag = 0;  
		//mpeg4    .flags             = AVFMT_GLOBALHEADER | AVFMT_ALLOW_FLUSH | AVFMT_TS_NEGATIVE,
		if (pFmtContext->oformat->flags & AVFMT_GLOBALHEADER)  
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
	if (!(pFmtContext->oformat->flags & AVFMT_NOFILE))
	{
		if(avio_open(&pFmtContext->pb, outFileName, AVIO_FLAG_WRITE) < 0)
		{
			av_log(NULL,AV_LOG_ERROR,"can not open output file handle!\n");
			return -6;
		}
	}
	//д���ļ�ͷ
	if(avformat_write_header(pFmtContext, NULL) < 0)
	{
		av_log(NULL,AV_LOG_ERROR,"can not write the header of the output file!\n");
		return -7;
	}	
	pRecFrame = alloc_picture(AV_PIX_FMT_YUV420P,pVideoInfo->width,pVideoInfo->height,16);

	return 0;
}
AVCodecContext* RecordMux::GetCodecCtx()
{
	return pFmtContext->streams[VideoIndex]->codec;
}

//¼���߳�.
DWORD WINAPI RecordThreadProc( LPVOID lpParam )
{
	
	if(lpParam != NULL)
	{
		RecordMux* rm = (RecordMux*)lpParam;
		rm->Run();
	}

	return 0;
}
RecordMux::RecordMux()
{
	InitializeCriticalSection(&VideoSection);
	pVideoCaps.clear();

	pAudioCap = new AudioCap();
	
	pVideoCaps.push_back(new VideoCap(pVideoCaps.size()));
	pVideoCaps.push_back(new VideoCap(pVideoCaps.size()));


	bStartRecord = false;
	bCapture = false;
	recordThreadQuit = true;
	cur_pts_v = cur_pts_a = 0;
	pDShowInputFmt = NULL;

	
}
/*
����¼�����
*/
int RecordMux::Start(const char* filePath,VideoInfo* pVideoInfo, AudioInfo* pAudioInfo,SubTitleInfo* pSubTitle)
{
	if(bStartRecord) return 0;
	int ret = OpenOutPut(filePath,pVideoInfo,pAudioInfo,pSubTitle);
	for(int i = 0; i< pVideoCaps.size();i++)
	{
		pVideoCaps[i]->StartRecord(AV_PIX_FMT_YUV420P,  pVideoInfo->width, pVideoInfo->height);
	}
	
	pRecFrame = alloc_picture(AV_PIX_FMT_YUV420P,pVideoInfo->width*2,pVideoInfo->height,16);

	//pAudioCap->StartRecord();
	pAudioCap->StartRecord(pFmtContext->streams[AudioIndex]->codec);
	CreateThread( NULL, 0, RecordThreadProc, this, 0, NULL);
	return ret;
}
/*
ֹͣ¼�����
*/
int RecordMux::Stop()
{
	//�����û������¼�񣬾�ֱ�ӷ���.
	if(false == bStartRecord)
	{
		return ERR_RECORD_OK;
	}
	bStartRecord = false;
	for(int i = 0; i < pVideoCaps.size();i++)
	{
		pVideoCaps[i]->StopRecord();
	}
	
	pAudioCap->StopRecord();
	while(!recordThreadQuit)
	{
		av_log(NULL,AV_LOG_ERROR,"RecordStop wait quit\r\n");
		Sleep(10);
	}
	return 0;
}
/*
�ر������豸.
*/
int RecordMux::Close()
{
	pAudioCap->Close();

	for(int i = 0; i < pVideoCaps.size();i++)
	{
		pVideoCaps[i]->Close();
	}

	return 0;
}
RecordMux::~RecordMux()
{
	for(int i = 0; i < pVideoCaps.size();i++)
	{
		if(pVideoCaps[i]!=NULL)
			pVideoCaps[i]->Close();
		delete pVideoCaps[i];
	}
	pVideoCaps.clear();
	if(pAudioCap) delete pAudioCap;
	pAudioCap;
}

AVFrame* RecordMux::MergeFrame(AVFrame* frame1, AVFrame* frame2)
{
	int offset = 0;
	int i = 0;
	
	//file1.ReadYUV420P(frame1);
	//file2.ReadYUV420P(frame2);
	if(frame2==NULL) return frame1;
	offset = 0;
	for( i = 0; i < frame1->height; i++)
	{
		memcpy(pRecFrame->data[0]+offset, frame1->data[0]+i*frame1->width, frame1->width);
		offset+=frame1->width;
		memcpy(pRecFrame->data[0]+offset, frame2->data[0]+i*frame2->width, frame2->width);
		offset+=frame2->width;
	}
	offset = 0;
	int w1 = frame1->width/2;
	int w2 = frame2->width/2;

	for( i = 0; i < frame1->height/2; i++)
	{

		memcpy(pRecFrame->data[1]+offset, frame1->data[1]+i*w1, w1);	
		memcpy(pRecFrame->data[2]+offset, frame1->data[2]+i*w1, w1);
		offset+= w1;

		memcpy(pRecFrame->data[1]+offset, frame2->data[1]+i*w2, w2);
		memcpy(pRecFrame->data[2]+offset, frame2->data[2]+i*w2, w2);

		offset+=w2;
		
	}

	return pRecFrame;

}
int RecordMux::choose_output(void)
{
#if 0
 	AVRational r;
	r.den = AV_TIME_BASE;
	r.num = 1;
    int64_t opts = av_rescale_q(pFmtContext->streams[VideoIndex]->cur_dts, pFmtContext->streams[VideoIndex]->time_base,r);
    int64_t opts2 = av_rescale_q(pFmtContext->streams[AudioIndex]->cur_dts, pFmtContext->streams[AudioIndex]->time_base,r);
   
	if(opts < opts2) return VideoIndex;
	else return AudioIndex;
#else
	if(av_compare_ts(cur_pts_v, pFmtContext->streams[VideoIndex]->codec->time_base, 
		cur_pts_a,pFmtContext->streams[AudioIndex]->codec->time_base) <= 0)
		return VideoIndex;
	else 
		return AudioIndex;
#endif
 
}
static double rint(double x)
{
	return x >= 0 ? floor(x + 0.5) : ceil(x - 0.5);
}
static av_always_inline av_const long int lrint(double x)
{
	return rint(x);
}
void RecordMux::Run()
{
	AVFrame* pSecordFrame = NULL;
	AVFrame* ptmp = NULL;
	bStartRecord = true;
	cur_pts_v = cur_pts_a = 0; //��λ����Ƶ��pts
	DWORD video_time_stamp = 0;
	DWORD audio_timestamp = 0;
	VideoFrameIndex = AudioFrameIndex = 0; //��λ����Ƶ��֡��.
	//MyFile file11("1.yuv");
	//MyFile file22("2.yuv");
	//MyFile file33("12.yuv");
	
	   int64_t start_time;
    start_time = av_gettime_relative();

	while(bStartRecord) //������¼���־���Ž���¼�񣬷����˳��߳�
	{

		if(choose_output() == VideoIndex)
		
		{
			AVRational rate;
			int nb_frames = 0;
			int sync_opts = 0;
			rate.num = 15;
			rate.den = 1;

			//double duration = 1/(av_q2d(rate) * av_q2d(GetCodecCtx()->time_base));
			//double delta = 1 + duration;

			
			//cur_pts_v = lrint(cur_pts_v);

			//if((pEncFrame = pVideoCaps[0]->GetAudioMatchFrame(0))!= NULL)
			//ǿ�н�֡�ʸߵ�����ͷ֡�ʽ��ͣ�Ȼ�����GetLastSample�������ﵽͬ��Ч���������Ҽ����5fps������ͷ.
			if( (pEncFrame = pVideoCaps[0]->GetLastSample()) != NULL )
			{
				int got_picture = 0;
				AVPacket pkt;
				AVRational base;
				base.den = AV_TIME_BASE;
				base.num = 1;
				//file11.WriteFrame(pEncFrame);
				
				int64_t fps = av_rescale_q( (av_gettime_relative() - start_time) ,base, GetCodecCtx()->time_base);
				if(cur_pts_v > fps)
				{
					continue;
				}
#if 1
				if(pVideoCaps[1] == NULL) pSecordFrame = NULL;
				else
				{
					pSecordFrame = pVideoCaps[1]->GetMatchFrame(pEncFrame->pts);
				}
#endif			
				video_time_stamp = pEncFrame->pts;
				//ptmp  = pEncFrame;
				ptmp = MergeFrame(pEncFrame,pSecordFrame);
				//file11.WriteFrame(pEncFrame);
				//file22.WriteFrame(pSecordFrame);
				//file33.WriteFrame(pRecFrame);
				
				
				ptmp->pts = cur_pts_v++;
				av_init_packet(&pkt);
				
				pkt.data = NULL;
				pkt.size = 0;
				//����һ֡��Ƶ.
				int ret = avcodec_encode_video2(pFmtContext->streams[VideoIndex]->codec, &pkt, ptmp, &got_picture);
				if(ret < 0)
				{
					//�������,������֡
					av_log(NULL,AV_LOG_ERROR,"encode err\r\n");
					continue;
				}
				
				if ( got_picture == 1 )
				{
 					pkt.stream_index = VideoIndex;
					//�������İ���Pts��dts��ת��������ļ���ָ����ʱ�� .�ڱ����Ϳ��Եó�����pts��dts.
					pkt.flags |= AV_PKT_FLAG_KEY;

					av_packet_rescale_ts(&pkt, pFmtContext->streams[VideoIndex]->codec->time_base, pFmtContext->streams[VideoIndex]->time_base);
				
					//д��һ��packet.
					//av_log(NULL,AV_LOG_ERROR,"video cur_dts=%d\r\n",pFmtContext->streams[VideoIndex]->cur_dts);
					ret = av_interleaved_write_frame(pFmtContext, &pkt);
					//av_log(NULL,AV_LOG_ERROR,"video cur_dts=%d\r\n",pFmtContext->streams[VideoIndex]->cur_dts);
					if(ret == 0)
					{
						//av_log(NULL,AV_LOG_ERROR,"video pts=%d\r\n",cur_pts_v);
					}
					else
					{
						av_log(NULL,AV_LOG_ERROR,"write video failed\r\n");
					}
					av_free_packet(&pkt);
				}
				else
				{
					av_log(NULL,AV_LOG_ERROR,"got  err\r\n");
				}
				
			}
			else
			{
				
			}
		}
		else
		{	
			AVFrame *frame = pAudioCap->GetAudioFrame();
			if(frame != NULL)
			{

				AVPacket pkt_out;
				av_init_packet(&pkt_out);
				int got_picture = -1;
				pkt_out.data = NULL;
				pkt_out.size = 0;

				frame->pts = cur_pts_a;
				
				cur_pts_a+=pFmtContext->streams[AudioIndex]->codec->frame_size;
				if (avcodec_encode_audio2(pFmtContext->streams[AudioIndex]->codec, &pkt_out, frame, &got_picture) < 0)
				{
					av_log(NULL,AV_LOG_ERROR,"can not decoder a frame");
				}
				if (got_picture) 
				{
					pkt_out.stream_index = AudioIndex; //ǧ��Ҫ�ǵü���仰������ᵼ��û����Ƶ��.
					av_packet_rescale_ts(&pkt_out, pFmtContext->streams[AudioIndex]->codec->time_base, pFmtContext->streams[AudioIndex]->time_base);
				
					int ret = av_interleaved_write_frame(pFmtContext, &pkt_out);
					if(ret == 0)
					{
						//av_log(NULL,AV_LOG_PANIC,"write audio ok\r\n");
					
						//av_log(NULL,AV_LOG_ERROR,"audio pts=%d\r\n",cur_pts_v);
						
					}
					else
					{
						av_log(NULL,AV_LOG_ERROR,"write audio failed\r\n");
					}
					
					av_free_packet(&pkt_out);
				}
				else
				{
					av_log(NULL,AV_LOG_PANIC,"xxxxxxxxxxxxxwrite audio file failed\r\n");
				}
				
			}
		}
	}
	av_log(NULL,AV_LOG_ERROR,"video pts=%d audio_pts=%d\r\n",cur_pts_v,cur_pts_a/1152);
	if(pEnc_yuv420p_buf)
	delete[] pEnc_yuv420p_buf;

	if(pEncFrame)
	{
		av_frame_free(&pEncFrame);
	}

	av_write_trailer(pFmtContext);

	avio_close(pFmtContext->pb);
	avformat_free_context(pFmtContext);

	
	recordThreadQuit = true;
	av_log(NULL,AV_LOG_INFO,"app  exit\r\n");
	
}
bool RecordMux::StartCap()
{
#if 0
	for(int i = 0; i < pVideoCaps.size();i++)
	{
		pVideoCaps[i]->Start();

	}
#endif
	return true;
}
#include "CaptureDevices.h"
int RecordMux::GetNum()
{
	return pVideoCaps.size();
}
int RecordMux::OpenCamera2(const char* psDevName,int index,const unsigned  int width,
													const unsigned  int height,
													unsigned  int &FrameRate,AVPixelFormat format, Video_Callback pCbFunc)
{
	int rt = 0;
	if(pDShowInputFmt == NULL)Init();

	
	rt = pVideoCaps[1]->OpenPreview(psDevName,index,pDShowInputFmt,width,height,FrameRate, AV_PIX_FMT_YUV420P,format, pCbFunc);

	if(rt != ERR_RECORD_OK) 
	{
		av_log(NULL,AV_LOG_ERROR,"open camera[1]->%s failed\r\n",psDevName);
		return rt;
	}
	return ERR_RECORD_OK;
}
int RecordMux::OpenCamera(const char* psDevName,int index,const unsigned  int width,
													const unsigned  int height,
													unsigned  int &FrameRate,AVPixelFormat format, Video_Callback pCbFunc)
{
	int rt = 0;
	if(pDShowInputFmt == NULL)Init();

	
	rt = pVideoCaps[0]->OpenPreview(psDevName,index,pDShowInputFmt,width,height,FrameRate, AV_PIX_FMT_YUV420P,format, pCbFunc);

	if(rt != ERR_RECORD_OK) 
	{
		av_log(NULL,AV_LOG_ERROR,"open camera[1]->%s failed\r\n",psDevName);
		return rt;
	}
	return ERR_RECORD_OK;

}
int RecordMux::OpenAudio(const char * psDevName)
{
	if(pDShowInputFmt == NULL)Init();
	return pAudioCap->Open(psDevName,pDShowInputFmt);
}
int RecordMux::Init()
{
#if 0
	MyFile f("111.yuv");
	f.FillTestBuffer();
	
	MyFile f2("222.yuv");
	f2.FillBuffer(0xFF, 320*240*1.5);

	file1.Open("111.yuv",AV_PIX_FMT_YUV420P,320,240,10);
	file2.Open("222.yuv",AV_PIX_FMT_YUV420P,320,240,10);
#endif
	
	pDShowInputFmt = av_find_input_format("dshow");
	return 0;
}