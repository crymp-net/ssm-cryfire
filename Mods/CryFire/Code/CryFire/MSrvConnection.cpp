//================================================================================
// File:    Code/CryFire/MSrvConnection.cpp
// Refactored to WinHTTP with Mandatory TLS/HTTPS
//================================================================================

#include "StdAfx.h"
#include "MSrvConnection.h"
#include "NetworkUtils.h"
#include "AsyncTasks.h"
#include "Game.h"
#include "GameRules.h"

#include <windows.h>
#include <winhttp.h> // Modern WinHTTP Header
#include <string>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>

// Link WinHTTP library explicitly via compiler pragma
#pragma comment(lib, "winhttp.lib")

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

// Struct Definitions
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

// Static Constants Refactoring
const char* const MSrvConnection::HOSTNAME = "crymp.org";
const uint         MSrvConnection::PORT = 443;       // Refactored from 80 to 443
const uint         MSrvConnection::TIMEOUT = 10000;     // milliseconds
const uint         MSrvConnection::DELAY = 30;        // seconds
const char* const MSrvConnection::VERSION = "6156";

bool MSrvConnection::UseGameSpyReplacement = false;
bool MSrvConnection::running = false;
bool MSrvConnection::announced = false;
float MSrvConnection::timer = 0;
fd_t MSrvConnection::sockFd = -1; // Unused in WinHTTP variant but retained for signature compatibility
struct sockaddr_in MSrvConnection::MSrvAddr;
std::string MSrvConnection::cookie;
std::map<std::string, MSrvConnection::ConnInfo>* MSrvConnection::validated = NULL;

