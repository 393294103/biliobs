#if !defined(BILIGLOBALDATAS_H)
#define BILIGLOBALDATAS_H

#include <string>
#include <stdint.h>

__declspec(selectany) bool gBili_isDisableLogin;

//file information
__declspec(selectany) std::wstring gBili_fileVersion;

//login data
__declspec(selectany) std::string gBili_mid;
__declspec(selectany) uint64_t gBili_expires;

__declspec(selectany) uint64_t gBili_roomId; //������ֻ����OnRetryButtonClickedִ�����֮��ſ���ʹ��
__declspec(selectany) std::string gBili_userLoginName; //��¼�����������
__declspec(selectany) std::string gBili_userName; //������ֻ���������ڹ�������OnUserInfoGot��һ�ε��ü�֮�����ʹ��
__declspec(selectany) std::string gBili_userFace; //������ֻ���������ڹ�������OnUserInfoGot��һ�ε��ü�֮�����ʹ��
__declspec(selectany) std::string gBili_pushServer; //��������ʱû����
__declspec(selectany) std::string gBili_pushPath; //��������ʱû����
__declspec(selectany) std::string gBili_danmakuServer; //��Ļ��������ַ

__declspec(selectany) std::string gBili_faceUrl;

__declspec(selectany) uint64_t gBili_rcost;
__declspec(selectany) uint64_t gBili_audienceCount;

#endif
