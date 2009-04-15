/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact:  Qt Software Information (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at qt-sales@nokia.com.
**
**************************************************************************/

#include "cdbdebugoutput.h"
#include "cdbdebugengine.h"
#include "cdbdebugengine_p.h"

#include <windows.h>
#include <inc/dbgeng.h>

namespace Debugger {
namespace Internal {

CdbDebugOutputBase::CdbDebugOutputBase()
{
}

STDMETHODIMP CdbDebugOutputBase::QueryInterface(
    THIS_
    IN REFIID InterfaceId,
    OUT PVOID* Interface
    )
{
    *Interface = NULL;

    if (IsEqualIID(InterfaceId, __uuidof(IUnknown)) ||
        IsEqualIID(InterfaceId, __uuidof(IDebugOutputCallbacksWide)))
    {
        *Interface = (IDebugOutputCallbacksWide*)this;
        AddRef();
        return S_OK;
    } else {
        return E_NOINTERFACE;
    }
}

STDMETHODIMP_(ULONG) CdbDebugOutputBase::AddRef(THIS)
{
    // This class is designed to be static so
    // there's no true refcount.
    return 1;
}

STDMETHODIMP_(ULONG) CdbDebugOutputBase::Release(THIS)
{
    // This class is designed to be static so
    // there's no true refcount.
    return 0;
}

STDMETHODIMP CdbDebugOutputBase::Output(
    THIS_
    IN ULONG mask,
    IN PCWSTR text
    )
{
    output(mask, QString::fromUtf16(text));
    return S_OK;
}

IDebugOutputCallbacksWide *CdbDebugOutputBase::getOutputCallback(IDebugClient5 *client)
{
    IDebugOutputCallbacksWide *rc;
     if (FAILED(client->GetOutputCallbacksWide(&rc)))
         return 0;
     return rc;
}

// ------------------------- CdbDebugOutput

// Return a prefix for debugger messages
static QString prefix(ULONG mask)
{
    if (mask & (DEBUG_OUTPUT_PROMPT_REGISTERS)) {
        static const QString p = QLatin1String("registers:");
        return p;
    }
    if (mask & (DEBUG_OUTPUT_EXTENSION_WARNING|DEBUG_OUTPUT_WARNING)) {
        static const QString p = QLatin1String("warning:");
        return p;
    }
    if (mask & (DEBUG_OUTPUT_ERROR)) {
        static const QString p = QLatin1String("error:");
        return p;
    }
    if (mask & DEBUG_OUTPUT_SYMBOLS) {
        static const QString p = QLatin1String("symbols:");
        return p;
    }
    static const QString commonPrefix = QLatin1String("cdb:");
    return commonPrefix;
}

enum OutputKind { DebuggerOutput, DebuggerPromptOutput, DebuggeeOutput, DebuggeePromptOutput };

static inline OutputKind outputKind(ULONG mask)
{
    if (mask & DEBUG_OUTPUT_DEBUGGEE)
        return DebuggeeOutput;
    if (mask & DEBUG_OUTPUT_DEBUGGEE_PROMPT)
        return DebuggeePromptOutput;
    if (mask & DEBUG_OUTPUT_PROMPT)
        return DebuggerPromptOutput;
    return DebuggerOutput;
}

CdbDebugOutput::CdbDebugOutput()
{
}

void CdbDebugOutput::output(ULONG mask, const QString &msg)
{
    if (debugCDB > 1)
        qDebug() << Q_FUNC_INFO << "\n    " << msg;

    switch (outputKind(mask)) {
    case DebuggerOutput:
        debuggerOutput(prefix(mask), msg);
        break;
    case DebuggerPromptOutput:
        emit debuggerInputPrompt(prefix(mask), msg);
        break;
    case DebuggeeOutput:
        emit debuggeeOutput(msg);
        break;
    case DebuggeePromptOutput:
        emit debuggeeInputPrompt(msg);
        break;
    }
}

} // namespace Internal
} // namespace Debugger
