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
#include <cerrno>


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
  auto spMessage = ReceiveCompleteMessage();
  if (!spMessage)
  {
    qDebug() << "Failed to deserialize message";
    return;
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
    if (fileNameBytes.size() > CMessage::MAX_PAYLOAD_SIZE)
    {
      qDebug() << "File name is too long:" << fileName;
      return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
      qDebug() << "Cannot open file for reading:" << filePath;
      return;
    }

    // send file itself
    // using chunked messages buffer
    char buffer[CMessage::MAX_PAYLOAD_SIZE] = {0};

    ssize_t fileSize = file.size();
    ssize_t totalFileBytesSent = 0;

    while (fileSize > totalFileBytesSent) {
      qint64 toRead = qMin(CMessage::MAX_PAYLOAD_SIZE, fileSize - totalFileBytesSent);
      QByteArray rawFileData = file.read(toRead);
      if (rawFileData.isEmpty())
      {
        qDebug() << "Error reading file";
        return;
      }
      
      ssize_t fileBytesRead = rawFileData.size();
      CMessage message(rawFileData, CMessage::EMessageType::MT_FILE, fileName);
      QByteArray rawMessage = message.Serialize();

      std::unique_lock<std::mutex> sendLock(m_sendMutex);
      ssize_t bytesSent = ::send(m_socket, rawMessage.constData(), rawMessage.size(), 0);
      sendLock.unlock();

      if (bytesSent < 0)
      {
        qDebug() << "Error sending file";
        return;
      }
      
      totalFileBytesSent += fileBytesRead;
      emit FileTransferProgress(totalFileBytesSent / fileSize * 100);
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
  
  if (byteArray.size() > CMessage::MAX_MESSAGE_SIZE)
  {
    qDebug() << "Text message is too long";
  }

  SendCompleteMessage(byteArray);
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
      qDebug() << "Cannot open file for writing:" << fileName;
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
        qDebug() << "Error receiving file" << fileName;
        return;
      }

      QByteArray rawMessage(buffer, bytesReceived);
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

void CChatEndpoint::SendCompleteMessage(QByteArray& byteBuffer)
{
  ssize_t messageSize = byteBuffer.size();
  ssize_t totalBytesSent = 0;

  std::lock_guard<std::mutex> sendLock(m_sendMutex);
  while (totalBytesSent < messageSize)
  {
    ssize_t bytesSent = ::send(m_socket, byteBuffer.constData(), byteBuffer.size(), 0);
    if (bytesSent < 0)
    {
      qDebug() << "Failed to send message bytes:" << std::strerror(errno);
      break;
    }

    byteBuffer.remove(0, bytesSent);
    totalBytesSent += bytesSent;
  }
}

std::shared_ptr<CMessage> CChatEndpoint::ReceiveCompleteMessage()
{
  std::lock_guard<std::mutex> recvLock(m_recvMutex);

  char headerBuffer[CMessage::HEADER_SIZE] = {0};
  ssize_t totalHeaderReceived = 0;
  ssize_t headerLeftToReceive = 0;
  // read complete header
  while (totalHeaderReceived < CMessage::HEADER_SIZE)
  {
    headerLeftToReceive = CMessage::HEADER_SIZE - totalHeaderReceived;
    ssize_t bytesReceived = ::recv(m_socket, headerBuffer + totalHeaderReceived, headerLeftToReceive, 0);
    if (bytesReceived < 0)
    {
      qDebug() << "Failed to receive header bytes:" << std::strerror(errno);
      return nullptr;
    }

    if (bytesReceived > 0)
    {
      totalHeaderReceived += bytesReceived;
    }
  }

  // deserialize header
  QByteArray byteBuffer(headerBuffer, totalHeaderReceived);
  auto spHeader = CMessage::TryDeserializeHeader(byteBuffer);
  if (!spHeader)
  {
    qDebug() << "Failed to deserialize header bytes";
    return nullptr;
  }

  char payloadBuffer[CMessage::MAX_PAYLOAD_SIZE] = {0};

  // dataLength can be > MAX_PAYLOAD_SIZE for files, so ensure receiving chunked messages <= MAX_PAYLOAD_SIZE
  const ssize_t requiredPayloadSize = qMin(CMessage::MAX_PAYLOAD_SIZE, spHeader->dataLength);
  ssize_t totalPayloadReceived = 0;
  ssize_t payloadLeftToReceive = 0;
  
  // receive complete payload
  while (totalPayloadReceived < requiredPayloadSize)
  {
    payloadLeftToReceive = requiredPayloadSize - totalPayloadReceived;
    ssize_t bytesReceived = ::recv(m_socket, payloadBuffer + totalPayloadReceived, payloadLeftToReceive, 0);
    if (bytesReceived < 0)
    {
      qDebug() << "Failed to receive payload bytes:" << std::strerror(errno);
      return nullptr;
    }

    if (bytesReceived > 0)
    {
      totalPayloadReceived += bytesReceived;
    }
  }

  // deserialize complete message
  byteBuffer.append(payloadBuffer, totalPayloadReceived);
  return CMessage::TryDeserialize(byteBuffer);
}
