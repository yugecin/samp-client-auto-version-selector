#define _CRT_SECURE_NO_DEPRECATE
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <tlhelp32.h>
#include <string.h>

#ifndef _DEBUG
#define OutputDebugStringA
#endif

static HANDLE *hSuspendedThreads;
static int numSuspendedThreads;

static
void
ResumeThreads()
{
	HANDLE hExistingThread;

	while (numSuspendedThreads) {
		hExistingThread = hSuspendedThreads[--numSuspendedThreads];
		ResumeThread(hExistingThread);
		CloseHandle(hExistingThread);
	}
	free(hSuspendedThreads);
	hSuspendedThreads = NULL;
}

/**
* @return non-zero on success
*/
static
int
GetConnectingSampServerIpPort(char *buf, int *port)
{
	char *cmdline;
	char *pos;

	cmdline = GetCommandLineA();
	OutputDebugStringA(cmdline);
	// skip gta path, just in case it contains "-h " or "-p "
	cmdline = strstr(cmdline, "gta_sa.exe");
	if (!cmdline) {
		OutputDebugStringA("no gta_sa.exe");
		return 0;
	}

	pos = strstr(cmdline, "-h ");
	if (!pos) {
		OutputDebugStringA("no -h");
		return 0;
	}
	pos += 3;
	while (*pos && *pos != ' ') {
		*(buf++) = *(pos++);
	}
	*buf = 0;

	pos = strstr(cmdline, "-p ");
	if (!pos) {
		OutputDebugStringA("no -p");
		return 0;
	}
	pos += 3;
	*port = 0;
	while (*pos && *pos != ' ') {
		*port = *port * 10 + *(pos++) - '0';
	}

	return 1;
}

/**
* @return non-zero if the server is DL version
*/
static
int
IsServerDL()
{
	char szDebug[200];
	char ip[30], rule_name[255], rule_value[255];
	char *ruleptr;
	unsigned char rule_name_len, rule_value_len;
	int port, await_attempt;
	struct sockaddr_in addr;
	struct sockaddr_in remote_client;
	WSADATA wsaData;
	WORD wWSAVersionRequested;
	SOCKET sock;
	DWORD flags;
#pragma pack(push,1)
	struct {
		char samp[4];
		int ip;
		char port[2];
		char opcode;
	} query_req;
	struct {
		char copy_of_request[11];
		short num_rules;
		char ruledata[20 * 255 * 2];
	} query_res;
	int recvsize;
#pragma pack(pop)

	if (!GetConnectingSampServerIpPort(ip, &port)) {
		OutputDebugStringA("Failed to find connecting samp server IP");
		return 0;
	}
	sprintf(szDebug, "Connecting to %s:%d", ip, port);
	OutputDebugStringA(szDebug);

	wWSAVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wWSAVersionRequested, &wsaData) ||
		LOBYTE(wsaData.wVersion) != 2 ||
		HIBYTE(wsaData.wVersion) != 2)
	{
		OutputDebugStringA("Failed to load WSA");
		return 0;
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) {
		OutputDebugStringA("Failed to create socket");
		WSACleanup();
		return 0;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(port);
	if (connect(sock, (struct sockaddr*) &addr, sizeof(addr)) == SOCKET_ERROR) {
		closesocket(sock);
		WSACleanup();
		return 0;
	}
	/*set non-blocking*/
	flags = 1; ioctlsocket(sock, FIONBIO, &flags);

	query_req.samp[0] = 'S';
	query_req.samp[1] = 'A';
	query_req.samp[2] = 'M';
	query_req.samp[3] = 'P';
	query_req.ip = *(int*) &addr.sin_addr.S_un;
	query_req.port[0] = port & 0xFF;
	query_req.port[1] = (port >> 8) & 0xFF;
	query_req.opcode = 'r';
	send(sock, (char*) &query_req, sizeof(query_req), 0);
	shutdown(sock, SD_SEND);

#define SLEEP_TIME 50
	for (await_attempt = 0; await_attempt < 2500 / SLEEP_TIME; await_attempt++) {
		Sleep(SLEEP_TIME);
		recvsize = recvfrom(sock, (char*) &query_res, sizeof(query_res), 0, 0, 0);
		if (recvsize > 11) {
			closesocket(sock);
			WSACleanup();
			sprintf(szDebug, "response from server: %d rules", query_res.num_rules);
			OutputDebugStringA(szDebug);
			ruleptr = query_res.ruledata;
			while (query_res.num_rules) {
				query_res.num_rules--;
				rule_name_len = (unsigned char) *ruleptr;
				memcpy(rule_name, ruleptr + 1, rule_name_len);
				ruleptr += 1 + rule_name_len;
				rule_value_len = (unsigned char) *ruleptr;
				memcpy(rule_value, ruleptr + 1, rule_value_len);
				ruleptr += 1 + rule_value_len;
				sprintf(szDebug, "rule %s: %s", rule_name, rule_value);
				OutputDebugStringA(szDebug);
				if (!strcmp("version", rule_name)) {
					return !!strstr(rule_value, "DL");
				}
			}
			return 0;
		}
	}

	OutputDebugStringA("server did not respond");
	closesocket(sock);
	WSACleanup();
	return 0;
}

