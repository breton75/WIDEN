﻿#ifndef SV_ABSTRACT_PROTOCOL_H
#define SV_ABSTRACT_PROTOCOL_H

#include <QObject>
#include <QDebug>
#include <QThread>
#include <QMutex>
#include <QTimer>
#include <QHash>
#include <QQueue>

#include <QJsonDocument>
#include <QJsonObject>

#include "device_defs.h"
#include "../signal/sv_signal.h"


namespace modus {

  struct SvQueue {
    QQueue<SvSignal*> queue;
    QMutex            mutex;

  };

  typedef QMap<QString, SvSignal*> SignalMap;

  class SvAbstractProtocol;
//  class SvAbstractDeviceThread;

}
    
class modus::SvAbstractProtocol: public QThread
{
  Q_OBJECT
  
public:
  SvAbstractProtocol() // без параметров!! ибо create()
  {
    clearSignals();
  }

/* обязательно виртуальный деструктор, чтобы вызывались деструкторы наследников */
  virtual ~SvAbstractProtocol()
  {  }

  virtual bool configure(const modus::DeviceConfig& cfg) = 0;
  const modus::DeviceConfig* config()         const { return &p_config;                }

  const QString   lastError()                 const { return p_last_error;             }
  const QDateTime lastParsedTime()            const { return p_last_parsed_time;       }
  const QDateTime lastOutputTime()            const { return p_last_formed_time;       }

  void setInputBuffer (modus::BUFF *input_buffer)   { p_input_buffer  = input_buffer;  }
  void setOutputBuffer(modus::BUFF *output_buffer)  { p_output_buffer = output_buffer; }
  void setSignalBuffer(modus::BUFF *signal_buffer)  { p_signal_buffer = signal_buffer; }

  /** работа с сигналами, привязанными к устройству **/
  int signalsCount()                          const { return p_input_signals.count();  }

  const modus::SignalMap* inputSignals()      const { return &p_input_signals;         }
  const modus::SignalMap* outputSignals()     const { return &p_output_signals;        }

  virtual bool bindSignal(SvSignal* signal)
  {
    try {

      switch (signal->config()->usecase) {

        case modus::IN:
        {
          if(p_input_signals.contains(signal->config()->name))
            throw SvException(QString("Повторяющееся имя сигнала: %1").arg(signal->config()->name));

          disposeSignal(signal);

          p_input_signals.insert(signal->config()->name, signal);

//          connect(this, &SvAbstractProtocol::inputBufferParsed, signal, &SvSignal::setDeviceAliveAge);

          break;

        }

        case modus::OUT:
        {
          if(p_output_signals.contains(signal->config()->name))
            throw SvException(QString("Повторяющееся имя сигнала: %1").arg(signal->config()->name));

          disposeSignal(signal);

          p_output_signals.insert(signal->config()->name, signal);

          connect(signal, &SvSignal::changed, this, &SvAbstractProtocol::queue);

          break;
        }

        default:
          throw SvException(QString("Невозможно привязать сигнал '%1' к устройству '%2'. Устройства работают только с сигналами IN и OUT")
                            .arg(signal->config()->name).arg(p_config.name));

      }

      return  true;

    } catch (SvException& e) {

      p_last_error = e.error;
      return false;
    }
  }

  virtual void clearSignals()
  {
    p_input_signals.clear();
    p_output_signals.clear();
  }

  inline void setSignalValue(const QString& signal_name, QVariant& value)
  {
    if(p_input_signals.contains(signal_name)) {

//      qDebug() << QString("SIGNAL_NAME: %1   VALUE: %2").arg(signal_name).arg(value);
      p_input_signals.value(signal_name)->setValue(value);

    }
  }

protected:
  modus::DeviceConfig  p_config;

  modus::BUFF*         p_input_buffer;
  modus::BUFF*         p_output_buffer;
  modus::BUFF*         p_signal_buffer;

  modus::SignalMap     p_input_signals;
  modus::SignalMap     p_output_signals;

  QString              p_last_error;

  bool                 p_is_active      = false;
  bool                 p_is_opened      = false;
  bool                 p_is_configured  = false;

  QDateTime            p_last_parsed_time;
  QDateTime            p_last_formed_time;

  modus::SvQueue       p_out_signal_queue;

  /** виртуальные функции **/
  virtual void disposeSignal (modus::SvSignal* signal) = 0;
  virtual bool processInputBuffer()                    = 0;
  virtual bool processSignalBuffer()                   = 0;

  virtual void run() override
  {
    p_is_active = true;

    while(p_is_active) {

      // ждем, когда закончится прием данных и парсим
      p_input_buffer->mutex.lock();

      // если требуется квитирование, то ответ формируется в parse_input_data
      if (processInputBuffer()) {

          p_last_parsed_time = QDateTime::currentDateTime();
          p_input_buffer->reset();

          // если данные распарсились, значит устройство на связи
          if(p_is_active)
            validateSignals(p_last_parsed_time);  // подтверждаем валидность привязанных сигналов
      }

      p_input_buffer->mutex.unlock();


      // формируем данные для отправки команд
      p_signal_buffer->mutex.lock();

      if (processSignalBuffer()) {

          p_last_formed_time = QDateTime::currentDateTime();

      }
      else
        p_signal_buffer->reset();

      p_signal_buffer->mutex.unlock();

    }
  }

  virtual void validateSignals(QDateTime& lastParsedTime)
  {
    foreach (modus::SvSignal* signal, p_input_signals.values()) {

      if(!signal->hasTimeout())
        signal->setDeviceAliveAge(lastParsedTime.toMSecsSinceEpoch());

      if(!signal->isAlive())
        signal->setValue(QVariant());
    }
  }

signals:
  void affirmDeviceAlive();
  void message(const QString msg, int level = sv::log::llDebug, int type  = sv::log::mtDebug);

public slots:  
  void queue(modus::SvSignal* signal)
  {
    QMutexLocker(&p_out_signal_queue.mutex);
    p_out_signal_queue.queue.enqueue(signal);
  }

  void stop()
  {
    p_is_active = false;
  }

  
};


#endif // SV_ABSTRACT_PROTOCOL_H