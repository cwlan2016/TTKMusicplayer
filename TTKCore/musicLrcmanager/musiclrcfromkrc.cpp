#include "musiclrcfromkrc.h"
#include "musiclogger.h"

#ifdef Q_CC_GNU
#   pragma GCC diagnostic ignored "-Wwrite-strings"
#   pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#include <QFile>
#include <sys/stat.h>
#include "zconf.h"
#include "zlib.h"

const wchar_t key[] = { L'@', L'G', L'a', L'w', L'^', L'2',
                        L't', L'G', L'Q', L'6', L'1', L'-',
                        L'Î', L'Ò', L'n', L'i'};

MusicLrcFromKrc::MusicLrcFromKrc()
{
    m_resultBytes = new uchar[1024*1024];
}

MusicLrcFromKrc::~MusicLrcFromKrc()
{
    delete[] m_resultBytes;
}

bool MusicLrcFromKrc::decode(const QString &input, const QString &output)
{
    FILE *fp;
    struct stat st;
    size_t dstsize;

    fp = fopen(input.toLocal8Bit().constData(), "rb");
    if(!fp)
    {
        M_LOGGER_ERROR("open file error !");
        return false;
    }

    if(fstat(fileno(fp), &st))
    {
        M_LOGGER_ERROR("fstat file error !");
        fclose(fp);
        return false;
    }

    uchar *src = new uchar[st.st_size];
    if(fread(src, sizeof(uchar), st.st_size, fp) != st.st_size)
    {
        M_LOGGER_ERROR("fread file error !");
        delete[] src;
        fclose(fp);
        return false;
    }

    if(memcmp(src, "krc1", 4) != 0)
    {
        M_LOGGER_ERROR("error file format !");
        delete[] src;
        fclose(fp);
        return false;
    }

    src += 4;
    for(int i = 0; i < st.st_size; i++)
    {
        src[i] = (uchar)(src[i] ^ key[i % 16]);
    }

    decompression(src, st.st_size, &dstsize);
    createLrc(m_resultBytes, MStatic_cast(int, dstsize));

    delete[] src;
    fclose(fp);

    ///write data to file
    if(!output.isEmpty())
    {
        QFile file(output);
        if(file.open(QIODevice::WriteOnly))
        {
            file.write(m_data);
            file.flush();
            file.close();
        }
    }
    return true;
}

QByteArray MusicLrcFromKrc::getDecodeString() const
{
    return m_data;
}

int MusicLrcFromKrc::sncasecmp(char *s1, char *s2, size_t n)
{
    unsigned int c1, c2;
    while(n)
    {
        c1 = (unsigned int) *s1++;
        c2 = (unsigned int) *s2++;

        c1 = (c1 >= 'A' && c1 <= 'Z') ? (c1 | 0x20) : c1;
        c2 = (c2 >= 'A' && c2 <= 'Z') ? (c2 | 0x20) : c2;

        if(c1 == c2)
        {
            if(c1)
            {
                n--;
                continue;
            }
            return 0;
        }
        return c1 - c2;
    }
    return 0;
}

int MusicLrcFromKrc::decompression(unsigned char *src, size_t srcsize, size_t *dstsize)
{
    *dstsize = 1024 * 1024;
    if(Z_OK != uncompress(m_resultBytes, (uLongf*)dstsize, src, srcsize))
    {
        return -1;
    }
    return 0;
}

int MusicLrcFromKrc::isfilter(char *tok)
{
    if(!sncasecmp(tok, const_cast<char*>(std::string("[id").c_str()), 3))
    {
        return 1;
    }
    else if(!sncasecmp(tok, const_cast<char*>(std::string("[by").c_str()), 3))
    {
        return 1;
    }
    else if(!sncasecmp(tok, const_cast<char*>(std::string("[hash").c_str()), 5))
    {
        return 1;
    }
    else if(!sncasecmp(tok, const_cast<char*>(std::string("[al").c_str()), 3))
    {
        return 1;
    }
    else if (!sncasecmp(tok, const_cast<char*>(std::string("[sign").c_str()), 5))
    {
        return 1;
    }
    else if(!sncasecmp(tok, const_cast<char*>(std::string("[total").c_str()), 6))
    {
        return 1;
    }
    else if(!sncasecmp(tok, const_cast<char*>(std::string("[offset").c_str()), 7))
    {
        return 1;
    }
    return 0;
}

void MusicLrcFromKrc::createLrc(unsigned char *lrc, int lrclen)
{
    m_data.clear();
    int top = 0;
    for(int i = 0; i<lrclen; i++)
    {
        int len;
        if(top == 0)
        {
            switch(lrc[i])
            {
                case '<':
                    top++;
                    break;
                case '[':
                    len = (strchr((char*)&lrc[i], ']') - (char*)&lrc[i]) + 1;
                    for(int j = 0; j<len; j++)
                    {
                        if(lrc[i+j] == ':')
                        {
                            if(isfilter((char*)&lrc[i]))
                            {
                                while(lrc[++i] != '\n' && i < lrclen)
                                {

                                }
                            }
                            goto filter_done;
                        }
                    }

                    for(int j = 0; j<len; j++)
                    {
                        int ms;
                        if(lrc[i + j] == ',')
                        {
                            char ftime[14];
                            lrc[i + j] = 0;
                            ms = atoi((char*)&lrc[i + 1]);
                            sprintf(ftime, "[%.2d:%.2d.%.2d]", (ms % (1000 * 60 * 60)) / (1000 * 60), (ms % (1000 * 60)) / 1000, (ms % (1000 * 60)) % 100);

                            for(j = 0; j < 10; j++)
                            {
                                m_data.append(ftime[j]);
                            }
                            i = i + len - 1;
                            break;
                        }
                    }
                    break;
        filter_done:
                default:
                    m_data.append(lrc[i]);
                    break;
            }

        }
        else if(top == 1 && lrc[i] == '>')
        {
            top--;
        }
    }
}
