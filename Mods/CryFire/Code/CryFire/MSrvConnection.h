//================================================================================
// File:    Code/CryFire/MSrvConnection.h
//                 ____                       ____
// Project: SSM  /\  _ `\                    /\  _`\   __
//               \ \ \/\_\    _  __   __  __ \ \ \_/  /\_\    _  __     ___
//                \ \ \/_/_  /\`'__\ /\ \/\ \ \ \  _\ \/_/   /\`'__\  /' __`\
//                 \ \ \_\ \ \ \ \_/ \ \ \_\ \ \ \ \/   /\`\ \ \ \_/ /\  \__/
//                  \ \____/  \ \_\   \/`____ \ \ \_\   \ \_\ \ \_\  \ \_____\
//                   \/___/    \/_/    `/___/\ \ \/_/    \/_/  \/_/   \/____ /
//                                        /\___/
//                                        \/__/
// Created on:  20.6.2014
// Last edited: 15.11.2014
//--------------------------------------------------------------------------------
// Description: Handles communication with new master server (GameSpy replacement)
//--------------------------------------------------------------------------------
// Authors:     Patrick Glatt (HipHipHurra)
//              Jan Broz (Youda008)
//================================================================================


#ifndef M_SRV_CONNECTION_INCLUDED
#define M_SRV_CONNECTION_INCLUDED


#include <windows.h>
#include <map>

#include "NetworkUtils.h"
#include "BlockingQueue.h"

#undef GetUserName       // windows.h defines some stupid macros, which overwrites Crysis methods names
#undef GetCommandLine


typedef unsigned int uint;


//----------------------------------------------------------------------------------------------------
class MSrvConnection {

  public:

	static const char * const HOSTNAME; // this constant must be defined in implemetation file or compile error
	static const uint         PORT;
	static const uint         TIMEOUT;
	static const uint         DELAY;
	static const char * const VERSION;

	/* with this you can control, if GameSpy replacement should be enabled or disabled
	   you can use it for example in CVar handler */
	static void onGSReplacementChange(bool enabled);
	
	/* tells if GameSpy replacement is enable */
	static bool useGameSpyReplacement();

	/* call this from some contructor or Init function of the game (for example CGameRules::CGameRules)
	   it is needed to initialize data structures and start thread */
	static void initialize();

	/* call this from some destructor or Terminate function of the game (for example CGameRules::~CGameRules)
	   it is needed to free memory resources and signal thread to quit */
	static void terminate();

	/* call this from some Update function of the game (for example CGameRules::Update)
	   it is needed to notify master server about current status and process results of other thread */
	static void onUpdate(float frameTime);

	/* call this from CGameRules::SendChatMessage function
	   it is needed to check the chat message for command !validate with parameters profileID and uID */
	static bool checkChatMessage(EntityId sourceId, const char * message);


  protected:

	// struct indicating the thread what to do
	enum HTTPMethod {HTTPGET, HTTPPOST};
	enum HTTPVersion {HTTP10, HTTP11};
	static const int SCORE_KILLS_KEY  = 100;
	static const int SCORE_DEATHS_KEY = 101;  // this is needed to get player attributes defined by lua
	static const int RANK_KEY         = 202;

	static bool               UseGameSpyReplacement;
	static bool               running;
	static bool               announced;
	static float              timer;
	static fd_t               sockFd;
	static struct sockaddr_in MSrvAddr;
	static std::string        cookie;
	struct ConnInfo {
		int profId;
		std::string name;
		ConnInfo() {}
		ConnInfo(int prof, const std::string & name) : profId(prof), name(name) {}
	};
	static std::map<std::string, ConnInfo> * validated;

	static void checkUpdateTime(float frameTime);
	static void announceServerStart();
	static void updateServerInfo();
	static void validateClient(EntityId sourceId, int profId, const char * uid, const char * name);
	static void * asyncGetMSrvAddr(void * arg);
	static void * asyncAnnounce(void * arg);
	static void * asyncUpdate(void * arg);
	static void * asyncValidate(void * arg);
	static void * handleValidation(void * arg);
	static void OnValidLogin(int channelId, EntityId playerId, int profileId, const char * name);
	static void OnInvalidLogin(int channelId, EntityId playerId, int profileId, const char * name);
	static const char * getServerDescription();
	static std::string getMapDownloadLink(const char * mapName);
	static std::string gatherPlayersInfo();
	static std::string formatURL(const char * format, ...);
	static std::string queryHTTP(const std::string & page, HTTPMethod method, HTTPVersion version);
	static std::string createRequest(const std::string & page, HTTPMethod method, HTTPVersion version);
	static std::string extractCookie(const std::string & response);
	static bool wasSuccessful(const std::string & response);
	static bool isLoginValid(const std::string & response);
	static int getKills(EntityId entId);
	static int getDeaths(EntityId entId);
	static int getRank(EntityId entId);
	static char * getNextArg(char * pos);

};

#endif // M_SRV_CONNECTION_INCLUDED
