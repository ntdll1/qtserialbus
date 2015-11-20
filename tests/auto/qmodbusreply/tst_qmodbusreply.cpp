/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtSerialBus module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtTest/QtTest>
#include <QtSerialBus/QModbusReply>

class tst_QModbusReply : public QObject
{
    Q_OBJECT

private slots:
    void tst_ctor();
    void tst_setFinished();
    void tst_setError_data();
    void tst_setError();
    void tst_setProtocolError_data();
    void tst_setProtocolError();
    void tst_setResult();
};

void tst_QModbusReply::tst_ctor()
{
    QModbusReply r(1, this);
    QCOMPARE(r.serverAddress(), 1);
    QCOMPARE(r.isFinished(), false);
    QCOMPARE(r.result().isValid(), false);
    QCOMPARE(r.protocolError(), QModbusPdu::ExtendedException);
    QCOMPARE(r.errorText(), QString());
    QCOMPARE(r.error(), QModbusReply::NoError);

    QModbusReply r2(2, this);
    QCOMPARE(r2.serverAddress(), 2);
    QCOMPARE(r2.isFinished(), false);
    QCOMPARE(r2.result().isValid(), false);
    QCOMPARE(r2.protocolError(), QModbusPdu::ExtendedException);
    QCOMPARE(r2.errorText(), QString());
    QCOMPARE(r2.error(), QModbusReply::NoError);
}

void tst_QModbusReply::tst_setFinished()
{
    QModbusReply replyTest(1);
    QCOMPARE(replyTest.serverAddress(), 1);
    QSignalSpy finishedSpy(&replyTest, SIGNAL(finished()));
    QSignalSpy errorSpy(&replyTest, SIGNAL(errorOccurred(QModbusReply::ReplyError)));

    QCOMPARE(replyTest.serverAddress(), 1);
    QCOMPARE(replyTest.isFinished(), false);
    QCOMPARE(replyTest.result().isValid(), false);
    QCOMPARE(replyTest.protocolError(), QModbusPdu::ExtendedException);
    QCOMPARE(replyTest.errorText(), QString());
    QCOMPARE(replyTest.error(), QModbusReply::NoError);

    QVERIFY(finishedSpy.isEmpty());
    QVERIFY(errorSpy.isEmpty());

    replyTest.setFinished(true);
    QVERIFY(finishedSpy.count() == 1);
    QVERIFY(errorSpy.isEmpty());
    QCOMPARE(replyTest.serverAddress(), 1);
    QCOMPARE(replyTest.isFinished(), true);
    QCOMPARE(replyTest.result().isValid(), false);
    QCOMPARE(replyTest.protocolError(), QModbusPdu::ExtendedException);
    QCOMPARE(replyTest.errorText(), QString());
    QCOMPARE(replyTest.error(), QModbusReply::NoError);

    replyTest.setFinished(false);
    QVERIFY(finishedSpy.count() == 1); // no further singal
    QVERIFY(errorSpy.isEmpty());
    QCOMPARE(replyTest.serverAddress(), 1);
    QCOMPARE(replyTest.isFinished(), false);
    QCOMPARE(replyTest.result().isValid(), false);
    QCOMPARE(replyTest.protocolError(), QModbusPdu::ExtendedException);
    QCOMPARE(replyTest.errorText(), QString());
    QCOMPARE(replyTest.error(), QModbusReply::NoError);
}

void tst_QModbusReply::tst_setError_data()
{
    QTest::addColumn<QModbusReply::ReplyError>("error");
    QTest::addColumn<QString>("errorText");

    QTest::newRow("ProtocolError") << QModbusReply::ProtocolError << QString("ProtocolError");
    QTest::newRow("NoError") << QModbusReply::NoError << QString("NoError");
    QTest::newRow("NoError-empty") << QModbusReply::NoError << QString();
    QTest::newRow("TimeoutError") << QModbusReply::TimeoutError << QString("TimeoutError");
    QTest::newRow("ReplyAbortedError") << QModbusReply::ReplyAbortedError << QString("AbortedError");
}

