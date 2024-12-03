#include "pch.h"
#include <iostream>
#include <map>
#include "maps.h"
#include "PatternScan.h"
#include "Hook/MinHook.h"
#pragma comment(lib, "libMinHook.x64.lib")
using namespace std;

Game currentGame;
typedef bool (__fastcall* folder_cond_is_node_disable)(void* dat, void* node);
typedef unsigned int(__fastcall* _party_get_member)(int idx, int var2);
typedef unsigned int(__fastcall* _party_get_party)();
typedef unsigned int(__fastcall* _NakamaGetCharaID)(unsigned int playerId);
typedef void*(__fastcall* _get_party_data)(int partyID);
typedef void(__fastcall* _GetPlayerAndCharaID)(void* thisPtr, int _category, unsigned int _timeline, unsigned int _clock, unsigned int* _r_chara_id, unsigned int* _r_player_id);

typedef unsigned int(__fastcall* _scene_get_chara_id)(void* thisptr);

inline _party_get_member get_main_member_by_slot = nullptr;
inline _get_party_data get_party_data_func = nullptr;
inline _party_get_party get_party = nullptr;
void* chara_id_patch_addr = nullptr;
_NakamaGetCharaID get_nakama_chara_id_func = nullptr;
_GetPlayerAndCharaID get_player_and_chara_id_func = nullptr;

_scene_get_chara_id scene_get_chara_id_func = nullptr;


inline intptr_t	ReadOffsetValue2(intptr_t address)
{
	intptr_t srcAddr = (intptr_t)address;
	intptr_t dstAddr = srcAddr + 4 + *(int32_t*)srcAddr;
	return dstAddr;
}

template<typename AT>
inline intptr_t	ReadCall2(AT address)
{
	return ReadOffsetValue2((intptr_t)address + 1);
}

void Patch(BYTE* dst, BYTE* src, unsigned int size)
{
	DWORD oldProtect;

	VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect);
	memcpy(dst, src, size);
	VirtualProtect(dst, size, oldProtect, &oldProtect);
}

void Nop(BYTE* dst, unsigned int size)
{
	DWORD oldProtect;

	VirtualProtect(dst, size, PAGE_EXECUTE_READWRITE, &oldProtect);
	memset(dst, 0x90, size);
	VirtualProtect(dst, size, oldProtect, &oldProtect);
}

folder_cond_is_node_disable GetConditionFolderFunc()
{
	switch (currentGame)
	{
	case Game::YakuzaLikeADragon:
		return (folder_cond_is_node_disable)PatternScan((void*)GetModuleHandle(NULL), "4C 8B DC 55 53 57 48 8B EC");
	case Game::LikeADragonInfiniteWealth:
		return (folder_cond_is_node_disable)PatternScan((void*)GetModuleHandle(NULL), "40 55 56 41 57 48 8D 6C 24 B9");
	}

	return nullptr;
}

folder_cond_is_node_disable is_node_disable_trampoline;
bool is_node_disable_detour(void* dat, void* node)
{
	return false;
}

_GetPlayerAndCharaID get_player_and_character_id_trampoline;
void get_player_and_character_id(void* thisPtr, int _category, unsigned int _timeline, unsigned int _clock, unsigned int* _r_chara_id, unsigned int* _r_player_id)
{
	int leader = get_main_member_by_slot(0, 0);

	if (leader == 4)
	{
		//ichiban is main party member
		return get_player_and_character_id_trampoline(thisPtr, _category, _timeline, _clock, _r_chara_id, _r_player_id);
	}
	else
	{
		//ichiban is not main party member
		if (leader != 0)
		{
			*_r_chara_id = get_nakama_chara_id_func(leader);
			*_r_player_id = leader;
			return;
		}
	}

	return get_player_and_character_id_trampoline(thisPtr, _category, _timeline, _clock, _r_chara_id, _r_player_id);
}

_NakamaGetCharaID nakama_get_chara_id_trampoline;
unsigned int nakama_get_chara_id(unsigned int playerID)
{
	if (playerID == 4)
	{
		int leader = get_main_member_by_slot(0, 0);

		if (leader == 4)
		{
			BYTE patch[3] = { 0x83, 0xF9, 0x4 };
			//ichiban is main party member
			Patch((BYTE*)chara_id_patch_addr, patch, 3);
		}
		else
		{
			BYTE patch[3] = { 0x83, 0xF9, 0x0 };
			//ichiban is not main party member
			Patch((BYTE*)chara_id_patch_addr, patch, 3);
		}
	}

	return nakama_get_chara_id_trampoline(playerID);
}

