//================================================================================
// File:    Code/CryFire/ScriptBind_Integer.cpp
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
// Last edited: 2.9.2016
//--------------------------------------------------------------------------------
// Description: integer support for Lua to store larger numbers accurately
//--------------------------------------------------------------------------------
// Authors:     Patrick Glatt (HipHipHurra)
//              Jan Broz (Youda008)
//================================================================================


#include "StdAfx.h"

#include "ScriptBind_Integer.h"

#include "ISystem.h"
#include "IGameFramework.h"
//#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>


//----------------------------------------------------------------------------------------------------
ScriptBind_Integer * ScriptBind_Integer::instance = NULL;

void ScriptBind_Integer::initialize(ISystem * pSystem, IGameFramework * pGameFramework)
{
	instance = new ScriptBind_Integer(pSystem, pGameFramework);
}

void ScriptBind_Integer::terminate()
{
	delete instance;
}

//----------------------------------------------------------------------------------------------------
ScriptBind_Integer::ScriptBind_Integer(ISystem * pSystem, IGameFramework * pGameFramework)

	: m_pSystem(pSystem)
	, m_pSS(pSystem->GetIScriptSystem())
	, m_pGameFW(pGameFramework)
{
	Init(m_pSS, m_pSystem, 1);

	RegisterMethods();

	SmartScriptTable thisTable(m_pSS);
	thisTable->Delegate(GetMethodsTable());
	m_pSS->SetGlobalValue("Int", thisTable);
}

ScriptBind_Integer::~ScriptBind_Integer()
{
}

void ScriptBind_Integer::RegisterMethods()
{
 #undef SCRIPT_REG_CLASSNAME
 #define SCRIPT_REG_CLASSNAME &ScriptBind_Integer::

	SCRIPT_REG_TEMPLFUNC(FromNumber, "number");
	SCRIPT_REG_TEMPLFUNC(FromString, "numberStr");
	SCRIPT_REG_TEMPLFUNC(FromTime, "year, month, day, hour, minute");
	SCRIPT_REG_TEMPLFUNC(FromCurrentTime, "");
	SCRIPT_REG_TEMPLFUNC(FromDuration, "days, hours, minutes, seconds");

	SCRIPT_REG_TEMPLFUNC(ToNumber, "intnum");
	SCRIPT_REG_TEMPLFUNC(ToString, "intnum");
	SCRIPT_REG_TEMPLFUNC(ToTime, "intnum");
	SCRIPT_REG_TEMPLFUNC(ToDuration, "intnum");

	SCRIPT_REG_TEMPLFUNC(Neg, "a");
	SCRIPT_REG_TEMPLFUNC(Inc, "a");
	SCRIPT_REG_TEMPLFUNC(Dec, "a");
	SCRIPT_REG_TEMPLFUNC(Add, "a, b");
	SCRIPT_REG_TEMPLFUNC(Sub, "a, b");
	SCRIPT_REG_TEMPLFUNC(Mul, "a, b");
	SCRIPT_REG_TEMPLFUNC(Div, "a, b");
	SCRIPT_REG_TEMPLFUNC(Mod, "a, b");

	SCRIPT_REG_TEMPLFUNC(IsZero, "a");
	SCRIPT_REG_TEMPLFUNC(IsNegative, "a");
	SCRIPT_REG_TEMPLFUNC(IsPositive, "a");
	SCRIPT_REG_TEMPLFUNC(Compare, "a, b");
	SCRIPT_REG_TEMPLFUNC(Less, "a, b");
	SCRIPT_REG_TEMPLFUNC(Greater, "a, b");
	SCRIPT_REG_TEMPLFUNC(LessOrEq, "a, b");
	SCRIPT_REG_TEMPLFUNC(GreaterOrEq, "a, b");
	SCRIPT_REG_TEMPLFUNC(Equals, "a, b");
}

//----------------------------------------------------------------------------------------------------
#define FROM_HANDLE(handle) (intptr_t)handle.ptr
#define TO_HANDLE(intnum) ScriptHandle((void*)(intptr_t)(intnum))

int ScriptBind_Integer::FromNumber(IFunctionHandler * pH, int number)
{
	return pH->EndFunction( TO_HANDLE(number) );
}

int ScriptBind_Integer::FromString(IFunctionHandler * pH, const char * numberStr)
{
	long int intnum;
	if (sscanf(numberStr, "%ld", &intnum) < 1)
		return pH->EndFunction();
	else
		return pH->EndFunction( TO_HANDLE(intnum) );
}

int ScriptBind_Integer::FromTime(IFunctionHandler * pH, int year, int month, int day, int hour, int minute)
{
	struct tm timestruct;
	timestruct.tm_year = year-1900; // years since 1900
	timestruct.tm_mon = month-1;    // months since January - [0,11]
	timestruct.tm_mday = day;       // day of the month - [1,31]
	timestruct.tm_hour = hour;      // hours since midnight - [0,23]
	timestruct.tm_min = minute;     // minutes after the hour - [0,59]

	time_t timestamp = mktime(&timestruct);
	return pH->EndFunction( TO_HANDLE(timestamp) );
}

int ScriptBind_Integer::FromCurrentTime(IFunctionHandler * pH)
{
	time_t timestamp = time(NULL);
	return pH->EndFunction( TO_HANDLE(timestamp) );
}