// Helper: Safely converts ASCII std::string to UTF-16 std::wstring
static std::wstring AnsiToWide(const std::string& str)
{
	if (str.empty()) return L"";
	int size_needed = MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

//----------------------------------------------------------------------------------------------------
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
void MSrvConnection::initialize()
{
	if (!validated)
		validated = new std::map<std::string, ConnInfo>;

	if (!running) {
		announced = false;
		AsyncTasks::addTask(asyncGetMSrvAddr, NULL, NULL);
	}
	timer = 2;
}

//----------------------------------------------------------------------------------------------------
void* MSrvConnection::asyncGetMSrvAddr(void* arg)
{
	// Retained for framework initialization architecture compatibility
	running = true;
	return NULL;
}

//----------------------------------------------------------------------------------------------------
void MSrvConnection::terminate()
{
	running = false;
}

//----------------------------------------------------------------------------------------------------
void MSrvConnection::onUpdate(float frameTime)
{
	if (!running)
		return;

	checkUpdateTime(frameTime);
}

//----------------------------------------------------------------------------------------------------
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
void MSrvConnection::announceServerStart()
{
	char localIP[16];
	InformParams* params = new InformParams;

	int port = gEnv->pConsole->GetCVar("sv_port")->GetIVal();
	int maxpl = gEnv->pConsole->GetCVar("sv_maxplayers")->GetIVal();
	int numpl = 0;
	const char* svname = gEnv->pConsole->GetCVar("sv_servername")->GetString();
	const char* svpass = strlen(gEnv->pConsole->GetCVar("sv_password")->GetString()) > 0 ? "true" : "";
	const char* map = g_pGame->GetIGameFramework()->GetLevelName(); map = map ? map : "";
	int remtime = (int)g_pGame->GetGameRules()->GetRemainingGameTime();
	std::string maplink = getMapDownloadLink(map);
	int ranked = gEnv->pConsole->GetCVar("sv_ranked")->GetIVal();
	NetworkUtils::GetLocalIP(localIP);
	const char* desc = getServerDescription();

	params->page = formatURL("/api/reg.php?port=%d&maxpl=%d&numpl=%d&name=%s&pass=%s&map=%s&timel=%d&mapdl=%s&ver=%s&ranked=%d&local=%s&desc=%s",
		port, maxpl, numpl, svname, svpass, map, remtime, maplink.c_str(), VERSION, ranked, localIP, desc);

	CF_Log(2, "announcing master server over HTTPS, that this server has started");
	AsyncTasks::addTask(asyncAnnounce, params, NULL);
}

//----------------------------------------------------------------------------------------------------
void MSrvConnection::updateServerInfo()
{
	char localIP[16];
	InformParams* params = new InformParams;

	int port = gEnv->pConsole->GetCVar("sv_port")->GetIVal();
	int maxpl = gEnv->pConsole->GetCVar("sv_maxplayers")->GetIVal();
	int numpl = g_pGame->GetGameRules()->GetPlayerCount();
	const char* svname = gEnv->pConsole->GetCVar("sv_servername")->GetString();
	const char* svpass = strlen(gEnv->pConsole->GetCVar("sv_password")->GetString()) > 0 ? "true" : "";
	const char* map = g_pGame->GetIGameFramework()->GetLevelName(); map = map ? map : "";
	int remtime = (int)g_pGame->GetGameRules()->GetRemainingGameTime();
	std::string maplink = getMapDownloadLink(map);
	std::string plstring = gatherPlayersInfo();
	int ranked = gEnv->pConsole->GetCVar("sv_ranked")->GetIVal();
	NetworkUtils::GetLocalIP(localIP);
	const char* desc = getServerDescription();

	params->page = formatURL("/api/up.php?port=%d&numpl=%d&name=%s&pass=%s&cookie=%s&map=%s&timel=%d&mapdl=%s&players=%s&ver=%s&ranked=%d&local=%s&desc=%s",
		port, numpl, svname, svpass, cookie.c_str(), map, remtime, maplink.c_str(), plstring.c_str(), VERSION, ranked, localIP, desc);

	CF_Log(4, "sending updated server status over HTTPS to master server");
	AsyncTasks::addTask(asyncUpdate, params, NULL);
}

//----------------------------------------------------------------------------------------------------
void* MSrvConnection::asyncAnnounce(void* arg)
{
	std::string response;
	InformParams* info = (InformParams*)arg;

	response = queryHTTP(info->page, HTTPPOST, HTTP11);
	if (!response.length()) {
		logErrorArg("request failed: %s", info->page.c_str());
		timer = 60;
		goto end1;
	}
	cookie = extractCookie(response);
	if (!cookie.length()) {
		logErrorArg("Cookie extraction failure from response data. Raw endpoint context: %s", info->page.c_str());
		goto end1;
	}
	announced = true;

end1:
	delete info;
	return NULL;
}

//----------------------------------------------------------------------------------------------------
void* MSrvConnection::asyncUpdate(void* arg)
{
	std::string response;
	InformParams* info = (InformParams*)arg;

	response = queryHTTP(info->page, HTTPPOST, HTTP11);
	if (!response.length()) {
		logErrorArg("request failed: %s", info->page.c_str());
		timer = 10;
		goto end2;
	}
	if (!wasSuccessful(response)) {
		logErrorArg("master rejected validation logic: %s", info->page.c_str());
		announced = false;
		timer = 10;
		goto end2;
	}

end2:
	delete info;
	return NULL;
}

//----------------------------------------------------------------------------------------------------
bool MSrvConnection::checkChatMessage(EntityId sourceId, const char* message)
{
	if (!UseGameSpyReplacement) return false;
	if (!running) return true;
	if (strncmp(message, "!validate ", 10) != 0) return false;

	char* msgcopy = new char[strlen(message) + 1];
	strcpy(msgcopy, message);

	char* prof = getNextArg(msgcopy + 9);
	char* uid = getNextArg(prof);
	char* name = getNextArg(uid);
	int profId;

	if (!prof || !uid || !name || !sscanf(prof, "%d", &profId)) {
		delete[] msgcopy;
		return true;
	}

	validateClient(sourceId, profId, uid, name);
	delete[] msgcopy;
	return true;
}

//----------------------------------------------------------------------------------------------------
void MSrvConnection::validateClient(EntityId sourceId, int profId, const char* uid, const char* name)
{
	std::map<std::string, ConnInfo>::const_iterator it;
	ValidateParams* Vparams;
	CActor* actor = g_pGame->GetGameRules()->GetActorByEntityId(sourceId);
	int channel = g_pGame->GetGameRules()->GetChannelId(sourceId);

	if (profId >= 800000 && profId <= 999999) {
		CF_Log(1, "kicking %s with outdated multiplayer client (old profile %d)", actor->GetEntity()->GetName(), profId);
		INetChannel* pNetChannel = g_pGame->GetGameRules()->GetGameFramework()->GetNetChannel((int)channel);
		if (!pNetChannel) return;
		pNetChannel->Disconnect(eDC_Kicked, "outdated client (download new at crymp.net)");
		return;
	}

	if (profId >= 1000000 && profId <= 2000000) {
		CF_Log(2, "approving temporary profile %d for %s", profId, actor->GetEntity()->GetName());
		OnValidLogin(channel, sourceId, profId, name);
		return;
	}

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

	CF_Log(2, "validating profile %d of %s at master server via secure HTTPS channel", profId, actor->GetEntity()->GetName());
	AsyncTasks::addTask(asyncValidate, Vparams, handleValidation);
}

//----------------------------------------------------------------------------------------------------
void* MSrvConnection::asyncValidate(void* arg)
{
	ValidateParams* params = (ValidateParams*)arg;
	ValidateResult* result = new ValidateResult;

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
void* MSrvConnection::handleValidation(void* arg)
{
	ValidateResult* result = (ValidateResult*)arg;
	CActor* actor = g_pGame->GetGameRules()->GetActorByEntityId(result->playerId);
	IEntity* entity = gEnv->pEntitySystem->GetEntity(result->playerId);

	if (!entity || !actor) {
		CF_Log(2, "player on channel %d left before validation terminated", result->channelId);
	}
	else if (isLoginValid(result->response)) {
		CF_Log(2, "profile %d of %s (acc name: %s) verified successfully", result->profileId, entity->GetName(), result->name.c_str());
		(*validated)[result->uID] = ConnInfo(result->profileId, result->name);
		OnValidLogin(result->channelId, result->playerId, result->profileId, result->name.c_str());
	}
	else {
		CF_Log(2, "profile %d of %s (acc name: %s) is explicitly invalid", result->profileId, entity->GetName(), result->name.c_str());
		OnInvalidLogin(result->channelId, result->playerId, result->profileId, result->name.c_str());
	}

	delete result;
	return NULL;
}

// [OnValidLogin, OnInvalidLogin, getServerDescription, getMapDownloadLink remain identical to keep game system bindings functional]
void MSrvConnection::OnValidLogin(int channelId, EntityId playerId, int profileId, const char* name)
{
	CGameRules* gr = g_pGame->GetGameRules();
	if (gr) {
		char strProfileID[16];
		sprintf(strProfileID, "%d", profileId);
		Script::CallMethod(gr->GetScriptTable(), "OnValidLogin", channelId, ScriptHandle(playerId), strProfileID, name);
	}
}

void MSrvConnection::OnInvalidLogin(int channelId, EntityId playerId, int profileId, const char* name)
{
	CGameRules* gr = g_pGame->GetGameRules();
	if (gr) {
		char strProfileID[16];
		sprintf(strProfileID, "%d", profileId);
		Script::CallMethod(gr->GetScriptTable(), "OnInvalidLogin", channelId, ScriptHandle(playerId), strProfileID, name);
	}
}

const char* MSrvConnection::getServerDescription()
{
	ScriptAnyValue cfg, Messages, ServerDescription;
	if (!gEnv->pScriptSystem->GetGlobalAny("cfg", cfg) || cfg.GetVarType() != svtObject) return "";
	if (!cfg.table->GetValueAny("AutoMessages", Messages) || Messages.GetVarType() != svtObject) return "";
	if (!Messages.table->GetValueAny("ServerDescription", ServerDescription) || ServerDescription.GetVarType() != svtString) return "";
	return ServerDescription.str;
}

std::string MSrvConnection::getMapDownloadLink(const char* mapName)
{
	std::string map, link;
	std::ifstream links;
	links.open("Mods/CryFire/MapDownloadLinks.txt");
	if (!links.is_open()) return "";
	while (links >> map >> link) {
		if (strcmp(map.c_str(), mapName) == 0) break;
	}
	links.close();
	if (link.compare(0, 7, "http://") == 0) return link.substr(7);
	return link;
}

std::string MSrvConnection::gatherPlayersInfo()
{
	std::ostringstream osstream;
	CGameRules::TPlayers players;
	IActorSystem* actorSystem = gEnv->pGame->GetIGameFramework()->GetIActorSystem();
	if (g_pGame->GetGameRules()->GetPlayerCount() <= 0) return "";
	g_pGame->GetGameRules()->GetPlayers(players);
	for (CGameRules::TPlayers::const_iterator pit = players.begin(); pit != players.end(); pit++) {
		CActor* plActor = static_cast<CActor*>(actorSystem->GetActor(*pit));
		IEntity* plEntity = gEnv->pEntitySystem->GetEntity(*pit);
		if (!plActor || !plEntity) continue;
		IScriptTable* plTable = plEntity->GetScriptTable();
		if (!plTable) continue;
		const char* profileId = NULL;
		plTable->GetValue("profile", profileId);
		osstream << '@' << plEntity->GetName() << '%' << getRank(*pit) << '%' << getKills(*pit) << '%' << getDeaths(*pit) << '%' << (profileId ? profileId : "0") << getTeam(*pit);
	}
	return osstream.str();
}

std::string MSrvConnection::formatURL(const char* format, ...)
{
	char o[16656];
	const char* ptr = format;
	char* optr = o;
	va_list vl;
	va_start(vl, format);
	while (*ptr) {
		if (*ptr == '%') {
			char w = *(ptr + 1);
			if (w == '%') { *optr = '%'; optr++; ptr++; continue; }
			if (w == 'd') {
				char buff[12];
				int val = va_arg(vl, int);
				itoa(val, buff, 10);
				if (val < 0) {
					*optr = '%'; optr++; *optr = '2'; optr++; *optr = 'D'; optr++;
					char* b = buff + 1; while (*b) { *optr = *b; optr++; b++; }
				}
				else {
					char* b = buff; while (*b) { *optr = *b; optr++; b++; }
				}
				ptr++;
			}
			else if (w == 's') {
				char* str = va_arg(vl, char*);
				char hex[] = "0123456789ABCDEF";
				while (*str) {
					if ((*str >= 'A' && *str <= 'Z') || (*str >= 'a' && *str <= 'z') || (*str >= '0' && *str <= '9')) {
						*optr = *str; optr++; str++;
					}
					else {
						*optr = '%'; optr++;
						*optr = (*(hex + ((*str >> 4) & 0xF))); optr++;
						*optr = (*(hex + ((*str) & 0xF))); optr++;
						str++;
					}
				}
				ptr++;
			}
			else if (w == 'f') {
				char buff[310];
				double val = va_arg(vl, double);
				sprintf(buff, "%f", val);
				if (val < 0) {
					*optr = '%'; optr++; *optr = '2'; optr++; *optr = 'D'; optr++;
					char* b = buff + 1; while (*b) { *optr = *b; optr++; b++; }
				}
				else {
					char* b = buff; while (*b) { *optr = *b; optr++; b++; }
				}
				ptr++;
			}
		}
		else {
			*optr = *ptr; optr++;
		}
		ptr++;
	}
	*(optr) = 0;
	va_end(vl);
	return std::string(o);
}

//----------------------------------------------------------------------------------------------------
// Completely refactored HTTP Core leveraging secure WinHTTP
//----------------------------------------------------------------------------------------------------
std::string MSrvConnection::queryHTTP(const std::string& page, HTTPMethod method, HTTPVersion version)
{
	std::string responseData = "";
	HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;

	std::wstring wHost = AnsiToWide(HOSTNAME);
	const char* svNameCStr = gEnv->pConsole->GetCVar("sv_servername")->GetString();
	std::wstring wUserAgent = svNameCStr ? AnsiToWide(svNameCStr) : L"CryEngineServer";

	// 1. Initialize WinHTTP Session
	hSession = WinHttpOpen(wUserAgent.c_str(),
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) returnErrorSys("", "WinHttpOpen failed");

	// Configure targeted request timeouts explicitly matching legacy definitions
	WinHttpSetTimeouts(hSession, TIMEOUT, TIMEOUT, TIMEOUT, TIMEOUT);

	// 2. Open Connection - Enforcing HTTPS Port 443 strictly ignoring any port 80 declarations
	hConnect = WinHttpConnect(hSession, wHost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (!hConnect) {
		WinHttpCloseHandle(hSession);
		returnErrorSys("", "WinHttpConnect failed");
	}

	// 3. Deconstruct URL for GET/POST transformations
	std::string path = page;
	std::string postPayload = "";
	size_t queryPos = page.find('?');

	if (method == HTTPPOST && queryPos != std::string::npos) {
		path = page.substr(0, queryPos);
		postPayload = page.substr(queryPos + 1);
	}

	std::wstring wPath = AnsiToWide(path);
	LPCWSTR verb = (method == HTTPPOST) ? L"POST" : L"GET";

	// 4. Create Request Handle with explicit Security Flags enforced
	hRequest = WinHttpOpenRequest(hConnect, verb, wPath.c_str(),
		NULL, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES,
		WINHTTP_FLAG_SECURE); // Enforces SSL/TLS validation
	if (!hRequest) {
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		returnErrorSys("", "WinHttpOpenRequest failed");
	}

	// 5. Build Headers and Submit Payload
	BOOL bResults = FALSE;
	if (method == HTTPPOST) {
		std::wstring headers = L"Content-Type: application/x-www-form-urlencoded\r\n";
		bResults = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1,
			(LPVOID)postPayload.c_str(), (DWORD)postPayload.length(),
			(DWORD)postPayload.length(), 0);
	}
	else {
		bResults = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
	}

	if (!bResults) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		returnErrorSys("", "WinHttpSendRequest failure");
	}

	// Receive Response Header Verification
	if (!WinHttpReceiveResponse(hRequest, NULL)) {
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);
		returnErrorSys("", "WinHttpReceiveResponse failure");
	}

	// 6. Pull down Data Buffer
	DWORD dwSize = 0;
	do {
		dwSize = 0;
		if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
		if (dwSize == 0) break;

		std::vector<char> buffer(dwSize);
		DWORD dwDownloaded = 0;

		if (WinHttpReadData(hRequest, &buffer[0], dwSize, &dwDownloaded)) {
			responseData.append(&buffer[0], dwDownloaded);
		}
	} while (dwSize > 0);

	// Clean handles
	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);
	WinHttpCloseHandle(hSession);

	if (responseData.empty()) returnError("", "Received completely empty response from server block");
	return responseData;
}