static
void
__stdcall
DoLoad(HMODULE hModule)
{
	if (IsServerDL()) {
		LoadLibraryA("samp-versions/0.3.DL/samp.dll");
	} else {
		LoadLibraryA("samp-versions/0.3.7/samp.dll");
	}
	ResumeThreads();
	FreeLibraryAndExitThread(hModule, /*exit code*/ 0);
}

BOOL
APIENTRY
DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	DWORD currentThreadId, currentProcessId;
	HANDLE hThread, hExistingThread;
	HANDLE hThreadSnapshot;
	THREADENTRY32 threadEntry;

	if (ul_reason_for_call != DLL_PROCESS_ATTACH) {
		return TRUE;
	}

	// Windows warns against doing things in DllMain
	// (https://docs.microsoft.com/en-us/windows/win32/dlls/dllmain)
	// so starting a thread instead that will do the ping and load the samp library.
	// In the meantime, this code below will suspend all threads of the process,
	// which then will be resumed in the thread once it's done loading samp.
	hThreadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, /*current process*/ 0);
	if (hThreadSnapshot != INVALID_HANDLE_VALUE) {
		threadEntry.dwSize = sizeof(THREADENTRY32);
		if (Thread32First(hThreadSnapshot, &threadEntry)) {
			numSuspendedThreads = 1;
			while (Thread32Next(hThreadSnapshot, &threadEntry)) {
				numSuspendedThreads++;
			}
			hSuspendedThreads = (void*) malloc(sizeof(HANDLE) * numSuspendedThreads);
			Thread32First(hThreadSnapshot, &threadEntry);
			currentThreadId = GetCurrentThreadId();
			currentProcessId = GetCurrentProcessId();
			numSuspendedThreads = 0;
			do {
				if (threadEntry.th32OwnerProcessID == currentProcessId &&
					threadEntry.th32ThreadID != currentThreadId)
				{
					hExistingThread = OpenThread(THREAD_SUSPEND_RESUME, TRUE, threadEntry.th32ThreadID);
					if (hExistingThread) {
						SuspendThread(hExistingThread);
						hSuspendedThreads[numSuspendedThreads] = hExistingThread;
						numSuspendedThreads++;
					}
				}
			} while (Thread32Next(hThreadSnapshot, &threadEntry));
			CloseHandle(hThreadSnapshot);
		} else {
			hSuspendedThreads = NULL;
			numSuspendedThreads = 0;
		}
		hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) DoLoad, hModule, 0, NULL);
		if (hThread) {
			CloseHandle(hThread);
			return TRUE;
		}
		ResumeThreads();
	}
	// User32.dll functions aren't supposed to be used in DllMain... but oh well.
	MessageBoxA(NULL, "Failed", "samp-client-auto-version-selector", MB_OK | MB_ICONERROR);
	return TRUE;
}
