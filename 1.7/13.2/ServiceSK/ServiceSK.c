/*Chapter 14. serviceSK.c
serverSK (Chapter 12) converted to an NT service */
/* Build as a mutltithreaded console application */
/*	Usage: simpleService [-c]								*/
/*			-c says to run as a console app, not a service	*/
/*  Main routine that starts the service control dispatcher */
/*	Everything at the beginning is a "service wrapper",
the same as in SimpleService.c" */

#pragma warning(disable : 4996)

#include "Everything.h"
#include "ClientServer.h"
#define UPDATE_TIME 1000	/* One second between udpdates */

VOID FileLogEvent(LPCTSTR, DWORD, BOOL);
void WINAPI ServiceMain(DWORD argc, LPTSTR argv[]);
VOID WINAPI ServerCtrlHandler(DWORD);
void UpdateStatus(int, int);
int  ServiceSpecific(int, LPTSTR *);

static FILE *hLogFile; /* Text Log file */
static SERVICE_STATUS hServStatus;
static SERVICE_STATUS_HANDLE hSStat; /* Service status handle for setting status */
volatile static int ShutFlag = 0;
volatile static int PauseFlag = 0;

static LPTSTR ServiceName = _T("serviceSK");
static LPTSTR LogFileName = "serviceSKLog.txt";
static BOOL consoleApp = FALSE, isService;
static SC_HANDLE hScm;

VOID _tmain(int argc, LPTSTR argv[])
{
	SERVICE_TABLE_ENTRY DispatchTable[] =
	{
		{ ServiceName,				ServiceMain },
		{ NULL,						NULL }
	};

	Options(argc, argv, _T("c"), &consoleApp, NULL);
	isService = !consoleApp;

	if (!WindowsVersionOK(3, 1))
		ReportError(_T("This program requires Windows NT 3.1 or greater"), 1, FALSE);

	if (isService) {
		if (!StartServiceCtrlDispatcher(DispatchTable))
			ReportError(_T("Failed calling StartServiceCtrlDispatcher"), 1, FALSE);
	}
	else {
		ServiceSpecific(argc, argv);
	}
	return;
}


/*	ServiceMain entry point, called when the service is created by
the main program.  */
void WINAPI ServiceMain(DWORD argc, LPTSTR argv[])
{
	DWORD i;

	hScm = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);

	/*  Set the current directory and open a log file, appending to
	an existing file */
	if (argc > 2) SetCurrentDirectory(argv[2]);
	hLogFile = fopen(LogFileName, _T("w+"));
	if (hLogFile == NULL) return;

	FileLogEvent(_T("Starting service. First log entry."), 0, FALSE);
	_ftprintf(hLogFile, _T("\nargc = %d"), argc);
	for (i = 0; i < argc; i++)
		_ftprintf(hLogFile, _T("\nargv[%d] = %s"), i, argv[i]);
	FileLogEvent(_T("Entering ServiceMain."), 0, FALSE);

	hServStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	hServStatus.dwCurrentState = SERVICE_START_PENDING;
	hServStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP |
		SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PAUSE_CONTINUE;
	hServStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
	hServStatus.dwServiceSpecificExitCode = 0;
	hServStatus.dwCheckPoint = 0;
	hServStatus.dwWaitHint = 2 * UPDATE_TIME;

	hSStat = RegisterServiceCtrlHandler(ServiceName, ServerCtrlHandler);
	if (hSStat == 0)
		FileLogEvent(_T("Cannot register control handler"), 100, TRUE);

	FileLogEvent(_T("Control handler registered successfully"), 0, FALSE);
	SetServiceStatus(hSStat, &hServStatus);
	FileLogEvent(_T("Service status set to SERVICE_START_PENDING"), 0, FALSE);

	/*  Start the service-specific work, now that the generic work is complete */
	if (ServiceSpecific(argc, argv) != 0) {
		hServStatus.dwCurrentState = SERVICE_STOPPED;
		hServStatus.dwServiceSpecificExitCode = 1;  /* Server initilization failed */
		SetServiceStatus(hSStat, &hServStatus);
		return;
	}
	FileLogEvent(_T("Service threads shut down. Set SERVICE_STOPPED status"), 0, FALSE);
	/*  We will only return here when the ServiceSpecific function
	completes, indicating system shutdown. */
	UpdateStatus(SERVICE_STOPPED, 0);
	FileLogEvent(_T("Service status set to SERVICE_STOPPED"), 0, FALSE);
	fclose(hLogFile);  /*  Clean up everything, in general */
	CloseServiceHandle(hScm);
	return;

}


