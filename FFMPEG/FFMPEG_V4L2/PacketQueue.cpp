#include "define.h"

extern bool quit;

PacketQueue::PacketQueue()
{
	int i;
	nb_packets = 0;
	size       = 0;
	
	for(i=0;i<BUFFER_NUM;i++)
	{
		packBuffer[i].useFlag = 0;
		packBuffer[i].pkt = av_packet_alloc();
	}

	mutex      = SDL_CreateMutex();
	cond       = SDL_CreateCond();
}

PacketQueue::~PacketQueue()
{
	int i;
	for(i=0;i<BUFFER_NUM;i++)
	{
		av_packet_free(&packBuffer[i].pkt);		
	}
}



struct _packBuffer* PacketQueue::getEmptyPkt(void)
{
	int i;
	for(i=0;i<BUFFER_NUM;i++)
	{
		if(packBuffer[i].useFlag == 0)
		{
			return &packBuffer[i];
		}
	}
	return NULL;
}

bool PacketQueue::enQueue(const AVPacket *packet)
{
	SDL_LockMutex(mutex);
	struct _packBuffer* temp = getEmptyPkt();
	
	if(temp == NULL)
	{
		temp = queue.front();
		queue.pop();
		av_packet_unref(temp->pkt);
		av_packet_ref(temp->pkt, packet);
		queue.push(temp);
		nb_packets = queue.size();
	}
	else
	{
		av_packet_ref(temp->pkt, packet);
		temp->useFlag = 1;
		size += temp->pkt->size;
		queue.push(temp);
		nb_packets++;
	}
	SDL_CondSignal(cond);
	SDL_UnlockMutex(mutex);


/*
	if(queue.size() >= BUFFER_NUM)
	{
		SDL_LockMutex(mutex);
		AVPacket pkt = queue.front();
		queue.pop();
		
		if (av_packet_ref(&pkt, packet) < 0)
			return false;
		queue.push(pkt);

		nb_packets = queue.size();

		SDL_CondSignal(cond);
		SDL_UnlockMutex(mutex);
	}
	else
	{
		AVPacket *pkt = av_packet_alloc();
		if (av_packet_ref(pkt, packet) < 0)
			return false;

		SDL_LockMutex(mutex);
		queue.push(*pkt);

		size += pkt->size;
		nb_packets++;

		SDL_CondSignal(cond);
		SDL_UnlockMutex(mutex);
	}*/

	
	return true;
}

bool PacketQueue::deQueue(AVPacket *packet, bool block)
{
	bool ret = false;

	SDL_LockMutex(mutex);
	while (true)
	{
		if (quit)
		{
			ret = false;
			break;
		}

		if (!queue.empty())
		{
			struct _packBuffer* temp = queue.front();
			
			av_packet_ref(packet, temp->pkt);
			temp->useFlag = 0;
			queue.pop();
			av_packet_unref(temp->pkt);
			nb_packets--;
			size -= packet->size;
			ret = true;
			break;
		}
		else if (!block)
		{
			ret = false;
			break;
		}
		else
		{
			SDL_CondWait(cond, mutex);
		}
	}
	SDL_UnlockMutex(mutex);
	return ret;
}