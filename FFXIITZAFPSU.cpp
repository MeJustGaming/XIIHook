// FFXIITZAFPSU.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "Config.h"
#include <iostream>
#include <string>
#include <Windows.h>
#include <ctime>
#include <cstdlib>


#define NOP 0x90 //NOP instruction

//Variable pointers
#define tftPtr 0x0090D398 //Min Frametime 1
#define tft1Ptr 0x0090E0C0 //Min Frametime 2 (Doesn't seem to do anything)
#define rftPtr 0x01F97058  //Reported frametime 1
#define rft1Ptr 0x01F98170 //Reported frametime 2 (Not to be trusted)
#define igmPtr 0x0207D798 /*In game multiplier
Generally becomes 1,2,4 though you can make it anything. Disabling instructions related to this can have some weird results.
*/
#define igm1Ptr 0x0207D794 /*
Wolrd multiplier (Eg skies and grass anims) @ < 1 this freezes the game, but there is still camera control
This is equal to the igm * the global time multiplier
*/
#define igm2Ptr 0x01E1A820 /*Mouse multiplier
Realative to all other multipliers
*/
#define igm3Ptr 0x0207D79C /*
Global time multiplier
When I found this everything else made so much more sense, this is what we will be changing
*/
#define igmSPtr 0x01FED754 /*
Currently selected state of igmptr
*/
#define uiOPtr 0x01E160C0 //Menu enabled
#define titlPtr 0x020A3380 //0 In game 1 In title
//Instruction pointers

#define multiUnlockPtr 0x0022AC1D
#define multiUnlockPtr1 0x0022AC27
#define multiUnlockPtr2 0x0022AE00
#define mouseUnlockPtr 0x0022AC58
#define fullSpeedUnlockPtr 0x00622274

HWND FindFFXII() {
	return FindWindow(0, L"FINAL FANTASY XII THE ZODIAC AGE");
}

inline void writeMinFrametime(HANDLE&hProcess, double&targetFrameTime) 
{
	DWORD protection;
	VirtualProtectEx(hProcess, (LPVOID)tftPtr, sizeof(double), PAGE_READWRITE, &protection);
	VirtualProtectEx(hProcess, (LPVOID)tft1Ptr, sizeof(double), PAGE_READWRITE, &protection);

	WriteProcessMemory(hProcess, (LPVOID)tftPtr, &targetFrameTime, sizeof(double), NULL);
	WriteProcessMemory(hProcess, (LPVOID)tft1Ptr, &targetFrameTime, sizeof(double), NULL);
}

void tickone(HANDLE&hProcess, double&targetFrameTime,float&inGameMultiplier, float&inGameMouseMultiplier, BOOL&uiSpeedRamp, UserConfig&uConfig) 
{
	if (uiSpeedRamp == 1 && uConfig.bEnableTransitionSpeedRamp) inGameMultiplier = 1;
	WriteProcessMemory(hProcess, (LPVOID)igm3Ptr, &inGameMultiplier, sizeof(float), NULL);
	WriteProcessMemory(hProcess, (LPVOID)igm2Ptr, &inGameMouseMultiplier, sizeof(float), NULL);
	uiSpeedRamp = 0;
}

void ticktwo(HANDLE&hProcess, float&inGameSpeedMulti, float&inGameMultiplier, uint8_t&igmState, BOOL&uiState, UserConfig&uConfig) //Make animations run at the correct speed, might slow gameplay down though
{
	if (!uiState) 
	{
		float base = (igmState == 0 ? uConfig.igmState0Override //Fix feedback loop
			: (igmState == 1 ? uConfig.igmState1Override : uConfig.igmState2Override));

		switch (uConfig.bEnableAdaptiveIGM) {
		case false:
			inGameSpeedMulti = base;
			break;
		case true:
			float relative = base + (-base * (1 - inGameMultiplier));
			inGameSpeedMulti = relative < 1 ? 1 : relative;
			break;
		}
	}
	else inGameSpeedMulti = 1; //Attempt to fix menu and loading slowdowns.

	WriteProcessMemory(hProcess, (LPVOID)igmPtr, &inGameSpeedMulti, sizeof(float), NULL);
}

