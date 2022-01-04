#include "injectorUI.h"

#include <tlhelp32.h>
#include "kernel.h"
#include "dbvm.h"
#include "injector.h"

#define FILE_READ_ACCESS          ( 0x0001 )    // file & pipe
#define FILE_WRITE_ACCESS         ( 0x0002 )    // file & pipe
#define METHOD_BUFFERED                 0
#define FILE_DEVICE_UNKNOWN             0x00000022
#define IOCTL_UNKNOWN_BASE					FILE_DEVICE_UNKNOWN
#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)
#define IOCTL_CE_LAUNCHDBVM						CTL_CODE(IOCTL_UNKNOWN_BASE, 0x083a, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_CE_READMSR						CTL_CODE(IOCTL_UNKNOWN_BASE, 0x083f, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_CE_ALLOCATE_MEMORY_FOR_DBVM		CTL_CODE(IOCTL_UNKNOWN_BASE, 0x0862, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_CE_VMXCONFIG						CTL_CODE(IOCTL_UNKNOWN_BASE, 0x082d, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

bool IsDBVMCapable() {
	int info[4];
	if (DBVM::IsIntel()) {
		__cpuid(info, 1);
		int c = info[2];
		if ((c >> 5) & 1)
			return true;
	}
	else if (DBVM::IsAMD()) {
		__cpuid(info, 0x80000001);
		int c = info[2];
		if ((c >> 2) & 1)
			return true;
	}
	return false;
}

bool VMXConfig(HANDLE hDevice, uint64_t Password1, uint32_t Password2, uint64_t Password3) {
#pragma pack(push, 1)
	struct
	{
		uint32_t Virtualization_Enabled;
		uint64_t Password1;
		uint32_t Password2;
		uint64_t Password3;
	} input;
#pragma pack(pop, 1)

	input.Virtualization_Enabled = 1;
	input.Password1 = Password1;
	input.Password2 = Password2;
	input.Password3 = Password3;

	DWORD BytesReturned;
	return DeviceIoControl(hDevice, IOCTL_CE_VMXCONFIG, &input, sizeof(input), 0, 0, &BytesReturned, 0);
}

void LaunchDBVM(HANDLE hDevice, std::wstring wPathImg) {
	struct intput
	{
		const wchar_t* dbvmimgpath;
		uint32_t cpuid;
	}input;

	input.cpuid = 0xFFFFFFFF;
	input.dbvmimgpath = wPathImg.c_str();

	DWORD BytesReturned;
	DeviceIoControl(hDevice, IOCTL_CE_LAUNCHDBVM, &input, sizeof(input), 0, 0, &BytesReturned, NULL);
}

uint64_t ReadMSR(HANDLE hDevice, uint32_t MSR) {
	uint64_t Result = 0;
	DWORD BytesReturned;
	DeviceIoControl(hDevice, IOCTL_CE_READMSR, &MSR, sizeof(MSR), &Result, sizeof(Result), &BytesReturned, 0);
	return Result;
}

bool AddMemoryDBVM(HANDLE hDevice, uint64_t PageCount) {
	DWORD BytesReturned;
	return DeviceIoControl(hDevice, IOCTL_CE_ALLOCATE_MEMORY_FOR_DBVM, &PageCount, sizeof(PageCount), 0, 0, &BytesReturned, 0);
}

DWORD GetCPUCount() {
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;
}

bool IsFileExist(const char* szPath) {
	FILE* in = fopen(szPath, "r"e);
	if (!in)
		return false;
	fclose(in);
	return true;
}

void StopDriver(const char* szServiceName) {
	SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	SC_HANDLE schService = OpenServiceA(schSCManager, szServiceName, SERVICE_ALL_ACCESS);
	if (!schService)
		return;

	for (unsigned i = 0; i < 5; i++) {
		SERVICE_STATUS serviceStatus;
		if (ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus))
			break;

		if (GetLastError() != ERROR_DEPENDENT_SERVICES_RUNNING)
			break;

		Sleep(1);
	}

	DeleteService(schService);
	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
}

