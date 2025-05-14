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
    if (fileNameBytes.size() >= CMessage::MAX_PAYLOAD_SIZE)
    {
      qDebug() << "File name is too long:" << fileName;
      return;
    }

    // check if file transfer is in progress, otherwise start one and track file
    std::unique_lock<std::mutex> filesLock(m_filesMutex);
    if (m_openedFiles.contains(filePath))
    {
      qDebug() << "File transfer is in progress already:" << filePath;
      return;
    } 

    auto spFile = std::make_shared<QFile>(filePath);
    if (!spFile || !spFile->open(QIODevice::ReadOnly)) {
      qDebug() << "Cannot open file for reading:" << filePath;
      return;
    }
    m_openedFiles.insert(filePath, spFile); // track file
    filesLock.unlock();

    ssize_t fileSize = spFile->size();
    ssize_t totalFileBytesSent = 0;
    ssize_t maxPayloadSize = CMessage::MAX_PAYLOAD_SIZE - fileName.size();

    // send file by chunks using CMessage
    while (fileSize > totalFileBytesSent) {
      qint64 toRead = qMin(maxPayloadSize, fileSize - totalFileBytesSent);
      QByteArray rawFileData = spFile->read(toRead);
      if (rawFileData.isEmpty())
      {
        qDebug() << "Error reading file";
        return;
      }
      
      ssize_t fileBytesRead = rawFileData.size();
      CMessage message(rawFileData, CMessage::EMessageType::MT_FILE, fileName, fileSize);
      QByteArray rawMessage = message.Serialize();

      SendCompleteMessage(rawMessage);
      if (!rawMessage.isEmpty())
      {
        qDebug() << "Error sending file";
        return;
      }
      
      totalFileBytesSent += fileBytesRead;
      double sentBytesRatio = static_cast<double>(totalFileBytesSent) / fileSize;
      int sentPercent = static_cast<int>(sentBytesRatio * 100.0);
      emit FileTransferProgress(sentPercent);
    }

    qDebug() << "Finished sending file" << filePath;
    
    // close file and do not track it anymore
    filesLock.lock();
    m_openedFiles[filePath]->close();
    if (!m_openedFiles.remove(filePath))
    {
      qDebug() << "File is untracked already:" << filePath;
    }
    filesLock.unlock();
    
    emit FileSendingFinished(fileName);
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
    return;
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
  QString fileName = spMessage->GetFilename();

  std::shared_ptr<QFile> spFile;
  // get file handle or create new one if receiving chunk of a new file
  std::unique_lock<std::mutex> filesLock(m_filesMutex);
  if (!m_openedFiles.contains(fileName))
  {
    spFile = std::make_shared<QFile>(fileName);
    if (!spFile || !spFile->open(QIODevice::WriteOnly)) {
      qDebug() << "Cannot open file for writing:" << fileName;
      return;
    }
    m_openedFiles.insert(fileName, spFile);
  } 
  else
  {
    spFile = m_openedFiles[fileName];
  }
  filesLock.unlock();
  
  QByteArray data = spMessage->GetData();
  ssize_t payloadSize = data.size();
  ssize_t totalWrittenToFile = 0;

  // write complete chunk to file
  while (totalWrittenToFile < payloadSize)
  {
    ssize_t writtenToFile = spFile->write(data.constData(), payloadSize - totalWrittenToFile);
    
    if (writtenToFile < 0)
    {
      qDebug() << "Failed to write into file" << fileName;
      return;
    }

    if (writtenToFile > 0)
    {
      data.remove(0, writtenToFile);
      totalWrittenToFile += writtenToFile;
    }
  }

  ssize_t expectedFileSize = spMessage->GetTotalFileSize();
  ssize_t currentFileSize = spFile->size();

  // check if transfer is finished
  if (currentFileSize == expectedFileSize)
  {
    qDebug() << "Finished receiving file" << fileName;
    
    // close file and do not track it anymore
    filesLock.lock();
    m_openedFiles[fileName]->close();
    
    if (!m_openedFiles.remove(fileName))
    {
      qDebug() << "File is untracked already:" << fileName;
    }
    filesLock.unlock();

    emit FileReceivingFinished(fileName);
  }
  else if (currentFileSize > expectedFileSize)
  {
    qDebug() << "Received" << currentFileSize << "bytes of file data, but expected" << expectedFileSize;
  }
  // else: file transfer is in progress
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

  const ssize_t requiredPayloadSize = spHeader->nameLength + spHeader->dataLength;
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
