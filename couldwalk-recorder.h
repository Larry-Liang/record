// ���� ifdef ���Ǵ���ʹ�� DLL �������򵥵�
// ��ı�׼�������� DLL �е������ļ��������������϶���� COULDWALKRECORDER_EXPORTS
// ���ű���ġ���ʹ�ô� DLL ��
// �κ�������Ŀ�ϲ�Ӧ����˷��š�������Դ�ļ��а������ļ����κ�������Ŀ���Ὣ
// COULDWALKRECORDER_API ������Ϊ�Ǵ� DLL ����ģ����� DLL ���ô˺궨���
// ������Ϊ�Ǳ������ġ�
#ifdef COULDWALKRECORDER_EXPORTS
#define COULDWALKRECORDER_API __declspec(dllexport)
#else
#define COULDWALKRECORDER_API __declspec(dllimport)
#endif

// �����Ǵ� couldwalk-recorder.dll ������
class COULDWALKRECORDER_API Ccouldwalkrecorder {
public:
	Ccouldwalkrecorder(void);
	// TODO: �ڴ�������ķ�����
};

extern COULDWALKRECORDER_API int ncouldwalkrecorder;

COULDWALKRECORDER_API int fncouldwalkrecorder(void);
