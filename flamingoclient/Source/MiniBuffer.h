#pragma once
#include <stdint.h>

//��������������ʧ�Ժ��Զ��ͷŵ������ڴ���
class CMiniBuffer
{
public:
	CMiniBuffer(int64_t nSize, BOOL bAutoRelease = TRUE);
	~CMiniBuffer();

	void Release();

	int64_t GetSize();
	char* GetBuffer();
	
	//TODO: ��һ���ӿڣ�ʹCMiniBuffer�������ֱ�ӱ������ַ���ָ��ʹ��
	//PSTR operator PSTR(); 
	
	void EnableAutoRelease(BOOL bAutoRelease);
	BOOL IsAutoRelease();

private:
	BOOL	m_bAutoRelease;
    int64_t	m_nSize;
	char*	m_pData;
};