#include "ChatWindow.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>

ChatWindow::ChatWindow(const QString &host, quint16 port)
{
  m_pChatView = new QTextEdit(this);
  m_pChatView->setReadOnly(true);
  m_pInput = new QLineEdit(this);
  m_pSendButton = new QPushButton("Send", this);
  m_chooseFileButton = new QPushButton("Choose file...", this);
  m_clearButton = new QPushButton("Clear", this);
  m_progressBar = new QProgressBar(this);
  m_progressBar->setVisible(false);
  m_statusBar = new QLabel(this);

  QVBoxLayout* pVerticalLayout = new QVBoxLayout(this);
  pVerticalLayout->addWidget(m_pChatView);
  pVerticalLayout->addWidget(m_pInput);
  pVerticalLayout->addWidget(m_pSendButton);

  QHBoxLayout* pHorizontalLayout = new QHBoxLayout(this);
  pHorizontalLayout->addWidget(m_chooseFileButton);
  pHorizontalLayout->addWidget(m_clearButton);
  pVerticalLayout->addLayout(pHorizontalLayout);

  pVerticalLayout->addWidget(m_progressBar);
  pVerticalLayout->addWidget(m_statusBar);
  setLayout(pVerticalLayout);

  m_pChatEndpoint = new CChatEndpoint(host, port);
  m_pChatEndpoint->moveToThread(&m_chatEndpointThread);
  m_chatEndpointThread.start();

  connect(m_pChatEndpoint, &CChatEndpoint::TextMessageReceived, this, &ChatWindow::OnTextMessageReceived);
  connect(m_pChatEndpoint, &CChatEndpoint::FileTransferProgress, this, &ChatWindow::OnFileTransferProgress);
  connect(m_pChatEndpoint, &CChatEndpoint::FileSendingFinished, this, &ChatWindow::OnFileSendingFinished);
  connect(m_pChatEndpoint, &CChatEndpoint::FileReceivingFinished, this, &ChatWindow::OnFileReceivingFinished);
  
  connect(m_pSendButton, &QPushButton::clicked, this, &ChatWindow::sendMessage);
  connect(m_chooseFileButton, &QPushButton::clicked, [&]() {
    QString filePath = QFileDialog::getOpenFileName(this, "Open File", "", "All Files (*.*)");
    if (!filePath.isEmpty())
    {
      QFileInfo fileInfo(filePath);
      auto fileName = fileInfo.fileName();
      m_statusBar->setText(filePath);
      m_pInput->setReadOnly(true);
      m_pInput->setText("Selected file: " + fileName);
    }
  });
  connect(m_clearButton, &QPushButton::clicked, [&]() {
    m_pInput->clear();
    m_pInput->setReadOnly(false);
    m_pInput->setFocus();
    m_progressBar->setVisible(false);
    m_progressBar->setValue(0);
    m_statusBar->clear();
  });

  QMetaObject::invokeMethod(m_pChatEndpoint, "EstablishConnection", Qt::QueuedConnection);
  QMetaObject::invokeMethod(m_pChatEndpoint, "EventLoop", Qt::QueuedConnection);
}

ChatWindow::~ChatWindow()
{
  m_chatEndpointThread.quit();
  m_chatEndpointThread.wait();
  m_pChatEndpoint->deleteLater();
}

void ChatWindow::sendMessage() {
  if (m_pInput->isReadOnly())
  {
    // file was selected, send it
    QString filePath = m_statusBar->text();
    QFileInfo fileInfo(filePath);
    auto fileName = fileInfo.fileName();

    m_progressBar->setVisible(true);
    m_pInput->clear();
    m_pInput->setReadOnly(false);
    m_pInput->setFocus();
    
    QFile file(filePath);
    if (file.exists())
    {
      qDebug() << "Send file" << filePath;
      m_pChatEndpoint->SendFile(filePath);
    }
    else
    {
      qDebug() << "Failed to send file since it does not exist anymore";
    }

    return;
  }

  // file was not selected, this is a text message
  QString msg = m_pInput->text();
  if (!msg.isEmpty())
  {
    m_pChatView->append("You: " + msg);
    m_pChatEndpoint->SendText(msg);
    m_pInput->clear();
  }
}

void ChatWindow::OnTextMessageReceived(const QString& msg) {
  m_pChatView->append("Peer: " + msg);
}

void ChatWindow::OnFileTransferProgress(int percent)
{
  qDebug() << "OnFileTransferProgress" << percent << "%";
  m_progressBar->setValue(percent);
}

void ChatWindow::OnFileSendingFinished(const QString& fileName)
{
  m_progressBar->setVisible(false);
  m_progressBar->setValue(0);
  m_statusBar->clear();

  m_pChatView->append("You: " + fileName);
}

void ChatWindow::OnFileReceivingFinished(const QString& fileName)
{
  m_pChatView->append("Peer: " + fileName);
}
