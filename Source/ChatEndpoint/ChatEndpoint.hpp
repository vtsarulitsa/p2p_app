#pragma once

#include "Message.hpp"
#include <QThread>
#include <QString>
#include <QPixmap>

class CChatEndpoint : public QObject
{
  Q_OBJECT

public:
  CChatEndpoint(const QString& host, quint16 port);
  // bool EstablishConnection();
  // void Send(const CMessage& message);
  // Message Receive();

signals:
  void SendText(const QString& message);
  void SendFile(const QString& filePath);

public slots:
  void OnTextMessageReceived(const QString& message);
  void OnFileTransferProgress(int percent);
  void OnFileTransferFinished();
  void OnErrorOccurred(const QString &err);

private:
  constexpr size_t BUFFER_SIZE = 4096;

  QString m_host;
  quint16 m_port;
  int m_fileSocketFd;
  int m_textSocketFd;
  bool m_isServer;

  class Worker;
  QThread m_workerThread;

};