void tst_QModbusReply::tst_setError()
{
    QFETCH(QModbusReply::ReplyError, error);
    QFETCH(QString, errorText);

    QModbusReply replyTest(1);
    QCOMPARE(replyTest.serverAddress(), 1);
    QSignalSpy finishedSpy(&replyTest, SIGNAL(finished()));
    QSignalSpy errorSpy(&replyTest, SIGNAL(errorOccurred(QModbusReply::ReplyError)));

    QVERIFY(finishedSpy.isEmpty());
    QVERIFY(errorSpy.isEmpty());

    replyTest.setError(error, errorText);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(replyTest.protocolError(), QModbusPdu::ExtendedException);
    QCOMPARE(replyTest.error(), error);
    QCOMPARE(replyTest.errorText(), errorText);
    QCOMPARE(errorSpy.at(0).at(0).value<QModbusReply::ReplyError>(), error);

    replyTest.setError(error, errorText);
    replyTest.setFinished(true);
    QCOMPARE(finishedSpy.count(), 3); //setError() implies call to setFinished()
    QCOMPARE(errorSpy.count(), 2);
}

void tst_QModbusReply::tst_setProtocolError_data()
{
    QTest::addColumn<QModbusPdu::ExceptionCode>("error");
    QTest::addColumn<QString>("errorText");

    QTest::newRow("IllegalFunction") << QModbusPdu::IllegalFunction << QString("foobar");
    QTest::newRow("IllegalDataAddress") << QModbusPdu::IllegalDataAddress << QString();
    QTest::newRow("GatewayPathUnavailable") << QModbusPdu::GatewayPathUnavailable << QString("no");
}

void tst_QModbusReply::tst_setProtocolError()
{
    QFETCH(QModbusPdu::ExceptionCode, error);
    QFETCH(QString, errorText);

    QModbusReply replyTest(1);
    QCOMPARE(replyTest.serverAddress(), 1);
    QSignalSpy finishedSpy(&replyTest, SIGNAL(finished()));
    QSignalSpy errorSpy(&replyTest, SIGNAL(errorOccurred(QModbusReply::ReplyError)));

    QVERIFY(finishedSpy.isEmpty());
    QVERIFY(errorSpy.isEmpty());

    replyTest.setProtocolError(error, errorText);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(errorSpy.count(), 1);
    QCOMPARE(replyTest.protocolError(), error);
    QCOMPARE(replyTest.error(), QModbusReply::ProtocolError);
    QCOMPARE(replyTest.errorText(), errorText);
    QCOMPARE(errorSpy.at(0).at(0).value<QModbusReply::ReplyError>(), QModbusReply::ProtocolError);

    replyTest.setProtocolError(error, errorText);
    replyTest.setFinished(true);
    QCOMPARE(finishedSpy.count(), 3); //setError() implies call to setFinished()
    QCOMPARE(errorSpy.count(), 2);
}

void tst_QModbusReply::tst_setResult()
{
    QModbusDataUnit unit(QModbusDataUnit::Coils, 5, {4,5,6});
    QCOMPARE(unit.startAddress(), 5);
    QCOMPARE(unit.valueCount(), 3u);
    QCOMPARE(unit.registerType(), QModbusDataUnit::Coils);
    QCOMPARE(unit.isValid(), true);
    QVector<quint16>  reference = { 4,5,6 };
    QCOMPARE(unit.values(), reference);

    QModbusReply replyTest(1);
    QCOMPARE(replyTest.serverAddress(), 1);
    QSignalSpy finishedSpy(&replyTest, SIGNAL(finished()));
    QSignalSpy errorSpy(&replyTest, SIGNAL(errorOccurred(QModbusReply::ReplyError)));

    QVERIFY(finishedSpy.isEmpty());
    QVERIFY(errorSpy.isEmpty());

    QCOMPARE(replyTest.result().startAddress(), -1);
    QCOMPARE(replyTest.result().valueCount(), 0u);
    QCOMPARE(replyTest.result().registerType(), QModbusDataUnit::Invalid);
    QCOMPARE(replyTest.result().isValid(), false);
    QCOMPARE(replyTest.result().values(), QVector<quint16>());

    replyTest.setResult(unit);
    QCOMPARE(finishedSpy.count(), 0);
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(replyTest.result().startAddress(), 5);
    QCOMPARE(replyTest.result().valueCount(), 3u);
    QCOMPARE(replyTest.result().registerType(), QModbusDataUnit::Coils);
    QCOMPARE(replyTest.result().isValid(), true);
    QCOMPARE(replyTest.result().values(), reference);
}

QTEST_MAIN(tst_QModbusReply)

#include "tst_qmodbusreply.moc"