_scene_get_chara_id scene_get_chara_id_trampoline;
unsigned int scene_get_chara_id(void* thisPtr)
{
	int leader = get_main_member_by_slot(0, get_party());

	if (leader == 0)
		return scene_get_chara_id_trampoline(thisPtr);

	return get_nakama_chara_id_func(leader);
}

_scene_get_chara_id scene_get_player_id_trampoline;
unsigned int scene_get_player_id(void* thisPtr)
{
	//int leader = get_main_member_by_slot(0, 1);
	//std::cout << "fish " << leader << std::endl;
	return get_main_member_by_slot(0, get_party());
}

//db/party.bin ids!
_get_party_data get_party_data_trampoline;
void* get_party_data(int partyID)
{
	int party = get_party();
	int leader = get_main_member_by_slot(0, party);

	if(leader == 0)
		return get_party_data_trampoline(partyID);

	int dataID = partyID;

	switch (leader)
	{
	case 4:
		dataID = 1;
		break;
	case 1:
		dataID = 2;
		break;
	case 6:
		dataID = 3;
		break;
	case 9:
		dataID = 4;
		break;
	case 10:
		dataID = 5;
		break;
	case 12:
		dataID = 6;
		break;
	case 13:
		dataID = 7;
		break;
	case 19:
		dataID = 8;
		break;
	case 20:
		dataID = 9;
		break;
	case 21:
		dataID = 10;
		break;
	}

	return get_party_data_trampoline(dataID);
}

