// Copyright (c) .NET Foundation and contributors. All rights reserved. Licensed under the Microsoft Reciprocal License. See LICENSE.TXT file in the project root for full license information.

#include "precomp.h"
#define BAAPI EXTERN_C HRESULT __stdcall

// internal function declarations

static int FilterResult(
    __in DWORD dwAllowedResults,
    __in int nResult
    );


// function definitions

/*******************************************************************
 UserExperienceParseFromXml - 

*******************************************************************/
extern "C" HRESULT UserExperienceParseFromXml(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __in IXMLDOMNode* pixnBundle
    )
{
    HRESULT hr = S_OK;
    IXMLDOMNode* pixnUserExperienceNode = NULL;

    // select UX node
    hr = XmlSelectSingleNode(pixnBundle, L"UX", &pixnUserExperienceNode);
    if (S_FALSE == hr)
    {
        hr = E_NOTFOUND;
    }
    ExitOnFailure(hr, "Failed to select user experience node.");

    // parse splash screen
    hr = XmlGetYesNoAttribute(pixnUserExperienceNode, L"SplashScreen", &pUserExperience->fSplashScreen);
    if (E_NOTFOUND != hr)
    {
        ExitOnFailure(hr, "Failed to to get UX/@SplashScreen");
    }

    // parse payloads
    hr = PayloadsParseFromXml(&pUserExperience->payloads, NULL, NULL, pixnUserExperienceNode);
    ExitOnFailure(hr, "Failed to parse user experience payloads.");

    // make sure we have at least one payload
    if (0 == pUserExperience->payloads.cPayloads)
    {
        hr = E_UNEXPECTED;
        ExitOnFailure(hr, "Too few UX payloads.");
    }

LExit:
    ReleaseObject(pixnUserExperienceNode);

    return hr;
}

/*******************************************************************
 UserExperienceUninitialize - 

*******************************************************************/
extern "C" void UserExperienceUninitialize(
    __in BURN_USER_EXPERIENCE* pUserExperience
    )
{
    ReleaseStr(pUserExperience->sczTempDirectory);
    PayloadsUninitialize(&pUserExperience->payloads);

    // clear struct
    memset(pUserExperience, 0, sizeof(BURN_USER_EXPERIENCE));
}

/*******************************************************************
 UserExperienceLoad - 

*******************************************************************/
extern "C" HRESULT UserExperienceLoad(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __in BOOTSTRAPPER_ENGINE_CONTEXT* pEngineContext,
    __in BOOTSTRAPPER_COMMAND* pCommand
    )
{
    HRESULT hr = S_OK;
    BOOTSTRAPPER_CREATE_ARGS args = { };
    BOOTSTRAPPER_CREATE_RESULTS results = { };

    args.cbSize = sizeof(BOOTSTRAPPER_CREATE_ARGS);
    args.pCommand = pCommand;
    args.pEngine = pEngineContext->pEngineForApplication;
    args.pfnBootstrapperEngineProc = EngineForApplicationProc;
    args.pvBootstrapperEngineProcContext = pEngineContext;
    args.qwEngineAPIVersion = MAKEQWORDVERSION(0, 0, 0, 2); // TODO: need to decide whether to keep this, and if so when to update it.

    results.cbSize = sizeof(BOOTSTRAPPER_CREATE_RESULTS);

    // Load BA DLL.
    pUserExperience->hUXModule = ::LoadLibraryW(pUserExperience->payloads.rgPayloads[0].sczLocalFilePath);
    ExitOnNullWithLastError(pUserExperience->hUXModule, hr, "Failed to load UX DLL.");

    // Get BootstrapperApplicationCreate entry-point.
    PFN_BOOTSTRAPPER_APPLICATION_CREATE pfnCreate = (PFN_BOOTSTRAPPER_APPLICATION_CREATE)::GetProcAddress(pUserExperience->hUXModule, "BootstrapperApplicationCreate");
    ExitOnNullWithLastError(pfnCreate, hr, "Failed to get BootstrapperApplicationCreate entry-point");

    // Create BA.
    hr = pfnCreate(&args, &results);
    ExitOnFailure(hr, "Failed to create BA.");

    pUserExperience->pUserExperience = results.pApplication;
    pUserExperience->pfnBAProc = results.pfnBootstrapperApplicationProc;
    pUserExperience->pvBAProcContext = results.pvBootstrapperApplicationProcContext;

LExit:
    return hr;
}

