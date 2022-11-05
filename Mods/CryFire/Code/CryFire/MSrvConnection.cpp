//================================================================================
// File:    Code/CryFire/MSrvConnection.cpp
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
// Last edited: 7.10.2016
//--------------------------------------------------------------------------------
// Description: Handles communication with new master server (GameSpy replacement)
//--------------------------------------------------------------------------------
// Authors:     Patrick Glatt (HipHipHurra)
//              Jan Broz (Youda008)
//================================================================================


/*--------------------------------------------------------------------------------
Implementation notes:
We can't contact the master server directly from the main game thread, because
network operations takes a lot of time and it would stop the whole game from
updating and cause a huge lag.
Also we can't just create 1 thread, which will automaticaly gather all info,
send it to the master server, process the result and then sleep <DELAY> seconds
before doing it another time. Accessing game structures such as CGame and CGameRules
is not synchronized and with multiple threads causes anomalies and crashes.
Same applies to CryLogAlways and CallScript.
So we must count the time in 'Update' function in the main game thread and
if it's the time to contact master server, gather all needed info and send
the task to the other thread through blocking queue. The other thread will be
created when the game starts and it will sleep as long as the queue is empty.
When something comes into the queue, it will wake up, perform the task and
save the result in a predefined place. The main game thread will regularly
check this place in 'Update' and if the result is ready, it will process it
or send it to lua by CallScript.
--------------------------------------------------------------------------------*/

#include "StdAfx.h"

#include "MSrvConnection.h"

#include "NetworkUtils.h"
#include "BlockingQueue.h"
#include "AsyncTasks.h"
#include "Game.h"
#include "GameRules.h"

#include <windows.h>
#include <string>
#include <sstream>
#include <fstream>
#include <map>

//using namespace std; // can't use this, because std::min, std::max get in conflict with min, max from Cry_Math.h

#define logError(message) CF_AsyncError("%s", message);

#define logErrorArg(format, arg) CF_AsyncError(format, arg);

#define returnError(retval, message) {\
	CF_AsyncError("%s", message);\
	return retval;\
}

#define returnErrorArg(retval, format, arg) {\
	CF_AsyncError(format, arg);\
	return retval;\
}

#define returnErrorSys(retval, message) {\
	int error = GetLastError();\
	CF_AsyncError("%s (%d)", message, error);\
	return retval;\
}

//----------------------------------------------------------------------------------------------------
// additional struct definition

struct InformParams {
	std::string page;
};

struct ValidateParams {
	std::string page;
	int channelId;
	EntityId playerId;
	int profileId;
	std::string uID;
	std::string name;
};

struct ValidateResult {
	std::string response;
	int channelId;
	EntityId playerId;
	int profileId;
	std::string uID;
	std::string name;
};

//----------------------------------------------------------------------------------------------------
// init static member variables
const char * const MSrvConnection::HOSTNAME = "crymp.net";
const uint         MSrvConnection::PORT     = 80;
const uint         MSrvConnection::TIMEOUT  = 10000; // miliseconds
const uint         MSrvConnection::DELAY    = 30; // seconds between sending info to master server
const char * const MSrvConnection::VERSION  = "6156";

bool MSrvConnection::UseGameSpyReplacement = false;
bool MSrvConnection::running = false;
bool MSrvConnection::announced = false;
float MSrvConnection::timer = 0;
fd_t MSrvConnection::sockFd = -1;
struct sockaddr_in MSrvConnection::MSrvAddr;
std::string MSrvConnection::cookie;
std::map<std::string, MSrvConnection::ConnInfo> * MSrvConnection::validated = NULL;

//----------------------------------------------------------------------------------------------------
// called when value of CVar cf_usegsreplacement is changed
void MSrvConnection::onGSReplacementChange(bool enabled)
{
	UseGameSpyReplacement = enabled && gEnv->pConsole->GetCVar("sv_lanonly")->GetIVal() == 0;
	gEnv->pSystem->GetIScriptSystem()->SetGlobalValue("UseGameSpyReplacement", UseGameSpyReplacement);
}

