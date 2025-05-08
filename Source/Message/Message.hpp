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
    quint32 nameLength;
    quint32 dataLength;
  };
  #pragma pack(pop)

  CMessage(const QByteArray &data, EMessageType type = MT_TEXT, const QString &filename = "");

  QByteArray Serialize() const;

  static std::shared_ptr<CMessage> TryDeserialize(QByteArray &buffer);

  EMessageType GetType() const;

  QString GetFilename() const;

  QByteArray GetData() const;

private:
  QByteArray m_data;
  EMessageType m_type;
  QString m_filename;
};
