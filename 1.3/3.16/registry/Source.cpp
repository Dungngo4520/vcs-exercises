#include "Everything.h"
#include "Windows.h"
#include "TlHelp32.h"
#include "iostream"

#define MAX_KEY_LENGTH 255
#define MAX_VALUE_NAME 16383
#define MAX_TREE_LEVEL 512
#define VK_ESCAPE 0x1B

void QueryKey(HKEY hKey) {
	TCHAR achKey[MAX_KEY_LENGTH]; // buffer for subkey name
	DWORD cbName; // size of name string 
	TCHAR achClass[MAX_PATH] = TEXT(""); // buffer for class name 
	DWORD cchClassName = MAX_PATH; // size of class string 
	DWORD cSubKeys = 0; // number of subkeys 
	DWORD cbMaxSubKey; // longest subkey size 
	DWORD cchMaxClass; // longest class string 
	DWORD cValues; // number of values for key 
	DWORD cchMaxValue; // longest value name 
	DWORD cbMaxValueData; // longest value data 
	DWORD cbSecurityDescriptor; // size of security descriptor 
	FILETIME ftLastWriteTime; // last write time 

	DWORD i;

	TCHAR achValue[MAX_VALUE_NAME];
	DWORD cchValue = MAX_VALUE_NAME;

	// Get the class name and the value count. 
	DWORD retCode = RegQueryInfoKey(
		hKey,
		achClass,
		&cchClassName,
		nullptr,
		&cSubKeys,
		&cbMaxSubKey,
		&cchMaxClass,
		&cValues,
		&cchMaxValue,
		&cbMaxValueData,
		&cbSecurityDescriptor,
		&ftLastWriteTime
	);

	// Enumerate the subkeys, until RegEnumKeyEx fails.
	if (cSubKeys != 0) {
		printf("\nNumber of subkeys: %d\n", cSubKeys);
		for (i = 0; i < cSubKeys; i++) {
			cbName = MAX_KEY_LENGTH;
			retCode = RegEnumKeyEx(hKey, i, achKey, &cbName, nullptr, nullptr, nullptr, &ftLastWriteTime);
			if (retCode == ERROR_SUCCESS) {
				printf("%d, %s\n", i + 1, achKey);
			}
		}
	}

	// Enumerate the key values. 
	if (cValues != 0) {
		printf("\nNumber of values: %d\n", cValues);
		for (i = 0, retCode = ERROR_SUCCESS; i < cValues; i++) {
			cchValue = MAX_VALUE_NAME;
			achValue[0] = '\0';
			retCode = RegEnumValue(hKey, i, achValue, &cchValue, nullptr, nullptr, nullptr, nullptr);

			if (retCode == ERROR_SUCCESS) {
				printf("%d, %s\n", i + 1, achValue);
			}
		}
	}
	printf("\n");
}