//----------------------------------------------------------------------------------------------------
bool MSrvConnection::useGameSpyReplacement()
{
	return UseGameSpyReplacement;
}

//----------------------------------------------------------------------------------------------------
// called from main thread when game starts - initializes communication with master server
void MSrvConnection::initialize()
{
	if (!validated)
		validated = new std::map<std::string, ConnInfo>;

	if (!running) { // if master server connection is not running, server has just started
		announced = false; // firstly we need to register this server at master, then we can send data
		AsyncTasks::addTask(asyncGetMSrvAddr, NULL, NULL);
	}
	timer = 2; // schedule update in some near future
}

//----------------------------------------------------------------------------------------------------
void * MSrvConnection::asyncGetMSrvAddr(void * arg)
{
	// query DNS to find IP of the host name
	if (!NetworkUtils::getSockaddr(HOSTNAME, PORT, &MSrvAddr))
		returnErrorSys(NULL, "get sockaddr failed");
	running = true;
		
	return NULL;
}

//----------------------------------------------------------------------------------------------------
// called from main thread when game ends - terminates communication with master server
void MSrvConnection::terminate()
{
	running = false;
}

//----------------------------------------------------------------------------------------------------
// called from main thread on update - performs actions needed on every update
void MSrvConnection::onUpdate(float frameTime)
{
	if (!running)
		return;

	checkUpdateTime(frameTime);
}

//----------------------------------------------------------------------------------------------------
// called from main thread on update - checks if it's time to contact master server and update server info
void MSrvConnection::checkUpdateTime(float frameTime)
{
	timer -= frameTime;
	if (timer <= 0) {
		timer = DELAY;
		if (!announced)
			announceServerStart();
		else
			updateServerInfo();
	}
}

//----------------------------------------------------------------------------------------------------
// called from main thread - schedules announcing master server, that this game server is online
void MSrvConnection::announceServerStart()
{
	char localIP [16];
	InformParams * params = new InformParams;

	// gather needed info and schedule contacting master server
	int port  = gEnv->pConsole->GetCVar("sv_port")->GetIVal();
	int maxpl = gEnv->pConsole->GetCVar("sv_maxplayers")->GetIVal();
	int numpl = 0;
	const char * svname = gEnv->pConsole->GetCVar("sv_servername")->GetString();
	const char * svpass = strlen(gEnv->pConsole->GetCVar("sv_password")->GetString()) > 0 ? "true" : "";
	const char * map = g_pGame->GetIGameFramework()->GetLevelName(); map = map ? map : "";
	int remtime = (int)g_pGame->GetGameRules()->GetRemainingGameTime();
	std::string maplink = getMapDownloadLink(map);
	int ranked = gEnv->pConsole->GetCVar("sv_ranked")->GetIVal();
	NetworkUtils::GetLocalIP(localIP);
	const char * desc = getServerDescription();

	params->page = formatURL("/api/reg.php?port=%d&maxpl=%d&numpl=%d&name=%s&pass=%s&map=%s&timel=%d&mapdl=%s&ver=%s&ranked=%d&local=%s&desc=%s",
		                                port,   maxpl,   numpl, svname, svpass,   map,   remtime, maplink.c_str(),VERSION,ranked,localIP,desc);

	CF_Log(2, "announcing master server, that this server has started");
	AsyncTasks::addTask(asyncAnnounce, params, NULL); // add task to the queue, from which will the other thread pop and do it
}

