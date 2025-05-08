#include "ChatWindow.hpp"
#include "Client.hpp"
#include "Server.hpp"
#include <QVBoxLayout>

ChatWindow::ChatWindow(bool isServer, const QString &host, quint16 port)
    : m_pClient(nullptr), m_pServer(nullptr)
{
    m_pChatView = new QTextEdit(this);
    m_pChatView->setReadOnly(true);
    m_pInput = new QLineEdit(this);
    m_pSendButton = new QPushButton("Send", this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_pChatView);
    layout->addWidget(m_pInput);
    layout->addWidget(m_pSendButton);
    setLayout(layout);

    connect(m_pSendButton, &QPushButton::clicked, this, &ChatWindow::sendMessage);

    if (isServer) {
        m_pServer = new CServer(port);
        connect(m_pServer, &ChatServer::messageReceived, this, &ChatWindow::onMessageReceived);
        m_pServer->start();
    } else {
        m_pClient = new CClient(host, port);
        connect(m_pClient, &ChatClient::messageReceived, this, &ChatWindow::onMessageReceived);
        m_pClient->start();
    }
}

void ChatWindow::sendMessage() {
    QString msg = m_pInput->text();
    if (!msg.isEmpty()) {
        m_pChatView->append("You: " + msg);
        if (m_pServer) m_pServer->send(msg);
        if (m_pClient) m_pClient->send(msg);
        m_pInput->clear();
    }
}

void ChatWindow::onMessageReceived(const QString &msg) {
    m_pChatView->append("Peer: " + msg);
}
