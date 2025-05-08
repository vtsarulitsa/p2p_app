#pragma once

#include <QThread>
#include <QString>

class CClient : public QThread
{
  Q_OBJECT
public:
  CClient(const QString &host, quint16 port);
  void run() override;
  void send(const QString &message);

signals:
  void messageReceived(const QString &message);

private:
  QString m_host;
  quint16 m_port;
  int m_sockfd;
};