//----------------------------------------------------------------------------------------------------
// called from main thread - schedules sending an updated server status to the master server
void MSrvConnection::updateServerInfo()
{
	char localIP [16];
	InformParams * params = new InformParams;

	// gather needed info and schedule contacting master server
	int port  = gEnv->pConsole->GetCVar("sv_port")->GetIVal();
	int maxpl = gEnv->pConsole->GetCVar("sv_maxplayers")->GetIVal();
	int numpl = g_pGame->GetGameRules()->GetPlayerCount();
	const char * svname = gEnv->pConsole->GetCVar("sv_servername")->GetString();
	const char * svpass = strlen(gEnv->pConsole->GetCVar("sv_password")->GetString()) > 0 ? "true" : "";
	const char * map = g_pGame->GetIGameFramework()->GetLevelName(); map = map ? map : "";
	int remtime = (int)g_pGame->GetGameRules()->GetRemainingGameTime();
	std::string maplink = getMapDownloadLink(map);
	std::string plstring = gatherPlayersInfo();
	int ranked = gEnv->pConsole->GetCVar("sv_ranked")->GetIVal();
	NetworkUtils::GetLocalIP(localIP);
	const char * desc = getServerDescription();

	params->page = formatURL("/api/up.php?port=%d&numpl=%d&name=%s&pass=%s&cookie=%s&map=%s&timel=%d&mapdl=%s&players=%s&ver=%s&ranked=%d&local=%s&desc=%s",
		                               port,   numpl, svname, svpass,cookie.c_str(),map, remtime, maplink.c_str(), plstring.c_str(),VERSION,ranked,localIP,desc);

	CF_Log(4, "sending updated server status to master server");
	AsyncTasks::addTask(asyncUpdate, params, NULL); // add task to the queue, from which will the other thread pop and do it
}

//----------------------------------------------------------------------------------------------------
void * MSrvConnection::asyncAnnounce(void * arg)
{
	std::string response;
	InformParams * info = (InformParams *)arg;

	// connect to the master server, send request and wait for response
	response = queryHTTP(info->page, HTTPPOST, HTTP11);
	if (!response.length()) {
		logErrorArg("request was: %s", info->page.c_str());
		timer = 60;
		goto end1;
	}
	cookie = extractCookie(response);
	if (!cookie.length()) {
		logErrorArg("request was: %s", info->page.c_str());
		//timer = 10;
		goto end1;
	}
	announced = true;
		
 end1:
	delete info;
	return NULL;
}

//----------------------------------------------------------------------------------------------------
void * MSrvConnection::asyncUpdate(void * arg)
{	
	std::string response;
	InformParams * info = (InformParams *)arg;

	// connect to the master server, send request and wait for response
	response = queryHTTP(info->page, HTTPPOST, HTTP11);
	if (!response.length()) {
		logErrorArg("request was: %s", info->page.c_str());
		timer = 10;
		goto end2;
	}
	if (!wasSuccessful(response)) {
		logErrorArg("request was: %s", info->page.c_str());
		announced = false;
		timer = 10;
		goto end2;
	}

 end2:
	delete info;
	return NULL;
}

//----------------------------------------------------------------------------------------------------
// called from SendChatMessage - checks the message for !validate command and splits it into arguments
bool MSrvConnection::checkChatMessage(EntityId sourceId, const char * message)
{
	char * msgcopy;
	char * prof;
	char * uid;
	char * name;
	int profId;

	if (!UseGameSpyReplacement)
		return false;
	if (!running)
		return true;

	if (strncmp(message, "!validate ", 10) != 0)
		return false;

	// make a copy because we need to modify it
	msgcopy = new char [strlen(message)+1];
	strcpy(msgcopy, message);

	// split input message into the char* strings
	prof = getNextArg(msgcopy + 9);
	uid = getNextArg(prof);
	name = getNextArg(uid);
	if (!prof || !uid || !name || !sscanf(prof, "%d", &profId)) {
		delete msgcopy;
		return true;
	}

	validateClient(sourceId, profId, uid, name);

	delete msgcopy;
	return true;
}

