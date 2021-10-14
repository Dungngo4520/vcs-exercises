#include "Everything.h"

#pragma warning(disable : 4996)

BOOL TraverseDirectory(LPTSTR, LPTSTR, DWORD, LPBOOL);
DWORD FileType(LPWIN32_FIND_DATA);
BOOL ProcessItem(LPWIN32_FIND_DATA, DWORD, LPBOOL);

int _tmain(int argc, LPTSTR argv[])
{
	BOOL flags[MAX_OPTIONS], ok = TRUE;
	TCHAR searchPattern[MAX_PATH + 1], currPath[MAX_PATH_LONG + 1], parentPath[MAX_PATH_LONG + 1];
	LPTSTR pSlash, pSearchPattern;
	int i, fileIndex;
	DWORD pathLength;

	fileIndex = Options(argc, argv, _T("Rl"), &flags[0], &flags[1], NULL);

	/* "Parse" the search pattern into two parts: the "parent"
		and the file name or wild card expression. The file name
		is the longest suffix not containing a slash.
		The parent is the remaining prefix with the slash.
		This is performed for all command line search patterns.
		If no file is specified, use * as the search pattern. */

	pathLength = GetCurrentDirectory(MAX_PATH_LONG, currPath);
	if (pathLength == 0 || pathLength >= MAX_PATH_LONG) { /* pathLength >= MAX_PATH_LONG (32780) should be impossible */
		ReportError(_T("GetCurrentDirectory failed"), 1, TRUE);
	}

	if (argc < fileIndex + 1)
		ok = TraverseDirectory(currPath, _T("*"), MAX_OPTIONS, flags);
	else for (i = fileIndex; i < argc; i++) {
		if (_tcslen(argv[i]) >= MAX_PATH) {
			ReportError(_T("The command line argument is longer than the maximum this program supports"), 2, FALSE);
		}
		_tcscpy(searchPattern, argv[i]);
		_tcscpy(parentPath, argv[i]);

		/* Find the rightmost slash, if any.
			Set the path and use the rest as the search pattern. */
		pSlash = _tstrrchr(parentPath, _T('\\'));
		if (pSlash != NULL) {
			*pSlash = _T('\0');
			_tcscat(parentPath, _T("\\"));

			//SetCurrentDirectory(parentPath); //not use SetCurrentDirectory
			GetFullPathName(parentPath, MAX_PATH_LONG, parentPath, NULL); // use GetFullPathName instead

			pSlash = _tstrrchr(searchPattern, _T('\\'));
			pSearchPattern = pSlash + 1;
		}
		else {
			_tcscpy(parentPath, _T(".\\"));
			pSearchPattern = searchPattern;
		}
		ok = TraverseDirectory(parentPath, pSearchPattern, MAX_OPTIONS, flags) && ok;
		//SetCurrentDirectory(currPath);	 /* Restore working directory. */ //not use SetCurrentDirectory
	}

	return ok ? 0 : 1;
}

static BOOL TraverseDirectory(LPTSTR parentPath, LPTSTR searchPattern, DWORD numFlags, LPBOOL flags)

/* Traverse a directory, carrying out an implementation specific "action" for every
	name encountered. The action in this version is "list, with optional attributes". */
	/* searchPattern: Relative or absolute searchPattern to traverse in the parentPath.  */
	/* On entry, the current direcotry is parentPath, which ends in a \ */
{
	HANDLE searchHandle;
	WIN32_FIND_DATA findData;
	BOOL recursive = flags[0];
	DWORD fType, iPass, lenParentPath;
	TCHAR subdirectoryPath[MAX_PATH + 1];

	/* Open up the directory search handle and get the
		first file name to satisfy the path name.
		Make two passes. The first processes the files
		and the second processes the directories. */

	if (_tcslen(searchPattern) == 0) {
		_tcscat(searchPattern, _T("*"));
	}
	/* Add a backslash, if needed, at the end of the parent path */
	if (parentPath[_tcslen(parentPath) - 1] != _T('\\')) { /* Add a \ to the end of the parent path, unless there already is one */
		_tcscat(parentPath, _T("\\"));
	}


	if (flags[1]) {
		printf("%4s%11s%20s\t%19s\t%19s %s\n", "Mode", "Length","CreationTime","LastAccessTime", "LastWriteTime", "Name");
		printf("%4s%11s%20s\t%19s\t%19s %s\n", "----", "------", "------------", "-------------", "-------------", "----");
	}
	/* Open up the directory search handle and get the
		first file name to satisfy the path name. Make two passes.
		The first processes the files and the second processes the directories. */
	for (iPass = 1; iPass <= 2; iPass++) {
		_tcscat(parentPath, searchPattern); //add * at the end of search path
		searchHandle = FindFirstFile(parentPath, &findData);
		parentPath[_tcsclen(parentPath) - 1] = _T('\0'); //remove *

		if (searchHandle == INVALID_HANDLE_VALUE) {
			ReportError(_T("Error opening Search Handle."), 0, TRUE);
			return FALSE;
		}

		/* Scan the directory and its subdirectories for files satisfying the pattern. */
		do {
			/* For each file located, get the type. List everything on pass 1.
				On pass 2, display the directory name and recursively process
				the subdirectory contents, if the recursive option is set. */
			fType = FileType(&findData);
			if (iPass == 1) /* ProcessItem is "print attributes". */
				ProcessItem(&findData, MAX_OPTIONS, flags);

			lenParentPath = (DWORD)_tcslen(parentPath);
			/* Traverse the subdirectory on the second pass. */
			if (fType == TYPE_DIR && iPass == 2 && recursive) {
				_tprintf(_T("\n%s%s:\n"), parentPath, findData.cFileName);

				//SetCurrentDirectory(findData.cFileName); //not use SetCurrentDirectory

				if (_tcslen(parentPath) + _tcslen(findData.cFileName) >= MAX_PATH_LONG - 1) {
					ReportError(_T("Path Name is too long"), 10, FALSE);
				}
				_tcscat(parentPath, findData.cFileName); /* The parent path terminates with \ before the _tcscat call */
				TraverseDirectory(parentPath, _T("*"), numFlags, flags);
				parentPath[_tcsclen(parentPath) - _tcsclen(findData.cFileName) - 1] = _T('\0');

				//not use SetCurrentDirectory
				//_tcscpy(subdirectoryPath, parentPath);
				//_tcscat(subdirectoryPath, findData.cFileName); /* The parent path terminates with \ before the _tcscat call */
				//TraverseDirectory(subdirectoryPath, _T("*"), numFlags, flags);
				//subdirectoryPath[_tcsclen(subdirectoryPath) - _tcsclen(findData.cFileName)-1] = _T('\0');
				//SetCurrentDirectory(_T("..")); /* Restore the current directory */
			}

			/* Get the next file or directory name. */

		} while (FindNextFile(searchHandle, &findData));

		FindClose(searchHandle);
	}
	return TRUE;
}