/*******************************************************************
 UserExperienceUnload - 

*******************************************************************/
extern "C" HRESULT UserExperienceUnload(
    __in BURN_USER_EXPERIENCE* pUserExperience
    )
{
    HRESULT hr = S_OK;

    ReleaseNullObject(pUserExperience->pUserExperience);

    if (pUserExperience->hUXModule)
    {
        // Get BootstrapperApplicationDestroy entry-point and call it if it exists.
        PFN_BOOTSTRAPPER_APPLICATION_DESTROY pfnDestroy = (PFN_BOOTSTRAPPER_APPLICATION_DESTROY)::GetProcAddress(pUserExperience->hUXModule, "BootstrapperApplicationDestroy");
        if (pfnDestroy)
        {
            pfnDestroy();
        }

        // Free BA DLL.
        if (!::FreeLibrary(pUserExperience->hUXModule))
        {
            hr = HRESULT_FROM_WIN32(::GetLastError());
            TraceError(hr, "Failed to unload BA DLL.");
        }
        pUserExperience->hUXModule = NULL;
    }

//LExit:
    return hr;
}

extern "C" HRESULT UserExperienceEnsureWorkingFolder(
    __in LPCWSTR wzBundleId,
    __deref_out_z LPWSTR* psczUserExperienceWorkingFolder
    )
{
    HRESULT hr = S_OK;
    LPWSTR sczWorkingFolder = NULL;

    hr = CacheEnsureWorkingFolder(wzBundleId, &sczWorkingFolder);
    ExitOnFailure(hr, "Failed to create working folder.");

    hr = StrAllocFormatted(psczUserExperienceWorkingFolder, L"%ls%ls\\", sczWorkingFolder, L".ba");
    ExitOnFailure(hr, "Failed to calculate the bootstrapper application working path.");

    hr = DirEnsureExists(*psczUserExperienceWorkingFolder, NULL);
    ExitOnFailure(hr, "Failed create bootstrapper application working folder.");

LExit:
    ReleaseStr(sczWorkingFolder);

    return hr;
}


extern "C" HRESULT UserExperienceRemove(
    __in BURN_USER_EXPERIENCE* pUserExperience
    )
{
    HRESULT hr = S_OK;

    // Remove temporary UX directory
    if (pUserExperience->sczTempDirectory)
    {
        hr = DirEnsureDeleteEx(pUserExperience->sczTempDirectory, DIR_DELETE_FILES | DIR_DELETE_RECURSE | DIR_DELETE_SCHEDULE);
        TraceError(hr, "Could not delete bootstrapper application folder. Some files will be left in the temp folder.");
    }

//LExit:
    return hr;
}

extern "C" int UserExperienceSendError(
    __in IBootstrapperApplication* pUserExperience,
    __in BOOTSTRAPPER_ERROR_TYPE errorType,
    __in_z_opt LPCWSTR wzPackageId,
    __in HRESULT hrCode,
    __in_z_opt LPCWSTR wzError,
    __in DWORD uiFlags,
    __in int nRecommendation
    )
{
    int nResult = IDNOACTION;
    DWORD dwCode = HRESULT_CODE(hrCode);
    LPWSTR sczError = NULL;

    // If no error string was provided, try to get the error string from the HRESULT.
    if (!wzError)
    {
        if (SUCCEEDED(StrAllocFromError(&sczError, hrCode, NULL)))
        {
            wzError = sczError;
        }
    }

    nResult = pUserExperience->OnError(errorType, wzPackageId, dwCode, wzError, uiFlags, 0, NULL, nRecommendation);

//LExit:
    ReleaseStr(sczError);
    return nResult;
}

