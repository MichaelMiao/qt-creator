/**************************************************************************
**
** Copyright (c) 2012 Nicolas Arnaud-Cormos
** Contact: http://www.qt-project.org/legal
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#ifndef MACROSPLUGIN_MACROEVENT_H
#define MACROSPLUGIN_MACROEVENT_H

#include "macros_global.h"

#include <QMap>

QT_BEGIN_NAMESPACE
class QByteArray;
class QVariant;
class QDataStream;
QT_END_NAMESPACE

namespace Macros {

class MACROS_EXPORT MacroEvent
{
public:
    MacroEvent();
    MacroEvent(const MacroEvent &other);
    virtual ~MacroEvent();
    MacroEvent& operator=(const MacroEvent &other);

    const QByteArray &id() const;
    void setId(const char *id);

    QVariant value(quint8 id) const;
    void setValue(quint8 id, const QVariant &value);

    void load(QDataStream &stream);
    void save(QDataStream &stream) const;

private:
    class MacroEventPrivate;
    MacroEventPrivate* d;
};

} // namespace Macros

#endif // MACROSPLUGIN_MACROEVENT_H
