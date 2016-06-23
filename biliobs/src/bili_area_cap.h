#ifndef BILI_AREA_CAP_H
#define BILI_AREA_CAP_H

#include <QWidget>
#include <vector>

class QShortcut;
class QPushButton;
class bili_area_tool;
class bili_area_zoom;
class BiLiPropertyDlg;

class bili_area_cap : public QWidget {

	Q_OBJECT

public:
	bili_area_cap(BiLiPropertyDlg* fromPropDlg, QWidget *parent = 0);
	~bili_area_cap();

	void mShow();
	QRect mSelectedWidRect;			//����ڵ�ǰ��ʾ�����Ͻǵ�����
	QPoint mSelectedWidRectOrgin;	//�����Ǹ����ε�ԭ��ľ�������
	
	HWND mGuessSelectedWindow() const;

	BiLiPropertyDlg* getStartupPropDlg() const { return mBelongTo; }

private slots:
	void onEscapeKeyPressed();
	void onButtonClicked(int btnId);

protected:
	virtual void paintEvent(QPaintEvent *e);
	virtual void mousePressEvent(QMouseEvent *e);
	virtual void mouseMoveEvent(QMouseEvent *e);
	virtual void mouseReleaseEvent(QMouseEvent *e);
	virtual void mouseDoubleClickEvent(QMouseEvent *e);
	virtual void leaveEvent(QEvent* e) override;

signals:
	void mSglSelectComplite(bool hasSelect);

private:
	BiLiPropertyDlg* mBelongTo;

	//ÿ����ʾ��һ���������ж��
	std::vector<QPixmap> mDesktopPixmaps;
	std::vector<int> mWidths;
	std::vector<int> mHeights;

	void update_current_screen();
	int mCurrentScreen;

	//���������Ǹ�ÿ����ʾ��һ���ģ����ص��ǵ�ǰ��ʾ����
	const QPixmap& mDesktopPixmap() const;
	int mWidth() const;
	int mHeight() const;

	bool mHasSelected;
													//�Զ�ѡ��ʱ��״̬
	enum MouseState {
		Default = 1<<0,
		Pressed = 1<<1,
		Move = 1<<2,
		Released = 1<<3,
		Optional = 1<<4,
		Selected = 1<<5
	};

	int mMouseState = MouseState::Default;

	//���������궼����Ե�ǰ��Ļ���Ͻǵ�
	QPoint mPosLeftTop;
	QPoint mPressStartPos;
	QPoint mPosRightBottom;
    QPoint screen_pos_;
													//�Զ���ѡ��ʱ��״̬
	bool mIsPressSelected;
	QPoint mPosPressSelected;						//��Ե�ǰ��ʾ�����Ͻǵ�����
	int mMouseShape;

	//�����������Ļ���Ͻǵ�����
	Qt::CursorShape mGetCurPosCursorShape(QPoint pos);

													//�任�Ѿ�ѡ�������
	void mMoveSelectRect(QPoint pos);
	void mZoomSelectRect(QPoint pos);

	//�����������Ļ���Ͻǵ�����
	QPoint mGetDragBottomRight(QPoint pos);

	QShortcut *mShortCut;

	//ȷ����ȡ����ť
	bili_area_tool *mAreaTool;
	void mMoveAreaTool();
	void mSetupUI();

	//�Ŵ󾵲���
	bili_area_zoom *mAreaZoom;
};

#endif // BILI_AREA_CAP_H