extern "C" HRESULT UserExperienceActivateEngine(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __out_opt BOOL* pfActivated
    )
{
    HRESULT hr = S_OK;
    BOOL fActivated;

    ::EnterCriticalSection(&pUserExperience->csEngineActive);
    if (InterlockedCompareExchange(reinterpret_cast<LONG*>(&pUserExperience->fEngineActive), TRUE, FALSE))
    {
        AssertSz(FALSE, "Engine should have been deactivated before activating it.");

        fActivated = FALSE;
        hr = HRESULT_FROM_WIN32(ERROR_INVALID_STATE);
    }
    else
    {
        fActivated = TRUE;
    }
    ::LeaveCriticalSection(&pUserExperience->csEngineActive);

    if (pfActivated)
    {
        *pfActivated = fActivated;
    }
    ExitOnRootFailure(hr, "Engine active cannot be changed because it was already in that state.");

LExit:
    return hr;
}

extern "C" void UserExperienceDeactivateEngine(
    __in BURN_USER_EXPERIENCE* pUserExperience
    )
{
    BOOL fActive = InterlockedExchange(reinterpret_cast<LONG*>(&pUserExperience->fEngineActive), FALSE);
    fActive = fActive; // prevents warning in "ship" build.
    AssertSz(fActive, "Engine should have be active before deactivating it.");
}

extern "C" HRESULT UserExperienceEnsureEngineInactive(
    __in BURN_USER_EXPERIENCE* pUserExperience
    )
{
    HRESULT hr = pUserExperience->fEngineActive ? HRESULT_FROM_WIN32(ERROR_BUSY) : S_OK;
    ExitOnRootFailure(hr, "Engine is active, cannot proceed.");

LExit:
    return hr;
}

extern "C" void UserExperienceExecuteReset(
    __in BURN_USER_EXPERIENCE* pUserExperience
    )
{
    pUserExperience->hrApplyError = S_OK;
    pUserExperience->hwndApply = NULL;
}

extern "C" void UserExperienceExecutePhaseComplete(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __in HRESULT hrResult
    )
{
    if (FAILED(hrResult))
    {
        pUserExperience->hrApplyError = hrResult;
    }
}

BAAPI UserExperienceOnDetectBegin(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __in BOOL fInstalled,
    __in DWORD cPackages
    )
{
    HRESULT hr = S_OK;
    BA_ONDETECTBEGIN_ARGS args = { };
    BA_ONDETECTBEGIN_RESULTS results = { };

    args.cbSize = sizeof(args);
    args.cPackages = cPackages;
    args.fInstalled = fInstalled;

    results.cbSize = sizeof(results);

    hr = pUserExperience->pfnBAProc(BOOTSTRAPPER_APPLICATION_MESSAGE_ONDETECTBEGIN, &args, &results, pUserExperience->pvBAProcContext);
    if (SUCCEEDED(hr) && results.fCancel)
    {
        hr = HRESULT_FROM_WIN32(ERROR_INSTALL_USEREXIT);
    }

    return hr;
}

BAAPI UserExperienceOnDetectComplete(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __in HRESULT hrStatus
    )
{
    HRESULT hr = S_OK;
    BA_ONDETECTCOMPLETE_ARGS args = { };
    BA_ONDETECTCOMPLETE_RESULTS results = { };

    args.cbSize = sizeof(args);
    args.hrStatus = hrStatus;

    results.cbSize = sizeof(results);

    hr = pUserExperience->pfnBAProc(BOOTSTRAPPER_APPLICATION_MESSAGE_ONDETECTCOMPLETE, &args, &results, pUserExperience->pvBAProcContext);
    ExitOnFailure(hr, "BA OnDetectComplete failed.");

LExit:
    return hr;
}

