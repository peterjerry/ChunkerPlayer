/*
 *  Copyright (c) 2009-2011 Carmelo Daniele, Dario Marchese, Diego Reforgiato, Giuseppe Tropea
 *  developed for the Napa-Wine EU project. See www.napa-wine.eu
 *
 *  This is free software; see lgpl-2.1.txt
 */

#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define TCP_BUF_SIZE 65536

static int listen_port = 0;
static int accept_fd = -1;
static int socket_fd = -1;
static int isRunning = 0;
static int isReceving = 0;
static char listen_path[256];
static pthread_t AcceptThread;
static pthread_t RecvThread;

static void* RecvThreadProc(void* params);
static void* AcceptThreadProc(void* params);

int initChunkPuller(const int port)
{
	struct sockaddr_in servaddr;
	int r;
	int fd;
  
	accept_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		return -1;
	}
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);
	r = bind(accept_fd, (struct sockaddr *)&servaddr, sizeof(servaddr));
	
	printf("listening on port %$d\n", port);
	
	listen(accept_fd, 10);
	
	if(pthread_create( &AcceptThread, NULL, &AcceptThreadProc, NULL) != 0)
	{
		fprintf(stderr,"TCP-INPUT-MODULE: could not start accepting thread!!\n");
		return NULL;
	}
	
	return accept_fd;
}

static void* AcceptThreadProc(void* params)
{
	struct sockaddr_storage their_addr;
    socklen_t addr_size;
    int fd = -1;
    
    isRunning = 1;
    
    while(isRunning)
    {
		printf("trying to accept connection...\n");
		fd = accept(accept_fd, (struct sockaddr *)&their_addr, &addr_size);
		printf("connection requested!!!\n");
		if(socket_fd == -1)
		{
			socket_fd = fd;
			isReceving = 1;
		}
		else
		{
			isReceving = 0;
			pthread_join(RecvThread, NULL);
			pthread_detach(RecvThread);
			socket_fd = fd;
		}
		if(pthread_create( &RecvThread, NULL, &RecvThreadProc, NULL) != 0)
		{
			fprintf(stderr,"TCP-INPUT-MODULE: could not start receveing thread!!\n");
			return NULL;
		}
	}
	
	return NULL;
}

static void* RecvThreadProc(void* params)
{
	int ret = -1;
	int fragment_size = 0;
	uint8_t* buffer = (uint8_t*) malloc(TCP_BUF_SIZE);
	while(isReceving)
	{
		ret=recv(socket_fd, &fragment_size, sizeof(uint32_t), 0);
		if(ret <= sizeof(uint32_t))
			continue;
		
		ret=recv(socket_fd, buffer, fragment_size, 0);
		while(ret <= fragment_size)
			ret+=recv(socket_fd, buffer+ret, fragment_size-ret, 0);
		
		if(enqueueBlock(buffer, fragment_size))
			fprintf(stderr, "TCP-INPUT-MODULE: could not enqueue a received chunk!! \n");
	}
	free(buffer);
	
	return NULL;
}

void finalizeChunkPuller()
{
	isRunning = 0;
	pthread_join(AcceptThread, NULL);
	pthread_detach(AcceptThread);
	
	if(socket_fd > 0)
		close(socket_fd);

	close(accept_fd);
}