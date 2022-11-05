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

/* !!TODO!!
	request IDs for user identification
	rewrite to WinAPI
	HTTPS support
*/

#include "StdAfx.h"

#include "Http.h"

#include "CryFire/NetworkUtils.h"
#include "CryFire/AsyncTasks.h"
#include "CryFire/Logging.h"

#include <windows.h>
#include <string>
#include <sstream>
#include <cstdio>

using namespace std;


//----------------------------------------------------------------------------------------------------
enum HttpReqType { GET=0, POST };
static const char * const HttpReqTypeStr [] = { "GET", "POST" };

// !!TODO!! IDs for user identification
struct HttpRequestParams {
	HttpReqType type;
	std::string hostName;
	uint16_t port;
	std::string urlPath;
	HTTP::Headers headers;
	std::string data;
	HTTP::ResultCallback userCallback;
	void * userArg;
};

struct HttpResultParams {
	std::string hostName;
	uint16_t port;
	std::string urlPath;
	int netStatus;
	uint httpStatus;
	HTTP::Headers respHeaders;
	std::string respData;
	HTTP::ResultCallback userCallback;
	void * userArg;
};

static std::string readUntilCRLF( stringstream & is )
{
	ostringstream oss;
	char c;
	bool wasCR = false;
	while (is.get(c)) {
		if (wasCR) {
			if (c == '\n') {
				break;
			} else if (c == '\r') { // write the previous CR but keep wasCR for the current one
				oss << '\r';
			} else {                // write both the previous CR and current character
				oss << '\r' << c;
			}
		} else {
			if (c != '\r') {
				oss << c;
			}
		}
		wasCR = c == '\r';
	}
	return oss.str();
}

static void moveChars( stringstream & in, ostringstream & out, uint maxChars = UINT_MAX )
{
	char c;
	while (in.get(c) && maxChars-- > 0)
		out.put(c);
}

//----------------------------------------------------------------------------------------------------
#define returnError( msgFormat, ... ) {\
	resParams->netStatus = WSAGetLastError();\
	CF_AsyncLog( 7, "$4HTTP: "msgFormat, __VA_ARGS__ );\
	closesocket( sockFd );\
	delete reqParams;\
	return resParams;\
}

#define returnErrorSys( msgFormat, ... ) {\
	resParams->netStatus = WSAGetLastError();\
	CF_AsyncLog( 7, "$4HTTP: "msgFormat" (%d)", __VA_ARGS__, resParams->netStatus );\
	closesocket( sockFd );\
	delete reqParams;\
	return resParams;\
}