BAAPI UserExperienceOnDetectForwardCompatibleBundle(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __in_z LPCWSTR wzBundleId,
    __in BOOTSTRAPPER_RELATION_TYPE relationType,
    __in_z LPCWSTR wzBundleTag,
    __in BOOL fPerMachine,
    __in DWORD64 dw64Version,
    __inout BOOL* pfIgnoreBundle
    )
{
    HRESULT hr = S_OK;
    BA_ONDETECTFORWARDCOMPATIBLEBUNDLE_ARGS args = { };
    BA_ONDETECTFORWARDCOMPATIBLEBUNDLE_RESULTS results = { };

    args.cbSize = sizeof(args);
    args.wzBundleId = wzBundleId;
    args.relationType = relationType;
    args.wzBundleTag = wzBundleTag;
    args.fPerMachine = fPerMachine;
    args.dw64Version = dw64Version;

    results.cbSize = sizeof(results);
    results.fIgnoreBundle = *pfIgnoreBundle;

    hr = pUserExperience->pfnBAProc(BOOTSTRAPPER_APPLICATION_MESSAGE_ONDETECTFORWARDCOMPATIBLEBUNDLE, &args, &results, pUserExperience->pvBAProcContext);
    ExitOnFailure(hr, "BA OnDetectForwardCompatibleBundle failed.");

    if (results.fCancel)
    {
        hr = HRESULT_FROM_WIN32(ERROR_INSTALL_USEREXIT);
    }
    *pfIgnoreBundle = results.fIgnoreBundle;

LExit:
    return hr;
}

BAAPI UserExperienceOnDetectRelatedBundle(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __in_z LPCWSTR wzBundleId,
    __in BOOTSTRAPPER_RELATION_TYPE relationType,
    __in_z LPCWSTR wzBundleTag,
    __in BOOL fPerMachine,
    __in DWORD64 dw64Version,
    __in BOOTSTRAPPER_RELATED_OPERATION operation
    )
{
    HRESULT hr = S_OK;
    BA_ONDETECTRELATEDBUNDLE_ARGS args = { };
    BA_ONDETECTRELATEDBUNDLE_RESULTS results = { };

    args.cbSize = sizeof(args);
    args.wzBundleId = wzBundleId;
    args.relationType = relationType;
    args.wzBundleTag = wzBundleTag;
    args.fPerMachine = fPerMachine;
    args.dw64Version = dw64Version;
    args.operation = operation;

    results.cbSize = sizeof(results);

    hr = pUserExperience->pfnBAProc(BOOTSTRAPPER_APPLICATION_MESSAGE_ONDETECTRELATEDBUNDLE, &args, &results, pUserExperience->pvBAProcContext);
    ExitOnFailure(hr, "BA OnDetectRelatedBundle failed.");

    if (results.fCancel)
    {
        hr = HRESULT_FROM_WIN32(ERROR_INSTALL_USEREXIT);
    }

LExit:
    return hr;
}

BAAPI UserExperienceOnDetectUpdate(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __in_z LPCWSTR wzUpdateLocation,
    __in DWORD64 dw64Size,
    __in DWORD64 dw64Version,
    __in_z_opt LPCWSTR wzTitle,
    __in_z_opt LPCWSTR wzSummary,
    __in_z_opt LPCWSTR wzContentType,
    __in_z_opt LPCWSTR wzContent,
    __inout BOOL* pfStopProcessingUpdates
    )
{
    HRESULT hr = S_OK;
    BA_ONDETECTUPDATE_ARGS args = { };
    BA_ONDETECTUPDATE_RESULTS results = { };

    args.cbSize = sizeof(args);
    args.wzUpdateLocation = wzUpdateLocation;
    args.dw64Size = dw64Size;
    args.dw64Version = dw64Version;
    args.wzTitle = wzTitle;
    args.wzSummary = wzSummary;
    args.wzContentType = wzContentType;
    args.wzContent = wzContent;

    results.cbSize = sizeof(results);
    results.fStopProcessingUpdates = *pfStopProcessingUpdates;

    hr = pUserExperience->pfnBAProc(BOOTSTRAPPER_APPLICATION_MESSAGE_ONDETECTUPDATE, &args, &results, pUserExperience->pvBAProcContext);
    ExitOnFailure(hr, "BA OnDetectUpdate failed.");

    if (results.fCancel)
    {
        hr = HRESULT_FROM_WIN32(ERROR_INSTALL_USEREXIT);
    }
    *pfStopProcessingUpdates = results.fStopProcessingUpdates;

LExit:
    return hr;
}

