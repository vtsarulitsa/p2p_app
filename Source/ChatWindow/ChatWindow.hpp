#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>

class CClient;
class CServer;

class ChatWindow : public QWidget
{
  Q_OBJECT

public:
  ChatWindow(bool isServer, const QString &host, quint16 port);

private slots:
  void sendMessage();
  void onMessageReceived(const QString &msg);

private:
  QTextEdit* m_pChatView;
  QLineEdit* m_pInput;
  QPushButton* m_pSendButton;

  CClient* m_pClient;
  CServer* m_pServer;
};
