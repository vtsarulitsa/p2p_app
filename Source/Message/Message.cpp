#include "Message.hpp"

CMessage::CMessage(const QByteArray &data, EMessageType type, const QString &filename) 
  : m_data(data), m_type(type), m_filename(filename) {}

QByteArray CMessage::Serialize() const
{
  QByteArray nameUtf8 = m_filename.toUtf8();
  SMessageHeader header = {
    static_cast<quint8>(m_type),
    static_cast<quint32>(nameUtf8.size()),
    static_cast<quint32>(m_data.size())
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

  int totalSize = HEADER_SIZE + header.nameLength + header.dataLength;
  if (buffer.size() < totalSize) return nullptr;

  auto message = std::make_shared<CMessage>(
    buffer.mid(HEADER_SIZE + header.nameLength, header.dataLength),
    static_cast<EMessageType>(header.type),
    QString::fromUtf8(buffer.mid(HEADER_SIZE, header.nameLength))
  );

  buffer = buffer.mid(totalSize);
  return message;
}

CMessage::EMessageType CMessage::GetType() const
{
  return m_type;
}

QString CMessage::GetFilename() const
{
  return m_filename;
}

QByteArray CMessage::GetData() const
{
  return m_data;
}
