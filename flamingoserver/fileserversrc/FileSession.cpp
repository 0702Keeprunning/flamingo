/**
 * FileSession.cpp
 * zhangyl, 2017.03.17
 **/
#include <string.h>
#include <sstream>
#include <list>
#include "../net/tcpconnection.h"
#include "../net/protocolstream.h"
#include "../base/logging.h"
#include "../base/singleton.h"
#include "FileMsg.h"
#include "FileManager.h"
#include "FileSession.h"

using namespace net;
using namespace balloon;

FileSession::FileSession(const std::shared_ptr<TcpConnection>& conn, const char* filebasedir/* = "filecache/"*/) :
TcpSession(conn), 
m_id(0),
m_seq(0),
m_strFileBaseDir(filebasedir)
{
}

FileSession::~FileSession()
{

}

void FileSession::OnRead(const std::shared_ptr<TcpConnection>& conn, Buffer* pBuffer, Timestamp receivTime)
{
    while (true)
    {
        //����һ����ͷ��С
        if (pBuffer->readableBytes() < (size_t)sizeof(file_msg))
        {
            LOG_INFO << "buffer is not enough for a package header, pBuffer->readableBytes()=" << pBuffer->readableBytes() << ", sizeof(msg)=" << sizeof(file_msg);
            return;
        }

        //����һ��������С
        file_msg header;
        memcpy(&header, pBuffer->peek(), sizeof(file_msg));
        if (pBuffer->readableBytes() < (size_t)header.packagesize + sizeof(file_msg))
            return;

        pBuffer->retrieve(sizeof(file_msg));
        std::string inbuf;
        inbuf.append(pBuffer->peek(), header.packagesize);
        pBuffer->retrieve(header.packagesize);
        if (!Process(conn, inbuf.c_str(), inbuf.length()))
        {
            LOG_WARN << "Process error, close TcpConnection";
            conn->forceClose();
        }
    }// end while-loop

}

bool FileSession::Process(const std::shared_ptr<TcpConnection>& conn, const char* inbuf, size_t length)
{
    balloon::BinaryReadStream readStream(inbuf, length);
    int32_t cmd;
    if (!readStream.ReadInt32(cmd))
    {
        LOG_WARN << "read cmd error !!!";
        return false;
    }

    //int seq;
    if (!readStream.ReadInt32(m_seq))
    {
        LOG_WARN << "read seq error !!!";
        return false;
    }

    std::string filemd5;
    size_t md5length;
    if (!readStream.ReadString(&filemd5, 0, md5length) || md5length == 0)
    {
        LOG_WARN << "read filemd5 error !!!";
        return false;
    }

    int64_t offset;
    if (!readStream.ReadInt64(offset))
    {
        LOG_WARN << "read offset error !!!";
        return false;
    }

    int64_t filesize;
    if (!readStream.ReadInt64(filesize))
    {
        LOG_WARN << "read filesize error !!!";
        return false;
    }

    string filedata;
    size_t filedatalength;
    if (!readStream.ReadString(&filedata, 0, filedatalength))
    {
        LOG_WARN << "read filedata error !!!";
        return false;
    }

   
    LOG_INFO << "Recv from client: cmd=" << cmd << ", seq=" << m_seq << ", header.packagesize:" << length << ", filemd5=" << filemd5 << ", md5length=" << md5length;
    LOG_DEBUG_BIN((unsigned char*)inbuf, length);

    switch (cmd)
    {
        //�ļ��ϴ�
        case msg_type_upload_req:
        {
            OnUploadFileResponse(filemd5, offset, filesize,  filedata, conn);
        }
            break;


        //�ͻ����ϴ����ļ�����, ������������
        case msg_type_download_req:
        {           
            //�������أ��ͻ��˲�֪���ļ���С�� ����ֵ��0
            if (filedatalength != 0)
                return false;
            OnDownloadFileResponse(filemd5, offset, filesize,  conn);
        }
            break;

        default:
            //pBuffer->retrieveAll();
            LOG_WARN << "unsupport cmd, cmd:" << cmd << ", connection name:" << conn->peerAddress().toIpPort();
            //conn->forceClose();
            return false;
    }// end switch

    ++ m_seq;

    return true;
}

