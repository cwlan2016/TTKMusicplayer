#include "musicdownloadbdinterface.h"
#include "musicnumberutils.h"
#include "musicsemaphoreloop.h"
#include "musictime.h"
#include "musicalgorithmutils.h"
#include "musicurlutils.h"
#///QJson import
#include "qjson/parser.h"
#include "qalg/qaeswrap.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QNetworkAccessManager>

void MusicDownLoadBDInterface::makeTokenQueryUrl(QNetworkRequest *request, const QString &id)
{
    QString key = MusicUtils::Algorithm::mdII(BD_SONG_ATTR_PA_URL, false).arg(id)
                  .arg(MusicTime::timeStamp());
    QString eKey = QString(QAesWrap::encrypt(key.toUtf8(), "4CC20A0C44FEB6FD", "2012061402992850"));
    MusicUtils::Url::urlEncode(eKey);
    QUrl musicUrl = MusicUtils::Algorithm::mdII(BD_SONG_ATTR_URL, false).arg(key).arg(eKey);

    request->setUrl(musicUrl);
    request->setRawHeader("Content-Type", "application/x-www-form-urlencoded");
    request->setRawHeader("User-Agent", MusicUtils::Algorithm::mdII(BD_UA_URL_1, ALG_UA_KEY, false).toUtf8());
#ifndef QT_NO_SSL
    QSslConfiguration sslConfig = request->sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request->setSslConfiguration(sslConfig);
#endif
}

void MusicDownLoadBDInterface::readFromMusicSongAttribute(MusicObject::MusicSongInformation *info, const QString &bit)
{
    QNetworkRequest request;
    makeTokenQueryUrl(&request, info->m_songId);

    QNetworkAccessManager manager;
    MusicSemaphoreLoop loop;
    QNetworkReply *reply = manager.get(request);
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), &loop, SLOT(quit()));
    loop.exec();

    if(!reply || reply->error() != QNetworkReply::NoError)
    {
        return;
    }

    QJson::Parser parser;
    bool ok;
    QVariant data = parser.parse(reply->readAll(), &ok);
    if(ok)
    {
        QVariantMap value = data.toMap();
        if(value["error_code"].toInt() == 22000 && value.contains("songurl"))
        {
            value = value["songurl"].toMap();
            QVariantList datas = value["url"].toList();
            foreach(const QVariant &var, datas)
            {
                if(var.isNull())
                {
                    continue;
                }

                value = var.toMap();
                int bitrate = value["file_bitrate"].toInt();
                if(bit.toInt() == bitrate)
                {
                    MusicObject::MusicSongAttribute attr;
                    attr.m_url = value["file_link"].toString();
                    if(attr.m_url.isEmpty())
                    {
                        continue;
                    }

                    if(attr.m_url.contains("pan."))
                    {
                        readFromMusicPayAttribute(info);
                    }
                    else
                    {
                        attr.m_duration = MusicTime::msecTime2LabelJustified(value["file_duration"].toInt()*1000);
                        attr.m_size = MusicUtils::Number::size2Label(value["file_size"].toInt());
                        attr.m_format = value["file_extension"].toString();
                        attr.m_bitrate = MusicUtils::Number::transfromBitrateToNormal(bitrate);
                        info->m_songAttrs.append(attr);
                    }
                }
            }
        }
    }
}

void MusicDownLoadBDInterface::readFromMusicSongAttribute(MusicObject::MusicSongInformation *info,
                                                          const QString &format, const QString &quality, bool all)
{
    QString formatString = format;
    foreach(const QString &f, formatString.split(","))
    {
        if(all)
        {
            if(f != "flac")
            {
                readFromMusicSongAttribute(info, f);
            }
            else
            {
                readFromMusicLLAttribute(info);
            }
        }
        else
        {
            if(f != "flac")
            {
                int bit = MusicUtils::Number::transfromBitrateToNormal(f.toInt());
                if(quality == QObject::tr("ST") && bit <= MB_64)
                {
                    readFromMusicSongAttribute(info, f);
                }
                else if(quality == QObject::tr("SD") && bit > MB_64 && bit <= MB_128)
                {
                    readFromMusicSongAttribute(info, f);
                }
                else if(quality == QObject::tr("HQ") && bit > MB_128 && bit <= MB_192)
                {
                    readFromMusicSongAttribute(info, f);
                }
                else if(quality == QObject::tr("SQ") && bit > MB_192 && bit <= MB_320)
                {
                    readFromMusicSongAttribute(info, f);
                }
            }
            else if(quality == QObject::tr("CD"))
            {
                readFromMusicLLAttribute(info);
            }
        }
    }
}

