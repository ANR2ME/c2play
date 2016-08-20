#pragma once

#include "Codec.h"
#include "Element.h"
#include "OutPin.h"

#include <string>
#include <map>


extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}


class MediaSourceElement : public Element
{
	std::string url;
	AVFormatContext* ctx = nullptr;
	
	int video_stream_idx = -1;
	void* video_extra_data;
	int video_extra_data_size = 0;
	AVCodecID video_codec_id;
	double frameRate;
	AVRational time_base;
	OutPinSPTR videoPin;


	int audio_stream_idx = -1;
	AVCodecID audio_codec_id;
	int audio_sample_rate = 0;
	int audio_channels = 0;
	OutPinSPTR audioPin;

	//std::map<int, OutPinSPTR> streamMap;
	ThreadSafeQueue<BufferSPTR> availableBuffers;
	std::vector<OutPinSPTR> streamList;


	static void PrintDictionary(AVDictionary* dictionary)
	{
		int count = av_dict_count(dictionary);

		AVDictionaryEntry* prevEntry = nullptr;

		for (int i = 0; i < count; ++i)
		{
			AVDictionaryEntry* entry = av_dict_get(dictionary, "", prevEntry, AV_DICT_IGNORE_SUFFIX);

			if (entry != nullptr)
			{
				printf("\tkey=%s, value=%s\n", entry->key, entry->value);
			}

			prevEntry = entry;
		}
	}

