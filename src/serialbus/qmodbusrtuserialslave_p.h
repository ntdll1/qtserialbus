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

#ifndef QMODBUSRTUSERIALSLAVE_P_H
#define QMODBUSRTUSERIALSLAVE_P_H

#include <QtSerialBus/qmodbusrtuserialslave.h>
#include <QtSerialBus/private/qmodbusadu_p.h>
#include <QtSerialBus/private/qmodbusserver_p.h>

#include <QtCore/qdebug.h>
#include <QtCore/qloggingcategory.h>
#include <QtSerialPort/qserialport.h>

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QT_MODBUS)
Q_DECLARE_LOGGING_CATEGORY(QT_MODBUS_LOW)

class QModbusRtuSerialSlavePrivate : public QModbusServerPrivate
{
    Q_DECLARE_PUBLIC(QModbusRtuSerialSlave)

public:
    QModbusRtuSerialSlavePrivate() Q_DECL_EQ_DEFAULT;

    void setupSerialPort()
    {
        Q_Q(QModbusRtuSerialSlave);

        m_serialPort = new QSerialPort(q);
        m_serialPort->setBaudRate(QSerialPort::Baud9600);
        m_serialPort->setParity(QSerialPort::NoParity);
        m_serialPort->setDataBits(QSerialPort::Data8);
        m_serialPort->setStopBits(QSerialPort::OneStop);

        QObject::connect(m_serialPort, &QSerialPort::readyRead, [this]() {
            Q_Q(QModbusRtuSerialSlave);

            const int size = m_serialPort->size();
            const QModbusSerialAdu adu(QModbusSerialAdu::Rtu, m_serialPort->read(size));
            qCDebug(QT_MODBUS_LOW) << "Received ADU:" << adu.rawData().toHex();

            // Index                         -> description
            // Server address                -> 1 byte
            // FunctionCode                  -> 1 byte
            // FunctionCode specific content -> 0-252 bytes
            // CRC                           -> 2 bytes

            QModbusCommEvent event = QModbusCommEvent::ReceiveEvent;
            if (isListenOnly())
                event |= QModbusCommEvent::ReceiveFlag::CurrentlyInListenOnlyMode;

            // We expect at least the server address, function code and CRC.
            if (adu.size() < 4) { // TODO: LRC should be 3 bytes.
                qCWarning(QT_MODBUS) << "Incomplete ADU received, ignoring";

                // The quantity of CRC errors encountered by the remote device since its last
                // restart, clear counters operation, or power�up. In case of a message
                // length < 4 bytes, the receiving device is not able to calculate the CRC.
                incrementCounter(QModbusServerPrivate::Counter::BusCommunicationError);
                storeModbusCommEvent(event | QModbusCommEvent::ReceiveFlag::CommunicationError);
                return;
            }

            // Server address is set to 0, this is a broadcast.
            m_processesBroadcast = (adu.serverAddress() == 0);
            if (q->processesBroadcast())
                event |= QModbusCommEvent::ReceiveFlag::BroadcastReceived;

            const QModbusRequest req = adu.pdu();
            const int pduSizeWithoutFcode = QModbusRequest::calculateDataSize(req.functionCode(),
                                                                              req.data());

            // server address byte + function code byte + PDU size + 2 bytes CRC
            if ((pduSizeWithoutFcode < 0) || ((2 + pduSizeWithoutFcode + 2) != adu.rawSize())) {
                qCWarning(QT_MODBUS) << "ADU does not match expected size, ignoring";
                // The quantity of messages addressed to the remote device that it could not
                // handle due to a character overrun condition, since its last restart, clear
                // counters operation, or power�up. A character overrun is caused by data
                // characters arriving at the port faster than they can be stored, or by the loss
                // of a character due to a hardware malfunction.
                incrementCounter(QModbusServerPrivate::Counter::BusCharacterOverrun);
                storeModbusCommEvent(event | QModbusCommEvent::ReceiveFlag::CharacterOverrun);
                return;
            }

            if (!adu.matchingChecksum()) {
                qCWarning(QT_MODBUS) << "Discarding request with wrong CRC, received:"
                                     << adu.checksum<quint16>() << ", calculated CRC:"
                                     << QModbusSerialAdu::calculateCRC(adu.data(), adu.size());
                // The quantity of CRC errors encountered by the remote device since its last
                // restart, clear counters operation, or power�up.
                incrementCounter(QModbusServerPrivate::Counter::BusCommunicationError);
                storeModbusCommEvent(event | QModbusCommEvent::ReceiveFlag::CommunicationError);
                return;
            }

            // The quantity of messages that the remote device has detected on the communications
            // system since its last restart, clear counters operation, or power�up.
            incrementCounter(QModbusServerPrivate::Counter::BusMessage);

            // If we do not process a Broadcast ...
            if (!q->processesBroadcast()) {
                // check if the server address matches ...
                if (q->serverAddress() != adu.serverAddress()) {
                    // no, not our address! Ignore!
                    qCDebug(QT_MODBUS) << "Wrong server address, expected" << q->serverAddress()
                                       << "got" << adu.serverAddress();
                    return;
                }
            } // else { Broadcast -> Server address will never match, deliberately ignore }

            // The quantity of messages addressed to the remote device, or broadcast, that the
            // remote device has processed since its last restart, clear counters operation, or
            // power�up.
            incrementCounter(QModbusServerPrivate::Counter::ServerMessage);
            storeModbusCommEvent(event); // store the final event before processing

            qCDebug(QT_MODBUS) << "Request PDU:" << req;
            const QModbusResponse response = q->processRequest(req);

            event = QModbusCommEvent::SentEvent; // reset event after processing
            if (isListenOnly())
                event |= QModbusCommEvent::SendFlag::CurrentlyInListenOnlyMode;

            if ((!response.isValid()) || isListenOnly() || q->processesBroadcast()) {
                // The quantity of messages addressed to the remote device for which it has
                // returned no response (neither a normal response nor an exception response),
                // since its last restart, clear counters operation, or power�up.
                incrementCounter(QModbusServerPrivate::Counter::ServerNoResponse);
                storeModbusCommEvent(event);
                return;
            }

            qCDebug(QT_MODBUS) << "Response PDU:" << response;

            const QByteArray result = QModbusSerialAdu::create(QModbusSerialAdu::Rtu,
                                                               q->serverAddress(), response);

            qCDebug(QT_MODBUS_LOW) << "Response ADU:" << result.toHex();

            if (!m_serialPort->isOpen()) {
                qCDebug(QT_MODBUS) << "Requesting serial port has closed.";
                q->setError(QModbusRtuSerialSlave::tr("Requesting serial port is closed"),
                            QModbusDevice::WriteError);
                incrementCounter(QModbusServerPrivate::Counter::ServerNoResponse);
                storeModbusCommEvent(event);
                return;
            }

            int writtenBytes = m_serialPort->write(result);
            if ((writtenBytes == -1) || (writtenBytes < result.size())) {
                qCDebug(QT_MODBUS) << "Cannot write requested response to serial port.";
                q->setError(QModbusRtuSerialSlave::tr("Could not write response to client"),
                            QModbusDevice::WriteError);
                incrementCounter(QModbusServerPrivate::Counter::ServerNoResponse);
                storeModbusCommEvent(event);
                return;
            }

            if (response.isException()) {
                switch (response.exceptionCode()) {
                case QModbusExceptionResponse::IllegalFunction:
                case QModbusExceptionResponse::IllegalDataAddress:
                case QModbusExceptionResponse::IllegalDataValue:
                    event |= QModbusCommEvent::SendFlag::ReadExceptionSent;
                    break;

                case QModbusExceptionResponse::ServerDeviceFailure:
                    event |= QModbusCommEvent::SendFlag::ServerAbortExceptionSent;
                    break;

                case QModbusExceptionResponse::ServerDeviceBusy:
                    // The quantity of messages addressed to the remote device for which it
                    // returned a server device busy exception response, since its last restart,
                    // clear counters operation, or power�up.
                    incrementCounter(QModbusServerPrivate::Counter::ServerBusy);
                    event |= QModbusCommEvent::SendFlag::ServerBusyExceptionSent;
                    break;

                case  QModbusExceptionResponse::NegativeAcknowledge:
                    // The quantity of messages addressed to the remote device for which it
                    // returned a negative acknowledge (NAK) exception response, since its last
                    // restart, clear counters operation, or power�up.
                    incrementCounter(QModbusServerPrivate::Counter::ServerNAK);
                    event |= QModbusCommEvent::SendFlag::ServerProgramNAKExceptionSent;
                    break;

                default:
                    break;
                }
                // The quantity of Modbus exception responses returned by the remote device since
                // its last restart, clear counters operation, or power�up.
                incrementCounter(QModbusServerPrivate::Counter::BusExceptionError);
            } else {
                switch (quint16(req.functionCode())) {
                case 0x0a: // Poll 484 (not in the official Modbus specification) *1
                case 0x0e: // Poll Controller (not in the official Modbus specification) *1
                case QModbusRequest::GetCommEventCounter: // fall through and bail out
                    break;
                default:
                    // The device's event counter is incremented once for each successful message
                    // completion. Do not increment for exception responses, poll commands, or fetch
                    // event counter commands.            *1 but mentioned here ^^^
                    incrementCounter(QModbusServerPrivate::Counter::CommEvent);
                    break;
                }
            }
            storeModbusCommEvent(event); // store the final event after processing
        });

        using TypeId = void (QSerialPort::*)(QSerialPort::SerialPortError);
        QObject::connect(m_serialPort, static_cast<TypeId>(&QSerialPort::error),
                         [this](QSerialPort::SerialPortError error) {
            if (error == QSerialPort::NoError)
                return;

            qCDebug(QT_MODBUS) << "QSerialPort error:" << error
                               << (m_serialPort ? m_serialPort->errorString() : QString());

            Q_Q(QModbusRtuSerialSlave);

            switch (error) {
            case QSerialPort::DeviceNotFoundError:
                q->setError(QModbusDevice::tr("Referenced serial device does not exist."),
                            QModbusDevice::ConnectionError);
                break;
            case QSerialPort::PermissionError:
                q->setError(QModbusDevice::tr("Cannot open serial device due to permissions."),
                            QModbusDevice::ConnectionError);
                break;
            case QSerialPort::OpenError:
            case QSerialPort::NotOpenError:
                q->setError(QModbusDevice::tr("Cannot open serial device."),
                            QModbusDevice::ConnectionError);
                break;
            case QSerialPort::ParityError:
                q->setError(QModbusDevice::tr("Parity error detected."),
                            QModbusDevice::ConfigurationError);
                incrementCounter(QModbusServerPrivate::Counter::BusCommunicationError);
                break;
            case QSerialPort::FramingError:
                q->setError(QModbusDevice::tr("Framing error detected."),
                            QModbusDevice::ConfigurationError);
                break;
            case QSerialPort::BreakConditionError:
                q->setError(QModbusDevice::tr("Break condition error detected."),
                            QModbusDevice::ConnectionError);
                break;
            case QSerialPort::WriteError:
                q->setError(QModbusDevice::tr("Write error."), QModbusDevice::WriteError);
                break;
            case QSerialPort::ReadError:
                q->setError(QModbusDevice::tr("Read error."), QModbusDevice::ReadError);
                break;
            case QSerialPort::ResourceError:
                q->setError(QModbusDevice::tr("Resource error."), QModbusDevice::ConnectionError);
                break;
            case QSerialPort::UnsupportedOperationError:
                q->setError(QModbusDevice::tr("Device operation is not supported error."),
                            QModbusDevice::ConfigurationError);
                break;
            case QSerialPort::TimeoutError:
                q->setError(QModbusDevice::tr("Timeout error."), QModbusDevice::TimeoutError);
                break;
            case QSerialPort::UnknownError:
                q->setError(QModbusDevice::tr("Unknown error."), QModbusDevice::UnknownError);
                break;
            default:
                qCDebug(QT_MODBUS) << "Unhandled QSerialPort error" << error;
                break;
            }
        });

        QObject::connect(m_serialPort, &QSerialPort::aboutToClose, [this]() {
            Q_Q(QModbusRtuSerialSlave);
            // update state if socket closure was caused by remote side
            if (q->state() != QModbusDevice::ClosingState)
                q->setState(QModbusDevice::UnconnectedState);
        });
    }

    void handleErrorOccurred(QSerialPort::SerialPortError);
    void serialPortReadyRead();
    void aboutToClose();

    QSerialPort *m_serialPort;
    bool m_processesBroadcast = false;
};

QT_END_NAMESPACE

#endif // QMODBUSRTUSERIALSLAVE_P_H
