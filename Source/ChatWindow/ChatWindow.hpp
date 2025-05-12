#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QThread>
#include "ChatEndpoint.hpp"

class ChatWindow : public QWidget
{
  Q_OBJECT

public:
  ChatWindow(const QString &host, quint16 port);
  ~ChatWindow();

private slots:
  void sendMessage();
  void OnTextMessageReceived(const QString& message);
  // void OnFileTransferProgress(int percent);
  // void OnFileTransferFinished();
  // void OnErrorOccurred(const QString &err);

private:
  QTextEdit* m_pChatView;
  QLineEdit* m_pInput;
  QPushButton* m_pSendButton;
  CChatEndpoint* m_pChatEndpoint;
  QThread m_chatEndpointThread;
};
