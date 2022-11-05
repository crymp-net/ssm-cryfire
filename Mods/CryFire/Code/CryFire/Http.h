//================================================================================
// File:    Code/CryFire/Http.h
//                 ____                       ____
// Project: SSM  /\  _ `\                    /\  _`\   __
//               \ \ \/\_\    _  __   __  __ \ \ \_/  /\_\    _  __     ___
//                \ \ \/_/_  /\`'__\ /\ \/\ \ \ \  _\ \/_/   /\`'__\  /' __`\
//                 \ \ \_\ \ \ \ \_/ \ \ \_\ \ \ \ \/   /\`\ \ \ \_/ /\  \__/
//                  \ \____/  \ \_\   \/`____ \ \ \_\   \ \_\ \ \_\  \ \_____\
//                   \/___/    \/_/    `/___/\ \ \/_/    \/_/  \/_/   \/____ /
//                                        /\___/
//                                        \/__/
// Created on:  20.2.2018
// Last edited: 20.2.2018
//--------------------------------------------------------------------------------
// Description: Asynchronous HTTP requests library
//--------------------------------------------------------------------------------
// Authors:     Patrick Glatt (HipHipHurra)
//              Jan Broz (Youda008)
//================================================================================


#ifndef HTTP_INCLUDED
#define HTTP_INCLUDED


#include <string>
#include <map>

typedef unsigned int uint;
typedef unsigned __int16 uint16_t;

//enum NetworkStatus { SUCCESS=0, SOCKET_FAILED, SET_TIMEOUT_FAILED, CONNECT_FAILED, SEND_FAILED, RECV_FAILED };


//----------------------------------------------------------------------------------------------------
class HTTP {

 public:

	static const uint TIMEOUT = 5000;
	
	typedef std::map<std::string, std::string> Headers;
	typedef void (* ResultCallback)( int netStatus, uint httpStatus, const Headers & respHeaders, const std::string & respData, void * userArg );

	/// performs an asynchronous HTTP GET request and calls you callback when result is ready
	static void Get( const char * hostName, uint16_t port, const char * urlPath,
	                 const Headers & headers, ResultCallback callback, void * callbackArg = NULL );
	/// performs an asynchronous HTTP POST request and calls you callback when result is ready
	static void Post( const char * hostName, uint16_t port, const char * urlPath,
	                  const Headers & headers, const std::string & data, ResultCallback callback, void * callbackArg = NULL );

 protected:

	static void * SendRequest( void * arg );
	static void * HandleResult( void * arg );

};

#endif // HTTP
