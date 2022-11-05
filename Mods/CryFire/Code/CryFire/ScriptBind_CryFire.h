//================================================================================
// File:    Code/CryFire/ScriptBind_CryFire.h
//                 ____                       ____
// Project: SSM  /\  _ `\                    /\  _`\   __
//               \ \ \/\_\    _  __   __  __ \ \ \_/  /\_\    _  __     ___
//                \ \ \/_/_  /\`'__\ /\ \/\ \ \ \  _\ \/_/   /\`'__\  /' __`\
//                 \ \ \_\ \ \ \ \_/ \ \ \_\ \ \ \ \/   /\`\ \ \ \_/ /\  \__/
//                  \ \____/  \ \_\   \/`____ \ \ \_\   \ \_\ \ \_\  \ \_____\
//                   \/___/    \/_/    `/___/\ \ \/_/    \/_/  \/_/   \/____ /
//                                        /\___/
//                                        \/__/
// Created on:  1.9.2016
// Last edited: 7.10.2017
//--------------------------------------------------------------------------------
// Description: DLL/C++ functions accessible in Lua
//--------------------------------------------------------------------------------
// Authors:     Patrick Glatt (HipHipHurra)
//              Jan Broz (Youda008)
//================================================================================


#ifndef SCRIPTBIND_CRYFIRE_INCLUDED
#define SCRIPTBIND_CRYFIRE_INCLUDED


#include <IScriptSystem.h>
#include <ScriptHelpers.h>
#include <IGameFramework.h>
#include <GameRules.h>

typedef unsigned int uint;


class ScriptBind_CryFire : public CScriptableBase {

 public:

	static void initialize(ISystem * pSystem, IGameFramework * pGameFramework);
	static void terminate();

	/// searches for a player by part of his name
	int GetPlayerByName(IFunctionHandler* pH, const char* name);
	/// retrieves the name of the current map
	int GetMapName(IFunctionHandler* pH);
	/// kicks a player, but shows him a ban message
	int KickWithBanMessage(IFunctionHandler* pH, ScriptHandle playerId, const char* reason);
	/// sets a value of a CVar, which normally cannot be set
	int ForceSetCVar(IFunctionHandler* pH, const char* CVar, const char* value);
	//int ForceSetCVar(IFunctionHandler* pH, const char* CVar, int value);
	//int ForceSetCVar(IFunctionHandler* pH, const char* CVar, float value);
	/// notifies the DLL about CryFire log verbosity change in lua
	int SetVerbosity(IFunctionHandler * pH, int verbosity);
	/// contacts DNS and gets IP address from host name
	int GetIPFromHost(IFunctionHandler * pH, const char * hostName);
	/// compares IP of server and client to find, if they are on same computer
	int IsOnLocalhost(IFunctionHandler * pH, ScriptHandle playerId);
	/// compares IP of server and client to find, if they are in same network
	int IsInLAN(IFunctionHandler * pH, ScriptHandle playerId);
	/// performs an asynchronous HTTP GET request and calls you callback when result is ready
	int HTTP_Get(IFunctionHandler * pH, const char * hostName, uint port, const char * urlPath,
	             SmartScriptTable headers, HSCRIPTFUNCTION luaCallback);
	/// performs an asynchronous HTTP POST request and calls you callback when result is ready
	int HTTP_Post(IFunctionHandler * pH, const char * hostName, uint port, const char * urlPath,
	              SmartScriptTable headers, const char * data, HSCRIPTFUNCTION luaCallback);
	/// does some tests
	int TestSpeed(IFunctionHandler * pH);
	/// function for experimenting
	int Test(IFunctionHandler * pH, SmartScriptTable table);

 protected:

	ScriptBind_CryFire(ISystem * pSystem, IGameFramework * pGameFramework);
	virtual ~ScriptBind_CryFire();

	void RegisterGlobals();
	void RegisterMethods();

	CGameRules * GetGameRules( IFunctionHandler * pH );

 protected:

	static ScriptBind_CryFire * instance;

	ISystem         * m_pSystem;
	IScriptSystem   * m_pSS;
	IGameFramework  * m_pGameFW;

};


#endif // SCRIPTBIND_CRYFIRE_INCLUDED