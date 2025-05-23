#pragma once

#include "Message.hpp"
#include <QThread>
#include <QString>
#include <QDebug>
#include <QPixmap>
#include <QHash>
#include <QFile>
#include <mutex>

class CChatEndpoint : public QObject
{
  Q_OBJECT

public:
  explicit CChatEndpoint(const QString& host, quint16 port);

public slots:
  void EventLoop();
  void EstablishConnection();
  void SendText(const QString& textMessage);
  void SendFile(const QString& filePath);

signals:
  void TextMessageReceived(const QString &msg);
  void FileTransferProgress(int percent);
  void FileSendingFinished(const QString& fileName);
  void FileReceivingFinished(const QString& fileName);
  void ErrorOccurred(const QString &err);

private:
  void SetupClient();
  void SetupServer();
  void ReceiveMessage();
  void ReceiveFile(const std::shared_ptr<CMessage>& spMessage);
  void SendCompleteMessage(QByteArray& byteBuffer);
  std::shared_ptr<CMessage> ReceiveCompleteMessage();

  static constexpr size_t BUFFER_SIZE = 4096;

  quint16 m_port;
  QString m_host;
  bool m_isServer;
  int m_socket = -1;
  std::mutex m_sendMutex;
  std::mutex m_recvMutex;
  QHash<QString, std::shared_ptr<QFile>> m_openedFiles;
  std::mutex m_filesMutex;
};
