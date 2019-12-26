// AtlServiceTest.cpp : WinMain 구현입니다.


#include "stdafx.h"
#include "resource.h"
#include "AtlServiceTest_i.h"


using namespace ATL;

#include <stdio.h>

class CAtlServiceTestModule : public ATL::CAtlServiceModuleT< CAtlServiceTestModule, IDS_SERVICENAME >
{
	HANDLE m_hevExit;

public :
	DECLARE_LIBID(LIBID_AtlServiceTestLib)
	DECLARE_REGISTRY_APPID_RESOURCEID(IDR_ATLSERVICETEST, "{e35bfda0-4ff4-4194-ad01-3cf55a5c54f8}")
	HRESULT InitializeSecurity() throw()
	{
		// TODO : CoInitializeSecurity를 호출하고 서비스에 올바른 보안 설정을 적용하십시오.
		// 제안 - PKT 수준 인증,
		// RPC_C_IMP_LEVEL_IDENTIFY의 가장 수준
		// 및 적절한 Null이 아닌 보안 설명자

		return S_OK;
	}

	void OnStop();

	HRESULT PreMessageLoop(int nShorCmd);
	HRESULT PostMessageLoop() throw();
	void RunMessageLoop() throw();

};

CAtlServiceTestModule _AtlModule;




//
extern "C" int WINAPI _tWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/,
								LPTSTR /*lpCmdLine*/, int nShowCmd)
{
	return _AtlModule.WinMain(nShowCmd);
}

void CAtlServiceTestModule::OnStop()
{
	SetEvent(m_hevExit);
	__super::OnStop();
}

HRESULT CAtlServiceTestModule::PreMessageLoop(int nShowCmd)
{
	m_hevExit = CreateEvent(NULL, true, false, NULL);
	if (m_hevExit == NULL)
		return HRESULT_FROM_WIN32(GetLastError());

	if (::InterlockedCompareExchange(&m_status.dwCurrentState, SERVICE_RUNNING, SERVICE_START_PENDING) == SERVICE_START_PENDING)
	{
		LogEvent(_T("Servcie started/resumed"));
		::SetServiceStatus(m_hServiceStatus, &m_status);
	}
	return S_OK;
}

HRESULT CAtlServiceTestModule::PostMessageLoop()
{
	if (m_hevExit != NULL)
		CloseHandle(m_hevExit);
	return __super::PostMessageLoop();
}

void CAtlServiceTestModule::RunMessageLoop()
{
	WaitForSingleObject(m_hevExit, INFINITE);
}
