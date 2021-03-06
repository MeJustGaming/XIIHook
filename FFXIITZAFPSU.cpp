#include "pch.h"
#include "md5.h" //Credit to Frank Thilo
#include "Utility.h"

#define overlayName L"FFXII FPS UNLOCKER OVERLAY"

bool InjectDLL(HANDLE& hProcess, const std::string& DLLPath)
{
	long DSize = DLLPath.length() + 1;

	LPVOID Alloc = VirtualAllocEx(hProcess, NULL, DSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (Alloc == NULL)
	{
		printf("[!]Fail to allocate memory in Target Process.\n");
		return false;
	}

	printf("Allocating memory in Targer Process.\n");
	int DidWrite = WriteProcessMemory(hProcess, Alloc, DLLPath.c_str(), DSize, 0);
	if (DidWrite == 0)
	{
		printf("Fail to write in Target Process memory.\n");
		return false;
	}
	printf("Creating Remote Thread in Target Process\n");

	DWORD dWord;
	LPTHREAD_START_ROUTINE addrLoadLibrary = (LPTHREAD_START_ROUTINE)GetProcAddress(LoadLibrary(L"kernel32"), "LoadLibraryA");
	HANDLE ThreadReturn = CreateRemoteThread(hProcess, NULL, 0, addrLoadLibrary, Alloc, 0, &dWord);
	WaitForSingleObject(ThreadReturn, INFINITE);
	if (ThreadReturn == NULL)
	{
		printf("Failed to create Remote Thread\n");
		return false;
	}

	if ((hProcess != NULL) && (Alloc != NULL) && (DidWrite != ERROR_INVALID_HANDLE) && (ThreadReturn != NULL))
	{
		printf("DLL Successfully Injected\n");
		CloseHandle(ThreadReturn);
		return true;
	}

	return false;
}


int main()
{
	const char dllRel[] = "XIIHook.dll";
	char dllPath[MAX_PATH];
	GetFullPathNameA(dllRel, MAX_PATH, dllPath, nullptr);

	//Process handles
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	HWND FFXIIWND = findXII();
	HANDLE hProcess;
	DWORD pId;

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
		//Overwriting instructions
		/*if (!WriteProcessMemory(hProcess, (LPVOID)mouseUnlockPtr, "\x90\x90\x90\x90\x90\x90\x90\x90", 8, NULL))
			failedToErase(0, hConsole);
		if (!WriteProcessMemory(hProcess, (LPVOID)mouseUnlockPtr0, "\x90\x90\x90\x90\x90\x90\x90\x90", 8, NULL))
			failedToErase(1, hConsole);
		if (!WriteProcessMemory(hProcess, (LPVOID)igmUnlockPtr, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 10, NULL))
			failedToErase(2, hConsole);
		if (!WriteProcessMemory(hProcess, (LPVOID)igmUnlockPtr0, "\x90\x90\x90\x90\x90\x90\x90\x90", 8, NULL))
			failedToErase(3, hConsole);
		if (!WriteProcessMemory(hProcess, (LPVOID)igmUnlockPtr1, "\x90\x90\x90\x90\x90\x90\x90\x90", 8, NULL))
			failedToErase(5, hConsole);
		if (!WriteProcessMemory(hProcess, (LPVOID)igmUnlockPtr2, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90", 10, NULL))
			failedToErase(6, hConsole);
		if (!WriteProcessMemory(hProcess, (LPVOID)fovUnlockPtr, "\x90\x90\x90\x90\x90\x90\x90", 7, NULL))
			failedToErase(7, hConsole);*/

		/*
		nop, nop, nop, nop, nop, nop, nop, nop, nop, nop, nop
		movss xmm7, 0x01e160bc
		nop, nop
		
		This will be hard to find again
		if (!WriteProcessMemory(hProcess, (LPVOID)actionAoeFixPtr, "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\xF3\x0F\x10\x3C\x25\xBC\x60\xE1\x01\x90\x90", 23, NULL))
			failedToErase(9, hConsole);*/
		/*
		lea eax, [rcx+340h]
		mov 0x1e160d4, eax
		nop
		
		if (!WriteProcessMemory(hProcess, (LPVOID)setAnimationRatePtr, "\x8D\x81\x40\x03\x00\x00\xA3\xD4\x60\xE1\x01\x00\x00\x00\x00\x90", 16, NULL));
			failedToErase(10, hConsole);*/

		if (!InjectDLL(hProcess, dllPath)) {
			printf("Injection failure!\n");
			for (;;) {}
		}
		CloseHandle(hProcess);

		printf("Injection successful. This window will close itself in 10 seconds.\n");
		Sleep(10000);
	}
	
	return 0;
}



