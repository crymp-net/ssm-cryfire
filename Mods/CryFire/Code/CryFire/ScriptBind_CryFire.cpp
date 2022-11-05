//================================================================================
// File:    Code/CryFire/ScriptBind_CryFire.cpp
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


#include "StdAfx.h"

#include "ScriptBind_CryFire.h"

#include "Game.h"
#include "GameRules.h"
#include "Actor.h"
#include "CryFire/NetworkUtils.h"
#include "CryFire/Http.h"
#include "CryFire/Logging.h"

#include <ctime>


//----------------------------------------------------------------------------------------------------
ScriptBind_CryFire * ScriptBind_CryFire::instance = NULL;

void ScriptBind_CryFire::initialize(ISystem * pSystem, IGameFramework * pGameFramework)
{
	instance = new ScriptBind_CryFire(pSystem, pGameFramework);
}

void ScriptBind_CryFire::terminate()
{
	delete instance;
}

//----------------------------------------------------------------------------------------------------
ScriptBind_CryFire::ScriptBind_CryFire(ISystem * pSystem, IGameFramework * pGameFramework)

	: m_pSystem(pSystem)
	, m_pSS(pSystem->GetIScriptSystem())
	, m_pGameFW(pGameFramework)
{
	Init(m_pSS, m_pSystem, 1);
	//SetGlobalName("CryFireDLL");

	RegisterGlobals();
	RegisterMethods();

	SmartScriptTable thisTable(m_pSS);
	thisTable->Delegate(GetMethodsTable());
	m_pSS->SetGlobalValue("CryFireDLL", thisTable);
}

ScriptBind_CryFire::~ScriptBind_CryFire()
{
}

void ScriptBind_CryFire::RegisterGlobals()
{

}

void ScriptBind_CryFire::RegisterMethods()
{
 #undef SCRIPT_REG_CLASSNAME
 #define SCRIPT_REG_CLASSNAME &ScriptBind_CryFire::

	SCRIPT_REG_TEMPLFUNC(GetPlayerByName, "name");
	SCRIPT_REG_TEMPLFUNC(GetMapName, "");
	SCRIPT_REG_TEMPLFUNC(KickWithBanMessage, "playerId, reason");
	SCRIPT_REG_TEMPLFUNC(ForceSetCVar, "CVar, strvalue");
	//SCRIPT_REG_TEMPLFUNC(ForceSetCVar, "CVar, intvalue");
	//SCRIPT_REG_TEMPLFUNC(ForceSetCVar, "CVar, fltvalue");
	SCRIPT_REG_TEMPLFUNC(SetVerbosity, "verbosity");
	SCRIPT_REG_TEMPLFUNC(GetIPFromHost, "hostName");
	SCRIPT_REG_TEMPLFUNC(IsOnLocalhost, "playerId");
	SCRIPT_REG_TEMPLFUNC(IsInLAN, "playerId");
	SCRIPT_REG_TEMPLFUNC(HTTP_Get, "hostName, port, urlPath, headers, luaCallback");
	SCRIPT_REG_TEMPLFUNC(HTTP_Post, "hostName, port, urlPath, headers, data, luaCallback");
	SCRIPT_REG_TEMPLFUNC(TestSpeed, "");
	SCRIPT_REG_TEMPLFUNC(Test, "arg");
}

CGameRules * ScriptBind_CryFire::GetGameRules(IFunctionHandler *pH)
{
	return g_pGame->GetGameRules();
}

int ScriptBind_CryFire::GetPlayerByName(IFunctionHandler* pH, const char* name)
{
	CGameRules* pGameRules = GetGameRules(pH);
	if (!pGameRules)
		return pH->EndFunction();

	CActor* pActor = pGameRules->GetActorByName(name);
	if (!pActor)
		return pH->EndFunction();
	
	return pH->EndFunction(pActor->GetEntity()->GetScriptTable());
}

int ScriptBind_CryFire::GetMapName(IFunctionHandler* pH)
{
	return pH->EndFunction(g_pGame->GetIGameFramework()->GetLevelName());
}

