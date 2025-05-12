#include "ChatEndpoint.hpp"
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QtConcurrent>
#include "Message.hpp"
#include <poll.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>


CChatEndpoint::CChatEndpoint(const QString& host, uint16_t port)
  : m_host(host), m_port(port), m_isServer(host.isNull() || host.isEmpty())
{}

void CChatEndpoint::EstablishConnection()
{
  if (m_isServer)
    SetupServer();
  else
    SetupClient();
}

void CChatEndpoint::EventLoop()
{
  constexpr int FD_COUNT = 1;
  pollfd fds[FD_COUNT];

  int timeoutMs = 100;
  while (true)
  {
    fds[0].fd = m_socket;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    int ret = poll(fds, FD_COUNT, timeoutMs);
    if (ret < 0) 
    {
      break;
    }

    if (fds[0].revents & POLLIN)
    {
      ReceiveMessage();
    }
  }
}

void CChatEndpoint::ReceiveMessage()
{
  char buffer[BUFFER_SIZE] = {0};

  std::unique_lock<std::mutex> recvLock(m_recvMutex);
  ssize_t bytesReceived = ::recv(m_socket, buffer, sizeof(buffer), 0);
  recvLock.unlock();

  if (bytesReceived < 0)
  {
    qDebug() << "Failed to receive message";
  }

  QByteArray byteArray(buffer, bytesReceived);
  qDebug() << "Received" << bytesReceived << "bytes";
  auto spMessage = CMessage::TryDeserialize(byteArray);
  if (!spMessage)
  {
    qDebug() << "Failed to deserialize message";
  }

  if (spMessage->GetType() == CMessage::EMessageType::MT_TEXT)
  {
    auto textMessage = QString::fromUtf8(spMessage->GetData());
    emit TextMessageReceived(textMessage);
  }
  else if (spMessage->GetType() == CMessage::EMessageType::MT_FILE)
  {
    ReceiveFile(spMessage);
  }
}

void CChatEndpoint::SendFile(const QString& filePath)
{
  QtConcurrent::run([this, filePath]() {
    // determine file name length
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    QByteArray fileNameBytes = fileName.toUtf8();
    if (fileNameBytes.size() > CMessage::PAYLOAD_SIZE)
    {
      emit ErrorOccurred("File name is too long");
      return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
      emit ErrorOccurred("Cannot open file");
      return;
    }

    // send file itself
    // using chunked messages buffer
    char buffer[CMessage::PAYLOAD_SIZE] = {0};

    ssize_t fileSize = file.size();
    ssize_t totalBytesSent = 0;
    while (!file.atEnd()) {
      ssize_t bytesRead = file.read(buffer, sizeof(buffer));
      if (bytesRead < 0)
      {
        emit ErrorOccurred("Error reading file");
        return;
      }

      QByteArray rawFileData(buffer, sizeof(buffer));
      CMessage message(rawFileData, CMessage::EMessageType::MT_FILE, fileName);
      QByteArray rawMessage = message.Serialize();

      std::unique_lock<std::mutex> sendLock(m_sendMutex);
      ssize_t bytesSent = ::send(m_socket, rawMessage.constData(), rawMessage.size(), 0);
      sendLock.unlock();

      if (bytesSent < 0)
      {
        emit ErrorOccurred("Error sending file");
        return;
      }
      totalBytesSent += bytesSent;
      emit FileTransferProgress(totalBytesSent / fileSize * 100);
    }

    emit FileTransferFinished();
    file.close();
  });
}

void CChatEndpoint::SendText(const QString& textMessage)
{
  QByteArray text = textMessage.toUtf8();
  CMessage message(text, CMessage::EMessageType::MT_TEXT);
  auto byteArray = message.Serialize();

  std::unique_lock<std::mutex> sendLock(m_sendMutex);
  ssize_t bytesSent = ::send(m_socket, byteArray.constData(), byteArray.size(), 0);
  if (bytesSent <= 0) 
  {
    qDebug() << "Failed to send text message";
  }
}

void CChatEndpoint::SetupClient() {
  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(m_port);
  ::inet_pton(AF_INET, m_host.toStdString().c_str(), &server_addr.sin_addr);

  m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
  if (::connect(m_socket, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0)
  {
    qDebug() << "Failed to connect to server" << m_host << "on port" << m_port;
    return;
  }

  qDebug() << "Connected to server" << m_host << "on port" << m_port;
}

void CChatEndpoint::SetupServer() {
  int protocol = 0;
  int serverFd = ::socket(AF_INET, SOCK_STREAM, protocol);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(m_port);
  addr.sin_addr.s_addr = INADDR_ANY;

  ::bind(serverFd, (sockaddr*) &addr, sizeof(addr));
  int backlog = 1;
  ::listen(serverFd, backlog);

  qDebug() << "Server listening on port" << m_port;

  sockaddr_in clientAddr{};
  socklen_t clientAddrLen = sizeof(clientAddr);
  m_socket = ::accept(serverFd, (sockaddr*) &clientAddr, &clientAddrLen);

  qDebug() << "Client connected.";
  ::close(serverFd);
}

void CChatEndpoint::ReceiveFile(const std::shared_ptr<CMessage>& spMessage) {
  QtConcurrent::run([this, spMessage]() {
    QString fileName = spMessage->GetFilename();
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly)) {
      emit ErrorOccurred("Can't open file for writing: " + fileName);
      return;
    }
  
    QByteArray data = spMessage->GetData();
    file.write(data.constData(), data.size());

    ssize_t fileSize = spMessage->GetTotalFileSize();
    ssize_t totalBytesReceived = data.size();

    // receive the rest of the file
    char buffer[BUFFER_SIZE];
    while(totalBytesReceived < fileSize)
    {
      std::unique_lock<std::mutex> recvLock(m_recvMutex);
      ssize_t bytesReceived = ::recv(m_socket, buffer, sizeof(buffer), 0);
      recvLock.unlock();
      if (bytesReceived < 0)
      {
        emit ErrorOccurred("Error receiving file");
        return;
      }

      QByteArray rawMessage(buffer, sizeof(buffer));
      auto spFileChunkMessage = CMessage::TryDeserialize(rawMessage);
      if(!spFileChunkMessage)
      {
        qDebug() << "Failed to deserialize message";
        break;
      }

      // text message can be received while receiving a file
      if(spFileChunkMessage->GetType() == CMessage::EMessageType::MT_TEXT)
      {
        QString textMessage(spFileChunkMessage->GetData());
        emit TextMessageReceived(textMessage);
      }
      else if(spFileChunkMessage->GetType() == CMessage::EMessageType::MT_FILE)
      {
        QByteArray fileChunk = spFileChunkMessage->GetData();
        file.write(fileChunk.constData(), fileChunk.size());
        totalBytesReceived += fileChunk.size();
      }
      
      // clear buffer
      memset(buffer, 0, sizeof(buffer));
    }

    emit FileTransferFinished();
  });
}


