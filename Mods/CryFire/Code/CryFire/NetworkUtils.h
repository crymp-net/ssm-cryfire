//================================================================================
// File:    Code/CryFire/NetworkUtils.h
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


#ifndef NETWORK_UTILS_INCLUDED
#define NETWORK_UTILS_INCLUDED


#include <windows.h>
#pragma comment(lib,"user32")
#pragma comment(lib,"Advapi32") // links this DLL with windows dynamic libraries
#pragma comment(lib,"WS2_32")


typedef unsigned int uint;
typedef int fd_t;
typedef unsigned long in_addr_t;


//----------------------------------------------------------------------------------------------------
class NetworkUtils {

  public:

	static bool GetIPFromHost(const char * hostName, char * outIP);
	static bool GetLocalIP(char * outIP);
	static bool IsLocalhost(const char * IPAddr);
	static bool IsInLAN(const char * IPAddr);
	static fd_t prepareSocket();
	static bool getSockaddr(const char * hostName, uint port, struct sockaddr_in * outAddr);
	static bool setTimeout(fd_t sockFd, uint milisecs);

  protected:

	static struct hostent * getHostent(const char * hostName);

};

#endif // NETWORK_UTILS_INCLUDED
