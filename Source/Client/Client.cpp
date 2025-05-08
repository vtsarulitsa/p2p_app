#include "Client.hpp"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <QDebug>

CClient::CClient(const QString &host, quint16 port)
  : m_host(host), m_port(port), m_sockfd(-1) {}

void CClient::run() {
  sockaddr_in server_addr{};
  m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(m_port);
  inet_pton(AF_INET, m_host.toStdString().c_str(), &server_addr.sin_addr);

  if (connect(m_sockfd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    qDebug() << "Failed to connect to server";
    return;
  }

  qDebug() << "Connected to server";

  char buffer[1024];
  while (true)
  {
    int bytes = recv(m_sockfd, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0)
    {
      break;
    }
    buffer[bytes] = '\0';
    emit messageReceived(QString::fromUtf8(buffer));
  }

  close(m_sockfd);
}

void CClient::send(const QString &message) {
  if (m_sockfd != -1) {
    QByteArray data = message.toUtf8();
    ::send(m_sockfd, data.data(), data.size(), 0);
  }
}
