#pragma once
// Copyright (c) .NET Foundation and contributors. All rights reserved. Licensed under the Microsoft Reciprocal License. See LICENSE.TXT file in the project root for full license information.


#ifdef __cplusplus
extern "C" {
#endif

// The first 1024 messages are reserved so that the BA messages have the same value here.
enum BA_FUNCTIONS_MESSAGE
{
    BA_FUNCTIONS_MESSAGE_ONDETECTBEGIN = BOOTSTRAPPER_APPLICATION_MESSAGE_ONDETECTBEGIN,
    BA_FUNCTIONS_MESSAGE_ONDETECTCOMPLETE = BOOTSTRAPPER_APPLICATION_MESSAGE_ONDETECTCOMPLETE,
    BA_FUNCTIONS_MESSAGE_ONPLANBEGIN = BOOTSTRAPPER_APPLICATION_MESSAGE_ONPLANBEGIN,
    BA_FUNCTIONS_MESSAGE_ONPLANCOMPLETE = BOOTSTRAPPER_APPLICATION_MESSAGE_ONPLANCOMPLETE,
    BA_FUNCTIONS_MESSAGE_ONSTARTUP = BOOTSTRAPPER_APPLICATION_MESSAGE_ONSTARTUP,

    BA_FUNCTIONS_MESSAGE_ONTHEMELOADED = 1024,
};

typedef HRESULT(WINAPI *PFN_BA_FUNCTIONS_PROC)(
    __in BA_FUNCTIONS_MESSAGE message,
    __in const LPVOID pvArgs,
    __inout LPVOID pvResults,
    __in_opt LPVOID pvContext
    );

struct BA_FUNCTIONS_CREATE_ARGS
{
    DWORD cbSize;
    DWORD64 qwBAFunctionsAPIVersion;
    BOOTSTRAPPER_CREATE_ARGS* pBootstrapperCreateArgs;
};

struct BA_FUNCTIONS_CREATE_RESULTS
{
    DWORD cbSize;
    PFN_BA_FUNCTIONS_PROC pfnBAFunctionsProc;
    LPVOID pvBAFunctionsProcContext;
};

struct BA_FUNCTIONS_ONTHEMELOADED_ARGS
{
    DWORD cbSize;
    THEME* pTheme;
    WIX_LOCALIZATION* pWixLoc;
};

struct BA_FUNCTIONS_ONTHEMELOADED_RESULTS
{
    DWORD cbSize;
};

typedef HRESULT(WINAPI *PFN_BA_FUNCTIONS_CREATE)(
    __in const BA_FUNCTIONS_CREATE_ARGS* pArgs,
    __inout BA_FUNCTIONS_CREATE_RESULTS* pResults
    );

typedef void (WINAPI *PFN_BA_FUNCTIONS_DESTROY)();

#ifdef __cplusplus
}
#endif
