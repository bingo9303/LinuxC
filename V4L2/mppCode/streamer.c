#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include "rtmp.h"
#include "streamer.h"
#include "v4l2.h"
#include "mpp.h"

MppContext      *mpp_ctx;


_Bool m_process_image(uint8_t *p, int size)
{ 
        return mpp_ctx->process_image(p,size,mpp_ctx);
}

void write_h264_para_file(StreamerContext *ctx)
{
        ctx->mpp_enc_data->fp_output = fopen("/tmp/output.h264", "wb+");
        ctx->mpp_enc_data->write_header(ctx->mpp_enc_data);
        ctx->mpp_enc_data->num_frames = 30;
        ctx->v4l2_ctx->main_loop(ctx->v4l2_ctx);
        ctx->mpp_enc_data->num_frames = 0;
        fclose(ctx->mpp_enc_data->fp_output);
        ctx->mpp_enc_data->fp_output = NULL;
}

int main(int argc,char* argv[])
{
        StreamerContext                 streamer_ctx;
        V4l2Context                     *v4l2_ctx;
        v4l2_ctx                        = alloc_v4l2_context();
        mpp_ctx                         = alloc_mpp_context();
        streamer_ctx.v4l2_ctx           = v4l2_ctx;
        streamer_ctx.mpp_enc_data       = mpp_ctx;
        /*Configure v4l2Context*/
        v4l2_ctx->process_image         = m_process_image;
        v4l2_ctx->force_format          = 1;
        v4l2_ctx->width                 = 1920;
        v4l2_ctx->height                = 1080;
        v4l2_ctx->pixelformat           = V4L2_PIX_FMT_YUYV;
        v4l2_ctx->field                 = V4L2_FIELD_INTERLACED;
        /*Configure MpiEncData*/
        mpp_ctx->width                  = v4l2_ctx->width;
		mpp_ctx->height                 = v4l2_ctx->height;
        mpp_ctx->fps                    = 30;
		mpp_ctx->gop                    = 60;
		mpp_ctx->bps                    = mpp_ctx->width * mpp_ctx->height / 8 * mpp_ctx->fps*2;
        mpp_ctx->write_frame            = NULL;//write_frame;

        /*Begin*/
        v4l2_ctx->open_device("/dev/video0",v4l2_ctx);
        v4l2_ctx->init_device(v4l2_ctx);  
        mpp_ctx->init_mpp(mpp_ctx);
        v4l2_ctx->start_capturing(v4l2_ctx);
        write_h264_para_file(&streamer_ctx);
      //  init_rtmp_streamer(argv[1]);
  
        v4l2_ctx->main_loop(v4l2_ctx);

        mpp_ctx->close(mpp_ctx);
        v4l2_ctx->close(v4l2_ctx);
        /*End*/
        return 0;
}




