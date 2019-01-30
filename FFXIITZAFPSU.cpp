#include "pch.h"
#include "md5.h" //Credit to Frank Thilo
#include "Interp.h"
#include "Config.h"
#include "Utility.h"
#include "d3dHook.h"
//#include "out/detours/detours.h"

#define overlayName L"FFXII FPS UNLOCKER OVERLAY"

struct gameVars 
{
	double cTime;
	float worldTime, inGameMultiplier, inGameMouseMultiplier;
	uint8_t gameStateEnum, inCutscene, igmState, lastigm;
	Interp::Interp igmInterp;
	UserConfig uConfig;

	gameVars() {
		cTime = 0; worldTime = 1; inGameMultiplier = 1; inGameMouseMultiplier = 1;
		gameStateEnum = 0; inCutscene = 0; igmState = 0; lastigm = 0;
	}
};

__forceinline void tickf(HANDLE&hProcess, gameVars&v)
{
	float nTime = (float)v.cTime;
	if (!v.gameStateEnum == 1 && !v.inCutscene == 1 && v.lastigm != v.igmState)
	{
		float base0 = (v.lastigm == 0 ? v.uConfig.igmState0Override
			: (v.lastigm == 1 ? v.uConfig.igmState1Override : v.uConfig.igmState2Override));
		float base = (v.igmState == 0 ? v.uConfig.igmState0Override
			: (v.igmState == 1 ? v.uConfig.igmState1Override : v.uConfig.igmState2Override));

		v.igmInterp.position = base0;
		v.igmInterp.position0 = base0;
		v.igmInterp.smoothTime = v.uConfig.easingTime;
		v.igmInterp.target = base;
		v.igmInterp.time0 = nTime;

		printf("Starting igm interp: %f -> %f\n", base0, base);	
	}
	else if (!v.gameStateEnum == 1 && !v.inCutscene == 1) 
	{
		float base = 1;
		switch (v.uConfig.bEnableEasing) 
		{
		case true:
			base = v.igmInterp.interp(nTime);

			if (nTime > v.igmInterp.time1) base = (v.igmState == 0 ? v.uConfig.igmState0Override
				: (v.igmState == 1 ? v.uConfig.igmState1Override : v.uConfig.igmState2Override));
			if (base != base || base == 0) base = 1; //If NaN set to 1
			break;
		case false:
			base = (v.igmState == 0 ? v.uConfig.igmState0Override
				: (v.igmState == 1 ? v.uConfig.igmState1Override : v.uConfig.igmState2Override));
			break;
		}

		v.inGameMultiplier = base;
	}
	else v.inGameMultiplier = 1; //Attempt to fix menu and loading slowdowns, as well as cutscene scaling

	uint8_t wtChanged = 0;
	if (v.worldTime > v.inGameMultiplier * 2)
	{
		v.worldTime = v.inGameMultiplier;
		wtChanged = 1;
	}

	WriteProcessMemory(hProcess, (LPVOID)igmPtr, &v.inGameMultiplier, sizeof(float), NULL);
	WriteProcessMemory(hProcess, (LPVOID)mouseMPtr, &v.inGameMouseMultiplier, sizeof(float), NULL);
	if (wtChanged==1) WriteProcessMemory(hProcess, (LPVOID)worldMPtr, &v.worldTime, sizeof(float), NULL);

	v.lastigm = v.igmState;
}


namespace
{
	d3dHook::D3DMenu* pMenu;
	gameVars* pVars;
	HWND* pOverlay;
	HWND* pGame;
	const MARGINS Margin = { 0, 0, Width, Height };
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
	switch (Message)
	{
	case WM_PAINT:
		d3dHook::Render(*pMenu, pVars->uConfig, pVars->gameStateEnum, *pOverlay, *pGame);
		break;

	case WM_CREATE:
		DwmExtendFrameIntoClientArea(hWnd, &Margin);
		break;

	case WM_DESTROY:
		printf("Overlay closed!\n");
		PostQuitMessage(1);
		return 0;

	default:
		return DefWindowProc(hWnd, Message, wParam, lParam);
		break;
	}
	return 0;
}

