//================================================================================
// File:   Code/CryFire/FSUtils.cpp
//                ____                        ____
// Project: SSM  /\  _ `\                    /\  _`\   __
//               \ \ \/\_\    _  __   __  __ \ \ \_/  /\_\    _  __     ___
//                \ \ \/_/_  /\`'__\ /\ \/\ \ \ \  _\ \/_/   /\`'__\  /' __`\
//                 \ \ \_\ \ \ \ \_/ \ \ \_\ \ \ \ \/   /\`\ \ \ \_/ /\  \__/
//                  \ \____/  \ \_\   \/`____ \ \ \_\   \ \_\ \ \_\  \ \______\
//                   \/___/    \/_/    `/___/\ \ \/_/    \/_/  \/_/   \/_____ /
//                                         /\___/
//                                         \/__/
// Created on:  10.9.2014
// Last edited: 2026 Refactor for VS2022
//--------------------------------------------------------------------------------
// Description: File System utilities for CryFire (VS2022 Native WinAPI Migrated)
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
#include <sys/stat.h>
#include <windows.h> // Native WinAPI replacement for dirent.h

#include "Game.h"
#include "GameRules.h"

// Define PATH_MAX fallback if not pulled via headers in VS2022
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif

typedef bool (*cmdFunc)(const char*);

char WDir[PATH_MAX];

void ConsoleMessage(CActor* pActor, const char* message)
{
#if 0
	CGameRules* pGameRules = g_pGame->GetGameRules();
	if (!pGameRules)
		return;
	pGameRules->SendTextMessage(eTextMessageConsole, message, eRMI_ToClientChannel, pActor->GetChannelId());
#endif
}

bool exists(const std::string& path)
{
#if 0
	struct stat properties;
	return (stat(path.c_str(), &properties) != -1);
#endif
	return false;
}

bool isDir(const std::string& path)
{
#if 0
	struct stat properties;
	if (stat(path.c_str(), &properties) == -1)
		return false;
	return (properties.st_mode & S_IFDIR) != 0;
#endif
	return false;
}

bool deleteFile(const std::string& path)
{
#if 0
	return remove(path.c_str()) == 0;
#endif
	return false;
}

bool deleteDir(const std::string& path)
{
#if 0
	return RemoveDirectoryA(path.c_str()) != 0;
#endif
	return false;
}

void printWDir(CActor* pActor);
void changeWDir(CActor* pActor, const char* args);
void listWDir(CActor* pActor);
void readFile(CActor* pActor, const char* args);
void createFile(CActor* pActor, const char* args);
void appendToFile(CActor* pActor, const char* args);
void modifyFile(CActor* pActor, const char* args);
void deleteEntry(CActor* pActor, const char* args);

bool tryDeleteDir(CActor* pActor, const std::string& path);
bool tryDeleteDirContent(CActor* pActor, const std::string& path);
bool tryDeleteFile(CActor* pActor, const std::string& path);

void printWDir(CActor* pActor)
{
#if 0
	char message[26 + PATH_MAX];
	sprintf(message, "$8[Access]$9 working dir: %s", WDir);
	ConsoleMessage(pActor, message);
#endif
}

void changeWDir(CActor* pActor, const char* args) // path
{
#if 0
	char message[32 + PATH_MAX];
	char prevWDir[PATH_MAX];
	char entry[64];
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
			strcpy(WDir + endPos + 1, entry);
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
#endif
}

void listWDir(CActor* pActor)
{
#if 0
	char message[128];
	std::string searchPath = WDir;
	searchPath += "\\*";

	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

	if (hFind == INVALID_HANDLE_VALUE) {
		sprintf(message, "$8[Access]$9 unable to open the directory (Error %lu)", GetLastError());
		ConsoleMessage(pActor, message);
		return;
	}

	ConsoleMessage(pActor, "$8[Access]$9 content of working directory:");

	do {
		std::string entrypath = WDir;
		entrypath += '\\';
		entrypath += findData.cFileName;

		struct stat props;
		if (stat(entrypath.c_str(), &props) == -1) {
			sprintf(message, "can't read info about %s", findData.cFileName);
			ConsoleMessage(pActor, message);
			continue;
		}

		// Mock/Calculate a relative pseudo-inode index mapping to replace old direntry->d_ino
		unsigned long pseudoIno = findData.ftCreationTime.dwLowDateTime ^ findData.ftLastWriteTime.dwLowDateTime;

		sprintf(message, "%5lu   %-25s %10ld %10ld %10ld %10ld\n",
			pseudoIno, findData.cFileName, props.st_size, props.st_ctime, props.st_mtime, props.st_atime);
		ConsoleMessage(pActor, message);

	} while (FindNextFileA(hFind, &findData) != 0);

	FindClose(hFind);
#endif
}