	void SetupPins()
	{
		int ret = avformat_find_stream_info(ctx, NULL);
		if (ret < 0)
		{
			throw Exception();
		}

		int streamCount = ctx->nb_streams;
		if (streamCount < 1)
		{
			throw Exception("No streams found");
		}

		for (int i = 0; i < streamCount; ++i)
		{
			streamList.push_back(nullptr);
		}

		printf("Streams (count=%d):\n", streamCount);

		for (int i = 0; i < streamCount; ++i)
		{
			//streamMap[i] = nullptr;


			AVStream* streamPtr = ctx->streams[i];
			AVCodecContext* codecCtxPtr = streamPtr->codec;
			AVMediaType mediaType = codecCtxPtr->codec_type;
			AVCodecID codec_id = codecCtxPtr->codec_id;

			switch (mediaType)
			{
			case AVMEDIA_TYPE_VIDEO:
			{
				VideoPinInfoSPTR info;

				if (video_stream_idx < 0)
				{
					video_stream_idx = i;
					video_extra_data = codecCtxPtr->extradata;
					video_extra_data_size = codecCtxPtr->extradata_size;

					video_codec_id = codec_id;


					frameRate = av_q2d(streamPtr->avg_frame_rate);
					time_base = streamPtr->time_base;

					info = std::make_shared<VideoPinInfo>();
					info->FrameRate = frameRate;
					
					
					ExtraDataSPTR ext = std::make_shared<ExtraData>();
					info->ExtraData = ext;

					// Copy codec extra data
					unsigned char* src = codecCtxPtr->extradata;
					int size = codecCtxPtr->extradata_size;

					for (int j = 0; j < size; ++j)
					{
						ext->push_back(src[j]);
					}
					printf("MediaSourceElement: copied extra data (size=%d)\n", size);
#if 1
					printf("EXTRA DATA = ");

					for (int j = 0; j < size; ++j)
					{
						printf("%02x ", src[j]);

					}

					printf("\n");
#endif

					ElementWPTR weakPtr = shared_from_this();
					videoPin = std::make_shared<OutPin>(weakPtr, info);

					AddOutputPin(videoPin);

					//streamMap[i] = videoPin;
					streamList[i] = videoPin;
				}



				switch (codec_id)
				{
				case CODEC_ID_MPEG2VIDEO:
					printf("stream #%d - VIDEO/MPEG2\n", i);
					if (info)
						info->StreamType = VideoStreamType::Mpeg2;
					break;

				case CODEC_ID_MPEG4:
					printf("stream #%d - VIDEO/MPEG4\n", i);
					if (info)
						info->StreamType = VideoStreamType::Mpeg4;
					break;

				case CODEC_ID_H264:
				{
					printf("stream #%d - VIDEO/H264\n", i);
					if (info)
						info->StreamType = VideoStreamType::Avc;
				}
				break;

				case AV_CODEC_ID_HEVC:
					printf("stream #%d - VIDEO/HEVC\n", i);
					if (info)
						info->StreamType = VideoStreamType::Hevc;
					break;


				case CODEC_ID_VC1:
					printf("stream #%d - VIDEO/VC1\n", i);
					if (info)
						info->StreamType = VideoStreamType::VC1;
					break;

				default:
					printf("stream #%d - VIDEO/UNKNOWN(%x)\n", i, codec_id);
					if (info)
						info->StreamType = VideoStreamType::Unknown;
					//throw NotSupportedException();
				}

				printf("\tfps=%f(%d/%d) ", frameRate,
					streamPtr->avg_frame_rate.num,
					streamPtr->avg_frame_rate.den);

				int width = codecCtxPtr->width;
				int height = codecCtxPtr->height;

				printf("SAR=(%d/%d) ",
					streamPtr->sample_aspect_ratio.num,
					streamPtr->sample_aspect_ratio.den);

				// TODO: DAR

				printf("\n");

			}
			break;

			case AVMEDIA_TYPE_AUDIO:
			{
				AudioPinInfoSPTR info;

				// Use the first audio stream
				if (audio_stream_idx == -1)
				{
					audio_stream_idx = i;
					audio_codec_id = codec_id;

					//info = std::make_shared<AudioPinInfo>();
					//// TODO: fill in info

					//audioPin = std::make_shared<OutPin>(shared_from_this(), info);
					//AddOutputPin(audioPin);

					//streamMap[i] = audioPin;
				}


				switch (codec_id)
				{
				case CODEC_ID_MP3:
					printf("stream #%d - AUDIO/MP3\n", i);
					break;

				case CODEC_ID_AAC:
					printf("stream #%d - AUDIO/AAC\n", i);
					break;

				case CODEC_ID_AC3:
					printf("stream #%d - AUDIO/AC3\n", i);
					break;

				case CODEC_ID_DTS:
					printf("stream #%d - AUDIO/DTS\n", i);
					break;

					//case AVCodecID.CODEC_ID_WMAV2:
					//    break;

				default:
					printf("stream #%d - AUDIO/UNKNOWN (0x%x)\n", i, codec_id);
					//throw NotSupportedException();
					break;
				}

				audio_channels = codecCtxPtr->channels;
				audio_sample_rate = codecCtxPtr->sample_rate;

			}
			break;


			case AVMEDIA_TYPE_SUBTITLE:
			{
				// TODO: Subtitle support

				switch (codec_id)
				{
				case  CODEC_ID_DVB_SUBTITLE:
					printf("stream #%d - SUBTITLE/DVB_SUBTITLE\n", i);
					break;

				case  CODEC_ID_TEXT:
					printf("stream #%d - SUBTITLE/TEXT\n", i);
					break;

				case  CODEC_ID_XSUB:
					printf("stream #%d - SUBTITLE/XSUB\n", i);
					break;

				case  CODEC_ID_SSA:
					printf("stream #%d - SUBTITLE/SSA\n", i);
					break;

				case  CODEC_ID_MOV_TEXT:
					printf("stream #%d - SUBTITLE/MOV_TEXT\n", i);
					break;

				case  CODEC_ID_HDMV_PGS_SUBTITLE:
					printf("stream #%d - SUBTITLE/HDMV_PGS_SUBTITLE\n", i);
					break;

				case  CODEC_ID_DVB_TELETEXT:
					printf("stream #%d - SUBTITLE/DVB_TELETEXT\n", i);
					break;

				case  CODEC_ID_SRT:
					printf("stream #%d - SUBTITLE/SRT\n", i);
					break;


				default:
					printf("stream #%d - SUBTITLE/UNKNOWN (0x%x)\n", i, codec_id);
					break;
				}
			}
			break;


			case AVMEDIA_TYPE_DATA:
				printf("stream #%d - DATA\n", i);
				break;

			default:
				printf("stream #%d - Unknown mediaType (%x)\n", i, mediaType);
				//throw NotSupportedException();
			}

		}
	}


public:
	MediaSourceElement(std::string url)
		: url(url)
	{
		AVDictionary* options_dict = NULL;

		/*
		Set probing size in bytes, i.e. the size of the data to analyze to get
		stream information. A higher value will enable detecting more information
		in case it is dispersed into the stream, but will increase latency. Must
		be an integer not lesser than 32. It is 5000000 by default.
		*/
		av_dict_set(&options_dict, "probesize", "10000000", 0);

		/*
		Specify how many microseconds are analyzed to probe the input. A higher
		value will enable detecting more accurate information, but will increase
		latency. It defaults to 5,000,000 microseconds = 5 seconds.
		*/
		av_dict_set(&options_dict, "analyzeduration", "10000000", 0);

		int ret = avformat_open_input(&ctx, url.c_str(), NULL, &options_dict);
		if (ret < 0)
		{
			printf("avformat_open_input failed.\n");
		}


		printf("Source Metadata:\n");
		PrintDictionary(ctx->metadata);


		//SetupPins();


		//// Chapters
		//int chapterCount = ctx->nb_chapters;
		//printf("Chapters (count=%d):\n", chapterCount);

		//AVChapter** chapters = ctx->chapters;
		//for (int i = 0; i < chapterCount; ++i)
		//{
		//	AVChapter* avChapter = chapters[i];

		//	int index = i + 1;
		//	double start = avChapter->start * avChapter->time_base.num / (double)avChapter->time_base.den;
		//	double end = avChapter->end * avChapter->time_base.num / (double)avChapter->time_base.den;
		//	AVDictionary* metadata = avChapter->metadata;

		//	printf("Chapter #%02d: %f -> %f\n", index, start, end);
		//	PrintDictionary(metadata);

		//	//if (optionChapter > -1 && optionChapter == index)
		//	//{
		//	//	optionStartPosition = start;
		//	//}
		//}


		//if (optionStartPosition > 0)
		//{
		//	if (av_seek_frame(ctx, -1, (long)(optionStartPosition * AV_TIME_BASE), 0) < 0)
		//	{
		//		printf("av_seek_frame (%f) failed\n", optionStartPosition);
		//	}
		//}


		// Create buffers
		for (int i = 0; i < 128; ++i)
		{
			AVPacketBufferPtr buffer = std::make_shared<AVPacketBuffer>((void*)this);
			availableBuffers.Push(buffer);
		}
	}

