// AtlSvcRun.cpp : WinMain 구현입니다.


#include "stdafx.h"
#include "resource.h"
#include "AtlSvcRun_i.h"
using namespace ATL;

#include <Sddl.h>
#include <AccCtrl.h>
#include <Aclapi.h>
#include <tlhelp32.h>
#include <Wtsapi32.h>
#pragma comment(lib, "Wtsapi32.lib")

#define RM_SYSTEM_SESSO 0
#define RM_USER_SESS1	1
#define RM_SYSTEM_SESS1 2

#define RUN_MODE RM_USER_SESS1

class CAtlSvcRunModule : public ATL::CAtlServiceModuleT< CAtlSvcRunModule, IDS_SERVICENAME >
{
	HANDLE m_hevExit;

public :
	DECLARE_LIBID(LIBID_AtlSvcRunLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ATLSVCRUN, "{32953f0d-2880-4d72-b01f-01d82794eb9b}")
	HRESULT InitializeSecurity() throw()
	{
		// TODO : CoInitializeSecurity를 호출하고 서비스에 올바른 보안 설정을 적용하십시오.
		// 제안 - PKT 수준 인증,
		// RPC_C_IMP_LEVEL_IDENTIFY의 가장 수준
		// 및 적절한 Null이 아닌 보안 설명자

		return S_OK;
	}

	HRESULT PreMessageLoop(int nShowCmd);
	HRESULT PostMessageLoop() throw();
	void RunMessageLoop() throw();
	void OnStop();
};

CAtlSvcRunModule _AtlModule;


extern "C" int WINAPI _tWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/,
								LPTSTR /*lpCmdLine*/, int nShowCmd)
{
	return _AtlModule.WinMain(nShowCmd);
}

HRESULT CAtlSvcRunModule::PreMessageLoop(int nShowCmd)
{
	m_hevExit = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (m_hevExit == NULL)
		return HRESULT_FROM_WIN32(GetLastError());

	if (::InterlockedCompareExchange(&m_status.dwCurrentState,
		SERVICE_RUNNING, SERVICE_START_PENDING) == SERVICE_START_PENDING)
	{
		LogEvent(_T("Service started/resumed"));
		::SetServiceStatus(m_hServiceStatus, &m_status);
	}
	return S_OK;
}

HRESULT CAtlSvcRunModule::PostMessageLoop()
{
	CloseHandle(m_hevExit);
	return __super::PostMessageLoop();
}
void CAtlSvcRunModule::OnStop()
{
	SetEvent(m_hevExit);

	__super::OnStop();
}

HANDLE AcquireTokenForLocalSystem(DWORD dwSessionId)
{
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE)
		return NULL;

	DWORD dwWinLogonId = 0;
	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(hSnap, &pe))
	{
		CloseHandle(hSnap);
		return NULL;
	}

	do
	{
		if (_tcsicmp(pe.szExeFile, _T("winlogon.exe")) == 0)
		{
			DWORD dwWLSessId = 0;
			if (ProcessIdToSessionId(pe.th32ProcessID, &dwWLSessId))
			{
				if (dwWLSessId == dwSessionId)
				{
					dwWinLogonId = pe.th32ProcessID;
					break;
				}
			}
		}
	} while (Process32Next(hSnap, &pe));
	CloseHandle(hSnap);
	if (dwWinLogonId == 0)
		return NULL;

	HANDLE hWLToken = NULL;
	HANDLE hWLProc = NULL;
	HANDLE hNewToken = NULL;
	HRESULT hr = S_OK;

	try
	{
		hWLProc = OpenProcess(MAXIMUM_ALLOWED, FALSE, dwWinLogonId);
		if (!hWLProc)
			throw GetLastError();

		BOOL bIsOK = OpenProcessToken
		(
			hWLProc,
			TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY |
			TOKEN_ADJUST_SESSIONID | TOKEN_READ | TOKEN_WRITE, &hWLToken
		);

		if (!bIsOK)
			throw GetLastError();

		bIsOK = DuplicateTokenEx
		(
			hWLToken, MAXIMUM_ALLOWED, NULL, SecurityIdentification, TokenPrimary, &hNewToken
		);

		if (!bIsOK)
			throw GetLastError();

	}
	catch (DWORD hr)
	{
		ATLTRACE(_T("Error occurred, code = %d "), hr);
	}
	if (hWLProc != NULL)
		CloseHandle(hWLProc);
	if (hWLToken != NULL)
		CloseHandle(hWLToken);

	return hNewToken;
}

void CAtlSvcRunModule::RunMessageLoop()
{
	TCHAR szCmdLine[MAX_PATH] = { 0, };
	GetModuleFileName(NULL, szCmdLine, MAX_PATH);
	PathRemoveFileSpec(szCmdLine);
	_tcscat_s(szCmdLine, _T("\\TestGuiApp.exe"));

	HANDLE hToken = NULL;
	PROCESS_INFORMATION pi;
	ZeroMemory(&pi, sizeof(pi));
	try
	{
#if (RUN_MODE == RM_SYSTEM_SESS0)
		STARTUPINFO si;
		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);

		BOOL bIsOK = CreateProcess
		(
			NULL, szCmdLine, NULL, NULL,
			FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi
		);
#elif (RUN_MODE == RM_USER_SEES1)
		DWORD dwSessionId = WTSGetActiveConsoleSessionId();
		if (!WTSQueryUserToken(dwSessionId, &hToken))
			throw GetLastError();

		STARTUPINFO si;
		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);

		BOOL bIsOK = CreateProcessAsUser
		(
			hToken, NULL, szCmdLine, NULL, NULL,
			false, NORMAL_PRIORITY, NULL, NULL, &si, &pi
		);

#else
		DWORD dwSessionId = WTSGetActiveConsoleSessionId();
		hToken = AcquireTokenForLocalSystem(dwSessionId);
		if (hToken == NULL)
			throw GetLastError();

		STARTUPINFO si;
		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);

		BOOL bIsOK = CreateProcessAsUser
		(
			hToken,
			NULL, szCmdLine, NULL, NULL,
			FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi
		);
#endif
		if (!bIsOK)
			throw GetLastError();
		CloseHandle(pi.hThread);

		WaitForInputIdle(pi.hProcess, INFINITE);
		CloseHandle(hToken);
		CloseHandle(pi.hProcess);
	}
	catch (DWORD hr)
	{
		ATLTRACE(_T("Error occurred, code=%d"), hr);
		if (hToken != NULL) CloseHandle(hToken);
	}

	WaitForSingleObject(m_hevExit, INFINITE);
}