/*	Control Handler Function */
VOID WINAPI ServerCtrlHandler(DWORD Control)
// requested control code 
{
	SC_HANDLE hSc;

	switch (Control) {
	case SERVICE_CONTROL_SHUTDOWN:
	case SERVICE_CONTROL_STOP:
		ShutFlag = TRUE;	/* Set the global ShutFlag flag */
		UpdateStatus(SERVICE_STOP_PENDING, -1);
		break;
	case SERVICE_CONTROL_PAUSE:
		PauseFlag = TRUE;
		UpdateStatus(SERVICE_PAUSE_PENDING, -1);
		break;
	case SERVICE_CONTROL_CONTINUE:
		PauseFlag = FALSE;
		UpdateStatus(SERVICE_CONTINUE_PENDING, -1);
		break;
	case SERVICE_CONTROL_INTERROGATE:
		hSc = OpenService(hScm, ServiceName, SERVICE_QUERY_STATUS);
		QueryServiceStatus(hSc, &hServStatus);
		_ftprintf(hLogFile, _T("\nStatus from  QueryServiceStatus"));
		_ftprintf(hLogFile, _T("\nService Status"));
		_ftprintf(hLogFile, _T("\nServiceType: %d"), hServStatus.dwServiceType);
		_ftprintf(hLogFile, _T("\nCurrentState: %d"), hServStatus.dwCurrentState);
		_ftprintf(hLogFile, _T("\nControlAccd: %d"), hServStatus.dwControlsAccepted);
		_ftprintf(hLogFile, _T("\nWin32ExitCode: %d"), hServStatus.dwWin32ExitCode);
		_ftprintf(hLogFile, _T("\nServiceSpecificExitCode: %d"), hServStatus.dwServiceSpecificExitCode);
		_ftprintf(hLogFile, _T("\nCheckPoint: %d"), hServStatus.dwCheckPoint);
		_ftprintf(hLogFile, _T("\ndwWaitHint: %d"), hServStatus.dwWaitHint);
		break;
	default:
		if (Control > 127 && Control < 256) /* User Defined */
			break;
	}
	UpdateStatus(-1, -1);
	return;
}

void UpdateStatus(int NewStatus, int Check)
/*  Set a new service status and checkpoint (either specific value or increment) */
{
	if (Check < 0) hServStatus.dwCheckPoint++;
	else			hServStatus.dwCheckPoint = Check;
	if (NewStatus >= 0) hServStatus.dwCurrentState = NewStatus;
	if (!SetServiceStatus(hSStat, &hServStatus))
		FileLogEvent(_T("Cannot set service status"), 101, TRUE);
	return;
}

/*	FileLogEvent is similar to the ReportError function used elsewhere
For a service, however, we ReportEvent rather than write to standard
error. Eventually, this function should go into the utility
library.  */

VOID FileLogEvent(LPCTSTR UserMessage, DWORD EventCode, BOOL PrintErrorMsg)

/*  General-purpose function for reporting system errors.
Obtain the error number and turn it into the system error message.
Display this information and the user-specified message to the open log FILE
UserMessage:		Message to be displayed to standard error device.
EventCode:
PrintErrorMessage:	Display the last system error message if this flag is set. */
{
	DWORD eMsgLen, ErrNum = GetLastError();
	LPTSTR lpvSysMsg;
	TCHAR MessageBuffer[512];
	/*  Not much to do if this fails but to keep trying. */

	if (PrintErrorMsg) {
		eMsgLen = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM, NULL,
			ErrNum, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpvSysMsg, 0, NULL);

		_stprintf(MessageBuffer, _T("\n%s %s ErrNum = %d. EventCode = %d."),
			UserMessage, lpvSysMsg, ErrNum, EventCode);
		if (lpvSysMsg != NULL) LocalFree(lpvSysMsg);
		/* Explained in Chapter 5. */
	}
	else {
		_stprintf(MessageBuffer, _T("\n%s EventCode = %d."),
			UserMessage, EventCode);
	}

	fputs(MessageBuffer, hLogFile);
	return;
}


