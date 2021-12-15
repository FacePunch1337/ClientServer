#pragma comment (lib, "Ws2_32.lib")

#define  _CRT_SECURE_NO_WARNINGS
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#include <wchar.h>
#include <stdio.h>

#define CMD_START_SERVER 1001
#define CMD_STOP_SERVER 1002

HINSTANCE hInst;
HWND grpEndpoint, grpLog, serverLog;
HWND btnStart, btnStop;
HWND editIP, editPort;
SOCKET listenSocket;

LRESULT CALLBACK WinProc(HWND, UINT, WPARAM, LPARAM);
DWORD CALLBACK CreateUI(LPVOID);
DWORD CALLBACK StartServer(LPVOID);
DWORD CALLBACK StopServer(LPVOID);

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR cmdLine, _In_ int showMode) {

	hInst = hInstance;

	const WCHAR WIN_CLASS_NAME[] = L"ServerWindow";
	WNDCLASS wc = { };
	wc.lpfnWndProc = WinProc;
	wc.hInstance = hInst;
	wc.lpszClassName = WIN_CLASS_NAME;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);

	ATOM mainWin = RegisterClassW(&wc);
	if (mainWin == FALSE) {
		MessageBoxW(NULL, L"WndClass Reg Error", L"Server Error", MB_OK | MB_ICONSTOP);
		return -1;
	}

	HWND hwnd = CreateWindowExW(0, WIN_CLASS_NAME, L"TCP Chat - Server", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, 640, 480, NULL, NULL, hInst, NULL);
	if (hwnd == NULL) {
		MessageBoxW(NULL, L"Error create window", L"Server Error", MB_OK | MB_ICONSTOP);
		return -2;
	}

	ShowWindow(hwnd, showMode);
	MSG msg = {};
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	return 0;
}

LRESULT CALLBACK WinProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_CREATE:
		CreateUI(&hWnd);
		break;
	case WM_DESTROY: PostQuitMessage(0); break;
	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC dc = BeginPaint(hWnd, &ps);
		FillRect(dc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
		EndPaint(hWnd, &ps);
		break;
	}
	case WM_CTLCOLORSTATIC: {
		HDC dc = (HDC)wParam;
		HWND ctl = (HWND)lParam;
		if (ctl != grpEndpoint && ctl != grpLog) {
			SetBkMode(dc, TRANSPARENT);
			SetTextColor(dc, RGB(20, 20, 50));
		}
		return (LRESULT)GetStockObject(NULL_BRUSH);
	}
	case WM_COMMAND: {
		int cmd = LOWORD(wParam);
		int ntf = HIWORD(lParam);
		switch (cmd) {
		case CMD_START_SERVER: StartServer(&hWnd); break;
		case CMD_STOP_SERVER: StopServer(&hWnd); break;
		}
	}
	
	}
	return DefWindowProcW(hWnd, msg, wParam, lParam);
}

