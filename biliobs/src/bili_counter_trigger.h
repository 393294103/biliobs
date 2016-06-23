#ifndef BILICOUNTERTRIGGER_H
#define BILICOUNTERTRIGGER_H

#include <functional>

//һ�����м����Ķ���
//����Ϊ0��ʱ�򴥷��¼�
//��;������ֱֹͣ�����������Ҫ�����Ǳߺ͵�Ļ���Ǳ߶�ͣ���Ժ����ͣ
//Ȼ�������ֲ���ȷ���ĸ����ĸ������Ǿͣ�ry
class BiliCounterTrigger
{
	volatile long count;
	std::function<void()> triggerAction;

	BiliCounterTrigger(std::function<void()>&& action);

	void Inc();
	void Dec();
public:
	void StartPendingAction();
	void FinishPendingAction();
	void Activate();

	static BiliCounterTrigger* Create(std::function<void()>&& action);
};

#endif
