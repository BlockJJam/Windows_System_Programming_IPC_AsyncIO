﻿// AtlSvcAuth.cpp : WinMain 구현입니다.


#include "stdafx.h"
#include "resource.h"
#include "AtlSvcAuth_i.h"
using namespace ATL;

#include <sddl.h>
#include <AccCtrl.h>
#include <AclAPI.h>
#include <stdio.h>
#include <iostream>
using namespace std;

class CAtlSvcAuthModule : public ATL::CAtlServiceModuleT< CAtlSvcAuthModule, IDS_SERVICENAME >
{
	HANDLE m_hevExit;

public :
	DECLARE_LIBID(LIBID_AtlSvcAuthLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ATLSVCAUTH, "{bbb37042-1d80-4585-b705-90ca8288df92}")
	HRESULT InitializeSecurity() throw()
	{
		// TODO : CoInitializeSecurity를 호출하고 서비스에 올바른 보안 설정을 적용하십시오.
		// 제안 - PKT 수준 인증,
		// RPC_C_IMP_LEVEL_IDENTIFY의 가장 수준
		// 및 적절한 Null이 아닌 보안 설명자

		return S_OK;
	}

	void OnStop();

	HRESULT PreMessageLoop(int nShowCmd);
	HRESULT PostMessageLoop() throw ();
	void RunMessageLoop() throw();
};
CAtlSvcAuthModule _AtlModule;

extern "C" int WINAPI _tWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/,
								LPTSTR /*lpCmdLine*/, int nShowCmd)
{
	return _AtlModule.WinMain(nShowCmd);
}

void CAtlSvcAuthModule::OnStop()
{
	SetEvent(m_hevExit);

	__super::OnStop();
}

#define EVENT_BY_APP	_T("Global\\EVENT_BY_APP")
#define EVENT_BY_SVC	_T("Global\\EVENT_BY_SVC")
PCTSTR LOW_INTEGRITY_SDDL_SACL = _T("S:(ML;;NW;;;LW)");

HRESULT CAtlSvcAuthModule::PreMessageLoop(int nShowCmd)
{
	SECURITY_ATTRIBUTES sa;
	try
	{
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.bInheritHandle = false;
		sa.lpSecurityDescriptor = PSECURITY_DESCRIPTOR(new BYTE[SECURITY_DESCRIPTOR_MIN_LENGTH]);
		if (!InitializeSecurityDescriptor(sa.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION))
			throw HRESULT_FROM_WIN32(GetLastError());
		if (!SetSecurityDescriptorDacl(sa.lpSecurityDescriptor, true, NULL, false))
			throw HRESULT_FROM_WIN32(GetLastError());
	}
	catch (HRESULT hr)
	{
		if (sa.lpSecurityDescriptor != NULL)
			delete[] PBYTE(sa.lpSecurityDescriptor);
		return hr;
	}
	m_hevExit = CreateEvent(&sa, true, false, EVENT_BY_SVC);
	delete[] PBYTE(sa.lpSecurityDescriptor);
	if (m_hevExit == NULL)
		return HRESULT_FROM_WIN32(GetLastError());

	PSECURITY_DESCRIPTOR pSD = NULL;
	try
	{
		BOOL bIsOK = ConvertStringSecurityDescriptorToSecurityDescriptor
		(
			LOW_INTEGRITY_SDDL_SACL, SDDL_REVISION_1, &pSD, NULL
		);
		if (!bIsOK)
			throw HRESULT_FROM_WIN32(GetLastError());

		PACL pSacl = NULL;
		BOOL fSaclCur = false, fSaclDef = false;
		bIsOK = GetSecurityDescriptorSacl(pSD, &fSaclCur, &pSacl, &fSaclDef);
		if (!bIsOK)
			throw HRESULT_FROM_WIN32(GetLastError());

		DWORD dwErrCode = SetSecurityInfo
		(
			m_hevExit, SE_KERNEL_OBJECT, LABEL_SECURITY_INFORMATION, NULL, NULL, NULL, pSacl
		);
		if (dwErrCode != ERROR_SUCCESS)
			throw HRESULT_FROM_WIN32(dwErrCode);

		LocalFree(pSD);
	}
	catch (HRESULT hr)
	{
		if (pSD != NULL) LocalFree(pSD);
		CloseHandle(m_hevExit);
		return hr;
	}
	if (::InterlockedCompareExchange(&m_status.dwCurrentState, SERVICE_RUNNING, SERVICE_START_PENDING) == SERVICE_START_PENDING)
	{
		LogEvent(_T("Service started/resumed"));
		::SetServiceStatus(m_hServiceStatus, &m_status);
	}

	return S_OK;
}

HRESULT CAtlSvcAuthModule::PostMessageLoop()
{
	if (m_hevExit != NULL)
		CloseHandle(m_hevExit);

	return __super::PostMessageLoop();
}

void CAtlSvcAuthModule::RunMessageLoop()
{
	HANDLE hevByApp = NULL;
	while (true)
	{
		hevByApp = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_BY_APP);
		Sleep(1000);
		if (hevByApp != NULL)
		{
			SetEvent(hevByApp);
			CloseHandle(hevByApp);
			break;
		}
	}

	WaitForSingleObject(m_hevExit, INFINITE);
}