/*	This is the service-specific function, or "main" and is
called from the more generic ServiceMain
It calls a renamed version of serverSK from
Chapter 12.
In general, you could use a separate source file, or even
put this in a DLL.  */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/* Chapter 12. Client/Server. SERVER PROGRAM.  SOCKET VERSION	*/
/* MODIFIED TO BE A SERVICE										*/
/* Execute the command in the request and return a response.	*/
/* Commands will be exeuted in process if a shared library 	*/
/* entry point can be located, and out of process otherwise	*/
/* ADDITIONAL FEATURE: argv[1] can be the name of a DLL supporting */
/* in process services */

struct sockaddr_in srvSAddr;		/* Server's Socket address structure */
struct sockaddr_in connectSAddr;	/* Connected socket with client details   */
WSADATA WSStartData;				/* Socket library data structure   */

typedef struct SERVER_ARG_TAG { /* Server thread arguments */
	volatile DWORD	number;
	volatile SOCKET	sock;
	volatile DWORD	status; /* 0: Does not exist, 1: Stopped, 2: Running
							3: Stop entire system */
	volatile HANDLE hSrvThread;
	HINSTANCE	 hDll; /* Shared libary handle */
} SERVER_ARG;

static BOOL ReceiveRequestMessage(REQUEST *pRequest, SOCKET);
static BOOL SendResponseMessage(RESPONSE *pResponse, SOCKET);
static DWORD WINAPI Server(SERVER_ARG *);
static DWORD WINAPI AcceptThread(SERVER_ARG *);
static DWORD WINAPI Handler(SERVER_ARG *);

static HANDLE sArgs[MAX_CLIENTS];
static SOCKET SrvSock = INVALID_SOCKET, ConnectSock = INVALID_SOCKET;

