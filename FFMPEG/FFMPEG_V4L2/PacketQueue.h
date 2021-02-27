
#ifndef PACKET_QUEUE_H
#define PACKET_QUEUE_H

#include "define.h"



struct _packBuffer
{
	AVPacket *pkt;
	char useFlag;
};

struct PacketQueue
{
	std::queue<struct _packBuffer*> queue;
	
	Uint32    nb_packets;
	Uint32    size;
	SDL_mutex *mutex;
	SDL_cond  *cond;

	
	struct _packBuffer	packBuffer[BUFFER_NUM];

	PacketQueue();
	~PacketQueue();
	bool enQueue(const AVPacket *packet);
	bool deQueue(AVPacket *packet, bool block);

	struct _packBuffer* getEmptyPkt(void);
	
};

#endif