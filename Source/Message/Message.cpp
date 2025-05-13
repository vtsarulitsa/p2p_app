#include "Message.hpp"

CMessage::CMessage(const QByteArray &data, EMessageType type, const QString &fileName, qint64 totalFileSize) 
  : m_data(data), m_type(type), m_fileName(fileName), m_totalFileSize(totalFileSize) {}

QByteArray CMessage::Serialize() const
{
  QByteArray nameUtf8 = m_fileName.toUtf8();

  SMessageHeader header = {
    static_cast<quint8>(m_type),
    static_cast<qint64>(nameUtf8.size()),
    m_totalFileSize,
    static_cast<qint64>(m_data.size())
  };

  QByteArray out;
  out.append(reinterpret_cast<const char*>(&header), sizeof(header));
  out.append(nameUtf8);
  out.append(m_data);

  return out;
}

std::shared_ptr<CMessage> CMessage::TryDeserialize(QByteArray &buffer)
{
  auto spHeader = TryDeserializeHeader(buffer);
  if (!spHeader)
  {
    return nullptr;
  }

  if (buffer.size() <= HEADER_SIZE + spHeader->nameLength)
  {
    // no payload
    return nullptr;
  }

  ssize_t payloadSize = buffer.size() - HEADER_SIZE + spHeader->nameLength;
  auto message = std::make_shared<CMessage>(
    buffer.mid(HEADER_SIZE + spHeader->nameLength, payloadSize),
    static_cast<EMessageType>(spHeader->type),
    QString::fromUtf8(buffer.mid(HEADER_SIZE, spHeader->nameLength)),
    spHeader->fileLength
  );

  buffer.clear();
  return message;
}

std::shared_ptr<CMessage::SMessageHeader> CMessage::TryDeserializeHeader(const QByteArray& buffer)
{
  if (buffer.size() < HEADER_SIZE)
  {
    return nullptr;
  }

  auto spHeader = std::make_shared<SMessageHeader>();
  if (!spHeader)
  {
    return nullptr;
  }
  memcpy(spHeader.get(), buffer.constData(), HEADER_SIZE);

  return spHeader;
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
