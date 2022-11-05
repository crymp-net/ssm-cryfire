//================================================================================
// File:    Code/CryFire/NetworkUtils.cpp
//                 ____                       ____
// Project: SSM  /\  _ `\                    /\  _`\   __
//               \ \ \/\_\    _  __   __  __ \ \ \_/  /\_\    _  __     ___
//                \ \ \/_/_  /\`'__\ /\ \/\ \ \ \  _\ \/_/   /\`'__\  /' __`\
//                 \ \ \_\ \ \ \ \_/ \ \ \_\ \ \ \ \/   /\`\ \ \ \_/ /\  \__/
//                  \ \____/  \ \_\   \/`____ \ \ \_\   \ \_\ \ \_\  \ \_____\
//                   \/___/    \/_/    `/___/\ \ \/_/    \/_/  \/_/   \/____ /
//                                        /\___/
//                                        \/__/
// Created on:  6.2.2014
// Last edited: 7.10.2017
//--------------------------------------------------------------------------------
// Description: utilities for network connection
//--------------------------------------------------------------------------------
// Authors:     Patrick Glatt (HipHipHurra)
//              Jan Broz (Youda008)
//================================================================================


#include "StdAfx.h"

#include "NetworkUtils.h"
#include "CryFire/Logging.h"

#include <windows.h>


//----------------------------------------------------------------------------------------------------
bool NetworkUtils::GetIPFromHost(const char * hostName, char * outIP)
{
	in_addr	         inaddr;
	struct hostent * host;

	host = getHostent(hostName);
	if (!host) {
		return false;
	}
	inaddr.s_addr = *(unsigned long*)host->h_addr;
	strncpy(outIP, inet_ntoa(inaddr), 16);
	outIP[15] = '\0';

	return true;
}

bool NetworkUtils::GetLocalIP(char * outIP)
{
	char hostName [128];
	struct in_addr addr;
	struct hostent * host;

	if (gethostname(hostName, sizeof(hostName)) == -1) {
		CF_AsyncError("failed to get local IP");
		strcpy(outIP, "error");
		return false;
	}

	host = gethostbyname(hostName);
	if (host == NULL) {
		CF_AsyncError("failed to get host by name %s", hostName);
		strcpy(outIP, "error");
		return false;
	}

	memcpy(&addr, host->h_addr_list[0], sizeof(struct in_addr));
	strcpy(outIP, inet_ntoa(addr));
	return true;
}

//----------------------------------------------------------------------------------------------------
bool NetworkUtils::IsLocalhost(const char * IPAddr)
{
	char localIP [16];

	if (strcmp(IPAddr, "127.0.0.1") == 0)
		return true;

	if (!GetLocalIP(localIP))
		return false;

	return strcmp(IPAddr, localIP) == 0;
}

//----------------------------------------------------------------------------------------------------
bool NetworkUtils::IsInLAN(const char * IPAddr)
{
	char localIP [16];
	if (!GetLocalIP(localIP))
		return false;

	const char * ch1 = IPAddr;
	const char * ch2 = localIP;
	// compare first 2 parts of IP until second dot
	while (true) {
		if (!*ch1 || !*ch2)
			return false;
		if (*ch1 != *ch2)
			return false;
		if (*ch1 == '.')
			break;
		ch1++; ch2++;
	}
	ch1++; ch2++;
	while (true) {
		if (!*ch1 || !*ch2)
			return false;
		if (*ch1 != *ch2)
			return false;
		if (*ch1 == '.')
			break;
		ch1++; ch2++;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------
fd_t NetworkUtils::prepareSocket()
{
	fd_t sockFd;

	// create socket
	sockFd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockFd < 0) {
		return -1;
	}

	return sockFd;
}

//----------------------------------------------------------------------------------------------------
bool NetworkUtils::getSockaddr(const char * hostName, uint port, struct sockaddr_in * outAddr)
{
	struct hostent * host;

	host = getHostent(hostName);
	if (!host) {
		return false;
	}

	memset(outAddr, 0, sizeof(*outAddr));
	outAddr->sin_family = AF_INET;
	outAddr->sin_port   = htons((u_short)port);
	memcpy(&outAddr->sin_addr, host->h_addr_list[0], host->h_length);

	return true;
}

//----------------------------------------------------------------------------------------------------
bool NetworkUtils::setTimeout(fd_t sockFd, uint milisecs)
{
	DWORD timeout = milisecs;

	if (setsockopt(sockFd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) != 0) {
		return false;
	}
	return true;
}

//----------------------------------------------------------------------------------------------------
struct hostent * NetworkUtils::getHostent(const char * hostName)
{
	in_addr_t inaddr = inet_addr(hostName);

	if (inaddr != INADDR_NONE)
		return gethostbyaddr((char *)&inaddr, sizeof(inaddr), AF_INET);
	else
		return gethostbyname(hostName);
}