void FileSession::OnUploadFileResponse(const std::string& filemd5, int64_t offset, int64_t filesize, const std::string& filedata, const std::shared_ptr<TcpConnection>& conn)
{
    if (filemd5.empty())
    {
        LOG_WARN << "Empty filemd5, connection name:" << conn->peerAddress().toIpPort();
        return;
    }
    
    std::string outbuf;
    BinaryWriteStream writeStream(&outbuf);
    
    //���������Ѿ����ڸ��ļ���ֱ�ӷ���
    if (Singleton<FileManager>::Instance().IsFileExsit(filemd5.c_str()))
    {
        writeStream.WriteInt32(msg_type_upload_resp);
        writeStream.WriteInt32(m_seq);
        writeStream.WriteString(filemd5);
        offset = filesize = -1;
        writeStream.WriteInt64(offset);
        writeStream.WriteInt64(filesize);
        string dummyfiledata;
        writeStream.WriteString(dummyfiledata);
        LOG_INFO << "Response to client: cmd=msg_type_upload_resp" << ", connection name:" << conn->peerAddress().toIpPort();
        writeStream.Flush();

        Send(outbuf);
        return;
    }
    
    writeStream.WriteInt32(msg_type_upload_resp);
    writeStream.WriteInt32(m_seq);
    writeStream.WriteString(filemd5);
    if (offset == 0)
    {
        string filename = m_strFileBaseDir;
        filename += filemd5;
        m_fp = fopen(filename.c_str(), "w");
        if (m_fp == NULL)
        {
            LOG_ERROR << "fopen file error, filemd5=" << filemd5 << ", connection name:" << conn->peerAddress().toIpPort();
            return;
        }
    }
    else
    {
        if (m_fp == NULL)
        {
            LOG_ERROR << "file pointer should not be null, filemd5=" << filemd5 << ", offset=" << offset << ", connection name:" << conn->peerAddress().toIpPort();
            return;
        }
    }

    fseek(m_fp, offset, SEEK_SET);
    if (fwrite(filedata.c_str(), filedata.length(), 1, m_fp) != 1)
    {
		LOG_ERROR << "fwrite error, filemd5: " << filemd5
					<< ", errno: " << errno << ", errinfo: " << strerror(errno)
					<< ", filedata.length(): " << filedata.length()
					<< ", m_fp: " << m_fp
					<< ", buffer size is 512*1024"
					<< ", connection name:" << conn->peerAddress().toIpPort();
        return;
    }

    //�ļ��ϴ��ɹ�
    if (offset + (int64_t)filedata.length() == filesize)
    {
        offset = filesize = -1;
        Singleton<FileManager>::Instance().addFile(filemd5.c_str());
        ResetFile();
    }

    writeStream.WriteInt64(offset);
    writeStream.WriteInt64(filesize);
    string dummyfiledatax;
    writeStream.WriteString(dummyfiledatax);
    writeStream.Flush();

    Send(outbuf);
           
    LOG_INFO << "Response to client: cmd=msg_type_upload_resp" << ", connection name:" << conn->peerAddress().toIpPort();
}

void FileSession::OnDownloadFileResponse(const std::string& filemd5, int64_t offset, int64_t filesize, const std::shared_ptr<TcpConnection>& conn)
{
    if (filemd5.empty())
    {
        LOG_WARN << "Empty filemd5, connection name:" << conn->peerAddress().toIpPort();
        return;
    }
    
    //TODO: �ͻ������ز����ڵ��ļ�����Ӧ��ͻ��ˣ�
    if (!Singleton<FileManager>::Instance().IsFileExsit(filemd5.c_str()))
    {
        LOG_WARN << "filemd5 not exsit, filemd5: " << filemd5 << ", connection name:" << conn->peerAddress().toIpPort();
        return;
    }

    if (m_fp == NULL)
    {
        string filename = m_strFileBaseDir;
        filename += filemd5;
        m_fp = fopen(filename.c_str(), "r+");
        if (m_fp == NULL)
        {
            LOG_ERROR << "fopen file error, filemd5: " << filemd5 << ", connection name:" << conn->peerAddress().toIpPort();
            return;
        }
        fseek(m_fp, 0, SEEK_END);
        m_filesize = ftell(m_fp);
		if (m_filesize <= 0)
		{
			LOG_ERROR << "m_filesize: " << m_filesize << ", errno: " << errno << ", filemd5: " << filemd5 << ", connection name : " << conn->peerAddress().toIpPort();
			return;
		}
        fseek(m_fp, 0, SEEK_SET);
    }

    std::string outbuf;
    BinaryWriteStream writeStream(&outbuf);
    writeStream.WriteInt32(msg_type_download_resp);
    writeStream.WriteInt32(m_seq);
    writeStream.WriteString(filemd5);

    string filedata;
    
    //m_offset += offset;
    int64_t currentSendSize = 512 * 1024;
    char buffer[512 * 1024] = { 0 };
    if (m_filesize <= m_offset + currentSendSize)
    {
        currentSendSize = m_filesize - m_offset;
    }

	LOG_INFO << "currentSendSize: " << currentSendSize 
			 << ", m_filesize: " << m_filesize 
			 << ", m_offset: " << m_offset 
			 << ", filemd5: " << filemd5
			 << ", connection name:" << conn->peerAddress().toIpPort();
		

	if (currentSendSize <= 0 || fread(buffer, currentSendSize, 1, m_fp) != 1)
	{
		LOG_ERROR << "fread error, filemd5: " << filemd5
					<< ", errno: " << errno << ", errinfo: " << strerror(errno)
					<< ", currentSendSize: " << currentSendSize
					<< ", m_fp: " << m_fp
					<< ", buffer size is 512*1024"
					<< ", connection name:" << conn->peerAddress().toIpPort();
	}


    writeStream.WriteInt64(m_offset);
    m_offset += currentSendSize;
    filedata.append(buffer, currentSendSize);   
    writeStream.WriteInt64(m_filesize);
    writeStream.WriteString(filedata);
    writeStream.Flush();

    LOG_INFO << "Response to client: cmd = msg_type_download_resp, filemd5: " << filemd5 << ", connection name:" << conn->peerAddress().toIpPort();

    Send(outbuf);

    //�ļ��Ѿ��������
    if (m_offset == m_filesize)
    {
        ResetFile();
    }
}

void FileSession::ResetFile()
{
    if (m_fp)
    {
        fclose(m_fp);
        m_offset = 0;
        m_filesize = 0;
		m_fp = NULL;
    }
}