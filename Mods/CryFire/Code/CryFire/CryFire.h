//================================================================================
// File:    Code/CryFire/CryFire.h
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


#ifndef CRYFIRE_INCLUDED
#define CRYFIRE_INCLUDED


#include <Game.h>
#include <GameRules.h>
#include "CryFire/Logging.h"

typedef unsigned int uint;


//----------------------------------------------------------------------------------------------------
class CryFire {

 public:
  	
	static void initialize( ISystem * pSystem, IGameFramework * pGameFramework );
	static void terminate();
	
	static void onUpdate( float frameTime );

	static bool onChatMessage( EChatMessageType type, EntityId sourceId, EntityId targetId, const char *msg );

 protected:

	static void hookOtherLibs();

 protected:

	static bool initialized;
	static CGameRules * pGameRules;

};

#endif // CRYFIRE_INCLUDED
