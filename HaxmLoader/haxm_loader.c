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

#define SERVICE_NAME L"haxm service for test"

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

static int DriverUninstall(void)
{
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hService = NULL;
    SERVICE_STATUS ss;
    int retval = 1;

    hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        printf("%s(): OpenSCManager failed.\r\n", __FUNCTION__);
        goto cleanup;
    }
    hService = OpenServiceW(hSCManager, SERVICE_NAME, SERVICE_ALL_ACCESS);
    if (!hService) { // service already deleted
        retval = 0;
        goto cleanup;
    }
    // service exist, try to stop it
    if (!ControlService(hService, SERVICE_CONTROL_STOP, &ss)) {
        if (GetLastError() != ERROR_SERVICE_NOT_ACTIVE) {
            printf("%s() Error: couldn't stop service. \r\n",
                   __FUNCTION__);
            PrintErrorMessage();
        }
    }
    // try to delete service, even stop service failed
    if (!DeleteService(hService)) {
        printf("%s() Error: couldn't delete service. \r\n",
                __FUNCTION__);
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
        printf("%s(): Driver is unloaded successfully\r\n",
               __FUNCTION__);
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
        printf("%s(): Failed to get absolute path:\r\n",
                __FUNCTION__);
        wprintf(L"  sysFilePath='%s'\r\n", sysFilePath);
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
        printf("%s() Error: Cannot locate driver file:\r\n",
                __FUNCTION__);
        wprintf(L"  driverLocation='%s'\r\n", driverLocation);
        goto cleanup;
    } else {
        CloseHandle(fileHandle);
    }

    hSCManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        printf("%s() Error: OpenSCManager Fail. \r\n", __FUNCTION__);
        goto cleanup;
    }

    hService = CreateServiceW(hSCManager, SERVICE_NAME,
                              SERVICE_NAME,
                              SERVICE_ALL_ACCESS,
                              SERVICE_KERNEL_DRIVER,
                              SERVICE_DEMAND_START,
                              SERVICE_ERROR_IGNORE,
                              driverLocation,
                              NULL, NULL, NULL, NULL, NULL);
    if (!hService) {
        printf("%s() Error: OpenDriverService failed \r\n",
               __FUNCTION__);
        goto cleanup;
    }

    if (!StartServiceW(hService, 0, NULL)) {
        printf("%s() Error: StartService, Couldn't start service. \r\n",
               __FUNCTION__);
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
        printf("%s(): Driver is loaded successfully\r\n", __FUNCTION__);
    return retval;
}

static void PrintUsage(void)
{
    printf("HaxmLoader version 1.0.0\r\n");
    printf("Usage: HaxmLoader [mode]\r\n");
    printf("  Modes:\r\n");
    printf("    -i <sys_file_path>: install driver"
           " (*.sys, must be signed)\r\n");
    printf("    -u: uninstall driver\r\n");
}

int __cdecl wmain(int argc, wchar_t *argv[])
{
    wchar_t *modeStr;
    size_t modeStrLen = 0;

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
    printf("invalid parameter\r\n");
    PrintUsage();
    return 1;
}
