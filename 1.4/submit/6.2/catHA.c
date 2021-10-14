/* Chapter 2. cat. */
/* cat [options] [files]
		Only the -s option is used. Others are ignored.
		-s suppresses error report when a file does not exist */

#include "Everything.h"

#define BUF_SIZE 0x200

static VOID CatFile(HANDLE, HANDLE);

int _tmain(int argc, LPTSTR argv[]) {
	HANDLE hInFile, hStdIn = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	BOOL dashS;
	int iArg, iFirstFile;

	/*	dashS will be set only if "-s" is on the command line. */
	/*	iFirstFile is the argv [] index of the first input file. */
	iFirstFile = Options(argc, argv, _T("s"), &dashS, NULL);

	_tprintf(_T("catHA %s\n"), argv[iFirstFile]);

	if (iFirstFile == argc) {
		/* No files in arg list. */
		CatFile(hStdIn, hStdOut);
		return 0;
	}


	/* Process the input files. */
	for (iArg = iFirstFile; iArg < argc; iArg++) {
		hInFile = (HANDLE)(_ttoi(argv[iArg]));
		CatFile(hInFile, hStdOut);
		if (GetLastError() != 0 && !dashS) {
			ReportError(_T("Cat Error: Could not process file completely."), 0, TRUE);
		}
		CloseHandle(hInFile);
	}
	return 0;
}

static VOID CatFile(HANDLE hInFile, HANDLE hOutFile) {
	DWORD nIn, nOut;
	BYTE buffer[BUF_SIZE];

	while (ReadFile(hInFile, buffer, BUF_SIZE, &nIn, NULL) && (nIn != 0) &&
		WriteFile(hOutFile, buffer, nIn, &nOut, NULL));

	return;
}
