// Chapter 2. Simple cci_f (modified Caesar cipher) implementation
#include <io.h>
#include "Everything.h"

#define BUF_SIZE 65536

// Simplified Caesar ciper implementation
// fIn : Soource file pathname
// fOut : Destionation file pathname
// shift : Numberical shift
// Behavior is modeled after CopyFile
BOOL cci_f(LPCTSTR fIn, LPCTSTR fOut, DWORD shift) {
	HANDLE hIn, hOut;
	DWORD nIn, nOut, iCopy;
	BYTE buffer[BUF_SIZE], bShift = (BYTE)shift;
	BOOL writeOK = TRUE;

	hIn = CreateFile(fIn, GENERIC_READ, 0, NULL, OPEN_EXISTING,
		FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hIn == INVALID_HANDLE_VALUE)
		return FALSE;

	hOut = CreateFile(fOut, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hOut == INVALID_HANDLE_VALUE) {
		CloseHandle(hIn);
		return FALSE;
	}

	while (writeOK && ReadFile(hIn, buffer, BUF_SIZE, &nIn, NULL) && nIn > 0) {
		for (iCopy = 0; iCopy < nIn; iCopy++)
			buffer[iCopy] = buffer[iCopy] + bShift;
		writeOK = WriteFile(hOut, buffer, nIn, &nOut, NULL);
	}

	CloseHandle(hIn);
	CloseHandle(hOut);

	return writeOK;
}

