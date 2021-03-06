﻿#ifndef SV_DEVICE_ADAPTOR_H
#define SV_DEVICE_ADAPTOR_H

#include <QObject>
#include <QDebug>

#include "../misc/sv_exception.h"
#include "../misc/sv_abstract_logger.h"

#include "../signal/sv_signal.h"
#include "interface/sv_interface_adaptor.h"
#include "protocol/sv_protocol_adaptor.h"
#include "device_defs.h"



namespace modus {

  class SvDeviceAdaptor;

}

class modus::SvDeviceAdaptor: public QObject
{
  Q_OBJECT

public:
  SvDeviceAdaptor(sv::SvAbstractLogger* logger = nullptr):
    m_logger(logger)
  {  }

  ~SvDeviceAdaptor()
  {
    emit stopAll();

    deleteLater();
  }

  bool init(const modus::DeviceConfig& config)
  {
    try {

      m_config = config;

      /* interface */
      m_interface = new modus::SvInterfaceAdaptor(m_logger);

      if(!m_interface->init(m_config, &m_io_buffer))
        throw SvException(m_interface->lastError());

      /* protocol */
      m_protocol = new modus::SvProtocolAdaptor(m_logger);

      if(!m_protocol->init(m_config, &m_io_buffer))
        throw SvException(m_protocol->lastError());

      return  true;

    } catch (SvException& e) {
      if(m_interface)
        delete m_interface;

      if(m_protocol)
        delete m_protocol;

      m_last_error = e.error;
      return  false;
    }
  }

  const modus::DeviceConfig* config() const        { return &m_config;   }

  bool start() {

    try {

      if(!m_protocol)
        throw SvException("Протокол не подключен");

      if(!m_interface)
        throw SvException("Интерфейс не подключен");

      for(modus::SvSignal* signal: m_signals)
        m_protocol->bindSignal(signal);

      connect(this, &modus::SvDeviceAdaptor::stopAll, m_protocol,  &modus::SvProtocolAdaptor::stop);
      connect(this, &modus::SvDeviceAdaptor::stopAll, m_interface, &modus::SvInterfaceAdaptor::stop);

      if(!m_interface->start())
        throw SvException(m_interface->lastError());

      if(!m_protocol->start())
        throw SvException(m_protocol->lastError());

      return true;

    }
    catch(SvException& e)
    {
      m_last_error = e.error;
      return false;
    }
  }

  void setLogger(sv::SvAbstractLogger* logger)
  {
    m_logger = logger;

    if(m_interface) m_interface->setLogger(m_logger);
    if(m_protocol)  m_protocol->setLogger(m_logger);

  }

  void stop() { emit stopAll(); }

  const QString lastError() const  { return m_last_error; }

//  bool isAlive()            const  { return m_protocol->p_is_active().toMSecsSinceEpoch() + m_config.timeout > QDateTime::currentDateTime().toMSecsSinceEpoch();  }

  /** работа с сигналами, привязанными к устройству **/
  void bindSignal(SvSignal* signal)
  {
    m_signals.append(signal);
  }

  QList<modus::SvSignal*>* Signals()       { return &m_signals; }

private:
  modus::DeviceConfig        m_config;
  QList<modus::SvSignal*>    m_signals;

  modus::SvProtocolAdaptor*  m_protocol  = nullptr;
  modus::SvInterfaceAdaptor* m_interface = nullptr;

  modus::IOBuffer            m_io_buffer;

  QString                    m_last_error;

  bool                       m_isOpened       = false;
  bool                       m_is_configured  = false;

  sv::SvAbstractLogger*      m_logger;

signals:
//  void message(const QString msg, int level = sv::log::llDebug, int type  = sv::log::mtDebug);
  void stopAll();

private slots:
  void log(const QString msg, int level = sv::log::llDebug, int type  = sv::log::mtDebug)
  {
//    qDebug() << msg;

    if(m_logger)
      *m_logger << sv::log::sender(m_config.name)
                << sv::log::TimeZZZ
                << sv::log::Level(level)
                << sv::log::MessageTypes(type)
                << msg
                << sv::log::endl;
  }
};

#endif // SV_DEVICE_ADAPTOR_H