DWORD WINAPI AppThread(HMODULE hModule)
{
#if _DEBUG
	AllocConsole();
	FILE* f;
	freopen_s(&f, "CONOUT$", "w", stdout);
#endif

	// Get the name of the current game
	wchar_t exePath[MAX_PATH + 1];
	GetModuleFileNameW(NULL, exePath, MAX_PATH);

	wstring wstr(exePath);
	string currentGameName = basenameBackslashNoExt(string(wstr.begin(), wstr.end()));

	currentGame = getGame(currentGameName);
	currentGameName = getGameName(currentGame);

	cout << "Game is: " << currentGameName << "!" << endl;


	MH_Initialize();

	//kinda broken
	if (currentGame == Game::YakuzaLikeADragon)
	{
		get_main_member_by_slot = (_party_get_member)(ReadCall2(PatternScan(GetModuleHandle(NULL), "E8 ? ? ? ? 8B D8 44 8B C7 48 8D 55 40")));
		chara_id_patch_addr = PatternScan(GetModuleHandle(NULL), "83 F9 ? 75 ? 41 8B 90 A8 00 00 00");
		get_player_and_chara_id_func = (_GetPlayerAndCharaID)(ReadCall2(PatternScan(GetModuleHandle(NULL), "E8 ? ? ? ? 48 8B B4 24 08 01 00 00 48 8B 9C 24 00 01 00 00")));
		get_nakama_chara_id_func = (_NakamaGetCharaID)(ReadCall2(PatternScan(GetModuleHandle(NULL), "E8 ? ? ? ? 8B F8 85 C0 B8 ? ? ? ? 0F 44 F8")));
		get_party_data_func = (_get_party_data)(PatternScan(GetModuleHandle(NULL), "40 53 48 83 EC ? 8B D9 B9 ? ? ? ? E8 ? ? ? ? 4C 8B C0 48 85 C0 74 ? 8B 50 10 48 03 D0 74 ? 3B 1A 73 ? F7 42 20 ? ? ? ? 74 ? 8B 42 1C 49 03 C0 4C 89 44 24 20 48 89 54 24 28 89 5C 24 30 44 8B 0C 98 4D 03 C8 4C 89 4C 24 38 74 ? 41 0F B6 01"));

		BYTE slotPatch[6] = {0x31, 0xC0, 0xC3, 0x90, 0x90, 0x90};
		void* isSlotLockedFunc = (void*)ReadCall2(PatternScan(GetModuleHandle(NULL), "E8 ? ? ? ? 84 C0 75 ? 8B 97 A4 0E 00 00"));
		Patch((BYTE*)isSlotLockedFunc, slotPatch, 6);

		BYTE* swapFuncCondition = PatternScan(GetModuleHandle(NULL), "75 ? 80 7A 02 ? 75 ? 45 0F B6 48 01");
		Nop(swapFuncCondition, 2);
		Nop(swapFuncCondition + 6, 2);

		MH_CreateHook((void*)get_nakama_chara_id_func, &nakama_get_chara_id, (LPVOID*)&nakama_get_chara_id_trampoline);
		MH_EnableHook((void*)get_nakama_chara_id_func);
	
	}

	if (currentGame == Game::LikeADragonInfiniteWealth)
	{
		get_party = (_party_get_party)(ReadCall2(PatternScan(GetModuleHandle(NULL), "E8 ? ? ? ? 8B C8 E8 ? ? ? ? 48 8B 4F 28 89 41 50")));
		get_main_member_by_slot = (_party_get_member)(ReadCall2(PatternScan(GetModuleHandle(NULL), "E8 ? ? ? ? 85 C0 0F 84 ? ? ? ? 44 89 6D BF")));
		get_nakama_chara_id_func = (_NakamaGetCharaID)(ReadCall2(PatternScan(GetModuleHandle(NULL), "E8 ? ? ? ? 8B F8 85 C0 B8 ? ? ? ? 0F 44 F8")));
		scene_get_chara_id_func = (_scene_get_chara_id)(PatternScan(GetModuleHandle(NULL), "8B 81 08 0A 00 00"));

		//Never lock a character slot
		BYTE slotPatch[6] = { 0x31, 0xC0, 0xC3, 0x90, 0x90, 0x90 };
		void* isSlotLockedFunc = (void*)ReadCall2(PatternScan(GetModuleHandle(NULL), "E8 ? ? ? ? 84 C0 75 ? 48 8B 0D ? ? ? ? 8B D3"));
		Patch((BYTE*)isSlotLockedFunc, slotPatch, 6);

		//Remove a check on the party member swap
		BYTE* swapFuncCondition = PatternScan(GetModuleHandle(NULL), "0F 84 ? ? ? ? 48 83 C1 ? 48 3B CA 75 ? 48 3B C2");
		Nop(swapFuncCondition, 6);

		BYTE* charaCond = PatternScan(GetModuleHandle(NULL), "74 ? 48 83 C4 ? 5B E9 ? ? ? ? 8B 4A 40");
		BYTE charaPatch[] = {0xEB};

		Patch(charaCond, charaPatch, 1);

		//disable chara order overwriting instructions
		Nop(PatternScan(GetModuleHandle(NULL), "42 89 2C 82"), 4);
		Nop(PatternScan(GetModuleHandle(NULL), "89 44 11 08 49 83 E9 ?"), 4);
		Nop(PatternScan(GetModuleHandle(NULL), "44 89 1C 82"), 4);
		//Nop(PatternScan(GetModuleHandle(NULL), "48 8B 16 8B 4E 0C"), 3);
		Nop(PatternScan(GetModuleHandle(NULL), "C7 01 ? ? ? ? 48 8B 16"), 6);

		void* scene_get_chara_id_func = (void*)ReadCall2(PatternScan(GetModuleHandle(NULL), "E9 ? ? ? ? 8B 4A 40"));

		MH_CreateHook((void*)scene_get_chara_id_func, &scene_get_chara_id, (LPVOID*)&scene_get_chara_id_trampoline);
		MH_EnableHook((void*)scene_get_chara_id_func);

		void* scene_get_player_id_func = (void*)ReadCall2(PatternScan(GetModuleHandle(NULL), "E8 ? ? ? ? 44 8B D0 4D 85 E4"));

		MH_CreateHook((void*)scene_get_player_id_func, &scene_get_player_id, (LPVOID*)&scene_get_player_id_trampoline);
		MH_EnableHook((void*)scene_get_player_id_func);

		void* get_party_data_func = (void*)ReadCall2(PatternScan(GetModuleHandle(NULL), "E8 ? ? ? ? 49 8B 4E 28 3B 41 50"));

		MH_CreateHook((void*)get_party_data_func, &get_party_data, (LPVOID*)&get_party_data_trampoline);
		MH_EnableHook((void*)get_party_data_func);
	}

	while (true)
	{
		Sleep(9999);
	}

	return 0;
}


BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)AppThread, hModule, 0, nullptr));
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}