void * HTTP::SendRequest( void * arg )
{
	char buffer [1024];
	HttpRequestParams * reqParams = (HttpRequestParams *)arg;
	HttpResultParams * resParams = new HttpResultParams;
	fd_t sockFd = INVALID_SOCKET;
	struct sockaddr_in addr;
	std::string reqStr, protocol, statusMsg, line;
	ostringstream request, respData;
	stringstream response;
	int length;
	Headers::const_iterator transferIt;

	resParams->hostName = reqParams->hostName;
	resParams->port = reqParams->port;
	resParams->urlPath = reqParams->urlPath;
	resParams->userCallback = reqParams->userCallback;
	resParams->userArg = reqParams->userArg;

	resParams->netStatus = 0; // default is no error, when something happens, it's overwritten with Windows error code
	resParams->httpStatus = 0; // default value in case of errors, when request is successfully parsed, it's overwritten with response value

	// establish TCP connection

	CF_AsyncLog( 7, "HTTP: connecting to %s:%u", reqParams->hostName.c_str(), (uint)reqParams->port );
	if ((sockFd = NetworkUtils::prepareSocket()) < 0)
		returnErrorSys( "failed to create TCP socket" );
	if (!NetworkUtils::setTimeout( sockFd, TIMEOUT ))
		returnErrorSys( "failed to set timeout for TCP socket" );
	if (!NetworkUtils::getSockaddr( reqParams->hostName.c_str(), (uint)reqParams->port, &addr ))
		returnErrorSys( "failed to get IP address for hostname %s", reqParams->hostName.c_str() );
	if (connect( sockFd, (sockaddr*)&addr, sizeof(addr) ) == SOCKET_ERROR)
		returnErrorSys( "failed to connect to %s:%u", reqParams->hostName.c_str(), (uint)reqParams->port );

	// build request

	request << HttpReqTypeStr[ reqParams->type ] << ' ' << reqParams->urlPath << ' ' << "HTTP/1.1\r\n";
	// add basic headers
	if (reqParams->headers.find("User-Agent") == reqParams->headers.end())
		reqParams->headers["User-Agent"] = "SSM CryFire HTTP client";
	reqParams->headers["Host"] = reqParams->hostName;
	reqParams->headers["Connection"] = "close";
	if (reqParams->type == POST) {
		if (reqParams->headers.find("Content-Type") == reqParams->headers.end())
			reqParams->headers["Content-Type"] = "application/x-www-form-urlencoded\r\n";
		sprintf( buffer, "%u", reqParams->data.length() );
		reqParams->headers["Content-length"] = buffer;
	}
	// print together with user headers
	for (Headers::const_iterator iter = reqParams->headers.begin(); iter != reqParams->headers.end(); iter++)
		request << iter->first << ": " << iter->second << "\r\n";
	request << "\r\n";

	// send request and receive response

	CF_AsyncLog( 7, "HTTP: sending %s request to %s", HttpReqTypeStr[ reqParams->type ], reqParams->urlPath.c_str() );
	reqStr = request.str();
	// send headers and content separately to prevent copying content string into request stream and then to request string
	if (send( sockFd, reqStr.c_str(), reqStr.length(), 0 ) == SOCKET_ERROR)
		returnErrorSys( "failed to send data through socket" );
	if (reqParams->data.length() > 0)
		if (send( sockFd, reqParams->data.c_str(), reqParams->data.length(), 0 ) == SOCKET_ERROR)
			returnErrorSys( "failed to send data through socket" );

	while ((length = recv( sockFd, buffer, sizeof(buffer) - 1, 0 )) > 0) {
		buffer[ length ] = '\0';
		response << buffer;
	}

	// parse response

	// status code
	response >> protocol >> resParams->httpStatus;
	if (!response.good())
		returnError( "invalid response headline" );
	statusMsg = readUntilCRLF( response );
	// headers
	while ((line = readUntilCRLF( response )).length() > 0) {
		size_t colon = line.find(':');
		if (colon != std::string::npos)
			resParams->respHeaders[ line.substr( 0, colon ) ] = line.substr( colon+2, std::string::npos );
	}
	if (!response.good())
		returnError( "response headers are incomplete" );
	// data
	transferIt = resParams->respHeaders.find("Transfer-Encoding");
	if (transferIt != resParams->respHeaders.end() && transferIt->second == "chunked") {
		uint chunkLen;
		while (response >> std::hex >> chunkLen) {
			if (chunkLen == 0)
				break;
			response.ignore(2); // skip \r\n after chunkLen
			moveChars( response, respData, chunkLen );
		}
	} else {
		moveChars( response, respData );
	}
	resParams->respData = respData.str();

	// send result report to result handler
	resParams->netStatus = WSAGetLastError();
	closesocket( sockFd );
	delete reqParams;
	return resParams;
}

void * HTTP::HandleResult( void * arg )
{
	HttpResultParams * resParams = (HttpResultParams *)arg;

	CF_Log( 7, "HTTP: result is ready for request to %s:%hu%s", resParams->hostName.c_str(), resParams->port, resParams->urlPath.c_str() );

	resParams->userCallback( resParams->netStatus, resParams->httpStatus, resParams->respHeaders, resParams->respData, resParams->userArg );

	delete resParams;
	return NULL;
}

//----------------------------------------------------------------------------------------------------
void HTTP::Get( const char * hostName, uint16_t port, const char * urlPath, const Headers & headers, ResultCallback callback, void * callbackArg )
{
	CF_Log( 5, "HTTP: scheduling asynchronous GET request to %s:%hu%s", hostName, port, urlPath );

	HttpRequestParams * reqParams = new HttpRequestParams;

	reqParams->type = GET;
	reqParams->hostName = hostName;
	reqParams->port = port;
	reqParams->urlPath = urlPath;
	reqParams->headers = headers;
	reqParams->userCallback = callback;
	reqParams->userArg = callbackArg;

	AsyncTasks::addTask( SendRequest, reqParams, HandleResult );
}

//----------------------------------------------------------------------------------------------------
void HTTP::Post( const char * hostName, uint16_t port, const char * urlPath, const Headers & headers, const std::string & data, ResultCallback callback, void * callbackArg )
{
	CF_Log( 5, "HTTP: scheduling asynchronous GET request to %s:%hu%s", hostName, port, urlPath );

	HttpRequestParams * reqParams = new HttpRequestParams;

	reqParams->type = POST;
	reqParams->hostName = hostName;
	reqParams->port = port;
	reqParams->urlPath = urlPath;
	reqParams->headers = headers;
	reqParams->data = data;
	reqParams->userCallback = callback;
	reqParams->userArg = callbackArg;

	AsyncTasks::addTask( SendRequest, reqParams, HandleResult );
}
