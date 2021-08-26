#include "Everything.h"

#define BUF_SIZE 15792  /* Optimal in several experiments. Small values such as 256 give very bad performance */

BOOL CopyFileToDirectory(LPTSTR fileName, LPTSTR directory, LPTSTR currentDirectory, BOOL isPreserve);
void CreateDirectoryFromPath(LPTSTR filePath, LPTSTR currentPath);
void setTime(HANDLE fileToSet, HANDLE original);
BOOL TraverseDirectoryAndCopy(LPTSTR directory, LPTSTR currentDirectory, LPTSTR original, BOOL flags[]);

int main(int argc, LPTSTR argv[]) {
	TCHAR oDirectory[MAX_PATH + 1];
	int fileIndex;
	BOOL flags[MAX_OPTIONS];

	GetCurrentDirectory(MAX_PATH, oDirectory);

	fileIndex = Options(argc, argv, _T("pr"), &flags[0], &flags[1], NULL);

	if (argc > 4 || argc < 3) {
		fprintf(stderr, "Usage: cpW [-p] sourceFile destination or cp [-r][-p] destination\n");
		return 1;
	}
	if (flags[1]) {
		TCHAR destination[MAX_PATH + 1];
		GetFullPathName(argv[fileIndex], MAX_PATH, destination, NULL);
		CreateDirectoryFromPath(destination, oDirectory);
		TraverseDirectoryAndCopy(destination, oDirectory, oDirectory, flags);
	}
	else {
		CreateDirectoryFromPath(argv[fileIndex + 1], oDirectory);
		if (!CopyFileToDirectory(argv[fileIndex], argv[fileIndex + 1], oDirectory, flags[0])) {
			fprintf(stderr, "Usage: cpW [-p] sourceFile destination or cp [-r][-p] destination\n");
			return 1;
		}
	}
	return 0;
}

/* Traverse current directory and copy all of its content to other directory. USE ABSOLUTE PATH */
static BOOL TraverseDirectoryAndCopy(LPTSTR destination, LPTSTR currentDirectory, LPTSTR parent, BOOL flags[]) {
	HANDLE searchHandle;
	WIN32_FIND_DATA findData;
	BOOL recursive = flags[1];
	DWORD fType, iPass;
	TCHAR subDirectory[MAX_PATH + 1];

	SetCurrentDirectory(currentDirectory);

	/* Open up the directory search handle and get the
		first file name to satisfy the path name. Make two passes.
		The first processes the files and the second processes the directories. */
	for (iPass = 1; iPass <= 2; iPass++) {

		//add \\ if path does not have
		if (currentDirectory[_tcslen(currentDirectory) - 1] != _T('\\')) {
			_tcscat_s(currentDirectory, MAX_PATH, _T("\\"));
		}
		if (destination[_tcslen(destination) - 1] != _T('\\')) {
			_tcscat_s(destination, MAX_PATH, _T("\\"));
		}

		_tcscat_s(currentDirectory, MAX_PATH, _T("*")); //search anyfile
		searchHandle = FindFirstFile(currentDirectory, &findData);
		if (GetLastError() == ERROR_NO_MORE_FILES) {
			CreateDirectoryFromPath(destination, parent); //just create emty folder
		}
		currentDirectory[_tcsclen(currentDirectory) - 1] = _T('\0');

		if (searchHandle == INVALID_HANDLE_VALUE) {
			ReportError(_T("Error opening Search Handle."), 0, TRUE);
			return FALSE;
		}

		/* Scan the directory and its subdirectories for files satisfying the pattern. */
		do {
			//check file type
			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				fType = ((lstrcmp(findData.cFileName, _T(".")) == 0 || lstrcmp(findData.cFileName, _T("..")) == 0)) ? TYPE_DOT : TYPE_DIR;
			}
			else fType = TYPE_FILE;

			if (iPass == 1 && fType == TYPE_FILE) {
				CopyFileToDirectory(findData.cFileName, destination, currentDirectory, flags[0]);
			}

			/* Traverse the subdirectory on the second pass. */
			if (fType == TYPE_DIR && iPass == 2 && recursive) {
				if (_tcslen(destination) + _tcslen(findData.cFileName) >= MAX_PATH_LONG - 1) {
					ReportError(_T("Path Name is too long"), 10, FALSE);
				}
				//add file name to current path to check if it is destination, if not it will cause infinite loop
				_tcscat_s(currentDirectory, MAX_PATH, findData.cFileName);
				_tcscat_s(currentDirectory, MAX_PATH, _T("\\"));

				if (lstrcmp(destination, currentDirectory) != 0) {
					_tcscat_s(destination, MAX_PATH, findData.cFileName);
					_tcscat_s(destination, MAX_PATH, _T("\\"));

					CreateDirectoryFromPath(destination, parent);
					TraverseDirectoryAndCopy(destination, currentDirectory, parent, flags);

					//remove current folder path if copied all of its files
					destination[_tcsclen(destination) - _tcsclen(findData.cFileName) - 1] = _T('\0');
					currentDirectory[_tcsclen(currentDirectory) - _tcsclen(findData.cFileName) - 1] = _T('\0');
				}
			}
		} while (FindNextFile(searchHandle, &findData) && lstrcmp(destination, currentDirectory) != 0);

		//just to make sure current directory is not change
		SetCurrentDirectory(parent);
		FindClose(searchHandle);
	}
	return TRUE;
}

