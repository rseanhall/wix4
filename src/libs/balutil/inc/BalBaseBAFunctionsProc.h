//-------------------------------------------------------------------------------------------------
// <copyright file="BalBaseBAFunctions.h" company="Outercurve Foundation">
//   Copyright (c) 2004, Outercurve Foundation.
//   This software is released under Microsoft Reciprocal License (MS-RL).
//   The license and further copyright text can be found in the file
//   LICENSE.TXT at the root directory of the distribution.
// </copyright>
//-------------------------------------------------------------------------------------------------

#pragma once

#include "BalBaseBootstrapperApplicationProc.h"
#include "BAFunctions.h"

/*******************************************************************
BalBaseBAFunctionsProc - requires pvContext to be of type IBAFunctions.
Provides a default mapping between the message based BAFunctions interface and
the COM-based BAFunctions interface.

*******************************************************************/
static HRESULT WINAPI BalBaseBAFunctionsProc(
    __in BA_FUNCTIONS_MESSAGE message,
    __in const LPVOID pvArgs,
    __inout LPVOID pvResults,
    __in_opt LPVOID pvContext
    )
{
    HRESULT hr = E_NOTIMPL;

    switch (message)
    {
    case BA_FUNCTIONS_MESSAGE_ONDETECTBEGIN:
        hr = BalBaseBootstrapperApplicationProc((BOOTSTRAPPER_APPLICATION_MESSAGE)message, pvArgs, pvResults, pvContext);
    }

    return hr;
}
