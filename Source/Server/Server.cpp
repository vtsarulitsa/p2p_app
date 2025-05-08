#include "Server.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <QDebug>

CServer::CServer(q uint16 port ) : m_port( port ), m_clientFd( -1 ) {}

void CServer::run()
{
  int protocol = 0;
  int serverFd = socket( AF_INET, SOCK_STREAM, protocol );
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons( m_port );
  addr.sin_addr.s_addr = INADDR_ANY;

  bind(serverFd, ( sockaddr* )&addr, sizeof( addr ));
  int backlog = 1;
  listen( serverFd, backlog );

  qDebug() << "Server listening on port" << m_port;

  sockaddr_in clientAddr{};
  socklen_t clientAddrLen = sizeof( clientAddr );
  m_clientFd = accept( serverFd, ( sockaddr* ) &clientAddr, &clientAddrLen );

  qDebug() << "Client connected.";

  char buffer[1024];
  while (true) {
    int flags = 0;
    int bytes = recv( m_clientFd, buffer, sizeof( buffer ) - 1, flags );
    if (bytes <= 0) break;
    buffer[bytes] = '\0';
    emit messageReceived( QString::fromUtf8( buffer ) );
  }

  close( m_clientFd );
  close( serverFd );
}

void CServer::send( const QString &message ) {
  if ( m_clientFd != -1 ) {
    QByteArray data = message.toUtf8();
    int flags = 0;
    ::send( m_clientFd, data.data(), data.size(), flags );
  }
}
