//================================================================================
// File:    Code/CryFire/Hooking.h
//                 ____                       ____
// Project: SSM  /\  _ `\                    /\  _`\   __
//               \ \ \/\_\    _  __   __  __ \ \ \_/  /\_\    _  __     ___
//                \ \ \/_/_  /\`'__\ /\ \/\ \ \ \  _\ \/_/   /\`'__\  /' __`\
//                 \ \ \_\ \ \ \ \_/ \ \ \_\ \ \ \ \/   /\`\ \ \ \_/ /\  \__/
//                  \ \____/  \ \_\   \/`____ \ \ \_\   \ \_\ \ \_\  \ \_____\
//                   \/___/    \/_/    `/___/\ \ \/_/    \/_/  \/_/   \/____ /
//                                        /\___/
//                                        \/__/
// Created on:  18.2.2017
// Last edited: 18.2.2017
//--------------------------------------------------------------------------------
// Description: Hooking utils allowing to hack into compiled machine code
//              (! specific for x86, won't work on x64 !)
//--------------------------------------------------------------------------------
// Authors:     Patrick Glatt (HipHipHurra)
//              Jan Broz (Youda008)
//================================================================================


#ifndef HOOKING_INCLUDED
#define HOOKING_INCLUDED

//typedef unsigned int uint;
typedef unsigned char byte;

/** hooks code at dstAddress and inserts a call to tgtFunction without loosing overwritten instrucions
  * !WARNING! this is x86 specific, it will not work on x64
  * @param dstAddr address into code, where will a jmp instruction be placed
  * @param numBytes number of bytes that will be overwritten, MUST cover whole instructions, never half of any!
  * @param tgtFunction pointer to your function that you want to execute at dstAddress
  * @param numDwordArgs number of 4-byte values, that will be forwarded from hooked function into tgtFunction
  */
void hookCode( uint dstAddr, uint numBytes, void * tgtFunction, uint numDwordArgs, bool useEBPforArgs );

#endif // HOOKING_INCLUDED
