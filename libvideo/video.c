#include "video.h"

int video_param_malloc(struct video_param **_video_param)
{
    *_video_param = (struct video_param *)malloc(sizeof(struct video_param));
    if (*_video_param == NULL)
    {
        return -1;
    }
    return 0;
}

void video_param_init(struct video_param *_video_param)
{
    _video_param->width = 1820,
    _video_param->height = 720,
    _video_param->fps = 25,
    _video_param->file_or_stream_flag = 1,
    //_video_param->file_or_stream_url = "/mnt/c/Users/wang-/Pictures/demo.mp4";
    _video_param->file_or_stream_url = "rtmp://192.168.31.66:1935/live/demo.flv";
}

int video_param_free(struct video_param **_video_param)
{
    free(*_video_param);
    return 0;
}

int video_malloc(struct video **_video)
{
    *_video = (struct video *)malloc(sizeof(struct video));
    if (*_video == NULL)
    {
        return -1;
    }
    return 0;
}

int video_init(struct video *_video, struct video_param *_video_param)
{
    _video->height = _video_param->height;
    _video->width = _video_param->width;
    _video->fps = _video_param->fps;
    _video->encoder = NULL;
    _video->encoder_context = NULL;
    _video->out_format_context = NULL;
    _video->sws_context = NULL;
    _video->yuv_frame = NULL;
    _video->rgb_frame = NULL;
    _video->pkt = NULL;
    _video->file_or_stream_flag = _video_param->file_or_stream_flag;
    _video->file_or_stream_url = _video_param->file_or_stream_url;

    av_log(NULL, AV_LOG_INFO, "=======================START========================\n");
    _video->encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (_video->encoder == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "无法找到 H264 编码器\n");
        return -1;
    }
    _video->encoder_context = avcodec_alloc_context3(_video->encoder);
    if (_video->encoder_context == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "无法找到分配编码器上下文\n");
        return -2;
    }

    //设置编码信息
    _video->encoder_context->bit_rate = 5200;
    _video->encoder_context->width = _video->width;
    _video->encoder_context->height = _video->height;
    _video->encoder_context->time_base.num = 1;
    _video->encoder_context->time_base.den = _video->fps;

    _video->encoder_context->framerate.num = _video->fps;
    _video->encoder_context->framerate.den = 1;
    _video->encoder_context->gop_size = 5;
    _video->encoder_context->max_b_frames = 2;
    _video->encoder_context->pix_fmt = AV_PIX_FMT_YUV420P;
    _video->encoder_context->codec_id = AV_CODEC_ID_H264;
    _video->encoder_context->codec_type = AVMEDIA_TYPE_VIDEO;
    _video->encoder_context->thread_count = 8;
    _video->encoder_context->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    //打开编码器
    int ret = avcodec_open2(_video->encoder_context, _video->encoder, NULL);
    if (ret < 0)
    {
        char *err = av_err2str(ret);
        av_log(NULL, AV_LOG_ERROR, "打开编码器异常:%s", err);
        return -1;
    }

    //獲取重采樣上下文
    _video->sws_context = sws_getCachedContext(_video->sws_context, _video->width,
                                               _video->height, AV_PIX_FMT_RGB24,
                                               _video->width,
                                               _video->height, AV_PIX_FMT_YUV420P,
                                               SWS_BICUBIC, NULL, NULL, NULL);

    if (_video->sws_context == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "重采樣context打開失敗\n");
        return -1;
    }

    //分配yuv frame
    _video->yuv_frame = av_frame_alloc();
    if (_video->yuv_frame == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "yuv 分配失敗\n");
        return -1;
    }
    _video->yuv_frame->format = AV_PIX_FMT_YUV420P;
    _video->yuv_frame->width = _video->width;
    _video->yuv_frame->height = _video->height;
    ret = av_frame_get_buffer(_video->yuv_frame, 32);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "av_frame_get_buffer  failed:%s\n", av_err2str(ret));
        return -1;
    }

    _video->rgb_frame = av_frame_alloc();
    if (_video->rgb_frame == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "rgb 分配失敗\n");
        return -1;
    }
    _video->rgb_frame->format = _video->encoder_context->pix_fmt;
    _video->rgb_frame->width = _video->encoder_context->width;
    _video->rgb_frame->height = _video->encoder_context->height;
    ret = av_image_alloc(_video->rgb_frame->data, _video->rgb_frame->linesize,
                         _video->encoder_context->width,
                         _video->encoder_context->height, _video->encoder_context->pix_fmt, 32);
    if (ret < 0)
    {
        fprintf(stderr, "Could not allocate raw picture buffer\n");
        return -1;
    }

    if (_video->file_or_stream_flag == 1)
    {
        avformat_alloc_output_context2(&_video->out_format_context, NULL, NULL, _video->file_or_stream_url);
        AVStream *video_stream = avformat_new_stream(_video->out_format_context, NULL);
        video_stream->id = 0;
        video_stream->codecpar->codec_tag = 0;
        video_stream->time_base.num = 1;
        video_stream->time_base.den = _video->fps;
        video_stream->avg_frame_rate.num = _video->fps;
        video_stream->avg_frame_rate.den = 1;
        avcodec_parameters_from_context(video_stream->codecpar, _video->encoder_context);

        av_log(NULL, AV_LOG_INFO, "========================av输出=======================\n");
        av_dump_format(_video->out_format_context, 0, _video->file_or_stream_url, 1);
        av_log(NULL, AV_LOG_INFO, "========================av输出=======================\n");

        AVDictionary *param = NULL;
        //H.264
        if (_video->encoder->id == AV_CODEC_ID_H264)
        {
            av_dict_set(&param, "preset", "medium", 0);
            av_dict_set(&param, "tune", "zerolatency", 0);
        }
        //H.265
        if (_video->encoder->id == AV_CODEC_ID_H265 || _video->encoder->id == AV_CODEC_ID_HEVC)
        {
            av_dict_set(&param, "x265-params", "crf=25", 0);
            av_dict_set(&param, "preset", "fast", 0);
            av_dict_set(&param, "tune", "zero-latency", 0);
        }

        if (!(_video->out_format_context->flags & AVFMT_NOFILE))
        {
            ret = avio_open(&_video->out_format_context->pb, _video->file_or_stream_url, AVIO_FLAG_WRITE);
            if (ret < 0)
            {
                fprintf(stderr, "Could not allocate raw picture buffer\n");
                return -1;
            }
        }
        ret = avformat_write_header(_video->out_format_context, NULL);
        if (ret != 0)
        {
            av_log(NULL, AV_LOG_ERROR, "avformat_write_header  failed!\n");
            return -1;
        }
    }
    _video->pkt = av_packet_alloc();
    if (_video->pkt == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "pkt 分配失敗\n");
        return -1;
    }
    av_init_packet(_video->pkt);

    return 0;
}