int ServiceSpecific(int argc, LPTSTR argv[])
/* int _tmain (int argc, LPCTSTR argv []) */
{
	/* serverSK source goes here */
	/* Server listening and connected sockets. */
	DWORD iThread, tStatus;
	SERVER_ARG sArgs[MAX_CLIENTS];
	HANDLE hAcceptThread = NULL;
	HINSTANCE hDll = NULL;

	if (!WindowsVersionOK(3, 1))
		ReportError(_T("This program requires Windows NT 3.1 or greater"), 1, FALSE);

	/* Console control handler to permit server shutdown */
	if (!SetConsoleCtrlHandler(Handler, TRUE))
		ReportError(_T("Cannot create Ctrl handler"), 1, TRUE);

	/*	Initialize the WS library. Ver 2.0 */
	if (WSAStartup(MAKEWORD(2, 0), &WSStartData) != 0)
		ReportError(_T("Cannot support sockets"), 1, TRUE);

	/* Open the shared command library DLL if it is specified on command line */
	if (argc > 1) {
		hDll = LoadLibrary(argv[1]);
		if (hDll == NULL) ReportError(argv[1], 0, TRUE);
	}

	/* Intialize thread arg array */
	for (iThread = 0; iThread < MAX_CLIENTS; iThread++) {
		sArgs[iThread].number = iThread;
		sArgs[iThread].status = 2; // 2: Running
		sArgs[iThread].sock = 0;
		sArgs[iThread].hDll = hDll;
		sArgs[iThread].hSrvThread = NULL;
	}
	/*	Follow the standard server socket/bind/listen/accept sequence */
	SrvSock = socket(PF_INET, SOCK_STREAM, 0);
	if (SrvSock == INVALID_SOCKET)
		ReportError(_T("Failed server socket() call"), 1, TRUE);

	/*	Prepare the socket address structure for binding the
	server socket to port number "reserved" for this service.
	Accept requests from any client machine.  */

	srvSAddr.sin_family = AF_INET;
	srvSAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	srvSAddr.sin_port = htons(SERVER_PORT);
	if (bind(SrvSock, (struct sockaddr *)&srvSAddr, sizeof(srvSAddr)) == SOCKET_ERROR)
		ReportError(_T("Failed server bind() call"), 2, TRUE);
	if (listen(SrvSock, MAX_CLIENTS) != 0)
		ReportError(_T("Server listen() error"), 3, TRUE);

	/* Main thread becomes listening/connecting/monitoring thread */
	/* Find an empty slot in the server thread arg array */
	while (!ShutFlag) {
		iThread = 0;
		while (!ShutFlag && !PauseFlag) {
			/* Continously poll the thread thState of all server slots in the sArgs table */
			if (sArgs[iThread].status == 1) {
				/* This thread stopped, either normally or there's a shutdown request */
				/* Wait for it to stop, and make the slot free for another thread */
				tStatus = WaitForSingleObject(sArgs[iThread].hSrvThread, INFINITE);
				if (tStatus != WAIT_OBJECT_0)
					ReportError(_T("Server thread wait error"), 4, TRUE);
				CloseHandle(sArgs[iThread].hSrvThread);
				sArgs[iThread].hSrvThread = NULL;
				sArgs[iThread].status = 2;
			}
			/* Free slot identified or shut down. Use a free slot for the next connection */
			if (sArgs[iThread].status == 2 || ShutFlag) break;

			/* Fixed July 25, 2014: iThread = (iThread++) % MAX_CLIENTS; */
			iThread = (iThread + 1) % MAX_CLIENTS;
			if (iThread == 0) Sleep(50); /* Break the polling loop */
										 /* An alternative would be to use an event to signal a free slot */
		}
		if (ShutFlag) break;
		if (PauseFlag) continue;
		/* sArgs[iThread] == SERVER_SLOT_FREE */
		/* Wait for a connection on this socket */
		/* Use a separate thread so that we can poll the ShutFlag flag */

		hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, &sArgs[iThread], 0, NULL);
		if (hAcceptThread == NULL)
			ReportError(_T("Error creating AcceptThreadread."), 1, TRUE);
		while (!ShutFlag && !PauseFlag) {
			tStatus = WaitForSingleObject(hAcceptThread, CS_TIMEOUT);
			if (tStatus == WAIT_OBJECT_0) {
				/* Connection is complete. sArgs[iThread] == SERVER_THREAD_RUNNING */
				if (!ShutFlag && !PauseFlag) {
					CloseHandle(hAcceptThread);
					hAcceptThread = NULL;
				}
				break;
			}
		}


	}  /* OUTER while (!ShutFlag) */

	   /* ShutFlag == TRUE */
	_tprintf(_T("Server shutdown in process. Wait for all server threads\n"));
	/* Wait for any active server threads to terminate */
	/* Try continuously as some threads may be long running. */

	while (TRUE) {
		int nRunningThreads = 0;
		for (iThread = 0; iThread < MAX_CLIENTS; iThread++) {
			if (sArgs[iThread].status == 2 || sArgs[iThread].status == 1) {
				if (WaitForSingleObject(sArgs[iThread].hSrvThread, 10000) == WAIT_OBJECT_0) {
					_tprintf(_T("Server thread on slot %d stopped.\n"), iThread);
					CloseHandle(sArgs[iThread].hSrvThread);
					sArgs[iThread].hSrvThread = NULL;
					sArgs[iThread].status = 0;
				}
				else
					if (WaitForSingleObject(sArgs[iThread].hSrvThread, 10000) == WAIT_TIMEOUT) {
						_tprintf(_T("Server thread on slot %d still running.\n"), iThread);
						nRunningThreads++;
					}
					else {
						_tprintf(_T("Error waiting on server thread in slot %d.\n"), iThread);
						ReportError(_T("Thread wait failure"), 0, TRUE);
					}

			}
		}
		if (nRunningThreads == 0) break;
	}

	if (hDll != NULL) FreeLibrary(hDll);

	/* Redundant shutdown */
	shutdown(SrvSock, SD_BOTH);
	closesocket(SrvSock);
	WSACleanup();
	if (hAcceptThread != NULL && WaitForSingleObject(hAcceptThread, INFINITE) != WAIT_OBJECT_0)
		ReportError(_T("Failed waiting for accept thread to terminate."), 7, FALSE);
	return 0;
}

static DWORD WINAPI AcceptThread(PVOID pArg)
{
	LONG addrLen;
	SERVER_ARG * pThArg = (SERVER_ARG *)pArg;

	addrLen = sizeof(connectSAddr);
	pThArg->sock =
		accept(SrvSock, (struct sockaddr *)&connectSAddr, &addrLen);
	if (pThArg->sock == INVALID_SOCKET) {
		ReportError(_T("accept: invalid socket error"), 1, TRUE);
		return 1;
	}
	/* A new connection. Create a server thread */
	pThArg->hSrvThread = (HANDLE)_beginthreadex(NULL, 0, Server, pThArg, 0, NULL);
	if (pThArg->hSrvThread == NULL)
		ReportError(_T("Failed creating server thread"), 1, TRUE);
	pThArg->status = 2;
	_tprintf(_T("Client accepted on slot: %d, using server thread %d.\n"), pThArg->number, GetThreadId(pThArg->hSrvThread));
	/* Exercise: Display client machine and process information */
	return 0;
}

