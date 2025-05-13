#pragma once

#include <memory>
#include <QByteArray>
#include <QString>
#include <QDataStream>

class CMessage
{
public:
  enum EMessageType : quint8
  {
    MT_TEXT = 0x01,
    MT_IMAGE = 0x02,
    MT_FILE = 0x03
  };

  #pragma pack(push, 1)
  struct SMessageHeader {
    quint8  type;
    qint64 nameLength;
    qint64 dataLength;
  };
  #pragma pack(pop)

  static constexpr ssize_t MAX_MESSAGE_SIZE = 4096;
  static constexpr ssize_t HEADER_SIZE = sizeof(SMessageHeader);
  static constexpr ssize_t MAX_PAYLOAD_SIZE = MAX_MESSAGE_SIZE - HEADER_SIZE;

  CMessage(const QByteArray &data, EMessageType type = MT_TEXT, const QString &filename = "", qint64 totalFileSize = 0);

  QByteArray Serialize() const;

  static std::shared_ptr<CMessage> TryDeserialize(QByteArray& buffer);

  static std::shared_ptr<SMessageHeader> TryDeserializeHeader(const QByteArray& buffer);

  EMessageType GetType() const;

  QString GetFilename() const;

  QByteArray GetData() const;

  ssize_t GetTotalFileSize() const;

private:
  QByteArray m_data;
  EMessageType m_type;
  QString m_fileName;
  qint64 m_totalFileSize;
};
