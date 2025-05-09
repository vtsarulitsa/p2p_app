#include "ChatEndpoint.hpp"
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <poll.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

class CChatEndpoint::Worker : public QObject {
  Q_OBJECT

public:
  Worker(const QString& host, uint16_t port)
    : m_host(host), m_port(port), m_isServer(host.isEmpty())
  {
    if (m_isServer)
      setupServer();
    else
      setupClient();  
  }

  void EventLoop() override
  {
    constexpr int FD_COUNT = 2;
    pollfd fds[FD_COUNT];
    fds[0].fd = m_textSocketFd;
    fds[0].events = POLLIN;
    fds[1].fd = m_fileSocketFd;
    fds[1].events = POLLIN;

    int timeoutMs = 100;
    while (true)
    {
      int ret = poll(fds, FD_COUNT, timeoutMs);
      if (ret < 0) break;

      if (fds[0].revents & POLLIN)
        handleChatMessage();

      if (fds[1].revents & POLLIN)
        receiveFile();
    }
  }

signals:
  void TextMessageReceived(const QString &msg);
  void FileTransferProgress(int percent);
  void fileTransferFinished();
  void ErrorOccurred(const QString &err);

public slots:
  void OnSendFile(const QString& filePath)
  {
    // TODO: block writing to m_fileSocketFd while another file is sending
    QtConcurrent::run([this, filePath]() {
      QFile file(filePath);
      if (!file.open(QIODevice::ReadOnly)) {
        emit ErrorOccurred("Cannot open file");
        return;
      }

      // determine file name length
      QFileInfo fileInfo(filePath);
      QByteArray fileName = fileInfo.fileName().toUtf8();
      if (fileName.size() > BUFFER_SIZE)
      {
        emit ErrorOccurred("File name is too long");
        return;
      }

      // send file name
      if (::send(m_fileSocketFd, fileName.constData(), fileName.size(), 0) < 0)
      {
        emit ErrorOccurred("Error sending file name");
        return;
      }

      // send file size
      qint64 totalSize = file.size();
      if (::send(m_fileSocketFd, totalSize, sizeof(totalSize), 0) < 0)
      {
        emit ErrorOccurred("Error sending file size");
        return;
      }

      // send file itself
      // using chunked messages buffer
      char buffer[BUFFER_SIZE] = {0};

      qint64 totalSent = 0;
      while (!file.atEnd()) {
        qint64 bytesRead = file.read(buffer, sizeof(buffer));
        if (bytesRead <= 0)
        {
          emit ErrorOccurred("Error reading file");
          return;
        }
        qint64 bytesSent = ::send(m_fileSocketFd, buffer, bytesRead, 0);

        if (bytesSent < 0)
        {
          emit ErrorOccurred("Error sending file");
          return;
        }
        totalSent += bytesSent;
        emit FileTransferProgress(totalSent / totalSize * 100);
      }

      emit FileTransferFinished();
      file.close();
    });
  }

  void OnSendText(const QString& message)
  {
    QByteArray data = message.toUtf8();
    ::send(m_textSocketFd, data.constData(), data.size(), 0);
  }

private:
  quint16 m_port;
  QString m_host;
  bool m_isServer;
  int m_textSocketFd = -1;
  int m_fileSocketFd = -1;

  void setupClient() {
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(m_port);
    ::inet_pton(AF_INET, m_host.toStdString().c_str(), &server_addr.sin_addr);

    m_textSocketFd = ::socket(AF_INET, SOCK_STREAM, 0);
    m_fileSocketFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (::connect(m_textSocketFd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0 ||
        ::connect(m_fileSocketFd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0)
    {
      qDebug() << "Failed to connect to server" << m_host << "on port" << m_port;
      return;
    }

    qDebug() << "Connected to server" << m_host << "on port" << m_port;
  }

  void setupServer() {
    int protocol = 0;
    int serverFd = ::socket(AF_INET, SOCK_STREAM, protocol);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    ::bind(serverFd, (sockaddr*) &addr, sizeof(addr));
    int backlog = 2;
    ::listen(serverFd, backlog);

    qDebug() << "Server listening on port" << m_port;

    sockaddr_in clientAddr{};
    socklen_t clientAddrLen = sizeof(clientAddr);
    m_textSocketFd = ::accept(serverFd, (sockaddr*) &clientAddr, &clientAddrLen);
    m_fileSocketFd = ::accept(serverFd, (sockaddr*) &clientAddr, &clientAddrLen);

    qDebug() << "Client connected."
    close(serverFd);
  }

  void handleChatMessage() {
    char buf[1024];
    ssize_t n = recv(m_textSocketFd, buf, sizeof(buf), 0);
    if (n > 0)
        emit TextMessageReceived(QString::fromUtf8(buf, n));
  }

  void receiveFile() {
    // TODO: block receiving on m_fileSocketFd while another file is sending/receiving
    QtConcurrent::run([this]() {
      char buffer[BUFFER_SIZE] = {0};

      // receive file name
      ssize_t bytesReceived = ::recv(m_fileSocketFd, buffer, sizeof(buffer), 0) 
      if (bytesReceived < 0)
      {
        emit ErrorOccurred("Error receiving file name");
        return;
      }
      buffer[bytesReceived] = '\0';
      QString fileName = QString::fromUtf8(buffer);

      // clear buffer
      memset(buffer, 0, sizeof(buffer));

      // receive file size
      bytesReceived = ::recv(m_fileSocketFd, buffer, sizeof(qint64), 0) 
      if (bytesReceived < 0)
      {
        emit ErrorOccurred("Error receiving file size");
        return;
      }
      qint64 fileSize = *(reinterpret_cast<qint64*>(buffer));

      // clear buffer
      memset(buffer, 0, sizeof(buffer));

      // create file
      QFile file(fileName);
      if (!file.open(QIODevice::WriteOnly)) {
        emit ErrorOccurred("Can't open file for writing: " + fileName);
        return;
      }

      // receive file chunks and write to file
      qint64 totalBytes = 0;
      while (true) {
        ssize_t bytesReceived = ::recv(m_fileSocketFd, buffer, sizeof(buffer), 0);
        if (bytesReceived < 0)
        {
          emit ErrorOccurred("Error receiving file");
          return;
        }
        file.write(buffer, bytesReceived);
        totalBytes += bytesReceived;
      }

      emit FileTransferFinished();
    });
  }
};

CChatEndpoint::CChatEndpoint(const QString &host, quint16 port)
{
  Worker pWorker = new Worker(host, port);
  pWorker->moveToThread(&m_workerThread);
  m_workerThread.start();

  QMetaObject::invokeMethod(pWorker, "EventLoop", Qt::QueuedConnection);
}

CChatEndpoint::~CChatEndpoint() {
  m_workerThread.quit();
  m_workerThread.wait();
}


