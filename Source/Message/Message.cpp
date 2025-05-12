#include "Message.hpp"

CMessage::CMessage(const QByteArray &data, EMessageType type, const QString &fileName, qint64 totalFileSize) 
  : m_data(data), m_type(type), m_fileName(fileName), m_totalFileSize(totalFileSize) {}

QByteArray CMessage::Serialize() const
{
  QByteArray nameUtf8 = m_fileName.toUtf8();
  SMessageHeader header = {
    static_cast<quint8>(m_type),
    static_cast<qint64>(nameUtf8.size()),
    static_cast<qint64>(m_totalFileSize)
  };

  QByteArray out;
  out.append(reinterpret_cast<const char*>(&header), sizeof(header));
  out.append(nameUtf8);
  out.append(m_data);

  return out;
}

std::shared_ptr<CMessage> CMessage::TryDeserialize(QByteArray &buffer)
{
  constexpr int HEADER_SIZE = sizeof(SMessageHeader);
  if (buffer.size() < HEADER_SIZE) return nullptr;

  SMessageHeader header;
  memcpy(&header, buffer.constData(), HEADER_SIZE);

  if (buffer.size() < MESSAGE_SIZE) return nullptr;

  auto message = std::make_shared<CMessage>(
    buffer.mid(HEADER_SIZE + header.nameLength, MESSAGE_SIZE),
    static_cast<EMessageType>(header.type),
    QString::fromUtf8(buffer.mid(HEADER_SIZE, header.nameLength)),
    header.dataLength
  );

  buffer = buffer.mid(MESSAGE_SIZE);
  return message;
}

CMessage::EMessageType CMessage::GetType() const
{
  return m_type;
}

QString CMessage::GetFilename() const
{
  return m_fileName;
}

QByteArray CMessage::GetData() const
{
  return m_data;
}

ssize_t CMessage::GetTotalFileSize() const
{
  return static_cast<ssize_t>(m_totalFileSize);
}