int ScriptBind_CryFire::KickWithBanMessage(IFunctionHandler* pH, ScriptHandle playerId, const char* reason)
{
	CGameRules* pGameRules = GetGameRules(pH);
	if (!pGameRules)
		return pH->EndFunction();

	int chnlId = pGameRules->GetChannelId((EntityId)playerId.n);
	if (!chnlId)
		return pH->EndFunction();

	INetChannel* pNetChannel = m_pGameFW->GetNetChannel((int)chnlId);
	if (!pNetChannel)
		return pH->EndFunction();

	pNetChannel->Disconnect(eDC_Banned, reason);

	return pH->EndFunction();
}

int ScriptBind_CryFire::ForceSetCVar(IFunctionHandler* pH, const char* CVar, const char* value)
{
	ICVar * cvar = gEnv->pConsole->GetCVar(CVar);
	if (!cvar)
		return pH->EndFunction(false);
	cvar->ForceSet(value);
	return pH->EndFunction(true);
}
/*
int ScriptBind_CryFire::ForceSetCVar(IFunctionHandler* pH, const char* CVar, int value)
{
	char strvalue[32];
	sprintf(strvalue, "%d", value);
	gEnv->pConsole->GetCVar(CVar)->ForceSet(strvalue);
	return pH->EndFunction();
}

int ScriptBind_CryFire::ForceSetCVar(IFunctionHandler* pH, const char* CVar, float value)
{
	char strvalue[32];
	sprintf(strvalue, "%d", value);
	gEnv->pConsole->GetCVar(CVar)->ForceSet(strvalue);
	return pH->EndFunction();
}
*/
int ScriptBind_CryFire::SetVerbosity(IFunctionHandler * pH, int verbosity)
{
	logVerbosity = verbosity;
	return pH->EndFunction();
}

int ScriptBind_CryFire::GetIPFromHost(IFunctionHandler * pH, const char * hostName)
{
	char IPAddress[16];
	if (!NetworkUtils::GetIPFromHost(hostName, IPAddress))
		return pH->EndFunction();
	
	return pH->EndFunction(IPAddress);
}

int ScriptBind_CryFire::IsOnLocalhost(IFunctionHandler * pH, ScriptHandle playerId)
{
	CActor* actor = g_pGame->GetGameRules()->GetActorByEntityId((EntityId)playerId.n);
	if (!actor)
		return pH->EndFunction();

	char IPAddress[16];
	if (!actor->GetIPAddress(IPAddress))
		return pH->EndFunction();

	bool isLocalhost = NetworkUtils::IsLocalhost(IPAddress);

	return pH->EndFunction(isLocalhost);
}

int ScriptBind_CryFire::IsInLAN(IFunctionHandler * pH, ScriptHandle playerId)
{
	CActor* actor = g_pGame->GetGameRules()->GetActorByEntityId((EntityId)playerId.n);
	if (!actor)
		return pH->EndFunction();

	char IPAddress[16];
	if (!actor->GetIPAddress(IPAddress))
		return pH->EndFunction();

	bool isLAN = NetworkUtils::IsInLAN(IPAddress);

	return pH->EndFunction(isLAN);
}

static void ScriptBind_HttpCallback( int netStatus, uint httpStatus, const HTTP::Headers & respHeaders, const std::string & respData, void * callbackArg )
{
	HSCRIPTFUNCTION luaCallback = (HSCRIPTFUNCTION)callbackArg;
	IScriptSystem * pSS = gEnv->pScriptSystem;

	// convert to C++ map to Lua table
	SmartScriptTable headersTable( pSS->CreateTable() );
	for (HTTP::Headers::const_iterator iter = respHeaders.begin(); iter != respHeaders.end(); iter++) {
		headersTable->SetValue( iter->first.c_str(), iter->second.c_str() );
	}

	pSS->BeginCall( luaCallback );
	pSS->PushFuncParam( netStatus );
	pSS->PushFuncParam( httpStatus );
	pSS->PushFuncParam( headersTable );
	pSS->PushFuncParam( respData.c_str() );
	pSS->EndCall();
}

