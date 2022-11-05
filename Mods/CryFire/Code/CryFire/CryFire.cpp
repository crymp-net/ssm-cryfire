//================================================================================
// File:    Code/CryFire/CryFire.cpp
//                 ____                       ____
// Project: SSM  /\  _ `\                    /\  _`\   __
//               \ \ \/\_\    _  __   __  __ \ \ \_/  /\_\    _  __     ___
//                \ \ \/_/_  /\`'__\ /\ \/\ \ \ \  _\ \/_/   /\`'__\  /' __`\
//                 \ \ \_\ \ \ \ \_/ \ \ \_\ \ \ \ \/   /\`\ \ \ \_/ /\  \__/
//                  \ \____/  \ \_\   \/`____ \ \ \_\   \ \_\ \ \_\  \ \_____\
//                   \/___/    \/_/    `/___/\ \ \/_/    \/_/  \/_/   \/____ /
//                                        /\___/
//                                        \/__/
// Created on:  15.2.2017
// Last edited: 15.2.2017
//--------------------------------------------------------------------------------
// Description: Entry point of CryFire hooks
//--------------------------------------------------------------------------------
// Authors:     Patrick Glatt (HipHipHurra)
//              Jan Broz (Youda008)
//================================================================================


#include "StdAfx.h"

#include "CryFire.h"

#include <ISystem.h>
#include <GameRules.h>

#include "CryFire/ScriptBind_CryFire.h"
#include "CryFire/ScriptBind_Integer.h"
#include "CryFire/AsyncTasks.h"
#include "CryFire/MSrvConnection.h"
#include "CryFire/Hooking.h"

#include <set>


//----------------------------------------------------------------------------------------------------
bool CryFire::initialized = false;
CGameRules * CryFire::pGameRules = NULL;

//----------------------------------------------------------------------------------------------------
// called on every map load
void CryFire::initialize( ISystem * pSystem, IGameFramework * pGameFramework )
{
	pGameRules = g_pGame->GetGameRules(); // pGameRules is deleted and allocated again on map change - must update!

	// !!! everything must be initialized in this order
	// !!! otherwise risking crash

	if (!initialized) {
		// prepare message queue for asynchronous logging
		Logging_initialize();

		if (!gEnv->pConsole) {
			CF_LogError("pConsole is NULL, wtf!");
			return;
		}

		// initialize thread for performing asynchronous tasks
		AsyncTasks::initialize( 1 + gEnv->pConsole->GetCVar("sv_maxplayers")->GetIVal() );
	}

	// bind CryFire C++ functions to Lua
	ScriptBind_CryFire::initialize( pSystem, pGameFramework );   // re-initialize everytime to update
	ScriptBind_Integer::initialize( pSystem, pGameFramework );   // pSystem pointer in case it changed

	// !!No-GameSpy: initializes connection and communication with master server
	if (MSrvConnection::useGameSpyReplacement())
		MSrvConnection::initialize();

	if (!initialized) {
		hookOtherLibs();   // hooking mustn't be done twice!!
	}

	initialized = true;
}

// called on every new map load
void CryFire::terminate()
{
	// !!No-GameSpy - added: terminates connection and communication with master server
	if (MSrvConnection::useGameSpyReplacement())
		MSrvConnection::terminate();
	// delete script table binded to lua
	ScriptBind_Integer::terminate();
	ScriptBind_CryFire::terminate();
/* not needed
	// terminates thread for asynchronous tasks
	AsyncTasks::terminate();
	Logging_terminate();
*/
}

void CryFire::onUpdate( float frameTime )
{
	if (!initialized)
		return;

	Logging_onUpdate();
	AsyncTasks::onUpdate( frameTime );
	if (MSrvConnection::useGameSpyReplacement())
		MSrvConnection::onUpdate( frameTime );
}

bool CryFire::onChatMessage( EChatMessageType type, EntityId sourceId, EntityId targetId, const char *msg )
{
	if (!initialized)
		return true;

	// !!No-GameSpy: check for !validate command comming from SafeWriting client
	if (MSrvConnection::checkChatMessage(sourceId, msg))
		return false;
	// notify lua and ask if hide the message
	bool showMsg = true;
	pGameRules->CallScriptReturn(pGameRules->GetScriptTable(), "OnChatMessage", type, ScriptHandle(sourceId), ScriptHandle(targetId), msg, showMsg);
	return showMsg;
}

//----------------------------------------------------------------------------------------------------
struct Address {
	void * VMT; // no idea why there is VMT, but there is
	unsigned char IPv4 [4];
	unsigned __int16 port;
	// there may be some other data after his, however the complete content of this struct
	// is hard to find, even with source codes of CryEngine3 :/
};

static std::map<uint,uint> connected;

static int onPacketReceived( const Address * from, byte * data, uint len )
{
	if (data[0] == '\x3E') { // disconnect packet

		if (data[1] == '\x19' && data[2] == '%' && data[3] == 's' && data[4] == '%' && data[5] == 's') {
			for (uint i = 2; i < len; i++)
				if (data[i] == '%')
					data[i] = '_';	
			CF_AsyncLog( 0, "crysisfs (crash) attack detected from %d.%d.%d.%d", from->IPv4[3], from->IPv4[2], from->IPv4[1], from->IPv4[0] );
		}
		std::map<uint,uint>::iterator connCnt = connected.find( *(uint*)&from->IPv4 );
		if (connCnt != connected.end()) {
			CF_AsyncLog( 1, "%u connect packets sent by this player", connCnt->second);
			connected.erase( *(uint*)&from->IPv4 );
		}
		return 1;

	} else if (data[0] == '\x3C') { // connect packet

		std::map<uint,uint>::iterator connCnt = connected.find( *(uint*)&from->IPv4 );
		if (connCnt == connected.end()) { // no connect packet was sent yet
			connCnt->second = 0;
		} else {
			(connCnt->second)++;
			if (connCnt->second > 10) { // sometimes client can resend connection packet, so consider it hack, when at least 10 were sent
				if (connCnt->second % 50 == 10) // display warning only every 50th attempt to not spam them during attack
					CF_AsyncLog( 0, "crysisdos (freeze) attack detected from %d.%d.%d.%d", from->IPv4[3], from->IPv4[2], from->IPv4[1], from->IPv4[0] );
				return 0;
			}
		}
		return 1;

	}

	return 1;
}

void CryFire::hookOtherLibs()
{
	CF_Log( 1, "hooking other DLL libraries" );
	// hook packet processing
	hookCode( 0x3954AAA7, 5, &onPacketReceived, 3, false ); // alternatively: 0x3954AAAC
}