//----------------------------------------------------------------------------------------------------
// called from main thread when !validate is received - schedules verifying profileId at master server
void MSrvConnection::validateClient(EntityId sourceId, int profId, const char * uid, const char * name)
{
	std::map<std::string, ConnInfo>::const_iterator it;
	ValidateParams * Vparams;
	CActor * actor = g_pGame->GetGameRules()->GetActorByEntityId(sourceId);
	int channel = g_pGame->GetGameRules()->GetChannelId(sourceId);

	// old temporary profile ID range - no longer supported
	if (profId >= 800000 && profId <= 999999) {
		CF_Log(1, "kicking %s with outdated multiplayer client (old profile %d)", actor->GetEntity()->GetName(), profId);
		INetChannel* pNetChannel = g_pGame->GetGameRules()->GetGameFramework()->GetNetChannel((int)channel);
		if (!pNetChannel)
			return;
		pNetChannel->Disconnect(eDC_Kicked, "outdated client (download new at crymp.net)");
		return;
	}

	// exception for temporary random profiles - approve without validation
	if (profId >= 1000000 && profId <= 2000000) {
		CF_Log(2, "approving temporary profile %d for %s", profId, actor->GetEntity()->GetName());
		OnValidLogin(channel, sourceId, profId, name);
		return;
	}

	// client with this uID has already validated himself, this is probably restart or map change,
	// pickup his previous profileID and don't bother master server again
	it = validated->find(uid);
	if (it != validated->end()) {
		CF_Log(2, "using previous profile %d of %s (acc name: %s)", profId, actor->GetEntity()->GetName(), it->second.name.c_str());
		OnValidLogin(channel, sourceId, it->second.profId, it->second.name.c_str());
		return;
	}

	Vparams = new ValidateParams;
	Vparams->page = formatURL("/api/validate.php?prof=%d&uid=%s", profId, uid);
	Vparams->channelId = channel;
	Vparams->playerId = sourceId;
	Vparams->uID = uid;
	Vparams->profileId = profId;
	Vparams->name = name;

	CF_Log(2, "validating profile %d of %s at master server", profId, actor->GetEntity()->GetName());
	AsyncTasks::addTask(asyncValidate, Vparams, handleValidation); // send task to the second thread
}

//----------------------------------------------------------------------------------------------------
void * MSrvConnection::asyncValidate(void * arg)
{
	ValidateParams * params = (ValidateParams *)arg;
	ValidateResult * result = new ValidateResult;
	
	// connect to the master server, send request and wait for response
	result->response = queryHTTP(params->page, HTTPPOST, HTTP11);
	result->channelId = params->channelId;
	result->playerId = params->playerId;
	result->profileId = params->profileId;
	result->uID = params->uID;
	result->name = params->name;

	delete params;
	return result;
}

//----------------------------------------------------------------------------------------------------
// called from main thread on update - checks if second thread finished some tasks and processes the results
void * MSrvConnection::handleValidation(void * arg)
{
	ValidateResult * result = (ValidateResult *)arg;
	CActor * actor = g_pGame->GetGameRules()->GetActorByEntityId(result->playerId);
	IEntity * entity = gEnv->pEntitySystem->GetEntity(result->playerId);

	if (!entity && !actor) {
		CF_Log(2, "player on channel %d left before validation (no entity and no actor)", result->channelId);
	} else if (!entity) {
		CF_Log(2, "player on channel %d left before validation (no entity)", result->channelId);
	} else if (!actor) {
		CF_Log(2, "player on channel %d left before validation (no actor)", result->channelId);
	} else if (isLoginValid(result->response)) {
		CF_Log(2, "profile %d of %s (acc name: %s) is valid", result->profileId, entity->GetName(), result->name.c_str());
		(*validated)[result->uID] = ConnInfo(result->profileId, result->name); // save profileId for restart or map change
		OnValidLogin(result->channelId, result->playerId, result->profileId, result->name.c_str());
	} else {
		CF_Log(2, "profile %d of %s (acc name: %s) is invalid", result->profileId, entity->GetName(), result->name.c_str());
		OnInvalidLogin(result->channelId, result->playerId, result->profileId, result->name.c_str());
	}
	
	delete result;
	return NULL;
}

//----------------------------------------------------------------------------------------------------
void MSrvConnection::OnValidLogin(int channelId, EntityId playerId, int profileId, const char * name)
{
	CGameRules * gr = g_pGame->GetGameRules();
	if (gr) {
		char strProfileID [16];
		sprintf(strProfileID, "%d", profileId);
		Script::CallMethod(gr->GetScriptTable(), "OnValidLogin", channelId, ScriptHandle(playerId), strProfileID, name);
	}
}