void MusicDownLoadBDInterface::readFromMusicLLAttribute(MusicObject::MusicSongInformation *info)
{
    QUrl musicUrl = MusicUtils::Algorithm::mdII(BD_SONG_INFO_URL, false).arg(info->m_songId);

    QNetworkRequest request;
    request.setUrl(musicUrl);
    request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
    request.setRawHeader("User-Agent", MusicUtils::Algorithm::mdII(BD_UA_URL_1, ALG_UA_KEY, false).toUtf8());
#ifndef QT_NO_SSL
    QSslConfiguration sslConfig = request.sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);
#endif
    QNetworkAccessManager manager;
    MusicSemaphoreLoop loop;
    QNetworkReply *reply = manager.get(request);
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), &loop, SLOT(quit()));
    loop.exec();

    if(!reply || reply->error() != QNetworkReply::NoError)
    {
        return;
    }

    QJson::Parser parser;
    bool ok;
    QVariant data = parser.parse(reply->readAll(), &ok);
    if(ok)
    {
        QVariantMap value = data.toMap();
        if(value["errorCode"].toInt() == 22000 && value.contains("data"))
        {
            value = value["data"].toMap();
            QVariantList datas = value["songList"].toList();
            foreach(const QVariant &var, datas)
            {
                if(var.isNull())
                {
                    continue;
                }

                value = var.toMap();
                value = value["linkinfo"].toMap();
                if(!value.keys().isEmpty())
                {
                    value = value[value.keys().first()].toMap();
                    MusicObject::MusicSongAttribute attr;
                    attr.m_url = value["songLink"].toString();
                    attr.m_size = MusicUtils::Number::size2Label(value["size"].toInt());
                    attr.m_format = value["format"].toString();
                    attr.m_bitrate = MusicUtils::Number::transfromBitrateToNormal(value["rate"].toInt());
                    attr.m_duration = MusicTime::msecTime2LabelJustified(value["time"].toInt()*1000);
                    info->m_songAttrs.append(attr);
                }
            }
        }
    }
}

void MusicDownLoadBDInterface::readFromMusicPayAttribute(MusicObject::MusicSongInformation *info)
{
    QUrl musicUrl = MusicUtils::Algorithm::mdII(BD_SONG_FMINFO_URL, false).arg(info->m_songId);

    QNetworkRequest request;
    request.setUrl(musicUrl);
    request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
    request.setRawHeader("User-Agent", MusicUtils::Algorithm::mdII(BD_UA_URL_1, ALG_UA_KEY, false).toUtf8());
#ifndef QT_NO_SSL
    QSslConfiguration sslConfig = request.sslConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);
#endif
    QNetworkAccessManager manager;
    MusicSemaphoreLoop loop;
    QNetworkReply *reply = manager.get(request);
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), &loop, SLOT(quit()));
    loop.exec();

    if(!reply || reply->error() != QNetworkReply::NoError)
    {
        return;
    }

    QJson::Parser parser;
    bool ok;
    QVariant data = parser.parse(reply->readAll(), &ok);
    if(ok)
    {
        QVariantMap value = data.toMap();
        if(value["errorCode"].toInt() == 22000 && value.contains("data"))
        {
            value = value["data"].toMap();
            QVariantList datas = value["songList"].toList();
            foreach(const QVariant &var, datas)
            {
                if(var.isNull())
                {
                    continue;
                }

                value = var.toMap();
                MusicObject::MusicSongAttribute attr;
                attr.m_url = value["songLink"].toString();
                attr.m_url.replace(MusicUtils::Algorithm::mdII(BD_SONG_YYDOWN_URL, false),
                                   MusicUtils::Algorithm::mdII(BD_SONG_SSDOWN_URL, false));
                attr.m_size = MusicUtils::Number::size2Label(value["size"].toInt());
                attr.m_format = value["format"].toString();
                attr.m_bitrate = MusicUtils::Number::transfromBitrateToNormal(value["rate"].toInt());
                info->m_songAttrs.append(attr);
            }
        }
    }
}