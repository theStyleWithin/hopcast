#ifndef RTMPCHANNELVIEW_HPP
#define RTMPCHANNELVIEW_HPP

#include <QDialog>
#include <QVariantMap>
#include "PLSDialogView.h"
#include "pls-performance.h"
namespace Ui {
class RtmpChannelView;
}

class PLSLoginInfo;
class QLabel;

class PLSRtmpChannelView : public PLSDialogView {
	Q_OBJECT

public:
	explicit PLSRtmpChannelView(const QVariantMap &oldData, QWidget *parent = nullptr);
	~PLSRtmpChannelView() override;
	void initUi();
	QVariantMap SaveResult() const;
	void loadFromData(const QVariantMap &oldData);
	void showResolutionGuide();

	void setPlatformCombboxIndex(const QString &channleName);

	enum CustomChannelType { RTMP = 0, SRT = 1, RIST = 2, OTHER = 3 };

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
	void updateSaveBtnAvailable();

	void on_SaveBtn_clicked();

	void on_CancelBtn_clicked();

	void on_PasswordVisible_toggled(bool);

	void on_RTMPUrlEdit_textChanged(const QString &);

	void on_PlatformCombbox_currentTextChanged(const QString &showText);

	void on_ServerComboBox_currentTextChanged(const QString &text);

	void on_OpenLink_clicked() const;

	void on_onlyPasteKey_toggled(bool checked);

private:
	void languageChange();
	void initCommbox();
	bool verifyRename();
	bool checkIsModified() const;
	bool isInfoValid();
	void updateRtmpInfos();
	bool isUrlRight(const QString &regular, const QString &url) const;

	void updatePlatform(const QVariantMap &oldData);

	void ValidateNameEdit();
	void IsHideSomeFrame(bool bShow);

	void UpdateServerList(const QString &channelName);

	void setServerUI(const QString &channelName);

	void handleContextMenu(QContextMenuEvent *event);
	void handlePasteOperation();
#ifdef HOPCAST_NO_NAVER
	/* HopCast: an optional picture for the channel (PNG, JPG, WebP or GIF, animated too). */
	void initIconRow();
	void refreshIconPreview();
	void applyIcon(QVariantMap &data) const;
	QLabel *m_iconPreview = nullptr;
	mutable QString m_iconCurrent;       // the copy kept in HopCast's folder (empty = default circle)
	mutable QString m_iconChosenSource;  // picked in this dialog, copied in when saving
	mutable bool m_iconCleared = false;
#endif
	//private:
	Ui::RtmpChannelView *ui;
	QVariantMap mOldData;
	bool isEdit = false;
	QMap<QString, QString> mRtmps;
	QStringList mPlatforms;
	CustomChannelType m_type = RTMP;
	QList<QPair<QString, QString>> mYoutubeRtmpServer;
	QList<QPair<QString, QString>> mTwitchServer;
};

#endif // RTMPCHANNELVIEW_HPP
