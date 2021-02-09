#ifndef SDL2_DISPLAY_H
#define SDL2_DISPLAY_H



#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/frame.h>
#include <SDL2/SDL.h>

#include <rockchip/config.h>
#include <rockchip/mpp_err.h>
#include <rockchip/mpp_meta.h>
#include <rockchip/mpp_task.h>
#include <rockchip/rk_mpi.h>
#include <rockchip/vpu_api.h>
#include <rockchip/mpp_buffer.h>
#include <rockchip/mpp_frame.h>
#include <rockchip/mpp_packet.h>
#include <rockchip/rk_mpi_cmd.h>
#include <rockchip/rk_type.h>
#include <rockchip/vpu.h>



#define MAX_CH_NUM		5
#define ENABLE_MMP_DECODE	0


typedef struct
{
	char inputPort;		// 0 ~ 3  port
	char freeze;        // 0 -- off, 1 -- on
	char horizontalFlip; // 0 -- off, 1 -- on
	char rotate; // 0 -- off, 1 -- left rotation, 2 -- right rotation.
	SDL_Rect crop;
	SDL_Rect scale;	
}LayerSetting;



typedef struct
{
	unsigned char layer[MAX_CH_NUM];
	unsigned char alpha[MAX_CH_NUM];
	char alphaFlag[5];				//1-淡入  2-淡出0-不做处理
} OutLayerSetting;


typedef struct
{
    MppCtx          ctx;
    MppApi          *mpi;
    RK_U32          eos;
    char            *buf;

    MppBufferGroup  frm_grp;
    MppBufferGroup  pkt_grp;
    MppPacket       packet;
    size_t          packet_size;
    MppFrame        frame;

    FILE            *fp_input;
    FILE            *fp_output;
    RK_S32          frame_count;
    RK_S32          frame_num;
    size_t          max_usage;
} MpiDecLoopData;


typedef struct _sdl2_display_info_multiple_input
{
/* 输出参数 */
	int outputChNum;
	char* windowsName;
	int windowsSize_width;
	int windowsSize_height;

/* 输入参数 */
	char** inputFile[MAX_CH_NUM];
	int inputSourceNum;	
	int imageSize_width[MAX_CH_NUM];
	int imageSize_height[MAX_CH_NUM];
	char* imageFrameBuffer[MAX_CH_NUM][3];		//一帧图像的buff	
	int pixformat[MAX_CH_NUM];
	int components[MAX_CH_NUM];
	unsigned int oneLineByteSize[MAX_CH_NUM][3];
	int frame_time[MAX_CH_NUM];
	double frame_rate[MAX_CH_NUM];
/* 图层参数 */
	LayerSetting layerInfo[MAX_CH_NUM];
	OutLayerSetting outLayerAlpha;
	int alphaTime;

/*  SDL2 参数 */
	SDL_Window *screen;
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture[MAX_CH_NUM];

/* FFMEPH参数 */
	AVFormatContext *pFormatCtx[MAX_CH_NUM];
	AVCodecContext *pCodecCtx[MAX_CH_NUM];
	AVCodec *pCodec[MAX_CH_NUM];
	AVPacket *packet[MAX_CH_NUM];
	AVFrame *pFrame[MAX_CH_NUM];
	AVFrame *pFrameDecode[MAX_CH_NUM];
	uint8_t *out_buffer[MAX_CH_NUM];
	struct SwsContext *sws_ctx[MAX_CH_NUM];
	int v_stream_idx[MAX_CH_NUM];

#if ENABLE_MMP_DECODE
    // base flow context
    MppCtx mmp_ctx[MAX_CH_NUM];
    MppApi *mmp_mpi[MAX_CH_NUM];

    // input / output
    MppPacket mmp_packet[MAX_CH_NUM];
    MppFrame  mmp_frame[MAX_CH_NUM];

    MpiCmd mmp_mpi_cmd[MAX_CH_NUM];
    MppParam mmp_param[MAX_CH_NUM];
    RK_U32 mmp_need_split[MAX_CH_NUM];
//    MppPollType mmp_timeout = 5;

    // paramter for resource malloc
    RK_U32 mmp_width[MAX_CH_NUM];
    RK_U32 mmp_height[MAX_CH_NUM];
    MppCodingType mmp_type[MAX_CH_NUM];

    // resources
    char *mmp_buf[MAX_CH_NUM];
    size_t mmp_packet_size[MAX_CH_NUM];
    MppBuffer mmp_pkt_buf[MAX_CH_NUM];
    MppBuffer mmp_frm_buf[MAX_CH_NUM];

    MpiDecLoopData mmp_data[MAX_CH_NUM];

#endif
	
}sdl2_display_info;


int init_sdl2_display(sdl2_display_info* sdl2_dev);
void quit_sdl2_display(sdl2_display_info* sdl2_dev);
void sdl2_display_frame(sdl2_display_info* sdl2_dev,int layerCh);
void sdl2_clear_frame(sdl2_display_info* sdl2_dev);
void sdl2_present_frame(sdl2_display_info* sdl2_dev);
void sdl2_SetAlpha(sdl2_display_info* sdl2_dev,int layerCh,unsigned char value);
int sdl2_GetAlpha(sdl2_display_info* sdl2_dev,int layerCh,unsigned char* value);
void sdl2_clear_Texture(sdl2_display_info* sdl2_dev,int layerCh);







#endif