bool StartDriver(const char* szServiceName, const char* szFilePath) {
	StopDriver(szServiceName);

	SC_HANDLE schSCManager = OpenSCManagerA(0, 0, SC_MANAGER_ALL_ACCESS);
	if (!schSCManager)
		return false;

	SC_HANDLE schService = CreateServiceA(schSCManager, // SCManager database
		szServiceName,         // name of service
		szServiceName,         // name to display
		SERVICE_ALL_ACCESS,    // desired access
		SERVICE_KERNEL_DRIVER, // service type
		SERVICE_DEMAND_START,  // start type
		SERVICE_ERROR_NORMAL,  // error control type
		szFilePath,            // service's binary
		0,					   // no load ordering group
		0,					    // no tag identifier
		0,					    // no dependencies
		0,					   // LocalSystem account
		0					   // no password
	);
	if (!schService) {
		CloseServiceHandle(schSCManager);
		return false;
	}

	bool bSuccess = StartServiceA(schService, 0, 0);
	DWORD dwError = GetLastError();
	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);
	return bSuccess || (dwError == ERROR_SERVICE_ALREADY_RUNNING);
}

DWORD GetPIDByName(const char* szProcName) {
	DWORD Result = 0;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	PROCESSENTRY32 pe32 = { 0 };
	pe32.dwSize = sizeof(pe32);
	if (Process32First(hSnapshot, &pe32)) {
		do {
			if (_stricmp(pe32.szExeFile, szProcName) == 0) {
				Result = pe32.th32ProcessID;
				break;
			}
		} while (Process32Next(hSnapshot, &pe32));
	}

	CloseHandle(hSnapshot);
	return Result;
}

void InjectorUI::OnButtonDBVM() {
	std::string strMsg;
	HANDLE hDevice = INVALID_HANDLE_VALUE;

	bool bSuccess = [&] {
		bool IsDBVMAlreadyLoaded = [&] {
			if (dbvm.GetVersion())
				return true;

			dbvm.SetPassword(default_password1, default_password2, default_password3);
			if (dbvm.GetVersion())
				return true;

			return false;
		}();

		if (IsDBVMAlreadyLoaded) {
			strMsg << "dbvm already loaded. "e;
			strMsg += std::to_string(dbvm.GetMemory() / 0x1000);
			strMsg += " pages free"e;
			return true;
		}

		if (!IsDBVMCapable()) {
			strMsg << "Your system DOES NOT support DBVM."e;
			return false;
		}

		char szCurrentDir[0x100];
		GetCurrentDirectoryA(sizeof(szCurrentDir), szCurrentDir);

		std::string strPathSys = szCurrentDir;
		strPathSys += "\\"e;
		strPathSys += szSysFileName;

		std::string strPathImg = szCurrentDir;
		strPathImg += "\\vmdisk.img"e;

		if (!IsFileExist(strPathSys.c_str())) {
			strMsg = szSysFileName;
			strMsg += " does not exist"e;
			return false;
		}

		if (!IsFileExist(strPathImg.c_str())) {
			strMsg << "vmdisk.img does not exist."e;
			return false;
		}

		std::string strSubKey = "SYSTEM\\CurrentControlSet\\Services\\"es;
		strSubKey += szServiceName;

		HKEY hKey;
		if (RegCreateKeyA(HKEY_LOCAL_MACHINE, strSubKey.c_str(), &hKey) || RegOpenKeyA(HKEY_LOCAL_MACHINE, strSubKey.c_str(), &hKey)) {
			strMsg << "Cannot open registry."e;
			return false;
		}

		std::string strRegValue;
		strRegValue = "\\Device\\"es + szServiceName;
		RegSetValueExA(hKey, "A"e, 0, REG_SZ, (const BYTE*)strRegValue.c_str(), DWORD(strRegValue.size() + 1));
		strRegValue = "\\DosDevices\\"es + szServiceName;
		RegSetValueExA(hKey, "B"e, 0, REG_SZ, (const BYTE*)strRegValue.c_str(), DWORD(strRegValue.size() + 1));
		strRegValue = "\\BaseNamedObjects\\"es + szProcessEventName;
		RegSetValueExA(hKey, "C"e, 0, REG_SZ, (const BYTE*)strRegValue.c_str(), DWORD(strRegValue.size() + 1));
		strRegValue = "\\BaseNamedObjects\\"es + szThreadEventName;
		RegSetValueExA(hKey, "D"e, 0, REG_SZ, (const BYTE*)strRegValue.c_str(), DWORD(strRegValue.size() + 1));
		RegCloseKey(hKey);

		if (!StartDriver(szServiceName, strPathSys.c_str())) {
			strMsg << "Could not launch DBVM: StartService Failed."e;
			return false;
		}

		hDevice = CreateFileA(
			("\\\\.\\"es + szServiceName).c_str(),
			GENERIC_READ | GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);

		if (hDevice == INVALID_HANDLE_VALUE) {
			strMsg << "Could not launch DBVM: INVALID_HANDLE_VALUE."e;
			return false;
		}

		if (dbvm.IsAMD()) {
			uint64_t msr = ReadMSR(hDevice, 0xC0010114);
			if ((msr & (1 << 3)) && (msr & (1 << 4))) {
				strMsg << "Could not launch DBVM: The AMD-v feature has been disabled in your BIOS."e;
				return false;
			}
		}
		else {
			uint64_t msr = ReadMSR(hDevice, 0x3A);
			if ((msr & 1) && !(msr & (1 << 2))) {
				strMsg << "Could not launch DBVM: The Intel-VT feature has been disabled in your BIOS."e;
				return false;
			}
		}

		LaunchDBVM(hDevice, L"\\??\\"es + std::wstring(strPathImg.begin(), strPathImg.end()));

		dbvm.SetDefaultPassword();
		if (!ExceptionHandler::TryExcept(
			[&] {dbvm.ChangePassword(default_password1, default_password2, default_password3); })) {
			strMsg << "Could not launch DBVM: DeviceIoControl Failed."e;
			return false;
		}

		bool IsHided = false;
		if (VMXConfig(hDevice, default_password1, default_password2, default_password3)) {
			if (AddMemoryDBVM(hDevice, (uint64_t)GetCPUCount() * 0x20)) {
				dbvm.HideDBVM();
				IsHided = true;
			}
		}

		strMsg << "dbvm loaded. "e;
		strMsg += std::to_string(dbvm.GetMemory() / 0x1000);
		strMsg += " pages free"e;
		if (IsHided)
			strMsg += "\ndbvm hide completed."e;

		return true;
	}();

	CloseHandle(hDevice);
	StopDriver(szServiceName);
	MessageBoxA(hWnd, strMsg.c_str(), ""e, bSuccess ? 0 : MB_ICONERROR);
}

