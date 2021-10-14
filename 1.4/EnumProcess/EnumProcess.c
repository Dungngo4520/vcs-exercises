#include "Everything.h"
#include "TlHelp32.h"
#include "Psapi.h"

void PrintProcessInfo(PROCESSENTRY32 pe32) {
	DWORD size = MAX_PATH;
	TCHAR fileName[MAX_PATH] = _T("<unknown>");
	FILETIME ftCreate, ftExit, ftKernel, ftUser;
	SYSTEMTIME stCreate = {0, 0, 0, 0, 0, 0, 0, 0};

	// Get a handle to the process.
	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe32.th32ProcessID);

	if (hProcess != NULL) {
		HMODULE hMod;
		DWORD cbNeeded;

		if (EnumProcessModulesEx(hProcess, &hMod, sizeof(hMod), &cbNeeded, LIST_MODULES_ALL)) {
			// Get process executable file path
			GetModuleFileNameEx(hProcess, hMod, fileName, MAX_PATH);
			//QueryFullProcessImageName(hProcess, 0, fileName, &size);

			// Get process start time
			GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser);
			FileTimeToLocalFileTime(&ftCreate, &ftCreate);
			FileTimeToSystemTime(&ftCreate, &stCreate);

		}
		CloseHandle(hProcess);
	}

	// Print the process name and identifier.
	_tprintf(_T("%-50s | %10u | %10u | %02d/%02d/%04d %02d:%02d:%02d | %s\n"),
	         pe32.szExeFile, pe32.th32ProcessID, pe32.th32ParentProcessID,
	         stCreate.wMonth, stCreate.wDay, stCreate.wYear,
	         stCreate.wHour, stCreate.wMinute, stCreate.wSecond,
	         fileName);
}

int main(void) {
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;

	// Take process snapshot

	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnap == INVALID_HANDLE_VALUE) {
		_tprintf(_T("CreateToolhelp32Snapshot failed with error: %x"), GetLastError());
		return 1;
	}
	else {
		pe32.dwSize = sizeof(PROCESSENTRY32);
		if (!Process32First(hProcessSnap, &pe32)) {
			_tprintf(_T("Process32First failed with error: %x"), GetLastError());
			return 1;
		}

		_tprintf(_T("%-50hs | %10hs | %10hs | %19hs | %hs\n"),
		         "Process name", "PID", "PPID", "Start time", "Executable file path");

		do {
			PrintProcessInfo(pe32);
		}
		while (Process32Next(hProcessSnap, &pe32));

		CloseHandle(hProcessSnap);

		system("pause");
	}
	return 0;
}
