#include <io.h>
#include "Everything.h"

BOOL cci_f(LPCTSTR, LPCTSTR, DWORD);

int _tmain(int argc, LPTSTR argv[]) {
	if (argc != 4)
		ReportError(_T("Usage: cci shift file1 file2"), 1, FALSE);

	if (!cci_f(argv[2], argv[3], _ttoi(argv[1])))
		ReportError(_T("Encryption failed."), 4, TRUE);

	return 0;
}