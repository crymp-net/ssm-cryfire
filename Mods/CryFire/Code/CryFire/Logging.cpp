//================================================================================
// File:    Code/CryFire/Logging.h
//                 ____                       ____
// Project: SSM  /\  _ `\                    /\  _`\   __
//               \ \ \/\_\    _  __   __  __ \ \ \_/  /\_\    _  __     ___
//                \ \ \/_/_  /\`'__\ /\ \/\ \ \ \  _\ \/_/   /\`'__\  /' __`\
//                 \ \ \_\ \ \ \ \_/ \ \ \_\ \ \ \ \/   /\`\ \ \ \_/ /\  \__/
//                  \ \____/  \ \_\   \/`____ \ \ \_\   \ \_\ \ \_\  \ \_____\
//                   \/___/    \/_/    `/___/\ \ \/_/    \/_/  \/_/   \/____ /
//                                        /\___/
//                                        \/__/
// Created on:  5.8.2016
// Last edited: 18.2.2017
//--------------------------------------------------------------------------------
// Description: simple C++ logging utilities
//--------------------------------------------------------------------------------
// Authors:     Patrick Glatt (HipHipHurra)
//              Jan Broz (Youda008)
//================================================================================


#include "StdAfx.h"

#include "Logging.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <cstdarg>
#include "Game.h"

#include "CryFire/BlockingQueue.h"


const uint MAX_LOG_LEN = 120;
const uint MAX_LOG_QUEUE = 32;


//--------------------------------------------------------------------------------
uint logVerbosity = 2;

FixedQueue<std::string> * asyncMessages = NULL;
HANDLE queueMutex;

//--------------------------------------------------------------------------------
void Logging_initialize()
{
	asyncMessages = new FixedQueue<std::string>( MAX_LOG_QUEUE );
	queueMutex = CreateMutex(NULL, false, NULL);
}

void Logging_terminate()
{
	delete asyncMessages;
	CloseHandle(queueMutex);
}

void Logging_onUpdate()
{
	static uint updID = 0;
	if (asyncMessages->count() != 0) {
		WaitForSingleObject( queueMutex, INFINITE );
			std::string message = asyncMessages->pop();
		ReleaseMutex( queueMutex );
		gEnv->pLog->LogWithType( ILog::eAlways, "%s", message.c_str() );
	}
}

//--------------------------------------------------------------------------------
void CF_Log(uint level, const char * format, ...)
{
	va_list args;
	va_start(args, format);

	if (level <= logVerbosity) {
		char * newFormat = new char [strlen(format) + 16];
		strcpy(newFormat, "[CryFire DLL] ");
		strcpy(newFormat+14, format);
		gEnv->pLog->LogV( ILog::eAlways, newFormat, args );
		delete [] newFormat;
	}

	va_end(args);
}

void CF_LogError( const char * format, ... )
{
	va_list args;
	va_start(args, format);

	int formatLen = strlen(format);
	char * newFormat = new char [formatLen + 26];
	strcpy(newFormat, "$4[CryFire DLL] Error: ");
	strcpy(newFormat+23, format);
	strcpy(newFormat+23+formatLen, "  ");
	gEnv->pLog->LogV( ILog::eAlways, newFormat, args );
	delete [] newFormat;

	va_end(args);
}

void CF_AsyncLog( uint level, const char * format, ... )
{
	va_list args;
	va_start(args, format);
	char buffer [MAX_LOG_LEN+1];

	if (level <= logVerbosity && asyncMessages->count() < MAX_LOG_QUEUE) {
		char * newFormat = new char [strlen(format) + 16];
		strcpy( newFormat, "[CryFire DLL] " );
		strcpy( newFormat+14, format );
		vsnprintf( buffer, sizeof(buffer), newFormat, args );
		buffer[ sizeof(buffer) - 1 ] = '\0';
		delete [] newFormat;
		WaitForSingleObject( queueMutex, INFINITE );
			if (asyncMessages->count() < MAX_LOG_QUEUE)
				asyncMessages->push( std::string( buffer ) );
		ReleaseMutex( queueMutex );
	}

	va_end(args);
}

void CF_AsyncError( const char * format, ... )
{
	va_list args;
	va_start(args, format);
	char buffer [MAX_LOG_LEN+1];

	if (asyncMessages->count() < MAX_LOG_QUEUE) {
		int formatLen = strlen(format);
		char * newFormat = new char [formatLen + 26];
		strcpy(newFormat, "$4[CryFire DLL] Error: ");
		strcpy(newFormat+23, format);
		strcpy(newFormat+23+formatLen, "  ");
		vsnprintf( buffer, sizeof(buffer), newFormat, args );
		buffer[ sizeof(buffer) - 1 ] = '\0';
		delete [] newFormat;
		WaitForSingleObject( queueMutex, INFINITE );
			if (asyncMessages->count() < MAX_LOG_QUEUE)
				asyncMessages->push( std::string( buffer ) );
		ReleaseMutex( queueMutex );
	}

	va_end(args);
}

//--------------------------------------------------------------------------------
bool RMIerror(const char * RMIname, channelId chnlId, const char * message, const char * moreInfo, const char * moreData)
{
	if (moreInfo && moreData)
		CryLogAlways("$4[CryFire DLL] RMI %s; from channel: %u; %s; %s: %s  ", RMIname, chnlId, message, moreInfo, moreData);
	else
		CryLogAlways("$4[CryFire DLL] RMI %s; from channel: %u; %s  ", RMIname, message, chnlId);
	return false;
}

bool RMIwarning(const char * RMIname, channelId chnlId, const char * realActor, const char * pretendedActor, const char * moreInfo, const char * moreData)
{
	if (moreInfo && moreData)
		CryLogAlways("$4[CryFire DLL] RMI %s; from channel: %u (%s); target: %s; %s: %s  ", RMIname, chnlId, realActor, pretendedActor, moreInfo, moreData);
	else
		CryLogAlways("$4[CryFire DLL] RMI %s; from channel: %u (%s); target: %s  ", RMIname, chnlId, realActor, pretendedActor);
	return false;
}

void RMIdebug(uint level, const char * RMIname, channelId chnlId, const char * realActor, const char * pretendedActor, const char * moreInfo, const char * moreData)
{
	if (level <= logVerbosity) {
		if (moreInfo && moreData)
			CryLogAlways("[CryFire DLL] RMI %s; from channel: %u (%s); target: %s; %s: %s", RMIname, chnlId, realActor, pretendedActor, moreInfo, moreData);
		else
			CryLogAlways("[CryFire DLL] RMI %s; from channel: %u (%s); target: %s", RMIname, chnlId, realActor, pretendedActor);
	}
}