int ScriptBind_Integer::FromDuration(IFunctionHandler * pH, int days, int hours, int minutes, int seconds)
{
	intptr_t timestamp = days*86400 + hours*3600 + minutes*60 + seconds;
	return pH->EndFunction( TO_HANDLE(timestamp) );
}

int ScriptBind_Integer::ToNumber(IFunctionHandler * pH, ScriptHandle intnum)
{
	return pH->EndFunction( (int)FROM_HANDLE(intnum) );
}

int ScriptBind_Integer::ToString(IFunctionHandler * pH, ScriptHandle intnum)
{
	char numStr [32];
	sprintf(numStr, "%ld", (long int)FROM_HANDLE(intnum));
	return pH->EndFunction( numStr );
}

int ScriptBind_Integer::ToTime(IFunctionHandler * pH, ScriptHandle intnum)
{
	SmartScriptTable timeTable(gEnv->pScriptSystem->CreateTable());

	time_t timestamp = (time_t)FROM_HANDLE(intnum);
	struct tm * timestruct = localtime(&timestamp);

	timeTable->SetValue("year", timestruct->tm_year+1900);
	timeTable->SetValue("month", timestruct->tm_mon+1);
	timeTable->SetValue("day", timestruct->tm_mday);
	timeTable->SetValue("hour", timestruct->tm_hour);
	timeTable->SetValue("minute", timestruct->tm_min);
	
	return pH->EndFunction( timeTable );
}

int ScriptBind_Integer::ToDuration(IFunctionHandler * pH, ScriptHandle intnum)
{
	SmartScriptTable timeTable(gEnv->pScriptSystem->CreateTable());

	intptr_t timestamp = FROM_HANDLE(intnum);

	int days = timestamp / 86400; timestamp %= 86400;
	int hours = timestamp / 3600; timestamp %= 3600;
	int minutes = timestamp / 60; timestamp %= 60;
	int seconds = timestamp;

	timeTable->SetValue("days", days);
	timeTable->SetValue("hours", hours);
	timeTable->SetValue("minutes", minutes);
	timeTable->SetValue("seconds", seconds);
	
	return pH->EndFunction( timeTable );
}

int ScriptBind_Integer::Neg(IFunctionHandler * pH, ScriptHandle a)
{
	return pH->EndFunction( TO_HANDLE( - FROM_HANDLE(a) ) );
}

int ScriptBind_Integer::Inc(IFunctionHandler * pH, ScriptHandle a)
{
	return pH->EndFunction( TO_HANDLE( FROM_HANDLE(a) + 1 ) );
}

int ScriptBind_Integer::Dec(IFunctionHandler * pH, ScriptHandle a)
{
	return pH->EndFunction( TO_HANDLE( FROM_HANDLE(a) - 1 ) );
}

int ScriptBind_Integer::Add(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b)
{
	return pH->EndFunction( TO_HANDLE( FROM_HANDLE(a) + FROM_HANDLE(b) ) );
}

int ScriptBind_Integer::Sub(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b)
{
	return pH->EndFunction( TO_HANDLE( FROM_HANDLE(a) - FROM_HANDLE(b) ) );
}

int ScriptBind_Integer::Mul(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b)
{
	return pH->EndFunction( TO_HANDLE( FROM_HANDLE(a) * FROM_HANDLE(b) ) );
}

int ScriptBind_Integer::Div(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b)
{
	return pH->EndFunction( TO_HANDLE( FROM_HANDLE(a) / FROM_HANDLE(b) ) );
}

int ScriptBind_Integer::Mod(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b)
{
	return pH->EndFunction( TO_HANDLE( FROM_HANDLE(a) % FROM_HANDLE(b) ) );
}

int ScriptBind_Integer::IsZero(IFunctionHandler * pH, ScriptHandle a)
{
	return pH->EndFunction( FROM_HANDLE(a) == 0 );
}

int ScriptBind_Integer::IsNegative(IFunctionHandler * pH, ScriptHandle a)
{
	return pH->EndFunction( FROM_HANDLE(a) < 0 );
}

int ScriptBind_Integer::IsPositive(IFunctionHandler * pH, ScriptHandle a)
{
	return pH->EndFunction( FROM_HANDLE(a) > 0 );
}

int ScriptBind_Integer::Compare(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b)
{
	return pH->EndFunction( (int)(FROM_HANDLE(a) - FROM_HANDLE(b)) );
}

int ScriptBind_Integer::Less(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b)
{
	return pH->EndFunction( FROM_HANDLE(a) < FROM_HANDLE(b) );
}

int ScriptBind_Integer::Greater(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b)
{
	return pH->EndFunction( FROM_HANDLE(a) > FROM_HANDLE(b) );
}

int ScriptBind_Integer::LessOrEq(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b)
{
	return pH->EndFunction( FROM_HANDLE(a) <= FROM_HANDLE(b) );
}

int ScriptBind_Integer::GreaterOrEq(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b)
{
	return pH->EndFunction( FROM_HANDLE(a) >= FROM_HANDLE(b) );
}

int ScriptBind_Integer::Equals(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b)
{
	return pH->EndFunction( FROM_HANDLE(a) == FROM_HANDLE(b) );
}
