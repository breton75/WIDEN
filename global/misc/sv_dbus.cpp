﻿#include "sv_dbus.h"

QMutex sv::SvDBus::mutex;

sv::SvDBus::SvDBus(const sv::log::Options options, const sv::log::Flags flags, QObject *parent):
  sv::SvAbstractLogger(options, flags, parent)
{

}

void sv::SvDBus::init()
{
  QDBusConnection::sessionBus().registerObject("/", this);

  new org::ame::modus(DBUS_SERVER_NAME, "/", QDBusConnection::sessionBus(), this);

//  ModusDBusAdaptor *mdba = new ModusDBusAdaptor(this);

}

void sv::SvDBus::log(sv::log::Level level, sv::log::MessageTypes type, const QString text, sv::log::sender sender, bool newline)
{

//  mutex.lock();

  if(p_options.logging && (level <= p_options.log_level))
  {

    QString msg = QString("%1").arg(text); //.arg(newline ? "\n" : "");
    sendmsg(sender.name, msg, sv::log::typeToString(type));

    if(newline)
      p_current_line_num++;

  }

  resetCurrentData();

//  mutex.unlock();
}

void sv::SvDBus::sendmsg(const QString &sender, const QString& message, const QString &type)
{
  // при создании лочится, при завершении функции - locker удаляется, и разлочивается
  QMutexLocker locker(&mutex);

  QDBusMessage msg = QDBusMessage::createSignal("/", DBUS_SERVER_NAME, "message");
  msg << sender << message << type;
  QDBusConnection::sessionBus().send(msg);
}

//sv::SvDBus &sv::SvDBus::instance()
//{
//  if(!_instance)
//    _instance = new sv::SvDBus();

//  return *_instance;
//}