// copy file to any directory. If the directory is not exist, it creates and copies file into it.
static BOOL CopyFileToDirectory(LPTSTR fileName, LPTSTR directory, LPTSTR currentDirectory, BOOL isPreserve) {
	HANDLE hIn, hOut;
	DWORD nIn, nOut;
	LPTSTR pFileExtension;
	CHAR buffer[BUF_SIZE];
	TCHAR fileExtension[MAX_PATH + 1];
	BOOL samePlace = FALSE;

	hIn = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hIn == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Cannot open input file. Error: %x\n", GetLastError());
		return FALSE;
	}

	SetCurrentDirectory(directory);

	_tcscpy_s(fileName, MAX_PATH, fileName); //get file name
	pFileExtension = _tstrrchr(fileName, _T('.')); //get file extension
	_tcscpy_s(fileExtension, MAX_PATH, pFileExtension);
	if (!(pFileExtension[1] == '\\')) {
		*pFileExtension = _T('\0'); // remove file extension
	}
	else {
		fileExtension[0] = _T('\0');
	}



	TCHAR tempName[MAX_PATH + 1];
	_tcscpy_s(tempName, MAX_PATH, fileName);
	_tcscat_s(tempName, MAX_PATH, fileExtension);

	hOut = CreateFile(tempName, GENERIC_WRITE, 0, NULL,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hOut == INVALID_HANDLE_VALUE) {
		while (GetLastError() == 32) {
			samePlace = TRUE;
			_tcscpy_s(tempName, MAX_PATH, fileName);
			_tcscat_s(tempName, MAX_PATH, _T(" - Copy"));
			_tcscat_s(tempName, MAX_PATH, fileExtension);

			hOut = CreateFile(tempName, GENERIC_WRITE, 0, NULL,
				CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		}
		if (!samePlace) {
			fprintf(stderr, "Cannot open output file. Error: %x\n", GetLastError());
			CloseHandle(hIn);
			return FALSE;
		}
	}

	while (ReadFile(hIn, buffer, BUF_SIZE, &nIn, NULL) && nIn > 0) {
		WriteFile(hOut, buffer, nIn, &nOut, NULL);
		if (nIn != nOut) {
			fprintf(stderr, "Fatal write error: %x\n", GetLastError());
			CloseHandle(hIn); CloseHandle(hOut);
			return FALSE;
		}
	}

	if (isPreserve) {
		setTime(hOut, hIn);
	}
	SetCurrentDirectory(currentDirectory);
	CloseHandle(hIn);
	CloseHandle(hOut);
	return TRUE;
}

// recursive create directory if not exist
static void CreateDirectoryFromPath(LPTSTR filePath, LPTSTR currentPath) {
	TCHAR directoryToCopy[MAX_PATH + 1];
	LPTSTR folder, nextFolder = NULL;
	_tcscpy_s(directoryToCopy, MAX_PATH, filePath);
	GetFullPathName(directoryToCopy, MAX_PATH, directoryToCopy, NULL); //get destination directory
	folder = _tcstok_s(directoryToCopy, _T("\\"), &nextFolder); // split directory into each folder path by backslash
	while (folder != NULL) {
		if (folder != NULL) {
			TCHAR temp[MAX_PATH + 1];
			_tcscpy_s(temp, MAX_PATH, folder);
			_tcscat_s(temp, MAX_PATH, _T("\\\\")); // must add \\ at the end of directory to set because there is disk letter
			if (!SetCurrentDirectory(temp)) {
				CreateDirectory(folder, NULL); // create directory if not exist
				SetCurrentDirectory(temp);
			}
			folder = _tcstok_s(NULL, _T("\\"), &nextFolder); //delete current folder path and move to next
		}
	}
	SetCurrentDirectory(currentPath);
}

static void setTime(HANDLE fileToSet, HANDLE original) {
	FILETIME lpCreationTime, lpLastAccessTime, lpLastWriteTime;
	GetFileTime(original, &lpCreationTime, &lpLastAccessTime, &lpLastWriteTime);
	SetFileTime(fileToSet, &lpCreationTime, &lpLastAccessTime, &lpLastWriteTime);
}