void MSrvConnection::OnInvalidLogin(int channelId, EntityId playerId, int profileId, const char * name)
{
	CGameRules * gr = g_pGame->GetGameRules();
	if (gr) {
		char strProfileID[16];
		sprintf(strProfileID, "%d", profileId);
		Script::CallMethod(gr->GetScriptTable(), "OnInvalidLogin", channelId, ScriptHandle(playerId), strProfileID, name);
	}
}

//----------------------------------------------------------------------------------------------------
// retrieves a server description from the Options.lua script
const char * MSrvConnection::getServerDescription()
{
	ScriptAnyValue cfg, Messages, ServerDescription;

	if (!gEnv->pScriptSystem->GetGlobalAny("cfg", cfg) || cfg.GetVarType() != svtObject)
		returnError("", "cfg table does not exist");
	if (!cfg.table->GetValueAny("AutoMessages", Messages) || Messages.GetVarType() != svtObject)
		returnError("", "cfg.AutoMessages table does not exist");
	if (!Messages.table->GetValueAny("ServerDescription", ServerDescription) || ServerDescription.GetVarType() != svtString)
		returnError("", "cfg.AutoMessages.ServerDescription table does not exist");

	return ServerDescription.str;
}

//----------------------------------------------------------------------------------------------------
// retrieves a download link for the map
std::string MSrvConnection::getMapDownloadLink(const char * mapName)
{
	std::string map, link;
	std::ifstream links;
	std::string filePath;
	bool found = false;
	
	filePath = "Mods/CryFire/MapDownloadLinks.txt";
	links.open(filePath.c_str());
	if (!links.is_open())
		returnErrorSys("", "can't open file Mods/CryFire/MapDownloadLinks.txt");

	while (links >> map >> link) {
		if (strcmp(map.c_str(), mapName) == 0) {
			found = true;
			break;
		}
	}
	if (!found)
		returnErrorArg("", "map link not found (%s)", mapName);

	links.close();
	if (link.length() < 7)
		returnError("", "map link is too short");
	if (link.compare(0, 7, "http://") == 0)
		return link.substr(7);
	return link;
}

//----------------------------------------------------------------------------------------------------
// iterates over connected players and constructs a string containing info about them
std::string MSrvConnection::gatherPlayersInfo()
{
	std::ostringstream osstream;

	const char * name;
	const char * profileId;
	int kills, deaths, rank;
	CGameRules::TPlayers players;
	IActorSystem * actorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	IEntitySystem * entitySystem = gEnv->pEntitySystem;
	CActor * plActor;
	IEntity * plEntity;
	IScriptTable * plTable;

	if (g_pGame->GetGameRules()->GetPlayerCount() <= 0)
		return ""; // no players in server

	g_pGame->GetGameRules()->GetPlayers(players);
	for (CGameRules::TPlayers::const_iterator pit = players.begin(); pit != players.end(); pit++) {
		plActor = static_cast<CActor *>(actorSystem->GetActor(*pit));
		if (!plActor)
			continue;
		plEntity = entitySystem->GetEntity(*pit);
		if (!plEntity)
			continue;
		plTable = plEntity->GetScriptTable();
		if (!plTable)
			continue;
		name = plEntity->GetName();
		kills = getKills(*pit);
		deaths = getDeaths(*pit);
		rank = getRank(*pit);
		profileId = NULL;
		plTable->GetValue("profile", profileId);
		osstream << '@' << name << '%' << rank << '%' << kills << '%' << deaths << '%' << (profileId ? profileId : "0");
	}

	return osstream.str();
}

