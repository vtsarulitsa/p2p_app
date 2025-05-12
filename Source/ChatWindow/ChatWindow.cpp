#include "ChatWindow.hpp"
#include <QVBoxLayout>

ChatWindow::ChatWindow(const QString &host, quint16 port)
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

    m_pChatEndpoint = new CChatEndpoint(host, port);
    m_pChatEndpoint->moveToThread(&m_chatEndpointThread);
    m_chatEndpointThread.start();

    connect(m_pChatEndpoint, &CChatEndpoint::TextMessageReceived, this, &ChatWindow::OnTextMessageReceived);
    connect(m_pSendButton, &QPushButton::clicked, this, &ChatWindow::sendMessage);

    QMetaObject::invokeMethod(m_pChatEndpoint, "EventLoop", Qt::QueuedConnection);
}

ChatWindow::~ChatWindow()
{
    m_chatEndpointThread.quit();
    m_chatEndpointThread.wait();
}

void ChatWindow::sendMessage() {
    QString msg = m_pInput->text();
    if (!msg.isEmpty()) {
        m_pChatView->append("You: " + msg);
        QMetaObject::invokeMethod(m_pChatEndpoint, "SendText", Qt::QueuedConnection, Q_ARG(QString, msg));
        m_pInput->clear();
    }
}

void ChatWindow::OnTextMessageReceived(const QString &msg) {
    m_pChatView->append("Peer: " + msg);
}
