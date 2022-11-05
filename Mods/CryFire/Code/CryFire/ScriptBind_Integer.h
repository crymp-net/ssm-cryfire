//================================================================================
// File:    Code/CryFire/ScriptBind_Integer.h
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


#ifndef SCRIPTBIND_INTEGER_INCLUDED
#define SCRIPTBIND_INTEGER_INCLUDED


#include <IScriptSystem.h>
#include <ScriptHelpers.h>
struct ISystem;
struct IGameFramework;


class ScriptBind_Integer : public CScriptableBase {

 public:

	static void initialize(ISystem * pSystem, IGameFramework * pGameFramework);
	static void terminate();

	int FromNumber(IFunctionHandler * pH, int number);
	int FromString(IFunctionHandler * pH, const char * numberStr);
	int FromTime(IFunctionHandler * pH, int year, int month, int day, int hour, int minute);
	int FromCurrentTime(IFunctionHandler * pH);
	int FromDuration(IFunctionHandler * pH, int days, int hours, int minutes, int seconds);

	int ToNumber(IFunctionHandler * pH, ScriptHandle intnum);
	int ToString(IFunctionHandler * pH, ScriptHandle intnum);
	int ToTime(IFunctionHandler * pH, ScriptHandle intnum);
	int ToDuration(IFunctionHandler * pH, ScriptHandle intnum);

	int Neg(IFunctionHandler * pH, ScriptHandle a);
	int Inc(IFunctionHandler * pH, ScriptHandle a);
	int Dec(IFunctionHandler * pH, ScriptHandle a);
	int Add(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b);
	int Sub(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b);
	int Mul(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b);
	int Div(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b);
	int Mod(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b);

	int IsZero(IFunctionHandler * pH, ScriptHandle a);
	int IsNegative(IFunctionHandler * pH, ScriptHandle a);
	int IsPositive(IFunctionHandler * pH, ScriptHandle a);
	int Compare(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b);
	int Less(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b);
	int Greater(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b);
	int LessOrEq(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b);
	int GreaterOrEq(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b);
	int Equals(IFunctionHandler * pH, ScriptHandle a, ScriptHandle b);

 protected:

	static ScriptBind_Integer * instance;

	ISystem         * m_pSystem;
	IScriptSystem   * m_pSS;
	IGameFramework  * m_pGameFW;

 protected:

	ScriptBind_Integer(ISystem * pSystem, IGameFramework * pGameFramework);
	virtual ~ScriptBind_Integer();

	void RegisterMethods();

};


#endif // SCRIPTBIND_CRYFIRE_INCLUDED