int ScriptBind_CryFire::HTTP_Get( IFunctionHandler * pH, const char * hostName, uint port, const char * urlPath, SmartScriptTable headers, HSCRIPTFUNCTION luaCallback )
{
	// convert Lua table to C++ map
	HTTP::Headers headerMap;
	IScriptTable::Iterator iter;
	for (iter = headers->BeginIteration(); headers->MoveNext( iter );) {
		if (iter.sKey == NULL)
			CF_LogError("HTTP header table does not contain string keys");
		if (iter.value.GetVarType() != svtString)
			CF_LogError("HTTP header table does not contain string value at key %s", iter.sKey);
		headerMap[ iter.sKey ] = iter.value.str;
	}
	headers->EndIteration( iter );

	HTTP::Get( hostName, (uint16_t)port, urlPath, headerMap, ScriptBind_HttpCallback, (void*)luaCallback );

	return pH->EndFunction();
}

int ScriptBind_CryFire::HTTP_Post( IFunctionHandler * pH, const char * hostName, uint port, const char * urlPath, SmartScriptTable headers, const char * data, HSCRIPTFUNCTION luaCallback )
{
	// convert Lua table to C++ map
	HTTP::Headers headerMap;
	IScriptTable::Iterator iter;
	for (iter = headers->BeginIteration(); headers->MoveNext( iter );) {
		if (iter.sKey == NULL)
			CF_LogError("HTTP header table does not contain string keys");
		if (iter.value.GetVarType() != svtString)
			CF_LogError("HTTP header table does not contain string value at key %s", iter.sKey);
		headerMap[ iter.sKey ] = iter.value.str;
	}
	headers->EndIteration( iter );

	HTTP::Post( hostName, (uint16_t)port, urlPath, headerMap, data, ScriptBind_HttpCallback, (void*)luaCallback );

	return pH->EndFunction();
}

int ScriptBind_CryFire::TestSpeed(IFunctionHandler * pH)
{
	CGameRules * pGameRules = g_pGame->GetGameRules();
	if (!pGameRules)
		return pH->EndFunction(false);

	if (pGameRules->GetPlayerCount() <= 0) {
		CryLogAlways("$4[Error] there must be at least 1 player for this test  ");
		return pH->EndFunction(false);
	}

	EntityId entId = pGameRules->GetPlayer(0);
	CActor* actor = pGameRules->GetActorByEntityId(entId);
	if (!actor) {
		CryLogAlways("$4[Error] actor not found for some reason  ");
		return pH->EndFunction(false);
	}
	int chnlId = actor->GetChannelId();
	INetChannel* chnl = m_pGameFW->GetNetChannel((int)chnlId);

	clock_t startTime;
	int i;

	startTime = clock();
	for (i = 0; i < 1000000; i++)
		actor = pGameRules->GetActorByEntityId(entId);
	CryLogAlways("1000000x GetActorByEntityId time: %d", clock()-startTime);

	startTime = clock();
	for (i = 0; i < 1000000; i++)
		actor = pGameRules->GetActorByChannelId(chnlId);
	CryLogAlways("1000000x GetActorByChannelId time: %d", clock()-startTime);

	startTime = clock();
	for (i = 0; i < 1000000; i++)
		chnl = m_pGameFW->GetNetChannel((int)chnlId);
	CryLogAlways("1000000x GetNetChannel time: %d", clock()-startTime);

	startTime = clock();
	for (i = 0; i < 1000000; i++)
		chnlId = m_pGameFW->GetGameChannelId(chnl);
	CryLogAlways("1000000x GetGameChannelId time: %d", clock()-startTime);

	return pH->EndFunction(true);
}

int ScriptBind_CryFire::Test(IFunctionHandler * pH, SmartScriptTable table)
{
	CryLogAlways("test C++ function called");

	if (!table) {
		CryLogAlways("table is NULL");
		return pH->EndFunction();
	}

	IScriptSystem * pSS = gEnv->pScriptSystem;
	
	for (int i = 0; i <= table->Count(); i++) {
		float num;
		if (table->GetAt( i, num ))
			CryLogAlways("%.1f", num);
		else
			CryLogAlways("item %d not found", i);
	}

	return pH->EndFunction();
}
