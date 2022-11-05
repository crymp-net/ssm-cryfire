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


#ifndef LOGGING_INCLUDED
#define LOGGING_INCLUDED


//--------------------------------------------------------------------------------

typedef unsigned int uint;

extern uint logVerbosity;

void Logging_initialize();
void Logging_terminate();
void Logging_onUpdate();

/// don't use this from other than primary thread, it will crash, use CF_AsyncLog instead
void CF_Log( uint level, const char * format, ... );
/// don't use this from other than primary thread, it will crash, use CF_AsyncError instead
void CF_LogError( const char * format, ... );

void CF_AsyncLog( uint level, const char * format, ... );
void CF_AsyncError( const char * format, ... );

typedef unsigned int channelId;

bool RMIerror( const char * RMIname, channelId chnlId, const char * message, const char * moreInfo = NULL, const char * moreData = NULL );
bool RMIwarning( const char * RMIname, channelId chnlId, const char * realActor, const char * pretendedActor, const char * moreInfo = NULL, const char * moreData = NULL );
void RMIdebug( uint level, const char * RMIname, channelId chnlId, const char * realActor, const char * pretendedActor, const char * moreInfo = NULL, const char * moreData = NULL );


#endif LOGGING_INCLUDED