int main()
{
	//Process handles
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	HWND FFXIIWND = findXII();
	HANDLE hProcess;
	DWORD pId;

	//Prefabricated variables
	const double oneOverSixty = (double)1L / (double)60L;
	//const double Rad2Deg = 0.0174532925199L;

	//Variables
	gameVars gVars;
	d3dHook::D3DMenu dMenu = d3dHook::D3DMenu();

	double targetFrameTime = 0;
	double realFrametime = 0;
	float timeScale = 1;
	float framerateCoef = 1;
	uint8_t titleState = 1;
	uint8_t uiState = 0;
	uint8_t cMenState = 0;
	uint8_t focusState = 0;
	uint8_t lastUseMenuLimitState = 0;
	uint8_t lastFocusState = 0;
	uint8_t inMovie = 0;

	//Initialization
	gVars.inGameMultiplier = 1; gVars.inGameMouseMultiplier = 1; gVars.worldTime = 1;
	gVars.uConfig = UserConfig();
	Config::UpdateUserConfig(gVars.uConfig);

	gVars.igmInterp = Interp::Interp();
	gVars.igmInterp.setType(gVars.uConfig.easingType);
	gVars.igmInterp.position = 1; gVars.igmInterp.position0 = 1; gVars.igmInterp.target = 1;

	//Give the user understanding of the console text colors
	SetConTAttribute(hConsole, cc_NORM, "Normal Text; ");
	SetConTAttribute(hConsole, cc_VERBOSE, "Verbose Text; ");
	SetConTAttribute(hConsole, cc_FUN, "Fun Text; ");
	SetConTAttribute(hConsole, cc_ERROR, "Error Text; ");
	SetConTAttribute(hConsole, cc_WARN, "Warning Text\n\n");

	//Do this math only one time
	SetConTAttribute(hConsole, cc_VERBOSE, "Normalizing config variables...\n");

	framerateCoef = 30 / gVars.uConfig.requestedMinFramerate;
	gVars.uConfig.requestedMinFramerate = 1 / gVars.uConfig.requestedMinFramerate;
	gVars.uConfig.requestedMinFramerateMenus = 1 / gVars.uConfig.requestedMinFramerateMenus;
	gVars.uConfig.requestedMinFramerateNoFocus = 1 / gVars.uConfig.requestedMinFramerateNoFocus;
	gVars.uConfig.requestedMinFramerateMovies = 1 / gVars.uConfig.requestedMinFramerateMovies;
	gVars.uConfig.fov = gVars.uConfig.fov * Rad2Deg;

	//Wait until the game it open before applying hacks and memory changes
	while (!FFXIIWND)
	{
		SetConTAttribute(hConsole, cc_WARN, "Could not find FFXII, make sure the game is open!\nChecking again...\n");
		FFXIIWND = findXII();
		Sleep(3000);
	}
	SetConTAttribute(hConsole, cc_NORM);

	//Attempt to open the process
	GetWindowThreadProcessId(FFXIIWND, &pId);
	hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pId);

	
	

	//Check the MD5 of the file; warn the user if it is awry; if it is a known MD5 of a stolen copy, report to the user that the program will not function at all.
	std::ifstream gameExecutable;
	std::string sMD5;
	char executableFullPath[MAX_PATH];
	long fSize;

	GetModuleFileNameExA(hProcess, NULL, executableFullPath, MAX_PATH);
	gameExecutable.open(executableFullPath, std::ios::binary | std::ios::in); gameExecutable.seekg(0, std::ios::end);
	fSize = gameExecutable.tellg(); gameExecutable.seekg(0, std::ios::beg);

	char* executableData = new char[fSize];

	gameExecutable.read(executableData, fSize);
	sMD5 = md5(executableData, fSize);
	std::transform(sMD5.begin(), sMD5.end(), sMD5.begin(), ::toupper);
	SetConTAttribute(hConsole, cc_WARN, "File MD5: ");
	SetConTAttribute(hConsole, cc_VERBOSE, "%s\n", sMD5.c_str());

	if (sMD5 == "D032914AC59BAFDD62F038832CC14525") //The MD5 of FFXII_TZA.exe from CPY
	{
		std::string yarrthatsmyshanty[] = {
			"Yar! That\'s be me shanty!\n",
			"Yar har har!\n",
			"Being a pirate\'s alright to be!\n",
			"Do what\'cha want \'cause a pirate is free!\n",
			"You should be ashamed of yourself!\n", //Hehe Turok joke
			"Take what you want, give nothing back!\n",
			"Do what \'cha want \'cause a pirate is free!\n",
			"If ye can’t trust a pirate, ye damn well can’t trust a merchant either!\n",
			"Not all treasure is silver & gold!\n",
			"Yar har fiddle dee dee\n",
			"Yohoho that be a pirate\'s life for me!\n",
			"I love this shanty!\n",
			"Where there is a sea, there are pirates.\n",
			"If ye thinks he be ready to sail a beauty, ye bett\'r be willin’ to sink with \'er!\n",
			"Ahh shiver me timbers!\n",
			"Keep calm and say Arrr\n",
			"Avast thur be somethin\' awry...\n",
			"All hands hoay; for today we\'ve a new crewmate!\n",
			"Shiver me timbers- you\'re dancing the hempen jig!\n",
			"I expected nothing more than hornswaggle matey!\n",
			"\'es a landlubber, I swea\'!\n",
			"Ah blimey cap\'n, we\'ve been caught!\n",
			"Yarr today you feed the fish.\n",
			"Heave ho! You can do better than that!\n",
			"Yerr... not much old salt in ye...\n",
			"Damned scallywag!\n",
			"Cap\'n we\'ve been scuttled!\n",
			"Yer\' sharkbait y\'hea?\n",
			"Yo ho ho today ye\' walk the plank.\n",
			"Yar har fiddle dee dee\nBeing a pirate is alright to be\nDo what \'cha want \'cause a pirate is free!\nYou are a pirate!\n"
		};
		size_t nShanties = yarrthatsmyshanty->size();
		int cShanty;
		int rError; //Fake error
		srand(time(NULL)); Sleep(rand() % 2 * nShanties); srand(time(NULL)); //Randomness
		rError = rand() % (999999999 - 100000000) + 9999999;
		for (int i = 0; i < 8; i++) { rError += rand() % 99999999; srand(time(NULL)); }
		cShanty = rand() % nShanties - 1;
		SetConTAttribute(hConsole, cc_WARN, "\n\nError: "); //"Error" = warn
		SetConTAttribute(hConsole, cc_ERROR, "0x%d\n", rError);
		printf("0x%d\n", rError);
		Sleep(rand() % 1333);
		SetConTAttribute(hConsole, cc_FUN, "\n%s\n", yarrthatsmyshanty[cShanty].c_str());
		for (;;) { Sleep(1); } //Infinite loop
	}
	//FFXII_TZA.exe from Steam
	else if (sMD5 != "F88350D39C8EDEECC3728A4ABC89289C")
		SetConTAttribute(hConsole, cc_VERBOSE, "Your file seems to be patched or updated. You may experience problems; if you experience issues please post them here: https://github.com/Drahsid/FFXIITZA-FPS-Unlocker/issues\n");

	SetConTAttribute(hConsole, cc_VERBOSE, "Found FFXII Window at PID: %lu!\n", pId);


	if (!hProcess) SetConTAttribute(hConsole, cc_ERROR, "Failed to open process...\n");
	else 
	{
		SetConTAttribute(hConsole, cc_VERBOSE);
		printf("Target frame time: %f\nSetting up... \n", gVars.uConfig.requestedMinFramerate);
		
		
		//Overwriting instructions
		if (gVars.uConfig.bEnableLockedMouseMulti || gVars.uConfig.bEnableAdaptiveMouse)
		{
			if (!WriteProcessMemory(hProcess, (LPVOID)mouseUnlockPtr, "\x90\x90\x90\x90\x90\x90\x90\x90", 8, NULL))
				failedToErase(0, hConsole);
			if (!WriteProcessMemory(hProcess, (LPVOID)mouseUnlockPtr0, "\x90\x90\x90\x90\x90\x90\x90\x90", 8, NULL))
				failedToErase(1, hConsole);
		}
		if (!WriteProcessMemory(hProcess, (LPVOID)igmUnlockPtr, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 10, NULL))
			failedToErase(2, hConsole);
		if (!WriteProcessMemory(hProcess, (LPVOID)igmUnlockPtr0, "\x90\x90\x90\x90\x90\x90\x90\x90", 8, NULL))
			failedToErase(3, hConsole);
		if (!WriteProcessMemory(hProcess, (LPVOID)igmUnlockPtr1, "\x90\x90\x90\x90\x90\x90\x90\x90", 8, NULL))
			failedToErase(4, hConsole);
		if (!WriteProcessMemory(hProcess, (LPVOID)igmUnlockPtr2, "\x90\x90\x90\x90\x90\x90\x90\x90", 8, NULL))
			failedToErase(5, hConsole);
		if (!WriteProcessMemory(hProcess, (LPVOID)igmUnlockPtr3, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 10, NULL))
			failedToErase(6, hConsole);
		if (!WriteProcessMemory(hProcess, (LPVOID)fovUnlockPtr, "\x90\x90\x90\x90\x90\x90\x90", 7, NULL))
			failedToErase(7, hConsole);
		/*
		nop, nop ,nop, nop, nop, nop, nop, nop, nop, nop, nop
		movss xmm7, 0x01e160bc
		nop, nop
		*/
		if (!WriteProcessMemory(hProcess, (LPVOID)actionAoeFixPtr, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\xF3\x0F\x10\x3C\x25\xBC\x60\xE1\x01\x90\x90", 23, NULL))
			failedToErase(9, hConsole);
		if (gVars.uConfig.bEnableFullSpeedMode)
			if (!WriteProcessMemory(hProcess, (LPVOID)fullSpeedUnlockPtr, "\x90\x90\x90\x90\x90\x90\x90\x90", 8, NULL))
				failedToErase(8, hConsole);


		
		//Setting default frametimes, loop until this works
		while (targetFrameTime != gVars.uConfig.requestedMinFramerate)
		{
			writeMinFrametime(hProcess, gVars.uConfig.requestedMinFramerate);
			ReadProcessMemory(hProcess, (LPVOID)tftPtr, (LPVOID)&targetFrameTime, sizeof(double), NULL);
		}

		printf("Setting desired FOV and Gamma...\n");
		DWORD protection;
		VirtualProtectEx(hProcess, (LPVOID)gammaPtr, sizeof(float), PAGE_READWRITE, &protection);
		VirtualProtectEx(hProcess, (LPVOID)fovPtr, sizeof(float), PAGE_READWRITE, &protection);

		WriteProcessMemory(hProcess, (LPVOID)gammaPtr, &gVars.uConfig.gamma, sizeof(float), NULL);
		WriteProcessMemory(hProcess, (LPVOID)fovPtr, &gVars.uConfig.fov, sizeof(float), NULL);
		WriteProcessMemory(hProcess, (LPVOID)framerateCoefPtr, &framerateCoef, sizeof(float), NULL);

		SetConTAttribute(hConsole, cc_WARN);

		clock_t tick = clock() / CLOCKS_PER_SEC;

		HWND cWnd = GetConsoleWindow();

		MSG Message;
		WNDCLASSEX wClass;
		wClass.cbClsExtra = NULL;
		wClass.cbSize = sizeof(WNDCLASSEX);
		wClass.cbWndExtra = NULL;
		wClass.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(0, 0, 0));
		wClass.hCursor = LoadCursor(0, IDC_ARROW);
		wClass.hIcon = LoadIcon(0, IDI_APPLICATION);
		wClass.hIconSm = LoadIcon(0, IDI_APPLICATION);
		wClass.hInstance = GetModuleHandle(nullptr);
		wClass.lpfnWndProc = WndProc;
		wClass.lpszClassName = overlayName;
		wClass.lpszMenuName = overlayName;
		wClass.style = CS_VREDRAW | CS_HREDRAW;
		if (!RegisterClassEx(&wClass))
			SetConTAttribute("Could not register WNDCLASS.\nThe in-game overlay will not work.", cc_ERROR);

		HWND overlay = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED, overlayName, overlayName, WS_POPUP, 1, 1, Width, Height, 0, 0, 0, 0);
		SetLayeredWindowAttributes(overlay, 0, 1.0f, LWA_ALPHA);
		SetLayeredWindowAttributes(overlay, 0, RGB(0, 0, 0), LWA_COLORKEY);

		if (overlay == NULL) SetConTAttribute("Overlay window failed to initialize.\n", cc_ERROR);

		pMenu = &dMenu;
		pVars = &gVars;
		pOverlay = &overlay;
		pGame = &FFXIIWND;

		d3dHook::DirectXInit(overlay);
		ShowWindow(overlay, SW_SHOW);

		for (;;)
		{
			if (!IsWindow(FFXIIWND)) 
			{
				SetConTAttribute(hConsole, cc_ERROR, "Window closed, stopping...\n");
				break;
			}

			if (PeekMessage(&Message, overlay, 0, 0, PM_REMOVE))
			{
				DispatchMessage(&Message);
				TranslateMessage(&Message);
			}

			ReadProcessMemory(hProcess, (LPVOID)rftPtr, (LPVOID)&realFrametime, sizeof(double), NULL);
			ReadProcessMemory(hProcess, (LPVOID)worldMPtr, (LPVOID)&gVars.worldTime, sizeof(float), NULL);
			ReadProcessMemory(hProcess, (LPVOID)igmSPtr, (LPVOID)&gVars.igmState, sizeof(uint8_t), NULL);
			ReadProcessMemory(hProcess, (LPVOID)uiOPtr, (LPVOID)&uiState, sizeof(uint8_t), NULL);
			ReadProcessMemory(hProcess, (LPVOID)titlPtr, (LPVOID)&titleState, sizeof(uint8_t), NULL);
			ReadProcessMemory(hProcess, (LPVOID)cMenPtr, (LPVOID)&cMenState, sizeof(uint8_t), NULL);
			ReadProcessMemory(hProcess, (LPVOID)cScenePtr, (LPVOID)&gVars.inCutscene, sizeof(uint8_t), NULL);
			ReadProcessMemory(hProcess, (LPVOID)moviePtr, (LPVOID)&inMovie, sizeof(uint8_t), NULL);

			focusState = FFXIIWND != GetForegroundWindow();

			//Faster than previous method; should cause less bugs
			gVars.gameStateEnum =
				gVars.inCutscene == 1
				? 4 : inMovie == 1 && uiState == 0
				? 3 : focusState == 1 
				? 2 : (uiState == 1 || cMenState == 1) //(uiState == 1 || titleState == 1 || cMenState == 1) TITLESTATE BREAKS REKS (Must find a more accurate value)
				? 1 
				: 0; 

			//if (gVars.gameStateEnum != lastUseMenuLimitState)
			//{
				switch (gVars.gameStateEnum)
				{
				case 0:
					//printf("User has exited a menu!\n");
					targetFrameTime = gVars.uConfig.requestedMinFramerate;
					break;
				case 1:
					//printf("User has entered a menu!\n");
					targetFrameTime = gVars.uConfig.requestedMinFramerateMenus;
					break;
				case 2:
					//printf("Game window has lost focus!\n");
					targetFrameTime = gVars.uConfig.requestedMinFramerateNoFocus;
					break;
				case 3:
					//printf("User has entered a movie!\n");
					targetFrameTime = gVars.uConfig.requestedMinFramerateMovies;
					break;
				case 4:
					//printf("The user is in a cutscene; reducing framerate!\n");
					targetFrameTime = gVars.uConfig.requestedMinFramerateMovies;
					break;
				}
				writeMinFrametime(hProcess, targetFrameTime);
				lastUseMenuLimitState = gVars.gameStateEnum; //Only write back to the frametime when needed
			//}

			//timeScale = realFrametime / targetFrameTime;
			gVars.inGameMouseMultiplier = gVars.uConfig.bEnableAdaptiveMouse ?
				(gVars.uConfig.lockedMouseMulti * gVars.inGameMultiplier) / timeScale
				: gVars.uConfig.lockedMouseMulti;

			WriteProcessMemory(hProcess, (LPVOID)gammaPtr, &gVars.uConfig.gamma, sizeof(float), NULL);

			gVars.cTime = ((double)clock() / CLOCKS_PER_SEC);
			if (gVars.cTime - tick >= (realFrametime))
			{ 
				tickf(hProcess, gVars);
				WriteProcessMemory(hProcess, (LPVOID)fovPtr, &gVars.uConfig.fov, sizeof(float), NULL);
				tick = gVars.cTime;
			}

			Sleep(1000 * (realFrametime / mainThreadUpdateCoef)); /*Improve CPU time
									  Also accounts for framerate fluctuations, with an effort to update x times per frame.*/
		}
	}

	CloseHandle(hProcess);
	std::cin.get();
	return 0;
}



