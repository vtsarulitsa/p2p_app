#pragma once

#include <QThread>
#include <QString>

class CServer : public QThread
{
  Q_OBJECT

public:
  explicit CServer( quint16 port );
  void run() override;
  void send( const QString &message );

signals:
  void messageReceived( const QString &message );

private:
  quint16 m_port;
  int m_clientFd;
};