BOOL WINAPI Handler(DWORD CtrlEvent)
{
	/* Recives ^C. Shutdown the system */
	if (CtrlEvent == CTRL_C_EVENT) {
		_tprintf(_T("Shutting Down\n"));
		InterlockedIncrement(&ShutFlag);
	}
	/* Recives Ctrl+Break. Shutdown the system */
	else if (CtrlEvent == CTRL_BREAK_EVENT) {
		if (PauseFlag) {
			_tprintf(_T("Continuing\n"));
			InterlockedDecrement(&PauseFlag);
		}
		else {
			_tprintf(_T("Pausing\n"));
			InterlockedIncrement(&PauseFlag);
		}
	}
	return TRUE;
}

static DWORD WINAPI Server(PVOID pArg)

/* Server thread function. There is a thread for every potential client. */
{
	/* Each thread keeps its own request, response,
	and bookkeeping data structures on the stack. */
	/* NOTE: All messages are in 8-bit characters */

	BOOL done = FALSE;
	STARTUPINFO startInfoCh;
	SECURITY_ATTRIBUTES tempSA = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
	PROCESS_INFORMATION procInfo;
	SOCKET connectSock;
	int commandLen;
	REQUEST request;	/* Defined in ClientServer.h */
	RESPONSE response;	/* Defined in ClientServer.h.*/
	char sysCommand[MAX_RQRS_LEN];
	TCHAR tempFile[100];
	HANDLE hTmpFile;
	FILE *fp = NULL;
	int(__cdecl *dl_addr)(char *, char *);
	SERVER_ARG * pThArg = (SERVER_ARG *)pArg;
	enum SERVER_THREAD_STATE threadState;

	GetStartupInfo(&startInfoCh);

	connectSock = pThArg->sock;
	/* Create a temp file name */
	tempFile[sizeof(tempFile) / sizeof(TCHAR) - 1] = _T('\0');
	_stprintf_s(tempFile, sizeof(tempFile) / sizeof(TCHAR) - 1, _T("ServerTemp%d.tmp"), pThArg->number);

	while (!done && !ShutFlag && !PauseFlag) { 	/* Main Server Command Loop. */
		done = ReceiveRequestMessage(&request, connectSock);

		request.record[sizeof(request.record) - 1] = '\0';
		commandLen = (int)(strcspn(request.record, "\n\t"));
		memcpy(sysCommand, request.record, commandLen);
		sysCommand[commandLen] = '\0';
		_tprintf(_T("Command received on server slot %d: %s\n"), pThArg->number, sysCommand);

		/* Restest ShutFlag, as it can be set from the console control handler. */
		done = done || (strcmp(request.record, "$Quit") == 0) || ShutFlag || PauseFlag;
		if (done) continue;

		/* Open the temporary results file. */
		hTmpFile = CreateFile(tempFile, GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, &tempSA,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hTmpFile == INVALID_HANDLE_VALUE)
			ReportError(_T("Cannot create temp file"), 1, TRUE);

		/* Check for a shared library command. For simplicity, shared 	*/
		/* library commands take precedence over process commands 	*/
		/* First, extract the command name (space delimited) */
		dl_addr = NULL; /* will be set if GetProcAddress succeeds */
		if (pThArg->hDll != NULL) { /* Try Server "In process" */
			char commandName[256] = "";
			int commandNameLength = (int)(strcspn(sysCommand, " "));
			strncpy_s(commandName, sizeof(commandName), sysCommand, min(commandNameLength, sizeof(commandName) - 1));
			dl_addr = (int(*)(char *, char *))GetProcAddress(pThArg->hDll, commandName);
			/* You really need to trust this DLL not to corrupt the server */
			/* This assumes that we don't allow the DLL to generate known exceptions */
			if (dl_addr != NULL) __try {
				(*dl_addr)(request.record, tempFile);
			}
			__except (EXCEPTION_EXECUTE_HANDLER) { /* Exception in the DLL */
				_tprintf(_T("Unhandled Exception in DLL. Terminate server. There may be orphaned processes."));
				return 1;
			}
		}

		if (dl_addr == NULL) { /* No inprocess support */
							   /* Create a process to carry out the command. */
			startInfoCh.hStdOutput = hTmpFile;
			startInfoCh.hStdError = hTmpFile;
			startInfoCh.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
			startInfoCh.dwFlags = STARTF_USESTDHANDLES;
			if (!CreateProcess(NULL, request.record, NULL,
				NULL, TRUE, /* Inherit handles. */
				0, NULL, NULL, &startInfoCh, &procInfo)) {
				PrintMsg(hTmpFile, _T("ERR: Cannot create process."));
				procInfo.hProcess = NULL;
			}
			CloseHandle(hTmpFile);
			if (procInfo.hProcess != NULL) {
				CloseHandle(procInfo.hThread);
				WaitForSingleObject(procInfo.hProcess, INFINITE);
				CloseHandle(procInfo.hProcess);
			}
		}

		/* Respond a line at a time. It is convenient to use
		C library line-oriented routines at this point. */

		/* Send the temp file, one line at a time, with header, to the client. */
		if (_tfopen_s(&fp, tempFile, _T("r")) == 0) {
			{
				response.rsLen = MAX_RQRS_LEN;
				while ((fgets(response.record, MAX_RQRS_LEN, fp) != NULL))
					SendResponseMessage(&response, connectSock);
			}
			/* Send a zero length message. Messages are 8-bit characters, not UNICODE. */
			response.record[0] = '\0';
			SendResponseMessage(&response, connectSock);
			fclose(fp); fp = NULL;
			DeleteFile(tempFile);
		}
		else {
			ReportError(_T("Failed to open temp file with command results"), 0, TRUE);
		}

	}   /* End of main command loop. Get next command */

		/* done || ShutFlag */
		/* End of command processing loop. Free resources and exit from the thread. */
	_tprintf(_T("Shuting down server thread number %d\n"), pThArg->number);
	/* Redundant shutdown. There are no further attempts to send or receive */
	shutdown(connectSock, SD_BOTH);
	closesocket(connectSock);

	threadState = pThArg->status = 1;

	return threadState;
}