int rgb_to_yuv(struct video *_video, unsigned char *rgb_buf)
{

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    avpicture_fill((AVPicture *)_video->rgb_frame, rgb_buf,
                   AV_PIX_FMT_RGB24, _video->width,
                   _video->height);
#pragma clang diagnostic pop
    int slice = sws_scale(_video->sws_context, _video->rgb_frame->data,
                          _video->rgb_frame->linesize, 0,
                          _video->height,
                          _video->yuv_frame->data, _video->yuv_frame->linesize);

    if (slice <= 0)
    {
        av_log(NULL, AV_LOG_ERROR, "rgb to yuv 失败\n");
        return -1;
    }

    _video->yuv_frame->pts =
        _video->pts++ * (_video->encoder_context->time_base.num * 5000 / _video->encoder_context->time_base.den);

    int ret = avcodec_send_frame(_video->encoder_context, _video->yuv_frame);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "rgb to yuv rgb_to_yuv 失败\n");
        return -2;
    }

    ret = avcodec_receive_packet(_video->encoder_context, _video->pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        return -3;
    else if (ret < 0)
    {
        fprintf(stderr, "Error during encoding\n");
        return -1;
    }
    ret = av_interleaved_write_frame(_video->out_format_context, _video->pkt);
    if (ret != 0)
    {
        av_log(NULL, AV_LOG_ERROR, "send 失败: %s\n", av_err2str(ret));
        return -2;
    }
    av_packet_unref(_video->pkt);
 
    return 0;
}

int write_trail(struct video *_video)
{
    av_write_trailer(_video->out_format_context);
}

int video_free(struct video **_video)
{
    av_log(NULL, AV_LOG_INFO, "=======================FREE========================\n");
    if ((*_video)->encoder_context != NULL)
    {
        avcodec_close((*_video)->encoder_context);
        avcodec_free_context(&(*_video)->encoder_context);
        (*_video)->encoder_context = NULL;
    }
    if ((*_video)->sws_context != NULL)
    {
        sws_freeContext((*_video)->sws_context);
        (*_video)->sws_context = NULL;
    }
    if ((*_video)->yuv_frame != NULL)
    {
        av_frame_free(&(*_video)->yuv_frame);
        (*_video)->yuv_frame = NULL;
    }
    if ((*_video)->rgb_frame != NULL)
    {
        av_frame_free(&(*_video)->rgb_frame);
        (*_video)->rgb_frame = NULL;
    }
    if ((*_video)->pkt != NULL)
    {
        av_packet_free(&(*_video)->pkt);
        (*_video)->pkt = NULL;
    }
    if ((*_video)->out_format_context != NULL)
    {
        avio_close((*_video)->out_format_context->pb);
        avformat_free_context((*_video)->out_format_context);
        (*_video)->out_format_context = NULL;
    }
    free(*_video);
    return 0;
}

void generate_rgb(struct video *_video, uint8_t *rgb)
{
    int x, y, cur;
    for (y = 0; y < _video->height; y++)
    {
        for (x = 0; x < _video->width; x++)
        {
            cur = 3 * (y * _video->width + x);
            rgb[cur + 0] = 0;
            rgb[cur + 1] = 0;
            rgb[cur + 2] = 0;
            if ((_video->rgb_frame->pts / 25) % 2 == 0)
            {
                if (y < _video->height / 2)
                {
                    if (x < _video->width / 2)
                    {
                        /* Black. */
                    }
                    else
                    {
                        rgb[cur + 0] = 255;
                    }
                }
                else
                {
                    if (x < _video->width / 2)
                    {
                        rgb[cur + 1] = 255;
                    }
                    else
                    {
                        rgb[cur + 2] = 255;
                    }
                }
            }
            else
            {
                if (y < _video->height / 2)
                {
                    rgb[cur + 0] = 255;
                    if (x < _video->width / 2)
                    {
                        rgb[cur + 1] = 255;
                    }
                    else
                    {
                        rgb[cur + 2] = 255;
                    }
                }
                else
                {
                    if (x < _video->width / 2)
                    {
                        rgb[cur + 1] = 255;
                        rgb[cur + 2] = 255;
                    }
                    else
                    {
                        rgb[cur + 0] = 255;
                        rgb[cur + 1] = 255;
                        rgb[cur + 2] = 255;
                    }
                }
            }
        }
    }
}