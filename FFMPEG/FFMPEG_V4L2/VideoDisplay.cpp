
#include "define.h"

// ÑÓ³Ùdelay msºóË¢ÐÂvideoÖ¡
void schedule_refresh(MediaState *media, int delay)
{
	SDL_AddTimer(delay, sdl_refresh_timer_cb, media);
}

uint32_t sdl_refresh_timer_cb(uint32_t interval, void *opaque)
{
	SDL_Event event;
	event.type = FF_REFRESH_EVENT;
	event.user.data1 = opaque;
	SDL_PushEvent(&event);
	return 0; /* 0 means stop timer */
}

unsigned char frameRata = 0;

void video_refresh_timer(void *userdata)
{
	MediaState *media = (MediaState*)userdata;
	VideoState *video = media->video;
	
	if (video->stream_index >= 0)
	{
		if (video->videoq->queue.empty())
			schedule_refresh(media, 1);
		else
		{
			video->frameq.deQueue(&video->frame);
			
			bingo_log(("333...%ld\r\n",getDebugTime()));

			schedule_refresh(media,1);


			switch(video->frame->format)
			{
				/*case AV_PIX_FMT_YUV420P:
					SDL_UpdateYUVTexture(video->bmp, NULL,
                             video->frame->data[0], video->frame->linesize[0],
                             video->frame->data[1], video->frame->linesize[1],
                             video->frame->data[2], video->frame->linesize[2]);
					break;*/
				default:
					
					bingo_log(("444...%ld\r\n",getDebugTime()));
					
					sws_scale(video->sws_ctx, (uint8_t const * const *)video->frame->data, video->frame->linesize, 0, 
						video->video_ctx->height, video->displayFrame->data, video->displayFrame->linesize);	
				
					bingo_log(("555...%ld\r\n",getDebugTime()));
					SDL_UpdateTexture(video->bmp, NULL, video->displayFrame->data[0], video->displayFrame->linesize[0]);
					break;
			}
			
						
			//SDL_RenderClear(video->renderer);
			SDL_RenderCopy(video->renderer, video->bmp, &video->rect_crop, &video->rect_scale);	 
			 
			bingo_log(("666...%ld\r\n",getDebugTime()));
			SDL_RenderPresent(video->renderer);
			bingo_log(("777...%ld\r\n",getDebugTime()));
			av_frame_unref(video->frame);
			frameRata++;

		}
	}
	else
	{
		schedule_refresh(media, 1);
	}
}