DWORD CALLBACK CreateUI(LPVOID params) {
	HWND hWnd = *((HWND*)params);
	grpEndpoint = CreateWindowExW(0, L"Button", L"EndPoint", BS_GROUPBOX | WS_CHILD | WS_VISIBLE,
		10, 10, 150, 150, hWnd, 0, hInst, NULL);
	CreateWindowExW(0, L"Static", L"IP:",  WS_CHILD | WS_VISIBLE,
		20, 30, 40, 15, hWnd, 0, hInst, NULL);
	editIP = CreateWindowExW(0, L"Edit", L"127.0.0.1",  WS_CHILD | WS_VISIBLE,
		60, 30, 80, 17, hWnd, 0, hInst, NULL);
	editPort = CreateWindowExW(0, L"Static", L"PORT:", WS_CHILD | WS_VISIBLE,
		20, 60, 45, 15, hWnd, 0, hInst, NULL);
	CreateWindowExW(0, L"Edit", L"8888", WS_CHILD | WS_VISIBLE,
		80, 60, 45, 17, hWnd, 0, hInst, NULL);

	grpLog = CreateWindowExW(0, L"Button", L"Server log", BS_GROUPBOX | WS_CHILD | WS_VISIBLE,
		170, 10, 300, 305, hWnd, 0, hInst, NULL);

	serverLog = CreateWindowExW(0, L"Listbox", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
		180, 30, 280, 275, hWnd, 0, hInst, NULL);

	btnStart = CreateWindowExW(0, L"Button", L"Start",  WS_CHILD | WS_VISIBLE,
		40, 90, 90, 25, hWnd, (HMENU)CMD_START_SERVER, hInst, NULL);
	btnStop = CreateWindowExW(0, L"Button", L"Stop", WS_CHILD | WS_VISIBLE,
		40, 120, 90, 25, hWnd, (HMENU)CMD_STOP_SERVER, hInst, NULL);
	EnableWindow(btnStop, FALSE);

	listenSocket = INVALID_SOCKET;

	return 0;
}

DWORD CALLBACK StartServer(LPVOID params) {
	HWND hWnd = *((HWND*)params);

	const size_t MAX_LEN = 100;
	WCHAR str[MAX_LEN];
	WSADATA wsaData;
	int err;

	err = WSAStartup(MAKEWORD(2,2), &wsaData);
	if (err != 0) {
		_snwprintf_s(str, MAX_LEN, L"Startup failed %d", err);
		SendMessageW(serverLog, LB_ADDSTRING, 0, (LPARAM)str);
		return -10;
	}

	listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSocket == INVALID_SOCKET) {
		_snwprintf_s(str, MAX_LEN, L"Socket failed %d", WSAGetLastError());
		WSACleanup();
		SendMessageW(serverLog, LB_ADDSTRING, 0, (LPARAM)str);
		return -20;
	}
	
	SOCKADDR_IN addr;
	addr.sin_family = AF_INET;
	char ip[20];
	LRESULT ipLen = SendMessageA(editIP, WM_GETTEXT, 20, (LPARAM)ip);
	ip[ipLen] = '\0';
	inet_pton(AF_INET, ip, &addr.sin_addr);
	char port[8];
	LRESULT portLen = SendMessageA(editPort, WM_GETTEXT, 20, (LPARAM)port);
	port[portLen] = '\0';
	addr.sin_port = htons(atoi(port));

	err = bind(listenSocket, (SOCKADDR*)&addr, sizeof(addr));

	if (err == SOCKET_ERROR) {
		_snwprintf_s(str, MAX_LEN, L"Socket bind error %d", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		SendMessageW(serverLog, LB_ADDSTRING, 0, (LPARAM)str);
		return -30;
	}

	err = listen(listenSocket, SOMAXCONN);
	if (err == SOCKET_ERROR) {
		_snwprintf_s(str, MAX_LEN, L"Socket listen error %d", WSAGetLastError());
		closesocket(listenSocket);
		WSACleanup();
		SendMessageW(serverLog, LB_ADDSTRING, 0, (LPARAM)str);
		return -40;
	}

	
		const size_t cSize = strlen(ip) + 1;
		wchar_t* wc = new wchar_t[cSize];
		mbstowcs(wc, ip, cSize);
		
		
	SendMessageW(serverLog, LB_ADDSTRING, 0, (LPARAM)ip);
	EnableWindow(btnStop, TRUE);
	EnableWindow(btnStart, FALSE);

	return 0;
}

DWORD CALLBACK StopServer(LPVOID params) {
	HWND hWnd = *((HWND*)params);
	closesocket(listenSocket);
	WSACleanup();
	listenSocket = INVALID_SOCKET;

	SendMessageW(serverLog, LB_ADDSTRING, 0, (LPARAM)L"Server stops");
	EnableWindow(btnStop, FALSE);
	EnableWindow(btnStart, TRUE);

	return 0;
}