BOOL ReceiveRequestMessage(REQUEST *pRequest, SOCKET sd)
{
	BOOL disconnect = FALSE;
	LONG32 nRemainRecv = 0, nXfer;
	LPBYTE pBuffer;

	/*	Read the request. First the header, then the request text. */
	nRemainRecv = RQ_HEADER_LEN;
	pBuffer = (LPBYTE)pRequest;

	while (nRemainRecv > 0 && !disconnect) {
		nXfer = recv(sd, pBuffer, nRemainRecv, 0);
		if (nXfer == SOCKET_ERROR)
			ReportError(_T("server request recv() failed"), 11, TRUE);
		disconnect = (nXfer == 0);
		nRemainRecv -= nXfer; pBuffer += nXfer;
	}

	/*	Read the request record */
	nRemainRecv = pRequest->rqLen;
	/* Exclude buffer overflow */
	nRemainRecv = min(nRemainRecv, MAX_RQRS_LEN);

	pBuffer = (LPSTR)pRequest->record;
	while (nRemainRecv > 0 && !disconnect) {
		nXfer = recv(sd, pBuffer, nRemainRecv, 0);
		if (nXfer == SOCKET_ERROR)
			ReportError(_T("server request recv() failed"), 12, TRUE);
		disconnect = (nXfer == 0);
		nRemainRecv -= nXfer; pBuffer += nXfer;
	}

	return disconnect;
}

BOOL SendResponseMessage(RESPONSE *pResponse, SOCKET sd)
{
	BOOL disconnect = FALSE;
	LONG32 nRemainRecv = 0, nXfer, nRemainSend;
	LPBYTE pBuffer;

	/*	Send the response up to the string end. Send in
	two parts - header, then the response string. */
	nRemainSend = RS_HEADER_LEN;
	pResponse->rsLen = (long)(strlen(pResponse->record) + 1);
	pBuffer = (LPBYTE)pResponse;
	while (nRemainSend > 0 && !disconnect) {
		nXfer = send(sd, pBuffer, nRemainSend, 0);
		if (nXfer == SOCKET_ERROR) ReportError(_T("server send() failed"), 13, TRUE);
		disconnect = (nXfer == 0);
		nRemainSend -= nXfer; pBuffer += nXfer;
	}

	nRemainSend = pResponse->rsLen;
	pBuffer = (LPSTR)pResponse->record;
	while (nRemainSend > 0 && !disconnect) {
		nXfer = send(sd, pBuffer, nRemainSend, 0);
		if (nXfer == SOCKET_ERROR) ReportError(_T("server send() failed"), 14, TRUE);
		disconnect = (nXfer == 0);
		nRemainSend -= nXfer; pBuffer += nXfer;
	}
	return disconnect;
}