BAAPI UserExperienceOnDetectUpdateBegin(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __in_z LPCWSTR wzUpdateLocation,
    __inout BOOL* pfSkip
    )
{
    HRESULT hr = S_OK;
    BA_ONDETECTUPDATEBEGIN_ARGS args = { };
    BA_ONDETECTUPDATEBEGIN_RESULTS results = { };

    args.cbSize = sizeof(args);
    args.wzUpdateLocation = wzUpdateLocation;

    results.cbSize = sizeof(results);
    results.fSkip = *pfSkip;

    hr = pUserExperience->pfnBAProc(BOOTSTRAPPER_APPLICATION_MESSAGE_ONDETECTUPDATEBEGIN, &args, &results, pUserExperience->pvBAProcContext);
    ExitOnFailure(hr, "BA OnDetectUpdateBegin failed.");

    if (results.fCancel)
    {
        hr = HRESULT_FROM_WIN32(ERROR_INSTALL_USEREXIT);
    }
    *pfSkip = results.fSkip;

LExit:
    return hr;
}

BAAPI UserExperienceOnDetectUpdateComplete(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __in HRESULT hrStatus,
    __inout BOOL* pfIgnoreError
    )
{
    HRESULT hr = S_OK;
    BA_ONDETECTUPDATECOMPLETE_ARGS args = { };
    BA_ONDETECTUPDATECOMPLETE_RESULTS results = { };

    args.cbSize = sizeof(args);
    args.hrStatus = hrStatus;

    results.cbSize = sizeof(results);
    results.fIgnoreError = *pfIgnoreError;

    hr = pUserExperience->pfnBAProc(BOOTSTRAPPER_APPLICATION_MESSAGE_ONDETECTUPDATECOMPLETE, &args, &results, pUserExperience->pvBAProcContext);
    ExitOnFailure(hr, "BA OnDetectUpdateComplete failed.");

    *pfIgnoreError = results.fIgnoreError;

LExit:
    return hr;
}

BAAPI UserExperienceOnPlanBegin(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __in DWORD cPackages
    )
{
    HRESULT hr = S_OK;
    BA_ONPLANBEGIN_ARGS args = { };
    BA_ONPLANBEGIN_RESULTS results = { };

    args.cbSize = sizeof(args);
    args.cPackages = cPackages;

    results.cbSize = sizeof(results);

    hr = pUserExperience->pfnBAProc(BOOTSTRAPPER_APPLICATION_MESSAGE_ONPLANBEGIN, &args, &results, pUserExperience->pvBAProcContext);
    if (SUCCEEDED(hr) && results.fCancel)
    {
        hr = HRESULT_FROM_WIN32(ERROR_INSTALL_USEREXIT);
    }

    return hr;
}

BAAPI UserExperienceOnPlanComplete(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __in HRESULT hrStatus
    )
{
    HRESULT hr = S_OK;
    BA_ONPLANCOMPLETE_ARGS args = { };
    BA_ONPLANCOMPLETE_RESULTS results = { };

    args.cbSize = sizeof(args);
    args.hrStatus = hrStatus;

    results.cbSize = sizeof(results);

    hr = pUserExperience->pfnBAProc(BOOTSTRAPPER_APPLICATION_MESSAGE_ONPLANCOMPLETE, &args, &results, pUserExperience->pvBAProcContext);
    ExitOnFailure(hr, "BA OnPlanComplete failed.");

LExit:
    return hr;
}

