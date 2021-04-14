#include <stdio.h>
#include <stdlib.h>
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>

struct video_param
{
    unsigned int width;
    unsigned int height;
    unsigned int fps;
    unsigned int file_or_stream_flag;
    const char *file_or_stream_url;
};

struct video
{
    AVCodec *encoder;
    AVCodecContext *encoder_context;
    AVFormatContext *out_format_context;
    struct SwsContext *sws_context;
    AVFrame *yuv_frame;
    AVFrame *rgb_frame;
    AVPacket *pkt;
    unsigned int width;
    unsigned int height;
    unsigned int fps;
    unsigned int file_or_stream_flag;
    const char *file_or_stream_url;
    int pts;
};

int video_param_malloc(struct video_param **_video_param);
void video_param_init(struct video_param *_video_param);
int video_param_free(struct video_param **_video_param);
int video_malloc(struct video **_video);
int video_init(struct video *_video, struct video_param *_video_param);
int rgb_to_yuv(struct video *_video, unsigned char *rgb_buf);
void generate_rgb(struct video *_video, uint8_t *rgb);
int write_trail(struct video *_video);
int video_free(struct video **_video);