//----------------------------------------------------------------------------------------------------
// Formats are simplified since WinHTTP abstracts standard HTTP protocol serialization wrappers entirely.
std::string MSrvConnection::createRequest(const std::string& page, HTTPMethod method, HTTPVersion version)
{
	return page;
}

//----------------------------------------------------------------------------------------------------
// Since WinHTTP returns only the response body payload (and drops technical headers like HTTP/1.1 200 OK), 
// response strings no longer possess "\r\n\r\n". We look for tokens directly in raw text.
std::string MSrvConnection::extractCookie(const std::string& response)
{
	if (response.empty()) returnError("", "Empty body data payload");

	size_t pos = response.find("<<Cookie>>");
	if (pos == std::string::npos) {
		returnErrorArg("", "cookie marker missing; content: %s", response.c_str());
	}
	return response.substr(pos + 10, 32);
}

//----------------------------------------------------------------------------------------------------
bool MSrvConnection::wasSuccessful(const std::string& response)
{
	if (response.empty()) returnError(false, "Null operational status payload");

	if (response.find("OK") == std::string::npos) {
		returnErrorArg(false, "master server report missing confirmation context: %s", response.c_str());
	}
	return true;
}

//----------------------------------------------------------------------------------------------------
bool MSrvConnection::isLoginValid(const std::string& response)
{
	if (response.empty()) returnError(false, "Empty registration verify context");
	if (response.find("%Validation:Failed%") != std::string::npos) return false;
	return true;
}