BAAPI UserExperienceOnShutdown(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __inout BOOTSTRAPPER_SHUTDOWN_ACTION* pAction
    )
{
    HRESULT hr = S_OK;
    BA_ONSHUTDOWN_ARGS args = { };
    BA_ONSHUTDOWN_RESULTS results = { };

    args.cbSize = sizeof(args);

    results.cbSize = sizeof(results);
    results.action = *pAction;

    hr = pUserExperience->pfnBAProc(BOOTSTRAPPER_APPLICATION_MESSAGE_ONSHUTDOWN, &args, &results, pUserExperience->pvBAProcContext);
    ExitOnFailure(hr, "BA OnShutdown failed.");

    *pAction = results.action;

LExit:
    return hr;
}

BAAPI UserExperienceOnStartup(
    __in BURN_USER_EXPERIENCE* pUserExperience
    )
{
    HRESULT hr = S_OK;
    BA_ONSTARTUP_ARGS args = { };
    BA_ONSTARTUP_RESULTS results = { };

    args.cbSize = sizeof(args);

    results.cbSize = sizeof(results);

    hr = pUserExperience->pfnBAProc(BOOTSTRAPPER_APPLICATION_MESSAGE_ONSTARTUP, &args, &results, pUserExperience->pvBAProcContext);
    ExitOnFailure(hr, "BA OnStartup failed.");

LExit:
    return hr;
}

BAAPI UserExperienceOnSystemShutdown(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __in DWORD dwEndSession,
    __inout BOOL* pfCancel
    )
{
    HRESULT hr = S_OK;
    BA_ONSYSTEMSHUTDOWN_ARGS args = { };
    BA_ONSYSTEMSHUTDOWN_RESULTS results = { };

    args.cbSize = sizeof(args);
    args.dwEndSession = dwEndSession;

    results.cbSize = sizeof(results);
    results.fCancel = *pfCancel;

    hr = pUserExperience->pfnBAProc(BOOTSTRAPPER_APPLICATION_MESSAGE_ONSYSTEMSHUTDOWN, &args, &results, pUserExperience->pvBAProcContext);
    ExitOnFailure(hr, "BA OnSystemShutdown failed.");
    
    *pfCancel = results.fCancel;

LExit:
    return hr;
}

extern "C" int UserExperienceCheckExecuteResult(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __in BOOL fRollback,
    __in DWORD dwAllowedResults,
    __in int nResult
    )
{
    // Do not allow canceling while rolling back.
    if (fRollback && (IDCANCEL == nResult || IDABORT == nResult))
    {
        nResult = IDNOACTION;
    }
    else if (FAILED(pUserExperience->hrApplyError) && !fRollback) // if we failed cancel except not during rollback.
    {
        nResult = IDCANCEL;
    }

    nResult = FilterResult(dwAllowedResults, nResult);
    return nResult;
}

extern "C" HRESULT UserExperienceInterpretResult(
    __in BURN_USER_EXPERIENCE* /*pUserExperience*/,
    __in DWORD dwAllowedResults,
    __in int nResult
    )
{
    int nFilteredResult = FilterResult(dwAllowedResults, nResult);
    return IDOK == nFilteredResult || IDNOACTION == nFilteredResult ? S_OK : IDCANCEL == nFilteredResult || IDABORT == nFilteredResult ? HRESULT_FROM_WIN32(ERROR_INSTALL_USEREXIT) : HRESULT_FROM_WIN32(ERROR_INSTALL_FAILURE);
}

extern "C" HRESULT UserExperienceInterpretExecuteResult(
    __in BURN_USER_EXPERIENCE* pUserExperience,
    __in BOOL fRollback,
    __in DWORD dwAllowedResults,
    __in int nResult
    )
{
    HRESULT hr = S_OK;

    // If we failed return that error unless this is rollback which should roll on.
    if (FAILED(pUserExperience->hrApplyError) && !fRollback)
    {
        hr = pUserExperience->hrApplyError;
    }
    else
    {
        int nCheckedResult = UserExperienceCheckExecuteResult(pUserExperience, fRollback, dwAllowedResults, nResult);
        hr = IDOK == nCheckedResult || IDNOACTION == nCheckedResult ? S_OK : IDCANCEL == nCheckedResult || IDABORT == nCheckedResult ? HRESULT_FROM_WIN32(ERROR_INSTALL_USEREXIT) : HRESULT_FROM_WIN32(ERROR_INSTALL_FAILURE);
    }

    return hr;
}


