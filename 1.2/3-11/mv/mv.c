#include "Everything.h"

#define BUF_SIZE 15792

void CreateDirectoryFromPath(LPTSTR path, LPTSTR currentPath);
BOOL TraverseDirectoryAndCopy(LPTSTR destination, LPTSTR currentDirectory, LPTSTR parent);
void setTime(HANDLE fileToSet, HANDLE original);
BOOL CopyFileToDirectory(LPTSTR fileName, LPTSTR directory, LPTSTR currentDirectory, BOOL isPreserve);

int main(int argc, LPTSTR argv[]) {
	if (argc != 3) {
		printf("ERROR:  Incorrect number of arguments\n\n");
		printf("Description:\n");
		printf("  Moves a directory and its contents\n\n");
		printf("Usage:\n");
		_tprintf(_T("  mv [source] [target_dir]\n\n"));
		return 1;
	}

	TCHAR  sourceLocation[MAX_PATH + 1], moveLocation[MAX_PATH + 1], currentDir[MAX_PATH + 1];
	LPTSTR pSlash;

	GetCurrentDirectory(MAX_PATH, currentDir);

	_tcscpy_s(sourceLocation, MAX_PATH, argv[1]);
	_tcscpy_s(moveLocation, MAX_PATH, argv[2]);
	GetFullPathName(moveLocation, MAX_PATH, moveLocation, NULL);
	GetFullPathName(sourceLocation, MAX_PATH, sourceLocation, NULL);

	CreateDirectoryFromPath(moveLocation, currentDir);

	// remove \\ at the end of path
	if (sourceLocation[_tcsclen(sourceLocation) - 1] == _T('\\')) {
		sourceLocation[_tcsclen(sourceLocation) - 1] = _T('\0');
	}

	//if there is not \\ at the end of move location then add \\ in order to add file name if move file
	if (moveLocation[_tcsclen(moveLocation) - 1] != _T('\\')) {
		_tcscat_s(moveLocation, MAX_PATH, _T("\\"));
	}
	pSlash = _tstrrchr(sourceLocation, _T('\\'));
	_tcscat_s(moveLocation, MAX_PATH, pSlash + 1);

	//remove \\ at the end of path
	if (moveLocation[_tcsclen(moveLocation) - 1] == _T('\\')) {
		moveLocation[_tcsclen(moveLocation) - 1] = _T('\0');
	}

	if (sourceLocation[0] == moveLocation[0]) {
		if (!MoveFileEx(sourceLocation, moveLocation, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
			printf("MoveFileEx failed with error %d\n", GetLastError());
			return 2;
		}
		else _tprintf(_T("%s has been moved to %s\n"), argv[1], argv[2]);
	}
	else {
		DWORD fType = GetFileAttributes(sourceLocation);
		if (fType & FILE_ATTRIBUTE_DIRECTORY) {

			//if move directory to other disk, just copy. I reused the cpW functions that i had written
			if (!TraverseDirectoryAndCopy(moveLocation, sourceLocation, currentDir)) {
				printf("MoveFileEx failed with error %d\n", GetLastError());
				return 2;
			}
			else _tprintf(_T("%s has been moved to %s\n"), argv[1], argv[2]);
		}
		else {
			if (!MoveFileEx(sourceLocation, moveLocation, MOVEFILE_COPY_ALLOWED)) {
				printf("MoveFileEx failed with error %d\n", GetLastError());
				return 2;
			}
			else _tprintf(_T("%s has been moved to %s\n"), argv[1], argv[2]);
		}
	}

	return 0;
}

// recursive create directory from path
static void CreateDirectoryFromPath(LPTSTR path, LPTSTR currentPath) {
	TCHAR directoryToCopy[MAX_PATH + 1];
	LPTSTR folder, nextFolder = NULL;
	_tcscpy_s(directoryToCopy, MAX_PATH, path);
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

/* Traverse current directory and copy all of its content to other directory. USE ABSOLUTE PATH */
static BOOL TraverseDirectoryAndCopy(LPTSTR destination, LPTSTR currentDirectory, LPTSTR parent) {
	HANDLE searchHandle;
	WIN32_FIND_DATA findData;
	DWORD fType, iPass;

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
			/* For each file located, get the type. List everything on pass 1.
				On pass 2, display the directory name and recursively process
				the subdirectory contents, if the recursive option is set. */

				//check file type
			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				fType = ((lstrcmp(findData.cFileName, _T(".")) == 0 || lstrcmp(findData.cFileName, _T("..")) == 0)) ? TYPE_DOT : TYPE_DIR;
			}
			else fType = TYPE_FILE;

			if (iPass == 1 && fType == TYPE_FILE) {
				CopyFileToDirectory(findData.cFileName, destination, currentDirectory, TRUE);
			}

			/* Traverse the subdirectory on the second pass. */
			if (fType == TYPE_DIR && iPass == 2) {
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
					TraverseDirectoryAndCopy(destination, currentDirectory, parent);

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

static void setTime(HANDLE fileToSet, HANDLE original) {
	FILETIME lpCreationTime, lpLastAccessTime, lpLastWriteTime;
	GetFileTime(original, &lpCreationTime, &lpLastAccessTime, &lpLastWriteTime);
	SetFileTime(fileToSet, &lpCreationTime, &lpLastAccessTime, &lpLastWriteTime);
}