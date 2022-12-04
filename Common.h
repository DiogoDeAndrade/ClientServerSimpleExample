#ifndef __COMMON_H
#define __COMMON_H

#pragma once

// Includes on Windows have to be different than OSX/*nix systems
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>

// Common includes
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#define LISTEN_PORT		10000

typedef struct
{
	int	msg_type;
	int msg_length;
} MessageBase;

typedef struct
{
	int		msg_type;
	int		msg_length;
	char	nickname[64];
} MessageSetNick;

typedef struct
{
	int		msg_type;
	int		msg_length;
	char	text[1024];
} MessageTextFromClient;

typedef struct
{
	int		msg_type;
	int		msg_length;
	char	nickname[64];
	char	text[1024];
} MessageTextFromServer;

typedef struct
{
	int	msg_type;
	int msg_length;
} MessageExit;

#define MSG_BASE				0
#define MSG_SETNICK				1
#define MSG_TEXT_FROM_CLIENT	2
#define MSG_TEXT_FROM_SERVER	3
#define MSG_EXIT				4

#endif