// internal functions

static int FilterResult(
    __in DWORD dwAllowedResults,
    __in int nResult
    )
{
    if (IDNOACTION == nResult || IDERROR == nResult) // do nothing and errors pass through.
    {
    }
    else
    {
        switch (dwAllowedResults)
        {
        case MB_OK:
            nResult = IDOK;
            break;

        case MB_OKCANCEL:
            if (IDOK == nResult || IDYES == nResult)
            {
                nResult = IDOK;
            }
            else if (IDCANCEL == nResult || IDABORT == nResult || IDNO == nResult)
            {
                nResult = IDCANCEL;
            }
            else
            {
                nResult = IDNOACTION;
            }
            break;

        case MB_ABORTRETRYIGNORE:
            if (IDCANCEL == nResult || IDABORT == nResult)
            {
                nResult = IDABORT;
            }
            else if (IDRETRY == nResult || IDTRYAGAIN == nResult)
            {
                nResult = IDRETRY;
            }
            else if (IDIGNORE == nResult)
            {
                nResult = IDIGNORE;
            }
            else
            {
                nResult = IDNOACTION;
            }
            break;

        case MB_YESNO:
            if (IDOK == nResult || IDYES == nResult)
            {
                nResult = IDYES;
            }
            else if (IDCANCEL == nResult || IDABORT == nResult || IDNO == nResult)
            {
                nResult = IDNO;
            }
            else
            {
                nResult = IDNOACTION;
            }
            break;

        case MB_YESNOCANCEL:
            if (IDOK == nResult || IDYES == nResult)
            {
                nResult = IDYES;
            }
            else if (IDNO == nResult)
            {
                nResult = IDNO;
            }
            else if (IDCANCEL == nResult || IDABORT == nResult)
            {
                nResult = IDCANCEL;
            }
            else
            {
                nResult = IDNOACTION;
            }
            break;

        case MB_RETRYCANCEL:
            if (IDRETRY == nResult || IDTRYAGAIN == nResult)
            {
                nResult = IDRETRY;
            }
            else if (IDCANCEL == nResult || IDABORT == nResult)
            {
                nResult = IDABORT;
            }
            else
            {
                nResult = IDNOACTION;
            }
            break;

        case MB_CANCELTRYCONTINUE:
            if (IDCANCEL == nResult || IDABORT == nResult)
            {
                nResult = IDABORT;
            }
            else if (IDRETRY == nResult || IDTRYAGAIN == nResult)
            {
                nResult = IDRETRY;
            }
            else if (IDCONTINUE == nResult || IDIGNORE == nResult)
            {
                nResult = IDCONTINUE;
            }
            else
            {
                nResult = IDNOACTION;
            }
            break;

        case WIU_MB_OKIGNORECANCELRETRY: // custom Windows Installer utility return code.
            if (IDOK == nResult || IDYES == nResult)
            {
                nResult = IDOK;
            }
            else if (IDCONTINUE == nResult || IDIGNORE == nResult)
            {
                nResult = IDIGNORE;
            }
            else if (IDCANCEL == nResult || IDABORT == nResult)
            {
                nResult = IDCANCEL;
            }
            else if (IDRETRY == nResult || IDTRYAGAIN == nResult || IDNO == nResult)
            {
                nResult = IDRETRY;
            }
            else
            {
                nResult = IDNOACTION;
            }
            break;

        case MB_RETRYTRYAGAIN: // custom return code.
            if (IDRETRY != nResult && IDTRYAGAIN != nResult)
            {
                nResult = IDNOACTION;
            }
            break;

        default:
            AssertSz(FALSE, "Unknown allowed results.");
            break;
        }
    }

    return nResult;
}