int main() {

	TCHAR path[MAX_KEY_LENGTH * MAX_TREE_LEVEL + 1];
	_tcscpy_s(path,MAX_VALUE_NAME,_T(""));

	printf("use command \"cd\" to open subkey, \"dir\" to list subkey and values\n");
	printf("press esc key to exit\n");
	system("pause");
	system("cls");
	//printf("1, HKEY_CLASS_ROOT\n");
	//printf("2, HKEY_CURRENT_USER\n");
	//printf("3, HKEY_LOCAL_MACHINE\n");
	//printf("4, HKEY_USERS\n");
	//printf("5, HKEY_CURRENT_CONFIG\n");

	HKEY hKey;
	RegOpenKeyEx(HKEY_CURRENT_USER, path, 0,KEY_READ, &hKey);
	while (!GetAsyncKeyState(VK_ESCAPE)) {
		//remove trailing backslash
		if (path[strlen(path) - 1] == '\\') {
			path[strlen(path) - 1] = '\0';
		}
		printf("HKEY_CURRENT_USER\\%s>", path);

		TCHAR command[MAX_VALUE_NAME + 1] = {0};
		_fgetts(command, sizeof command,stdin); //get input
		//remove trailing newline
		if (command[strlen(command) - 1] == _T('\n')) {
			command[strlen(command) - 1] = '\0';
		}

		//traverse key
		if (_tcsncmp(command, "cd ", 3 * sizeof TCHAR) == 0) {
			TCHAR inputPath[MAX_KEY_LENGTH + 1] = {0};
			LPTSTR firstSpace = _tcschr(command,_T(' ')); // get the path from input

			if (firstSpace != nullptr) {
				//remove excessive space
				while (firstSpace[0] == ' ') {
					firstSpace = firstSpace + 1;
				}

				_tcscpy_s(inputPath, MAX_KEY_LENGTH, firstSpace);
			}

			//if path is .., open parent key
			if (_tcscmp(inputPath, _T("..")) == 0) {
				const LPTSTR pSlash = _tcsrchr(path, '\\');
				if (pSlash == nullptr) {
					path[0] = _T('\0');
				}
				else {
					*pSlash = _T('\0');
				}

				RegOpenKeyEx(HKEY_CURRENT_USER, path, 0,KEY_READ, &hKey);
			}
			else {
				//append inputPath to path
				if (path[0] != '\0') {
					_tcscat_s(path,_T("\\"));
				}
				_tcscat_s(path,MAX_KEY_LENGTH, inputPath);

				//if failed to open subkey, remove the inputPath from path and open parent key
				if (RegOpenKeyEx(HKEY_CURRENT_USER, path, 0,KEY_READ, &hKey) != ERROR_SUCCESS) {
					const LPTSTR pSlash = _tcsrchr(path, '\\');
					if (pSlash == nullptr) {
						path[0] = _T('\0');
					}
					else {
						*pSlash = _T('\0');
					}
					RegOpenKeyEx(HKEY_CURRENT_USER, path, 0,KEY_READ, &hKey);
				}
			}
		}

		else if (_tcscmp(command, "dir") == 0) {
			//enumerate key and value
			QueryKey(hKey);
		}
		else {
			//open value and change, only for DWORD and TEXT
			DWORD dataType = 0;
			DWORD dataSize = 0;
			PVOID pvData = nullptr;
			constexpr DWORD flags = RRF_RT_REG_SZ | RRF_RT_DWORD;

			//get data type and size
			LONG result = RegGetValue(hKey, nullptr, command, flags, &dataType, &pvData, &dataSize);

			if (dataType == REG_SZ) {
				const DWORD buffSize = dataSize / sizeof(TCHAR) + 1;
				const auto text = new TCHAR[buffSize];

				result = RegGetValue(hKey, nullptr, command, flags, nullptr, text, &dataSize);

				if (result == ERROR_SUCCESS) {
					printf("Value: %s\nType: REG_SZ", text);
					printf("Do you wish to change: Y/N: ");
				}
			}
			else if (dataType == REG_DWORD) {
				result = RegGetValue(hKey, nullptr, command, flags, nullptr, &pvData, &dataSize);
				if (result == ERROR_SUCCESS) {
					printf("Value: %d\nType: REG_DWORD\n", (int)pvData);
					printf("Do you wish to change: Y/N: ");
				}
			}

			if (result == ERROR_SUCCESS) {
				TCHAR option[MAX_VALUE_NAME];
				_fgetts(option, sizeof(option),stdin);
				if (option[strlen(option) - 1] == _T('\n')) {
					option[strlen(option) - 1] = '\0';
				}

				if (_tcscmp(option,_T("Y")) == 0 || _tcscmp(option,_T("y")) == 0) {
					printf("Enter new value: ");
					RegOpenKeyEx(HKEY_CURRENT_USER, path, 0,KEY_READ | KEY_SET_VALUE, &hKey);

					TCHAR newValue[MAX_VALUE_NAME];
					_fgetts(newValue, sizeof newValue,stdin);
					if (newValue[strlen(newValue) - 1] == _T('\n')) {
						newValue[strlen(newValue) - 1] = '\0';
					}
					if (dataType == REG_SZ) {
						if (RegSetValueEx(hKey, command, 0,REG_SZ, (LPBYTE)newValue, sizeof(newValue)) ==
							ERROR_SUCCESS) {
							printf("Success\n");
						}
						else printf("Failed to set value.\n");
					}
					else if (dataType == REG_DWORD) {
						//get number from input, if there is none, 0 is used
						TCHAR* ptr;
						DWORD value = _tcstol(newValue, &ptr, 10);
						if (RegSetValueEx(hKey, command, 0,REG_DWORD, (LPBYTE)&value, sizeof(value)) ==
							ERROR_SUCCESS) {
							printf("Success\n");
						}
						else printf("Failed to set value.\n");
					}
				}
			}

		}
	}
	RegCloseKey(hKey);
}