static DWORD FileType(LPWIN32_FIND_DATA pFileData)

/* Return file type from the find data structure.
	Types supported:
		TYPE_FILE:	If this is a file
		TYPE_DIR:	If this is a directory other than . or ..
		TYPE_DOT:	If this is . or .. directory */
{
	BOOL isDir;
	DWORD fType;
	fType = TYPE_FILE;
	isDir = (pFileData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
	if (isDir)
		if (lstrcmp(pFileData->cFileName, _T(".")) == 0
			|| lstrcmp(pFileData->cFileName, _T("..")) == 0)
			fType = TYPE_DOT;
		else fType = TYPE_DIR;
	return fType;
}
/*  END OF BOILERPLATE CODE */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static BOOL ProcessItem(LPWIN32_FIND_DATA pFileData, DWORD numFlags, LPBOOL flags)

/* Function to process(list attributes, in this case)
	the file or directory. This implementation only shows
	the low order part of the file size. */
{
	const TCHAR fileTypeChar[] = { _T(' '), _T('d') };
	DWORD fType = FileType(pFileData);
	BOOL longList = flags[1];
	//SYSTEMTIME lastWrite;
	SYSTEMTIME localLastWrite, lastAccess, creation;

	//uncomment this line to not list the . and .. directory
	//if (fType != TYPE_FILE && fType != TYPE_DIR) return FALSE;

	if (longList) {
		_tprintf(_T("%5c"), fileTypeChar[fType == TYPE_FILE ? 0 : 1]);
		_tprintf(_T("%10d"), pFileData->nFileSizeLow);

		//result in local time
		FILETIME localFileTime;
		FileTimeToLocalFileTime(&(pFileData->ftLastWriteTime), &localFileTime);

		FileTimeToSystemTime(&(pFileData->ftCreationTime), &creation);
		_tprintf(_T("\t%02d/%02d/%04d %02d:%02d:%02d"),
			creation.wMonth, creation.wDay,
			creation.wYear, creation.wHour,
			creation.wMinute, creation.wSecond);

		FileTimeToSystemTime(&(pFileData->ftLastAccessTime), &lastAccess);
		_tprintf(_T("\t%02d/%02d/%04d %02d:%02d:%02d"),
			lastAccess.wMonth, lastAccess.wDay,
			lastAccess.wYear, lastAccess.wHour,
			lastAccess.wMinute, lastAccess.wSecond);

		FileTimeToSystemTime(&(pFileData->ftLastWriteTime), &localLastWrite);
		_tprintf(_T("\t%02d/%02d/%04d %02d:%02d:%02d"),
			localLastWrite.wMonth, localLastWrite.wDay,
			localLastWrite.wYear, localLastWrite.wHour,
			localLastWrite.wMinute, localLastWrite.wSecond);

		//result in system time
		/*FileTimeToSystemTime(&(pFileData->ftLastWriteTime), &lastWrite);
		_tprintf(_T("	%02d/%02d/%04d %02d:%02d:%02d"),
			lastWrite.wMonth, lastWrite.wDay,
			lastWrite.wYear, lastWrite.wHour,
			lastWrite.wMinute, lastWrite.wSecond);*/
	}
	_tprintf(_T(" %s\n"), pFileData->cFileName);
	return TRUE;
}