void InjectorUI::OnButtonSetPassword() {
	if (strlen(szLicense) < 20) {
		MessageBoxA(hWnd, "License too short."e, ""e, MB_ICONERROR);
		return;
	}

	uint64_t password1 = *(uint64_t*)(szLicense + 0) ^ 0xda2355698be6166c;
	uint32_t password2 = *(uint32_t*)(szLicense + 8) ^ 0x6765fa70;
	uint64_t password3 = *(uint64_t*)(szLicense + 12) ^ 0xe21cb5155c065962;

	bool bSuccess = [&] {
		dbvm.SetPassword(password1, password2, password3);
		if (dbvm.GetVersion())
			return true;

		dbvm.SetPassword(default_password1, default_password2, default_password3);
		if (ExceptionHandler::TryExcept([&] { dbvm.ChangePassword(password1, password2, password3); }))
			return true;

		return false;
	}();

	MessageBoxA(hWnd, bSuccess ? "Success"e : "Failed"e, ""e, bSuccess ? 0 : MB_ICONERROR);
}

void InjectorUI::OnButtonInject() {
	std::string strMsg;

	bInjected = [&] {
		DWORD dwPid = GetPIDByName(szProcessName);
		if (!dwPid) {
			strMsg << "No target process."e;
			return false;
		}

		FILE* in = fopen(szDLLName, "r"e);
		if (!in) {
			strMsg << "DLL file not exist."e;
			return false;
		}
		fclose(in);

		//if (InjectionType == EInjectionType::NxBitSwap && !dbvm.GetVersion()) {
		if (!dbvm.GetVersion()) {
			strMsg << "DBVM is not loaded."e;
			return false;
		}

		HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPid);
		if (!hProcess) {
			strMsg << "Failed opening target process"e;
			return false;
		}

		Kernel kernel(dbvm);
		Injector injector(RemoteProcess(hProcess, &kernel));
		bool Result = injector.MapRemoteModuleAFromFileName(dwPid, szDLLName, true, szLicense, InjectionType, szIntoDLL);
		strMsg = injector.GetErrorMsg();
		CloseHandle(hProcess);

		return Result;
	}();

	if (!bInjected) {
		MessageBoxA(hWnd, strMsg.c_str(), ""e, MB_ICONERROR);
		return;
	}
}