void readFile(CActor* pActor, const char* args) // filename
{
#if 0
	char buffer[128];
	int bufpos = 0;
	int line = 1;
	std::string fileName;
	FILE* file;
	int c;

	while (*args == ' ') { args++; }
	if (*args == '\0') {
		ConsoleMessage(pActor, "$8[Access]$9 invalid args");
		return;
	}

	fileName += WDir; fileName += '\\'; fileName += args;

	// Safe alternative for VS2022 deprecation warnings on legacy fopen
	if (fopen_s(&file, fileName.c_str(), "r") != 0 || !file) {
		sprintf_s(buffer, "$8[Access]$9 unable to open the file (%s)", strerror(errno));
		ConsoleMessage(pActor, buffer);
		return;
	}

	ConsoleMessage(pActor, "$8[Access]$9 content of the file:");
	bufpos = sprintf_s(buffer, "%4d ", line);
	while ((c = fgetc(file)) != EOF) {
		if (c == '\n') {
			buffer[bufpos] = '\0';
			ConsoleMessage(pActor, buffer);
			bufpos = sprintf_s(buffer, "%4d ", ++line);
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
				bufpos += sprintf_s(buffer + bufpos, sizeof(buffer) - bufpos, "\\x%02X", (unsigned int)(unsigned char)c);
		}
		if (bufpos >= 100) {
			buffer[bufpos] = '\0';
			ConsoleMessage(pActor, buffer);
			bufpos = sprintf_s(buffer, "%4d ", ++line);
		}
	}
	buffer[bufpos] = '\0';
	ConsoleMessage(pActor, buffer);

	fclose(file);
#endif
}

void createFile(CActor* pActor, const char* args) // filename
{

}

void appendToFile(CActor* pActor, const char* args) // filename, text
{

}

void modifyFile(CActor* pActor, const char* args) // filename, line, text
{

}

void deleteEntry(CActor* pActor, const char* args)
{
#if 0
	char message[26 + PATH_MAX];
	std::string fileName;

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
#endif
}

bool tryDeleteDir(CActor* pActor, const std::string& path)
{
#if 0
	char message[64 + PATH_MAX];
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
#endif
	return false;
}

bool tryDeleteDirContent(CActor* pActor, const std::string& path)
{
#if 0
	char message[128];
	std::string searchPath = path + "\\*";

	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

	if (hFind == INVALID_HANDLE_VALUE) {
		sprintf(message, "unable to open directory (Error %lu)", GetLastError());
		ConsoleMessage(pActor, message);
		return false;
	}

	bool success = true;

	do {
		// Skip "." and ".."
		if (findData.cFileName[0] == '.' && (findData.cFileName[1] == '\0' || (findData.cFileName[1] == '.' && findData.cFileName[2] == '\0')))
			continue;

		std::string entryPath = path + '\\' + findData.cFileName;

		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			success &= tryDeleteDir(pActor, entryPath);
		else
			success &= tryDeleteFile(pActor, entryPath);

	} while (FindNextFileA(hFind, &findData) != 0);

	FindClose(hFind);
	return success;
#endif
	return false;
}

bool tryDeleteFile(CActor* pActor, const std::string& path)
{
#if 0
	char message[128];
	bool deleted = deleteFile(path);
	if (!deleted) {
		sprintf(message, "failed to delete file %s (%s)", path.c_str(), strerror(errno));
		ConsoleMessage(pActor, message);
	}
	return deleted;
#endif
	return false;
}