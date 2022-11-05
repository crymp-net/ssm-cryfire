//================================================================================
// File:    Code/CryFire/FSUtils.h
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


#include <string>
#include <IGameRulesSystem.h>
#include <IEntity.h>
#include "Actor.h"

bool exists(const std::string & path);
bool isDir(const std::string & path);
bool copyFile(const std::string & source, const std::string & target);
bool deleteFile(const std::string & path);
bool createDir(const std::string & path);
bool deleteDir(const std::string & path);
void printWDir(CActor * pActor);
void changeWDir(CActor * pActor, const char * args);
void listWDir(CActor * pActor);
void readFile(CActor * pActor, const char * args);
void deleteEntry(CActor * pActor, const char * args);
