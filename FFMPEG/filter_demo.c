#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
 
#define INPUT_WIDTH 848
#define INPUT_HEIGHT 480
#define INPUT_FORMAT AV_PIX_FMT_YUV420P
 
 
static int init_filter_graph(AVFilterGraph **graph, AVFilterContext **src,
                             AVFilterContext **sink)
{
    char *filter_descr = "scale=1696:960";
    char args[512];
    int ret = 0;
    AVFilterGraph *filter_graph;
    AVFilterContext *buffer_ctx;
    AVFilterContext *buffersink_ctx;
    const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_RGB24, AV_PIX_FMT_NONE };
    AVRational time_base = (AVRational){1, 25};
    
    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=0/1",
             INPUT_WIDTH, INPUT_HEIGHT, INPUT_FORMAT,
             time_base.num, time_base.den);
    ret = avfilter_graph_create_filter(&buffer_ctx, buffersrc, "in",
                                       args, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        goto end;
    }
    
    /* buffer audio sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       NULL, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
        goto end;
    }
    
    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
        goto end;
    }
    
    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */
    
    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffer_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;
    
    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;
    
    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_descr,
                                        &inputs, &outputs, NULL)) < 0)
        goto end;
    
    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;
    
    
    *graph = filter_graph;
    *src   = buffer_ctx;
    *sink  = buffersink_ctx;
    
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    
    return ret;
}
 
 
static void scale_video(void)
{
    const char *in_filename = "/Users/zhw/Desktop/resource/sintel_yuv420p_848x480.yuv";
    const char *out_filename = "/Users/zhw/Desktop/result.rgb";
    FILE *in_file = NULL, *out_file = NULL;
    int width = INPUT_WIDTH, height = INPUT_HEIGHT;
    AVFilterGraph *graph;
    AVFilterContext *src, *sink;
    AVFrame *frame;
    int err;
    uint8_t yuv[width * height * 3 / 2];
    
    
    in_file = fopen(in_filename, "rb");
    if (!in_file) {
        printf("open file %s fail\n", in_filename);
        exit(1);
    }
    
    out_file = fopen(out_filename, "wb");
    if (!out_file) {
        printf("open file %s fail\n", out_filename);
        exit(1);
    }
    
    /* 创建frame，存储输入数据 */
    frame  = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Error allocating the frame\n");
        exit(1);
    }
    
    /* 设置filtergraph. */
    err = init_filter_graph(&graph, &src, &sink);
    if (err < 0) {
        fprintf(stderr, "Unable to init filter graph:");
        exit(1);
    }
    
    while (!feof(in_file)) {
        
        /* Set up the frame properties and allocate the buffer for the data. */
        frame->format = INPUT_FORMAT;
        frame->width = width;
        frame->height = height;
        
        err = av_frame_get_buffer(frame, 0);
        if (err < 0)
            exit(1);
        
        //清空
        memset(yuv, 0, width * height * 3 / 2);
        
        //读取Y、U、V
        fread(yuv, 1, width * height * 3 / 2, in_file);
        
        //设置Y、U、V数据到frame
        /* Y */
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                frame->data[0][y * frame->linesize[0] + x] = yuv[y * width + x];
            }
        }
        /* U and V */
        for (int y = 0; y < height/2; y++) {
            for (int x = 0; x < width/2; x++) {
                frame->data[1][y * frame->linesize[1] + x] = yuv[width * height + y * width / 2 + x];
                frame->data[2][y * frame->linesize[2] + x] = yuv[width * height * 5 / 4 + y * width / 2 + x];
            }
        }
        
        //添加frame到buffer，用于转换
        err = av_buffersrc_add_frame(src, frame);
        if (err < 0) {
            av_frame_unref(frame);
            fprintf(stderr, "Error submitting the frame to the filtergraph:");
            exit(1);
        }
        
        //获取输出数据
        while ((err = av_buffersink_get_frame(sink, frame)) >= 0) {
            /*宽高都缩放成了原来的2倍
             写入文件，仅对rgb格式可以这么写。
             如果是yuv格式，需要将frame->data[0]、frame->data[1]、frame->data[2]写入文件或者转换一下*/
            fwrite(frame->data[0], 1, (width * 2) * (height * 2) * 3, out_file);
            av_frame_unref(frame);
        }
        
        if (err == AVERROR(EAGAIN)) {
            /* 需要更多的数据 */
            continue;
        } else if (err == AVERROR_EOF) {
            /* 到文件尾*/
            break;
        } else if (err < 0) {
            /* An error occurred. */
            fprintf(stderr, "Error filtering the data:");
            exit(1);
        }
        
    }
    
    
    avfilter_graph_free(&graph);
    av_frame_free(&frame);
    
    printf("finish\n");
    
}

