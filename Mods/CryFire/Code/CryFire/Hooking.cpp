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


#include "StdAfx.h"

#include "Hooking.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <windows.h>

#include "CryFire/Logging.h"


//----------------------------------------------------------------------------------------------------
#define WRITE_INSTR( dst, instr )\
{\
	memcpy( dst, instr, sizeof( instr )-1 );\
	dst += sizeof( instr ) - 1;\
}
#define WRITE_DWORD( dst, num )\
{\
	*(int *)(dst) = (int)(num);\
	dst += 4;\
}
#define WRITE_BYTE( dst, b )\
{\
	*dst = (byte)(b);\
	dst += 1;\
}
#define WRITE_DATA( dst, src, len )\
{\
	memcpy( dst, src, len );\
	dst += len;\
}

// !WARNING! this is x86 specific, it will not work on x64
void hookCode( uint dstAddr, uint numBytes, void * tgtFunction, uint numDwordArgs, bool useEBPforArgs )
{
	byte * c = (byte *)dstAddr;
	//CryLogAlways("bytes at %08X: %02X %02X %02X %02X %02X %02X %02X %02X", dstAddr, c[0],c[1],c[2],c[3],c[4],c[5],c[6],c[7],c[8]);

	byte * dst = (byte *)dstAddr;
	byte * func = (byte *)tgtFunction;

	// we need to manually construct a block of code,
	// that will execute the few instructions we will overwrite with jump
	byte * newBlock = (byte *)malloc( 3 + numDwordArgs*(useEBPforArgs?3:4) + 5 + 6 + numBytes + 5 + 4 );
	byte * blk = newBlock;

	WRITE_INSTR( blk, "\x50" );  // push eax             we need to save even these volatite registers
	WRITE_INSTR( blk, "\x51" );  // push ecx             because we forcely interrupt the function in the middle
	WRITE_INSTR( blk, "\x52" );  // push edx
	for (uint i = 0; i < numDwordArgs; i++) { //         re-push all original arguments
		if (useEBPforArgs) {
			WRITE_INSTR( blk, "\xFF\x75" );    // push [ebp+x]
			WRITE_BYTE( blk, (numDwordArgs-i+1)*4 ); //     x
		} else {
			WRITE_INSTR( blk, "\xFF\x74\x24" );// push [esp+x]
			WRITE_BYTE( blk, (numDwordArgs+3)*4 ); //     x
		}
	}
	WRITE_INSTR( blk, "\xE8" );  // call
	WRITE_DWORD( blk, func - (blk + 4) );      // relative address of the target function
	WRITE_INSTR( blk, "\x83\xC4\x0C" ); // add esp,12    clean arguments on stack
	WRITE_INSTR( blk, "\x5A" );  // pop edx
	WRITE_INSTR( blk, "\x59" );  // pop ecx              restore registers
	WRITE_INSTR( blk, "\x58" );  // pop eax
	WRITE_DATA( blk, dst, numBytes ); //                 execute the original instructions overwritten with hook jump
	WRITE_INSTR( blk, "\xE9" );  // jmp
	WRITE_DWORD( blk, (dst + 5) - (blk + 4) ); // relative address of the next instruction in the original code
	WRITE_DWORD( blk, 0 );

	// make this block of code executable
	DWORD unused;
	if (VirtualProtect( blk, blk - newBlock, PAGE_EXECUTE_READWRITE, &unused ) == 0) {
		CF_LogError( "hooking %0X failed, unable to set jumpcave executable (%u)", dstAddr, GetLastError() );
		return;
	}

	// write a jump instruction into the original function
	DWORD oldProtect;
  VirtualProtect( dst, numBytes, PAGE_READWRITE, &oldProtect );
	WRITE_INSTR( dst, "\xE9" );  // jmp
	WRITE_DWORD( dst, newBlock - (dst + 4) );  // relative address of the new block
  for (uint i = 5; i < numBytes; i++)
	WRITE_INSTR( dst, "\x90" );  // nop              fill the rest with NOPs
  VirtualProtect( dst, numBytes, oldProtect, &oldProtect );
}