//----------------------------------------------------------------------------------------------------
// improve later
std::string MSrvConnection::formatURL(const char * format, ...)
{
	char o[16656];
	const char *ptr=format;
	char *optr=o;
	va_list vl;
	va_start(vl,format);
	while(*ptr){
		if(*ptr=='%'){
			char w=*(ptr+1);
			if(w=='%'){
				*optr='%'; optr++; ptr++; continue;
			}
			if(w=='d'){
				char buff[12];
				int val=va_arg(vl,int);
				itoa(val,buff,10);
				if(val<0){
					*optr='%'; optr++;
					*optr='2'; optr++;
					*optr='D'; optr++;
					char *b=buff+1;
					while(*b){
						*optr=*b; optr++; b++;
					}
				} else{
					char *b=buff;
					while(*b){
						*optr=*b; optr++; b++;
					}
				}
				ptr++;
			} else if(w=='s'){
				char *str=va_arg(vl,char*);
				char hex[]="0123456789ABCDEF";
				while(*str){
					if((*str>='A' && *str<='Z') || (*str>='a' && *str<='z') || (*str>='0' && *str<='9')){
						*optr=*str;
						optr++; str++;
					} else {
						*optr='%'; optr++;
						*optr=(*(hex+((*str>>4)&0xF))); optr++;
						*optr=(*(hex+((*str)&0xF))); optr++;
						str++;
					}
				}
				ptr++;
			} else if(w=='f'){
				char buff[310];
				double val=va_arg(vl,double);
				sprintf(buff,"%f",val);
				if(val<0){
					*optr='%'; optr++;
					*optr='2'; optr++;
					*optr='D'; optr++;
					char *b=buff+1;
					while(*b){
						*optr=*b; optr++; b++;
					}
				} else{
					char *b=buff;
					while(*b){
						*optr=*b; optr++; b++;
					}
				}
				ptr++;
			}
		} else {
			*optr=*ptr;
			optr++;
		}
		ptr++;
	}
	*(optr)=0;
	va_end(vl);
	return std::string(o);
}

//----------------------------------------------------------------------------------------------------
// called from the new thread - connects to the master server, sends the desired request and retrieves the answer
std::string MSrvConnection::queryHTTP(const std::string & page, HTTPMethod method, HTTPVersion version)
{
	char buffer [131070];
	std::string request;
	std::string response;
	const char * toSend;
	int length, sent, error;

	if ((sockFd = NetworkUtils::prepareSocket()) < 0)
		returnErrorSys("", "prepare socket failed");
	if (!NetworkUtils::setTimeout(sockFd, TIMEOUT))
		returnErrorSys("", "set timeout failed");

	if (connect(sockFd, (sockaddr*)&MSrvAddr, sizeof(MSrvAddr)) == SOCKET_ERROR)
		returnErrorSys("", "connect failed");		

	request = createRequest(page, method, version);

	toSend = request.c_str();
	length = request.length();
	if (request.length() > 1024) {
		for (sent = 0; sent < length; sent += 1024)
			if (send(sockFd, toSend+sent, min(1024, length-sent), 0) == SOCKET_ERROR)
				returnErrorSys("", "send failed");
	}
	else
		if (send(sockFd, toSend, length, NULL) == SOCKET_ERROR)
			returnErrorSys("", "send failed");

	while ((length = recv(sockFd, buffer, sizeof(buffer), 0)) > 0)
		response.append(buffer, length);

	error = GetLastError();
	if (error && !response.length())
		returnErrorArg("", "no response (%d)", error);
	if (!response.length())
		returnError("", "empty response");

	closesocket(sockFd);
	sockFd = -1;

	return response;
}

//----------------------------------------------------------------------------------------------------
// constructs a string of HTTP request
std::string MSrvConnection::createRequest(const std::string & page, HTTPMethod method, HTTPVersion version)
{
	std::string append;
	std::ostringstream request;
	int pos;

	if (method == HTTPGET)
		request << "GET ";
	else
		request << "POST ";
	if (method == HTTPPOST && (pos = page.find('?')) != std::string::npos) {
		request << page.substr(0, pos) << ' ';
		append = page.substr(pos+1);
	}
	else
		request << page;
	if (version != HTTP11)
		request << "HTTP/1.0\r\n";
	else
		request << "HTTP/1.1\r\nUser-Agent: " << gEnv->pConsole->GetCVar("sv_servername")->GetString() << "\r\n";
	request << "Host: " << HOSTNAME << "\r\n";
	if (method == HTTPPOST)
		request << "Content-length: " << append.length() << "\r\n" << "Content-Type: application/x-www-form-urlencoded\r\n";
	request << "\r\n";
	if (append.length())
		request << append << "\r\n";

	return request.str();
}

