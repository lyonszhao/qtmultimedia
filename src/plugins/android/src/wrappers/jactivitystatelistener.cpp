/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "jactivitystatelistener.h"

#include <qmap.h>

QT_BEGIN_NAMESPACE

static jclass g_qtActivityStateListenerClass = 0;
static QMap<jlong, JActivityStateListener*> g_objectMap;

// native methods for QtCameraFragment.java
static void notifyPause(JNIEnv*, jobject, jlong id)
{
    JActivityStateListener *obj = g_objectMap.value(id, 0);
    if (obj)
        emit obj->paused();
}

static void notifyResume(JNIEnv*, jobject, jlong id)
{
    JActivityStateListener *obj = g_objectMap.value(id, 0);
    if (obj)
        emit obj->resumed();
}

JActivityStateListener::JActivityStateListener()
    : QObject()
    , QJNIObject(g_qtActivityStateListenerClass, "(J)V", reinterpret_cast<jlong>(this))
    , m_Id(reinterpret_cast<jlong>(this))
{
    if (m_jobject) {
        g_objectMap.insert(m_Id, this);
        callMethod<void>("create");
    }
}

JActivityStateListener::~JActivityStateListener()
{
    if (m_jobject)
        g_objectMap.remove(m_Id);
    callMethod<void>("destroy");
}

static JNINativeMethod methods[] = {
    {"notifyPause", "(J)V", (void *)notifyPause},
    {"notifyResume", "(J)V", (void *)notifyResume}
};

bool JActivityStateListener::initJNI(JNIEnv *env)
{
    jclass clazz = env->FindClass("org/qtproject/qt5/android/multimedia/QtActivityStateListener");
    if (env->ExceptionCheck())
        env->ExceptionClear();

    if (clazz) {
        g_qtActivityStateListenerClass = static_cast<jclass>(env->NewGlobalRef(clazz));
        if (env->RegisterNatives(g_qtActivityStateListenerClass,
                                 methods,
                                 sizeof(methods) / sizeof(methods[0])) < 0) {
            return false;
        }
    }

    return true;
}

QT_END_NAMESPACE