int main()
{
	//Hack stuff
	HWND FFXIIWND = FindFFXII();
	HANDLE hProcess;
	DWORD pId;

	//Config stuff
	UserConfig uConfig;
	Config::UpdateUserConfig(uConfig);

	//Prefabricated vars
	double oneOverSixty = (double)1L / (double)60L;

	//Ingame vars
	double targetFrameTime = 0;
	double realFrametime = 0;
	float inGameMultiplier = 0;
	float inGameMouseMultiplier = 0;
	float inGameSpeedMulti = 0;
	uint8_t igmState = 1;
	BOOL titleState = 1;
	BOOL lastTitleState = 1;
	BOOL uiState = 0;
	BOOL lastUIState = 0;
	BOOL uiSpeedRamp = 0;

	while (!FFXIIWND)
	{
		std::cout << "Could not find FFXII, make sure the game is open!\nChecking again...\n";
		FFXIIWND = FindFFXII();
		Sleep(1000);
	}

	GetWindowThreadProcessId(FFXIIWND, &pId);
	hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pId);
	std::cout << "Found FFXII Window at PID: " << pId << "!\n";


	if (!hProcess) std::cout << "Failed to open process...\n";
	else 
	{
		std::cout << "Target frame time=" << targetFrameTime << "\n";
		std::cout << "Setting up... \n";
		ReadProcessMemory(hProcess, (LPVOID)tftPtr, (LPVOID)&targetFrameTime, sizeof(double), NULL);
		ReadProcessMemory(hProcess, (LPVOID)rftPtr, (LPVOID)&realFrametime, sizeof(double), NULL);
		ReadProcessMemory(hProcess, (LPVOID)igmPtr, (LPVOID)&inGameMultiplier, sizeof(float), NULL);
		ReadProcessMemory(hProcess, (LPVOID)igmSPtr, (LPVOID)&igmState, sizeof(uint8_t), NULL);
		ReadProcessMemory(hProcess, (LPVOID)igm2Ptr, (LPVOID)&inGameMouseMultiplier, sizeof(float), NULL);
		ReadProcessMemory(hProcess, (LPVOID)uiOPtr, (LPVOID)&uiState, sizeof(BOOL), NULL);
		ReadProcessMemory(hProcess, (LPVOID)titlPtr, (LPVOID)&titleState, sizeof(BOOL), NULL);

		std::cout << "Read current target frametime as " << targetFrameTime << std::endl;

		//Overwriting instructions
		if (uConfig.bEnableLockedMouseMulti || uConfig.bEnableAdaptiveMouse)
			if (!WriteProcessMemory(hProcess, (LPVOID)mouseUnlockPtr, "\x90\x90\x90\x90\x90\x90\x90\x90", 8, NULL))
				std::cout << "Failed to erase instructions... \n";
		if (!WriteProcessMemory(hProcess, (LPVOID)multiUnlockPtr1, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 10, NULL))
			std::cout << "Failed to erase instructions... \n";
		if (uConfig.bEnableFullSpeedMode) 
			if (!WriteProcessMemory(hProcess, (LPVOID)fullSpeedUnlockPtr, "\x90\x90\x90\x90\x90\x90\x90\x90", 8, NULL))
				std::cout << "Failed to erase instructions... \n";
		
		//Setting default frametimes, loop until this works
		while (targetFrameTime != uConfig.requestedMinFrameTime) 
		{
			writeMinFrametime(hProcess, uConfig.requestedMinFrameTime);
			ReadProcessMemory(hProcess, (LPVOID)tftPtr, (LPVOID)&targetFrameTime, sizeof(double), NULL);
		}


		clock_t tick = clock();
		clock_t tick0 = clock();

		for (;;)
		{
			if (!IsWindow(FFXIIWND)) {
				std::cout << "Window closed, stopping...\n";
				break;
			}

			ReadProcessMemory(hProcess, (LPVOID)rftPtr, (LPVOID)&realFrametime, sizeof(realFrametime), NULL);
			ReadProcessMemory(hProcess, (LPVOID)uiOPtr, (LPVOID)&uiState, sizeof(uiState), NULL);
			ReadProcessMemory(hProcess, (LPVOID)igmSPtr, (LPVOID)&igmState, sizeof(uint8_t), NULL);
			ReadProcessMemory(hProcess, (LPVOID)titlPtr, (LPVOID)&titleState, sizeof(BOOL), NULL);

			if (uiState != lastUIState) 
			{
				switch ((int)uiState) 
				{
				case 0:
					std::cout << "UI State is 0\n";
					targetFrameTime = uConfig.requestedMinFrameTime;
					uiSpeedRamp = 1;
					break;
				case 1:
					std::cout << "UI State is 1\n";
					targetFrameTime = uConfig.requestedMinFrameTimeMenus;
					break;
				}
				writeMinFrametime(hProcess, targetFrameTime);
				lastUIState = uiState;
			}

			if (titleState != lastTitleState) //Doesn't work on first launch
			{
				switch ((int)titleState)
				{
				case 0:
					if (uiState == 1) break;
					std::cout << "Title State is 0\n";
					targetFrameTime = uConfig.requestedMinFrameTime;
					break;
				case 1:
					std::cout << "Title State is 1\n";
					targetFrameTime = uConfig.requestedMinFrameTimeMenus;
					break;
				}
				writeMinFrametime(hProcess, targetFrameTime);
				lastTitleState = titleState;
			}

			inGameMultiplier = (float)(realFrametime / oneOverSixty);
			inGameMouseMultiplier = uConfig.bEnableAdaptiveMouse ? uConfig.lockedMouseMulti / (1 - inGameMultiplier)
				: uConfig.lockedMouseMulti;
			
			
			if (FFXIIWND != GetForegroundWindow())
			{
				targetFrameTime = uConfig.requestedMinFrameTimeNoFocus;
				writeMinFrametime(hProcess, targetFrameTime);
			}
			else
			{
				targetFrameTime = (uiState != 1 && titleState != 1) ? uConfig.requestedMinFrameTime : uConfig.requestedMinFrameTimeMenus;
				writeMinFrametime(hProcess, targetFrameTime);
			}

			if ((clock() - tick) >= (2 * realFrametime) * 1000)
			{ 
				tickone(hProcess, targetFrameTime, inGameMultiplier, inGameMouseMultiplier, uiSpeedRamp, uConfig); 
				tick = clock();
			}
			if (clock() - tick0 >= realFrametime * 1000) //Update anim speed approx every frame, so that we don't update it faster than the game
			{
				ticktwo(hProcess, inGameSpeedMulti, inGameMultiplier, igmState, uiState, uConfig);
				tick0 = clock();
			}
			
			/*float worldTime = (igmState == 0 ? 0.7f : (igmState == 1 ? 1.7f : 3.7f)); //* inGameMultiplier * inGameSpeedMulti;
			WriteProcessMemory(hProcess, (LPVOID)igm1Ptr, &worldTime, sizeof(float), NULL); //Hopefully fast enough to not cause issues
			!!!!Softlocks at the title screen
			*/

			Sleep(realFrametime / uConfig.mainThreadUpdateCoef); /*Improve CPU time
									  Also accounts for framerate fluctuations, with an effort to update x times per frame.*/
		}
	}

	CloseHandle(hProcess);
	std::cin.get();
	return 0;
}
