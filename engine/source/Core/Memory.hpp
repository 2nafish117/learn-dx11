#pragma once


#include <windows.h>
#include <memoryapi.h>
#include <sysinfoapi.h>

#include <spdlog/spdlog.h>
#include "Basic.hpp"

void TestMemoryStuff() {
	// typedef struct _SYSTEM_INFO {
	// 	union {
	// 		DWORD dwOemId;          // Obsolete field...do not use
	// 		struct {
	// 			WORD wProcessorArchitecture;
	// 			WORD wReserved;
	// 		} DUMMYSTRUCTNAME;
	// 	} DUMMYUNIONNAME;
	// 	DWORD dwPageSize;
	// 	LPVOID lpMinimumApplicationAddress;
	// 	LPVOID lpMaximumApplicationAddress;
	// 	DWORD_PTR dwActiveProcessorMask;
	// 	DWORD dwNumberOfProcessors;
	// 	DWORD dwProcessorType;
	// 	DWORD dwAllocationGranularity;
	// 	WORD wProcessorLevel;
	// 	WORD wProcessorRevision;
	// } SYSTEM_INFO, *LPSYSTEM_INFO;

	SYSTEM_INFO sysInfo{};
	GetSystemInfo(&sysInfo);
	u32 pageSize = sysInfo.dwPageSize;


	void* mem = VirtualAlloc(nullptr, 128, MEM_COMMIT, PAGE_READWRITE);
	int* intPtr = (int*)mem;
	*intPtr = 4;

	
	byte* bytePtr = (byte*)mem;
	
	byte* bPtr1 = bytePtr + 64;
	byte* bPtr2 = bytePtr + 512;
	//byte* bPtr3 = bytePtr + 4*1024 + 32;

	*bPtr1 = 69;
	*bPtr2 = 69;
	//*bPtr3 = 69;

	assert(VirtualFree(mem, 0, MEM_RELEASE));

	*bPtr1 = 69;
	*bPtr2 = 69;
	//*bPtr3 = 69;
}