	virtual void Initialize() override
	{
		ClearOutputPins();

		SetupPins();

		// Chapters
		int chapterCount = ctx->nb_chapters;
		printf("Chapters (count=%d):\n", chapterCount);

		AVChapter** chapters = ctx->chapters;
		for (int i = 0; i < chapterCount; ++i)
		{
			AVChapter* avChapter = chapters[i];

			int index = i + 1;
			double start = avChapter->start * avChapter->time_base.num / (double)avChapter->time_base.den;
			double end = avChapter->end * avChapter->time_base.num / (double)avChapter->time_base.den;
			AVDictionary* metadata = avChapter->metadata;

			printf("Chapter #%02d: %f -> %f\n", index, start, end);
			PrintDictionary(metadata);

			//if (optionChapter > -1 && optionChapter == index)
			//{
			//	optionStartPosition = start;
			//}
		}
	}

	void RetireBuffer(AVPacketBufferSPTR buffer)
	{
		AVPacketBufferPtr newBuffer = std::make_shared<AVPacketBuffer>((void*)this);
		availableBuffers.Push(newBuffer);

		Wake();
	}

	virtual void DoWork() override
	{
		BufferPTR freeBuffer;

		// Reap freed buffers
		for (auto& entry : streamList)
		{
			//printf("MediaElement (%s) DoWork checking pin for reaping.\n", Name().c_str());

			OutPinSPTR pin = entry;
			if (pin)
			{
				//printf("MediaElement (%s) DoWork reaping buffers for pin.\n", Name().c_str());

				while (pin->TryGetAvailableBuffer(&freeBuffer))
				{
					//// Free the memory allocated to the buffers by libav
					AVPacketBufferPTR buffer = std::static_pointer_cast<AVPacketBuffer>(freeBuffer);
					//buffer->Reset();

					//// Reuse the buffer
					//availableBuffers.Push(freeBuffer);
					////printf("MediaElement (%s) DoWork buffer reaped.\n", Name().c_str());

					RetireBuffer(buffer);
				}
			}
		}


		//printf("MediaElement (%s) DoWork availableBuffers count=%d.\n", Name().c_str(), availableBuffers.Count());

		// Process
		while (availableBuffers.TryPop(&freeBuffer))
		{
			//printf("MediaElement (%s) DoWork availableBuffers.TryPop=true.\n", Name().c_str());

			AVPacketBufferPTR buffer = std::static_pointer_cast<AVPacketBuffer>(freeBuffer);

			if (av_read_frame(ctx, buffer->GetAVPacket()) < 0)
			{
				// End of file
				// TODO: Terminate? (return false)
				
				// Free the memory allocated to the buffers by libav
				buffer->Reset();
				availableBuffers.Push(buffer);
				Wake();

				usleep(1);

				//printf("MediaElement (%s) DoWork av_read_frame failed.\n", Name().c_str());
			}
			else
			{
				AVPacket* pkt = buffer->GetAVPacket();

				//printf("MediaElement (%s) DoWork pin[%d] got AVPacket.\n", Name().c_str(), pkt->stream_index);

				if (pkt->pts != AV_NOPTS_VALUE)
				{
					AVStream* streamPtr = ctx->streams[pkt->stream_index];
					buffer->SetTimeStamp(av_q2d(streamPtr->time_base) * pkt->pts);
					buffer->SetTimeBase(streamPtr->time_base);

					//printf("MediaSourceElement: Set buffer timestamp=%f\n", buffer->TimeStamp());
				}

				//AddFilledBuffer(buffer);
				OutPinSPTR pin = streamList[pkt->stream_index];
				if(pin)
				{
					pin->SendBuffer(freeBuffer);
					//printf("MediaElement (%s) DoWork pin[%d] buffer sent.\n", Name().c_str(), pkt->stream_index);
				}
				else
				{
					// Free the memory allocated to the buffers by libav
					//buffer->Reset();

					//availableBuffers.Push(buffer);
					//Wake();

					RetireBuffer(buffer);

					//printf("MediaElement (%s) DoWork pin[%d] = nullptr.\n", Name().c_str(), pkt->stream_index);
				}
			}
		}
	}

	//virtual void Flush() override
	//{
	//}
};