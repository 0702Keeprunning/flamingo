#pragma once

#include <thread>
#include <mutex>
#include <string>
#include <memory>
#include <condition_variable>
#include <stdint.h>

class CRecvMsgThread;

//����ͨ�Ų�ֻ�������ݴ���ͽ���
class CIUSocket
{
public:
	CIUSocket(CRecvMsgThread* pThread);
    ~CIUSocket(void);

    CIUSocket(const CIUSocket& rhs) = delete;
    CIUSocket& operator = (const CIUSocket& rhs) = delete;

    bool Init();
    void Uninit();

    void Join();
	
	void	LoadConfig();
	void	SetServer(PCTSTR lpszServer);
	void	SetFileServer(PCTSTR lpszFileServer);
    void	SetImgServer(PCTSTR lpszImgServer);
	void	SetProxyServer(PCTSTR lpszProxyServer);
	void	SetPort(short nPort);
	void    SetFilePort(short nFilePort);
    void    SetImgPort(short nImgPort);
	void	SetProxyPort(short nProxyPort);
	void    SetProxyType(long nProxyType);

	PCTSTR  GetServer() const;
	PCTSTR	GetFileServer() const;
    PCTSTR	GetImgServer() const;
	short   GetPort() const;
    short	GetFilePort() const;
    short	GetImgPort() const;

    /** 
    *@param timeout ��ʱʱ�䣬��λΪs
    **/
	BOOL	Connect(int timeout = 3);
	BOOL	ConnectToFileServer();
    BOOL	ConnectToImgServer();
	
	BOOL	IsClosed();
	BOOL	IsFileServerClosed();
    BOOL	IsImgServerClosed();
	
	void	Close();
	void	CloseFileServerConnection();
    void	CloseImgServerConnection();

	bool    CheckReceivedData();							//�ж���ͨSocket���Ƿ��յ����ݣ��з���true��û�з���false

	//�첽�ӿ�
    void    Send(const std::string& strBuffer);
	
	//ͬ���ӿ�
	BOOL    SendOnFilePort(const char* pBuffer, int64_t nSize);	
    BOOL	RecvOnFilePort(char* pBuffer, int64_t nSize);
    BOOL    SendOnImgPort(const char* pBuffer, int64_t nSize);
    BOOL	RecvOnImgPort(char* pBuffer, int64_t nSize);

private:   
    void    SendThreadProc();
    void    RecvThreadProc();

    void    SendHeartbeatPackage();

    bool	Send();
    bool	Recv();
    

private:	
	SOCKET							m_hSocket;				//һ����;Socket��������socket��
	SOCKET							m_hFileSocket;			//���ļ���Socket������socket��
    SOCKET							m_hImgSocket;			//�������Socket������socket��
	short							m_nPort;
	short							m_nFilePort;
    short							m_nImgPort;
	CString							m_strServer;			//��������ַ
	CString							m_strFileServer;		//�ļ���������ַ
    CString							m_strImgServer;		    //ͼƬ��������ַ
	
	long							m_nProxyType;			//�������ͣ�0��ʹ�ô�����
	CString							m_strProxyServer;		//�����������ַ
	short							m_nProxyPort;			//����������˿ں�

    long                            m_nLastDataTime;        //���һ���շ����ݵ�ʱ��
    std::mutex                      m_mutexLastDataTime;    //����m_nLastDataTime�Ļ�����
    long                            m_nHeartbeatInterval;   //������ʱ��������λ��
    int32_t                         m_nHeartbeatSeq;        //���������к�
	
	BOOL							m_bConnected;
	BOOL							m_bConnectedOnFileSocket;
    BOOL                            m_bConnectedOnImgSocket;

    std::shared_ptr<std::thread>    m_spSendThread;
    std::shared_ptr<std::thread>    m_spRecvThread;

    std::string                     m_strSendBuf;
    std::string                     m_strRecvBuf;

    std::mutex                      m_mtSendBuf;

    std::condition_variable         m_cvSendBuf;
    std::condition_variable         m_cvRecvBuf;

    bool                            m_bStop;

	CRecvMsgThread*					m_pRecvMsgThread;
};