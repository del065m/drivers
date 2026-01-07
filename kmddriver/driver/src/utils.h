#pragma once
#include <ntifs.h>

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;

typedef uint8 undefined1;
typedef uint16 undefined2;
typedef uint32 undefined4;
typedef uint64 undefined8;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned long ulong;
typedef unsigned long long ulonglong;
typedef unsigned char byte;
typedef unsigned char uchar;
typedef long long longlong;


namespace fn {
	extern "C" {
		NTSTATUS ZwSetSystemTime(PLARGE_INTEGER SystemTime, PLARGE_INTEGER PreviousTime);
	}

	namespace bootvid_dll {
		extern "C" {
			void VidBitBlt(int* param_1, uint param_2, uint param_3);
			void VidBitBltEx(uint param_1, uint param_2, uint param_3, uint param_4, uint* param_5);
			void VidBufferToScreenBlt(uint* param_1, uint param_2, uint param_3, uint param_4, uint param_5, int param_6);
			undefined1 VidCleanUp();
			void VidDisplaySting(byte* param_1, ulonglong param_2);
			void VidDisplayStringXY(byte* param_1, uint param_2, int param_3, char param_4);
			bool VidInitialize(int param_1, char param_2);
			void VidResetDisplay(char param_1);
			ulonglong VidScreenToBufferBlt(uint* param_1, uint param_2, int param_3, int param_4, uint param_5, uint param_6);
			void VidSetScrollRegion(undefined4 param_1, undefined4 param_2, undefined4 param_3, undefined4 param_4);
			undefined4 VidSetTextColor(undefined4 param_1);
			ulonglong VidSolidColorFill(uint param_1, uint param_2, uint param_3, uint param_4, byte param_5);
		}
	}
	namespace hal_dll {
		extern "C" {
			ULONG HalSetBusData(BUS_DATA_TYPE BusDataType, ULONG BusNumber, ULONG SlotNumber, PVOID Buffer, ULONG Length);
		}
	}

	namespace ntoskrnl {
		extern "C" {
			int AlpcCreateSecurityContext(undefined8 param1, undefined8 param2, int param3, longlong param4); 
			int AlpcGetHeaderSize(uint param1);
			longlong AlpcGetMessageAttribute(uint* param1, uint param2);
			undefined8 AlpcInitializeMessageAttribute(undefined4 param1, undefined4* param2, ulonglong param3, ulonglong* param4);
			undefined4 BgkDisplayCharacter(undefined2 param1, undefined4 param2, undefined4 param3, undefined4 param4, undefined4 param5);
			undefined4 BgkGetConsoleState(undefined8 param1);
			undefined4 BgkGetCursorState(undefined8 param1, undefined8 param2, undefined8 param3);
			undefined4 BgkSetCursor(undefined4 param1, undefined4 param2, undefined4 param3);
		}
	}
	namespace windef {
		typedef void* HBITMAP;
		struct SIZEL {
			LONG cx;
			LONG cy;
		};
	}
	namespace win32k {
		extern "C" {
			windef::HBITMAP EngCreateBitmap(windef::SIZEL sizl, LONG lWidth, ULONG iFormat, FLONG fl, PVOID pvBits);
		}
	}
}