//----------------------------------------------------------------------------------------------------
// extracts a cookie from a response string
std::string MSrvConnection::extractCookie(const std::string & response)
{
	std::string content;
	size_t pos, sepPos;

	sepPos = response.find("\r\n\r\n");
	if (sepPos == std::string::npos)
		returnError("", "invalid structure of HTTP response");
	if (response.length() >= sepPos+7 && response.compare(sepPos+4, 3, "\xEF\xBB\xBF") == 0)
		content = response.substr(sepPos+7);
	else
		content = response.substr(sepPos+4);
	if (content.length() < 10)// || content.compare(0, 10, "<<Cookie>>") != 0)
		returnErrorArg("", "cookie is missing; content: %s", content.c_str());
	pos = content.find("<<Cookie>>");
	if (pos == std::string::npos)
		returnErrorArg("", "cookie is missing; content: %s", content.c_str());
	content = content.substr(pos+10, 32);

	return content;
}

//----------------------------------------------------------------------------------------------------
// tells if master server response indicates success
bool MSrvConnection::wasSuccessful(const std::string & response)
{
	std::string content;
	size_t pos, sepPos;

	sepPos = response.find("\r\n\r\n");
	if (sepPos == std::string::npos)
		returnError(false, "invalid structure of HTTP response");
	if (response.length() >= sepPos+7 && response.compare(sepPos+4, 3, "\xEF\xBB\xBF") == 0)
		content = response.substr(sepPos+7);
	else
		content = response.substr(sepPos+4);
	if (content.length() < 2)// || content.compare(0, 2, "OK") != 0)
		returnErrorArg(false, "master server reports error; content: %s", content.c_str());
	pos = content.find("OK");
	if (pos == std::string::npos)
		returnErrorArg(false, "master server reports error; content: %s", content.c_str());

	return true;
}

//----------------------------------------------------------------------------------------------------
bool MSrvConnection::isLoginValid(const std::string & response)
{
	std::string content;
	size_t sepPos;

	if (!response.length())
		returnError(false, "response is empty");
	sepPos = response.find("\r\n\r\n");
	if (sepPos == std::string::npos)
		returnError(false, "invalid structure of HTTP response");
	content = response.substr(sepPos+4);
	if (content.find("%Validation:Failed%") != std::string::npos)
		return false;

	return true;
}

//----------------------------------------------------------------------------------------------------
int MSrvConnection::getKills(EntityId entId)
{
	int val = 0;
	IEntity * entity = gEnv->pEntitySystem->GetEntity(entId);
	if (entity)
		g_pGame->GetGameRules()->GetSynchedEntityValue(entId, SCORE_KILLS_KEY, val);
	return val;
}

//----------------------------------------------------------------------------------------------------
int MSrvConnection::getDeaths(EntityId entId)
{
	int val = 0;
	IEntity * entity = gEnv->pEntitySystem->GetEntity(entId);
	if (entity)
		g_pGame->GetGameRules()->GetSynchedEntityValue(entId, SCORE_DEATHS_KEY, val);
	return val;
}

//----------------------------------------------------------------------------------------------------
int MSrvConnection::getRank(EntityId entId)
{
	int val = 0;
	IEntity * entity = gEnv->pEntitySystem->GetEntity(entId);
	const char * gameRules = g_pGame->GetGameRules()->GetEntity()->GetClass()->GetName();
	if (entity && strcmp(gameRules, "PowerStruggle") == 0)
		g_pGame->GetGameRules()->GetSynchedEntityValue(entId, RANK_KEY, val);
	return val;
}

//----------------------------------------------------------------------------------------------------
char * MSrvConnection::getNextArg(char * pos)
{
	if (!pos) return NULL;

	while (true) {
		if (!*pos) return NULL;
		if (*pos == ' ') break;
		pos++;
	}
	while (true) {
		if (!*pos) return NULL;
		if (*pos != ' ') break;
		*pos = '\0';
		pos++;
	}

	return pos;
}