// [Unmodified Core Getters to preserve native game bindings]
int MSrvConnection::getKills(EntityId entId) {
	int val = 0; IEntity* entity = gEnv->pEntitySystem->GetEntity(entId);
	if (entity) g_pGame->GetGameRules()->GetSynchedEntityValue(entId, SCORE_KILLS_KEY, val);
	return val;
}

int MSrvConnection::getDeaths(EntityId entId) {
	int val = 0; IEntity* entity = gEnv->pEntitySystem->GetEntity(entId);
	if (entity) g_pGame->GetGameRules()->GetSynchedEntityValue(entId, SCORE_DEATHS_KEY, val);
	return val;
}

int MSrvConnection::getRank(EntityId entId) {
	int val = 0; IEntity* entity = gEnv->pEntitySystem->GetEntity(entId);
	const char* gameRules = g_pGame->GetGameRules()->GetEntity()->GetClass()->GetName();
	if (entity && strcmp(gameRules, "PowerStruggle") == 0) g_pGame->GetGameRules()->GetSynchedEntityValue(entId, RANK_KEY, val);
	return val;
}

int MSrvConnection::getTeam(EntityId entId) {
	return g_pGame->GetGameRules()->GetTeam(entId);
}

char* MSrvConnection::getNextArg(char* pos) {
	if (!pos) return NULL;
	while (true) { if (!*pos) return NULL; if (*pos == ' ') break; pos++; }
	while (true) { if (!*pos) return NULL; if (*pos != ' ') break; *pos = '\0'; pos++; }
	return pos;
}