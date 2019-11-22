/*
 * Copyright (c) 2016 Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the copyright holder nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <strsafe.h>
#include <windows.h>

#define WIDE2(x) L##x
#define WIDE1(x) WIDE2(x)
#define WFUNCTION WIDE1(__FUNCTION__)

#define SERVICE_NAME_RELEASE L"IntelHaxm"
#define SERVICE_NAME L"IntelHaxmTest"
#define SERVICE_DISPLAY_NAME L"Intel HAXM Test Service"

static void PrintErrorMessage(void)
{
    DWORD LastError;
    LPVOID lpMsgBuf;

    LastError = GetLastError();
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                   FORMAT_MESSAGE_FROM_SYSTEM |
                   FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL,
                   LastError,
                   // Default language
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   (LPWSTR)&lpMsgBuf,
                   0,
                   NULL);
    wprintf(L"Error Code: %d, %s\r\n", LastError, (wchar_t *)lpMsgBuf);
    LocalFree(lpMsgBuf);
}

BOOL IsTokenElevated() {
    BOOL bElevated = FALSE;
    HANDLE hToken = NULL;
    TOKEN_ELEVATION procTokenElevation;
    DWORD cbSize = sizeof(TOKEN_ELEVATION);

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken) ||
        (hToken == NULL)) {
        wprintf(L"%s(): OpenProcessToken failed.\r\n", WFUNCTION);
        PrintErrorMessage();
        return FALSE;
    }

    if (GetTokenInformation(hToken, TokenElevation, &procTokenElevation,
        sizeof(procTokenElevation), &cbSize)) {
        bElevated = procTokenElevation.TokenIsElevated;
    } else {
        wprintf(L"%s(): GetTokenInformation failed.\r\n", WFUNCTION);
        PrintErrorMessage();
    }

    CloseHandle(hToken);

    return bElevated;
}

static int StopService(SC_HANDLE hService, const wchar_t * serviceName)
{
    int retval = 1;

    if (hService) {
        SERVICE_STATUS ss = {0};
        SERVICE_STATUS_PROCESS ssp = { 0 };
        DWORD ssp_size = 0;

        if (QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO,
            (LPBYTE)(&ssp), sizeof(ssp), &ssp_size)) {
            if (ssp.dwCurrentState == SERVICE_RUNNING) {
                if (ControlService(hService, SERVICE_CONTROL_STOP, &ss)) {
                    wprintf(L"%s(): Service %s is stopped.\r\n", WFUNCTION,
                            serviceName);
                    retval = 0;
                } else if (GetLastError() == ERROR_SERVICE_NOT_ACTIVE) {
                    wprintf(L"%s(): Service %s is already stopped.\r\n",
                            WFUNCTION, serviceName);
                    retval = 0;
                } else {
                    wprintf(L"%s() Error: couldn't stop service \"%s\".\r\n",
                            WFUNCTION, serviceName);
                    PrintErrorMessage();
                }
            } else {
                wprintf(L"%s(): Service %s is not running.\r\n", WFUNCTION,
                        serviceName);
                retval = 0;
            }
        }
    }

    return retval;
}

static int DriverUninstall(void)
{
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hService = NULL;
    int retval = 1;

    hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        wprintf(L"%s(): OpenSCManager failed.\r\n", WFUNCTION);
        goto cleanup;
    }
    // Stop release service
    hService = OpenServiceW(hSCManager,
                            SERVICE_NAME_RELEASE, SERVICE_ALL_ACCESS);
    if (hService) {
        StopService(hService, SERVICE_NAME_RELEASE);
        CloseServiceHandle(hService);
    }
    // Stop and delete test service
    hService = OpenServiceW(hSCManager, SERVICE_NAME, SERVICE_ALL_ACCESS);
    if (hService) {
        StopService(hService, SERVICE_NAME);
        if (DeleteService(hService)) {
            retval = 0;
        }
        else {
            wprintf(L"%s() Error: couldn't delete service \"%s\".\r\n",
                    WFUNCTION, SERVICE_NAME);
            goto cleanup;
        }
    } else {
        if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST) {
            wprintf(L"%s(): Service %s already deleted.\r\n", WFUNCTION,
                    SERVICE_NAME);
            retval = 0;
        }
        retval = 0;
    }
cleanup:
    if (retval)
        PrintErrorMessage();
    if (hService)
        CloseServiceHandle(hService);
    if (hSCManager)
        CloseServiceHandle(hSCManager);
    if (retval == 0)
        wprintf(L"%s(): Driver is unloaded successfully.\r\n", WFUNCTION);

    return retval;
}

static int DriverInstall(const wchar_t *sysFilePath)
{
    DWORD pathLen;
    HANDLE fileHandle;
    wchar_t driverLocation[MAX_PATH];
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hService = NULL;
    int retval = 1;

    pathLen = GetFullPathNameW(sysFilePath, MAX_PATH, driverLocation, NULL);
    if (!pathLen || pathLen >= MAX_PATH) {
        wprintf(L"%s(): Failed to get absolute path:\r\n", WFUNCTION);
        wprintf(L"  sysFilePath=\"%s\"\r\n", sysFilePath);
        goto cleanup;
    }

    // check if sysFile exists
    fileHandle = CreateFileW(driverLocation,
                             GENERIC_READ,
                             0,
                             NULL,
                             OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL,
                             NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        wprintf(L"%s() Error: Cannot locate driver file:\r\n", WFUNCTION);
        wprintf(L"  driverLocation='%s'\r\n", driverLocation);
        goto cleanup;
    } else {
        CloseHandle(fileHandle);
    }

    // Clean existing
    DriverUninstall();

    hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        wprintf(L"%s() Error: OpenSCManager Fail.\r\n", WFUNCTION);
        goto cleanup;
    }

    hService = CreateServiceW(hSCManager, SERVICE_NAME,
                              SERVICE_DISPLAY_NAME,
                              SERVICE_ALL_ACCESS,
                              SERVICE_KERNEL_DRIVER,
                              SERVICE_DEMAND_START,
                              SERVICE_ERROR_IGNORE,
                              driverLocation,
                              NULL, NULL, NULL, NULL, NULL);
    if (hService) {
        wprintf(L"%s(): Service %s created.\r\n", WFUNCTION, SERVICE_NAME);
    } else {
        wprintf(L"%s() Error: OpenDriverService \"%s\"failed.\r\n", WFUNCTION,
                SERVICE_NAME);
        goto cleanup;
    }

    if (StartServiceW(hService, 0, NULL)) {
        wprintf(L"%s(): Service %s started.\r\n", WFUNCTION, SERVICE_NAME);
    } else {
        wprintf(L"%s() Error: StartService, Couldn't start service \"%s\".\r\n",
            WFUNCTION, SERVICE_NAME);
        goto cleanup;
    }
    retval = 0;
cleanup:
    if (retval)
        PrintErrorMessage();
    if (hService)
        CloseServiceHandle(hService);
    if (hSCManager)
        CloseServiceHandle(hSCManager);
    if (retval == 0)
        wprintf(L"%s(): Driver %s is loaded successfully.\r\n", WFUNCTION,
                sysFilePath);
    return retval;
}

static void PrintUsage(void)
{
    wprintf(L"HaxmLoader version 1.1.0\r\n");
    wprintf(L"Usage: HaxmLoader [mode]\r\n");
    wprintf(L"  Modes:\r\n");
    wprintf(L"    -i <sys_file_path>: install driver"
            L" (*.sys, must be signed)\r\n");
    wprintf(L"    -u: uninstall driver\r\n");
}

int __cdecl wmain(int argc, wchar_t *argv[])
{
    wchar_t *modeStr;
    size_t modeStrLen = 0;

    if (!IsTokenElevated()) {
        wprintf(L"Please run HaxmLoader as Administrators.\r\n");
        return 0;
    }

    if (argc == 1) {
        PrintUsage();
        return 0;
    }

    modeStr = argv[1];
    // Expect modeStr == { L'-', modeChar, NULL }
    if (StringCchLengthW(modeStr, 3, &modeStrLen) == S_OK &&
        modeStrLen == 2 && modeStr[0] == L'-') {
        switch (modeStr[1]) {
            case L'i':
                if (argc == 3)
                    return DriverInstall(argv[2]);
                break;
            case L'u':
                if (argc == 2)
                    return DriverUninstall();
                break;
            default:
                break;
        }
    }
    wprintf(L"Invalid parameter\r\n");
    PrintUsage();
    return 1;
}
