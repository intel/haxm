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
#include <stdlib.h>
#include <windows.h>
#include <winioctl.h>

#define SERVICE_NAME "haxm service for test"
#define HAX_DEVICE "\\\\.\\HAX"

void PrintErrorMessage(void)
{
    DWORD LastError;
    LPVOID lpMsgBuf;

    LastError = GetLastError();
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                  FORMAT_MESSAGE_FROM_SYSTEM |
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  LastError,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                  (LPTSTR)&lpMsgBuf,
                  0,
                  NULL);
    printf("Error Code: %d, %s\r\n", LastError, (char *)lpMsgBuf);
    LocalFree(lpMsgBuf);
}

int DriverUninstall(void)
{
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hService = NULL;
    SERVICE_STATUS ss;
    int retval = 1;

    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager) {
        printf("%s(): OpenSCManager failed.\r\n", __FUNCTION__);
        goto cleanup;
    }
    hService = OpenService(hSCManager, SERVICE_NAME, SERVICE_ALL_ACCESS);
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

int DriverInstall(char *sysFileName)
{
    HANDLE fileHandle;
    UCHAR driverLocation[MAX_PATH];
    SC_HANDLE hSCManager = NULL;
    SC_HANDLE hService = NULL;
    int retval = 1;

    GetCurrentDirectory(MAX_PATH, (LPTSTR)driverLocation);
    strcat((char *)driverLocation, "\\");
    strcat((char *)driverLocation, sysFileName);

    // check if sysFile exists
    fileHandle = CreateFile((LPCSTR)driverLocation,
                            GENERIC_READ,
                            0,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);
    if (fileHandle == INVALID_HANDLE_VALUE) {
        printf("%s() Error: Cannot locate driver file %s\r\n",
               __FUNCTION__, driverLocation);
        goto cleanup;
    } else {
        CloseHandle(fileHandle);
    }

    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if(!hSCManager) {
        printf("%s() Error: OpenSCManager Fail. \r\n", __FUNCTION__);
        goto cleanup;
    }

    hService = CreateService(hSCManager, SERVICE_NAME,
                             SERVICE_NAME,
                             SERVICE_ALL_ACCESS,
                             SERVICE_KERNEL_DRIVER,
                             SERVICE_DEMAND_START,
                             SERVICE_ERROR_IGNORE,
                             (LPCSTR) driverLocation,
                             NULL, NULL, NULL, NULL, NULL);
    if (!hService) {
        printf("%s() Error: OpenDriverService failed \r\n",
               __FUNCTION__);
        goto cleanup;
    }

    if (!StartService(hService, 0, NULL)) {
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

int WriteToReg(int mem_limit, int log_level)
{
    HKEY hk = NULL;
    int retval = 0;

    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\HAXM\\HAXM",
                       0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE,
                       NULL, &hk, NULL) != ERROR_SUCCESS) {
        printf("%s(): failed to create/open registry key\r\n",
               __FUNCTION__);
        PrintErrorMessage();
        return 1;
    }

    if (RegSetValueEx(hk, "MemLimit", 0,
                      REG_DWORD, (const BYTE *)&mem_limit,
                      sizeof(DWORD)) != ERROR_SUCCESS) {
        printf("%s(): failed to set MemLimit\r\n", __FUNCTION__);
        PrintErrorMessage();
        retval = 1;
    }

    if (RegSetValueEx(hk, "LogLevel", 0,
                      REG_DWORD, (const BYTE *)&log_level,
                      sizeof(DWORD)) != ERROR_SUCCESS) {
        printf("%s(): failed to set LogLevel\r\n", __FUNCTION__);
        PrintErrorMessage();
        retval = 1;
    }

    if (retval == 0)
        printf("%s(): write mem_limit (%dMB) and log level (%d) to "
               "registry done\r\n", __FUNCTION__, mem_limit, log_level);
    return retval;
}

void PrintUsage(void)
{
    printf("Usage: HaxmLoader [mode]\r\n");
    printf("  Modes:\r\n");
    printf("    -i <filename>: install driver (*.sys, "
           "must be in current dir and signed)\r\n");
    printf("    -u: uninstall driver\r\n");
    printf("    -s <mem_limit> <log_level>: set memory "
           "limit (in MB) and log level (0~3)\r\n");
}

int __cdecl main(int argc, char *argv[])
{
    if (argc == 1) {
        PrintUsage();
        return 0;
    }
    if (argv[1][0] == '-') {
        switch (argv[1][1]) {
            case 'i':
                if (argc == 3)
                    return DriverInstall(argv[2]);
                break;
            case 'u':
                if (argc == 2)
                    return DriverUninstall();
                break;
            case 's':
                if (argc == 4)
                    return WriteToReg(atoi(argv[2]),
                                      atoi(argv[3]));
                break;
            default:
                break;
        }
    }
    printf("invalid parameter\r\n");
    PrintUsage();
    return 1;
}
