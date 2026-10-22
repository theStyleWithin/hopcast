#include "PLSRtmpChannelView.h"
#include <QClipboard>
#include "PLSAlertView.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QUuid>
#include <QComboBox>
#include <QListView>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QUrl>
#include <QValidator>
#include "ChannelCommonFunctions.h"
#include "LogPredefine.h"
#include "PLSChannelDataAPI.h"
#include "frontend-api.h"
#include "PLSComboBox.h"
#include "ResolutionGuidePage.h"
#include "obs-app.hpp"
#include "obs.h"
#include "pls-channel-const.h"
#include "pls-gpop-data.hpp"
#include "pls-net-url.hpp"
#include "ui_PLSRtmpChannelView.h"

using namespace ChannelData;

PLSRtmpChannelView::PLSRtmpChannelView(const QVariantMap &oldData, QWidget *parent) : PLSDialogView(parent, {}, CreateWinId::Create), ui(new Ui::RtmpChannelView), mOldData(oldData)
{
	PLS_PERFORMANCE_FUNCTION();
	PLS_DISABLE_UISTEP_V2(this);
	pls_add_css(this, {"PLSRTMPChannelView", "PLSLiveInfoBase"});
	initUi();
	PLS_PERFORMANCE_START(getServer);
	mTwitchServer = initTwitchServer();
	mYoutubeRtmpServer = getObsServer(YOUTUBE_RTMP);
	PLS_PERFORMANCE_END(getServer);
	loadFromData(oldData);
	QMetaObject::invokeMethod(
		this,
		[this] {
			pls_flush_style(ui->UserIDEdit);
			pls_flush_style(ui->UserPasswordEdit);
		},
		Qt::QueuedConnection);
	UpdateServerList(ui->PlatformCombbox->currentData().toString());
	setServerUI(ui->PlatformCombbox->currentData().toString());
	updateSaveBtnAvailable();
	PLS_PERFORMANCE_START(pls_uistep);
	pls_uistep_v2_set_title(this, QStringLiteral("Rtmp Channel View"));
	pls_uistep_v2_bind(ui->RTMPUrlEdit, ui->PlatformLabel);
	pls_uistep_v2_bind(ui->ServerComboBox, ui->ServerLabel);
	pls_uistep_v2_bind(ui->PlatformCombbox, ui->PlatformLabel);
	pls_uistep_v2_bind(ui->NameEdit, ui->NameLabel);
	pls_uistep_v2_enable(ui->StreamKeyEdit, false);
	pls_uistep_v2_enable(ui->UserIDEdit, false);
	pls_uistep_v2_enable(ui->UserPasswordEdit, false);
	pls_uistep_v2_enable(ui->PasswordVisible, PLS_UI_STEPS_V2_SIGNAL_CLICKED, false);
	pls_uistep_v2_custom(ui->PasswordVisible, PLS_UI_STEPS_V2_SIGNAL_TOGGLED, PLS_UI_STEPS_V2_ACTION_CLICK, QStringLiteral("button"),
			     [PasswordVisible = ui->PasswordVisible]() { return !PasswordVisible->isChecked() ? QStringLiteral("Show Password") : QStringLiteral("Hide Password"); });
	PLS_PERFORMANCE_END(pls_uistep);
#ifdef HOPCAST_NO_NAVER
	initIconRow();
#endif
}

PLSRtmpChannelView::~PLSRtmpChannelView()
{
	delete ui;
}

void PLSRtmpChannelView::initUi()
{
	PLS_PERFORMANCE_FUNCTION();
	PLS_PERFORMANCE_START(setupUi);
	this->setupUi(ui);
	PLS_PERFORMANCE_END(setupUi);
	ui->MenuFrame->hide();
	this->setHasCloseButton(false);
#ifdef HOPCAST_NO_NAVER
	setFixedSize(720, 770); // room for the picture row
#else
	setFixedSize(720, 710);
#endif
	setResizeEnabled(false);
	PLS_PERFORMANCE_START(createResolutionButtonsFrame);
#ifndef HOPCAST_NO_NAVER // HopCast: no PRISM output setting guide
	auto btnsWidget = ResolutionGuidePage::createResolutionButtonsFrame(this);
	ui->horizontalLayout_8->addWidget(btnsWidget);
	ui->horizontalLayout_8->setAlignment(btnsWidget, Qt::AlignRight);
#endif
	PLS_PERFORMANCE_END(createResolutionButtonsFrame);

#ifdef HOPCAST_NO_NAVER
	ui->OpenLink->hide();
#endif
	languageChange();
	updateRtmpInfos();
	initCommbox();
	PLS_PERFORMANCE_START(setText);
	ui->PlatformLabel->setText(QString(common::LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("setting.channel.rtmp.url")));
	ui->ServerLabel->setText(QString(common::LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("Basic.AutoConfig.StreamPage.Server")));
	ui->StreamKeyLabel->setText(QString(common::LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("setting.channel.rtmp.streamkey")));
	ui->NameLabel->setText(QString(common::LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("setting.channel.rtmp.name")));
	ui->UserPasswordEdit->installEventFilter(this);
	ui->StreamKeyEdit->installEventFilter(this);
#if defined(Q_OS_MACOS)
	ui->horizontalLayout->addWidget(ui->SaveBtn);
#endif
	PLS_PERFORMANCE_END(setText);
	PLS_PERFORMANCE_START(connect);
	connect(ui->NameEdit, &QLineEdit::textEdited, this, &PLSRtmpChannelView::updateSaveBtnAvailable, Qt::QueuedConnection);
	connect(ui->StreamKeyEdit, &QLineEdit::textEdited, this, &PLSRtmpChannelView::updateSaveBtnAvailable, Qt::QueuedConnection);
	connect(ui->RTMPUrlEdit, &QLineEdit::textEdited, this, &PLSRtmpChannelView::updateSaveBtnAvailable, Qt::QueuedConnection);
	connect(ui->UserIDEdit, &QLineEdit::textEdited, this, &PLSRtmpChannelView::updateSaveBtnAvailable, Qt::QueuedConnection);
	connect(ui->UserPasswordEdit, &QLineEdit::textEdited, this, &PLSRtmpChannelView::updateSaveBtnAvailable, Qt::QueuedConnection);
	PLS_PERFORMANCE_END(connect);
	ui->onlyPasteKey->setSpac(6);
	bool onlyPasteKeyEnabled = config_get_bool(App()->GetUserConfig(), common::CONFIG_SECTION_RTMP_CHANNEL, common::CONFIG_KEY_ONLY_PASTE_KEY);
	ui->onlyPasteKey->setChecked(onlyPasteKeyEnabled);
}

QVariantMap PLSRtmpChannelView::SaveResult() const
{
	if (m_type == SRT || m_type == RIST) {
		auto tmpData = mOldData;
		tmpData[g_channelRtmpUrl] = ui->RTMPUrlEdit->text().trimmed();
		tmpData[g_streamKey] = ui->StreamKeyEdit->text();
		if (m_type == SRT) {
			tmpData[g_data_type] = SRTType;
			tmpData[g_channelName] = CUSTOM_SRT;
		} else {
			tmpData[g_data_type] = RISTType;
			tmpData[g_channelName] = CUSTOM_RIST;
		}
		tmpData[g_nickName] = ui->NameEdit->text();
		QString userID = ui->UserIDEdit->text();
		tmpData[g_rtmpUserID] = userID;
		QString password = ui->UserPasswordEdit->text();
		tmpData[g_password] = password;
#ifdef HOPCAST_NO_NAVER
		applyIcon(tmpData);
#endif
		return tmpData;
	}
	auto tmpData = mOldData;
	tmpData[g_nickName] = ui->NameEdit->text();
	tmpData[g_channelRtmpUrl] = ui->RTMPUrlEdit->text().trimmed();
	tmpData[g_streamKey] = ui->StreamKeyEdit->text();
	QString platfromName;

	if (ui->PlatformCombbox->currentIndex() == 0 || ui->PlatformCombbox->currentData().toString() == CHANNELS_TR(UserInputRTMP)) {
		platfromName = CUSTOM_RTMP;
	} else {
		platfromName = ui->PlatformCombbox->currentData().toString();
	}

	tmpData[g_channelName] = platfromName;

	QString userID = ui->UserIDEdit->text();
	tmpData[g_rtmpUserID] = userID;
	QString password = ui->UserPasswordEdit->text();
	tmpData[g_password] = password;

	if (ui->PlatformCombbox->currentData().toString() == TWITCH) {
		QString text = QTStr("setting.output.server.auto");
		if (ui->ServerComboBox->currentText() == text) {
			tmpData[g_isTwitchRtmpServerAuto] = true;
		} else {
			tmpData[g_isTwitchRtmpServerAuto] = false;
		}
	}

#ifdef HOPCAST_NO_NAVER
	applyIcon(tmpData);
#endif
	return tmpData;
}

void PLSRtmpChannelView::updatePlatform(const QVariantMap &oldData)
{
	QString platform = getInfo(oldData, g_channelName);
	int index = ui->PlatformCombbox->findData(platform, Qt::UserRole, Qt::MatchContains);
	ui->PlatformCombbox->setDisabled(true);
	if (index != -1) {
		QSignalBlocker blocker(ui->PlatformCombbox);
		ui->PlatformCombbox->setCurrentIndex(index);
	}

	auto type = getInfo(oldData, g_data_type, RTMPType);
	if (type == RTMPType && platform == CUSTOM_RTMP) {
		QSignalBlocker blocker(ui->PlatformCombbox);
		int index = ui->PlatformCombbox->findData(CHANNELS_TR(UserInputRTMP));
		ui->PlatformCombbox->setCurrentIndex(index);
		m_type = RTMP;
	} else if (type == SRTType) {
		QSignalBlocker blocker(ui->PlatformCombbox);
		int index = ui->PlatformCombbox->findData(CHANNELS_TR(UserInputSRT));
		ui->PlatformCombbox->setCurrentIndex(index);
		m_type = SRT;
	} else if (type == RISTType) {
		QSignalBlocker blocker(ui->PlatformCombbox);
		int index = ui->PlatformCombbox->findData(CHANNELS_TR(UserInputRIST));
		ui->PlatformCombbox->setCurrentIndex(index);
		m_type = RIST;
	}
}

void PLSRtmpChannelView::loadFromData(const QVariantMap &oldData)
{
	PLS_PERFORMANCE_FUNCTION();
	auto uuid = getInfo(oldData, g_channelUUID);
	ui->CategoryLabel->setText(tr("Channels.RTMadd.Catogry"))->setUUID(uuid);
#ifdef HOPCAST_NO_NAVER
	m_iconCurrent = getInfo(oldData, g_userIconCachePath, QString());
#endif
	isEdit = getInfo(oldData, g_isUpdated, false);
	this->setWindowTitle(isEdit ? CHANNELS_TR(RTMPEdit) : CHANNELS_TR(RTMPadd.titlebar));
	if (!isEdit) {
		return;
	}

	updatePlatform(oldData);
	if (m_type == SRT || m_type == RIST) {
		IsHideSomeFrame(true);
	} else {
		IsHideSomeFrame(false);
	}
	auto platform = ui->PlatformCombbox->currentData().toString();
	UpdateServerList(platform);
	QString displayName = getInfo(oldData, g_nickName);
	ui->TitleLabel->setText(CHANNELS_TR(RTMPEdit));
	ui->NameEdit->setText(displayName);
	ui->NameEdit->setModified(true);

	QSignalBlocker block(ui->RTMPUrlEdit);
	ui->RTMPUrlEdit->setEnabled(false);
	QString rtmpUrl = getInfo(oldData, g_channelRtmpUrl);
	ui->RTMPUrlEdit->setText(rtmpUrl);
	if (platform == TWITCH) {
		bool bServerAuto = getInfo(oldData, g_isTwitchRtmpServerAuto, false);
		if (bServerAuto) {
			ui->RTMPUrlEdit->setText(ui->ServerComboBox->currentData().toString());
		}
	}
	ui->RTMPUrlEdit->setModified(false);
	QString streamKey = getInfo(oldData, g_streamKey);
	ui->StreamKeyEdit->setText(streamKey);

	QString userID = getInfo(oldData, g_rtmpUserID);
	ui->UserIDEdit->setText(userID);
	QString password = getInfo(oldData, g_password);
	ui->UserPasswordEdit->setText(password);
#ifndef HOPCAST_NO_NAVER
	pls_async_call_mt([this, oldData]() { ResolutionGuidePage::checkResolution(this, getInfo(oldData, g_channelUUID)); });
#endif
}

void PLSRtmpChannelView::showResolutionGuide()
{
	ResolutionGuidePage::showResolutionGuideCloseAfterChange(this);
}

void PLSRtmpChannelView::setPlatformCombboxIndex(const QString &channleName)
{
	int index = ui->PlatformCombbox->findData(channleName);
	ui->PlatformCombbox->setCurrentIndex(index);
}

bool PLSRtmpChannelView::eventFilter(QObject *watched, QEvent *event)
{
	auto parent = dynamic_cast<QWidget *>(watched)->parentWidget();
	switch (event->type()) {
	case QEvent::FocusIn:
		parent->setProperty("isFocus", true);
		refreshStyle(parent);
		break;
	case QEvent::FocusOut:
		parent->setProperty("isFocus", false);
		refreshStyle(parent);
		break;
	case QEvent::KeyPress:
		if (ui->onlyPasteKey->isChecked() && (watched == ui->StreamKeyEdit) && static_cast<QKeyEvent *>(event)->matches(QKeySequence::Paste)) {
			handlePasteOperation();
			return true;
		}
		break;
	case QEvent::ContextMenu:
		if (ui->onlyPasteKey->isChecked() && (watched == ui->StreamKeyEdit)) {
			handleContextMenu(static_cast<QContextMenuEvent *>(event));
			return true;
		}
		break;
	default:
		break;
	}
	return false;
}

void PLSRtmpChannelView::handleContextMenu(QContextMenuEvent *event)
{
	QMenu *menu = ui->StreamKeyEdit->createStandardContextMenu();
	for (QAction *action : menu->actions()) {
		auto text = action->text();
		QString pasteShortcut = QKeySequence(QKeySequence::Paste).toString(QKeySequence::NativeText);
		if (text.contains("Paste") || text.contains(pasteShortcut)) {
			action->disconnect(ui->StreamKeyEdit);
			connect(action, &QAction::triggered, this, [this]() { handlePasteOperation(); });
			break;
		}
	}
	menu->exec(event->globalPos());
	menu->deleteLater();
}

void PLSRtmpChannelView::on_SaveBtn_clicked()
{
	QSignalBlocker blocker(ui->NameEdit);
	if (!verifyRename()) {
		return;
	} else if (isUrlRight("^http[s]?://\\w+", ui->StreamKeyEdit->text())) {
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_CHANNEL_RTMP_STREAMKEY_ERROR, PLSErrKeyAllAlert, {},
						      PLSErrorHandler::ExtraData("RTMP channel stream key https URL rejected"), this);
		return;
	}
	if (checkIsModified()) {
		this->accept();
		return;
	}
	this->reject();
}

void PLSRtmpChannelView::on_CancelBtn_clicked()
{
	this->reject();
}

void PLSRtmpChannelView::on_PasswordVisible_toggled(bool isCheck)
{
	ui->UserPasswordEdit->setEchoMode(isCheck ? QLineEdit::Password : QLineEdit::Normal);
	PLS_UI_ACTION("Click Password Visible Finished");
}

void PLSRtmpChannelView::on_RTMPUrlEdit_textChanged(const QString &rtmpUrl)
{
	if (m_type == SRT || m_type == RIST) {
		updateSaveBtnAvailable();
		return;
	}

	if (ui->PlatformCombbox->currentIndex() == 0) {
		QSignalBlocker bloker(ui->PlatformCombbox);
		ui->PlatformCombbox->setCurrentText(CHANNELS_TR(UserInputRTMP));
	}

	auto platformStr = guessPlatformFromRTMP(rtmpUrl.trimmed());
	if (platformStr != BAND && platformStr != NOW && platformStr != CUSTOM_RTMP && platformStr != CHZZK && platformStr != NAVER_SHOPPING_LIVE) {
		if (platformStr == AFREECATV) {
			platformStr = TR_AFREECATV;
		}
		QSignalBlocker bloker(ui->PlatformCombbox);
		ui->PlatformCombbox->setCurrentText(platformStr);
		UpdateServerList(platformStr);
		m_type = OTHER;
		IsHideSomeFrame(false);
		setServerUI(platformStr);
	} else if (platformStr == CUSTOM_RTMP) {
		QSignalBlocker bloker(ui->PlatformCombbox);
		int index = 0;
		if (isUrlRight("^srt?://\\w+", rtmpUrl.trimmed())) {
			index = ui->PlatformCombbox->findData(CHANNELS_TR(UserInputSRT));
			m_type = SRT;
			platformStr = CUSTOM_SRT;
			IsHideSomeFrame(true);
		} else if (isUrlRight("^rist?://\\w+", rtmpUrl.trimmed())) {
			index = ui->PlatformCombbox->findData(CHANNELS_TR(UserInputRIST));
			m_type = RIST;
			platformStr = CUSTOM_RIST;
			IsHideSomeFrame(true);
		} else {
			index = ui->PlatformCombbox->findData(CHANNELS_TR(UserInputRTMP));
			m_type = RTMP;
			IsHideSomeFrame(false);
		}
		ui->PlatformCombbox->setCurrentIndex(index);
	} else {
		platformStr = CUSTOM_RTMP;
		m_type = RTMP;
		IsHideSomeFrame(false);
	}
	if (ui->NameEdit->text().isEmpty() || !ui->NameEdit->isModified()) {
		QSignalBlocker bloker(ui->NameEdit);
		ui->NameEdit->setText(platformStr);
		ui->NameEdit->setModified(false);
	}

	updateSaveBtnAvailable();
}

void PLSRtmpChannelView::on_PlatformCombbox_currentTextChanged(const QString &showText)
{
	QString platForm = ui->PlatformCombbox->currentData().toString();
	UpdateServerList(platForm);
	PRE_LOG_UI_MSG(QString("PlatformCombbox clicked to:" + platForm).toUtf8().constData(), PLSRtmpChannelView)
#ifndef HOPCAST_NO_NAVER
	ResolutionGuidePage::checkResolutionForPlatform(this, platForm, channel_data::ChannelDataType::RTMPType);
#endif
	if (platForm == CHANNELS_TR(UserInputRTMP) || platForm == CHANNELS_TR(UserInputSRT) || ui->PlatformCombbox->currentIndex() == 0 || platForm == CHANNELS_TR(UserInputRIST)) {

		ui->RTMPUrlEdit->setEnabled(true);

		QSignalBlocker block(ui->RTMPUrlEdit);
		ui->RTMPUrlEdit->clear();
		ui->StreamKeyEdit->clear();

		bool bmodify = ui->NameEdit->isModified();
		QSignalBlocker blockName(ui->NameEdit);
		if (platForm == CHANNELS_TR(UserInputRTMP)) {
			if (!bmodify) {
				ui->NameEdit->setText(CUSTOM_RTMP);
			}
			m_type = RTMP;
			IsHideSomeFrame(false);
		} else if (platForm == CHANNELS_TR(UserInputSRT)) {
			if (!bmodify) {
				ui->NameEdit->setText(CUSTOM_SRT);
			}
			m_type = SRT;
			IsHideSomeFrame(true);
		} else if (platForm == CHANNELS_TR(UserInputRIST)) {
			if (!bmodify) {
				ui->NameEdit->setText(CUSTOM_RIST);
			}
			m_type = RIST;
			IsHideSomeFrame(true);
		} else {
			m_type = OTHER;
			ui->NameEdit->clear();
			IsHideSomeFrame(false);
		}
		ui->ServerFrame->hide();
		ui->ServerComboBox->hide();
		updateSaveBtnAvailable();
		return;
	}
	m_type = OTHER;
	IsHideSomeFrame(false);
	ui->RTMPUrlEdit->setEnabled(false);
	if (ui->NameEdit->text().isEmpty() || !ui->NameEdit->isModified()) {
		QSignalBlocker block(ui->NameEdit);
		ui->NameEdit->setText(showText);
		ui->NameEdit->setModified(false);
	}

	if (auto retIte = mRtmps.find(platForm); retIte != mRtmps.end()) {
		QSignalBlocker block(ui->RTMPUrlEdit);
		ui->RTMPUrlEdit->setText(retIte.value());
		ui->RTMPUrlEdit->setModified(false);
	}
	auto bTwitchChannel = platForm == TWITCH;
	auto bYoutubeChannel = platForm == YOUTUBE;
	if (bTwitchChannel || bYoutubeChannel) {
		ui->ServerFrame->show();
		ui->ServerComboBox->show();
		if ((bTwitchChannel && GlobalVars::g_bUseAPIServer) || bYoutubeChannel) {
			QSignalBlocker block(ui->RTMPUrlEdit);
			ui->RTMPUrlEdit->setText(ui->ServerComboBox->currentData().toString());
			ui->RTMPUrlEdit->setModified(false);
		}
	} else {
		ui->ServerFrame->hide();
		ui->ServerComboBox->hide();
	}
	updateSaveBtnAvailable();
}

void PLSRtmpChannelView::on_ServerComboBox_currentTextChanged(const QString &text)
{
	auto platform = ui->PlatformCombbox->currentData().toString();
	auto bTwitchChannel = platform == TWITCH;
	if (!bTwitchChannel && platform != YOUTUBE) {
		PLS_INFO("PLSRtmpChannelView", "current platform is %s", platform.toUtf8().constData());
		return;
	}
	ui->RTMPUrlEdit->setEnabled(false);
	auto url = ui->ServerComboBox->currentData().toString();
	if (!url.isEmpty()) {
		QSignalBlocker block(ui->RTMPUrlEdit);
		if (bTwitchChannel && url == "auto" && !GlobalVars::g_bUseAPIServer) {
			url = mRtmps.value(platform);
		}
		PLS_INFO("PLSRtmpChannelView", "current server is %s", url.toUtf8().constData());
		ui->RTMPUrlEdit->setText(url);
		ui->RTMPUrlEdit->setModified(false);
	}
	updateSaveBtnAvailable();
}

void PLSRtmpChannelView::on_OpenLink_clicked() const
{
#ifdef HOPCAST_NO_NAVER
	return;
#endif
	if (!QDesktopServices::openUrl(g_streamKeyPrismHelper)) {
		PRE_LOG(" error open url " + g_streamKeyPrismHelper, ERROR)
	}
	PLS_UI_ACTION("In Rtmp Channel View Open Help Link Finished");
}

void PLSRtmpChannelView::on_onlyPasteKey_toggled(bool checked)
{
	config_set_bool(App()->GetUserConfig(), common::CONFIG_SECTION_RTMP_CHANNEL, common::CONFIG_KEY_ONLY_PASTE_KEY, checked);
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
	PLS_INFO("PLSRtmpChannelView", "onlyPasteKey setting saved: %s", checked ? "true" : "false");
}

void PLSRtmpChannelView::handlePasteOperation()
{
	auto key = QApplication::clipboard()->text().trimmed();
	if (key.isEmpty()) {
		PLS_WARN("PLSRtmpChannelView", "user clipboard is empty");
		return;
	}
	PLS_INFO_KR("PLSRtmpChannelView", "the stream key of user clipboard is %s", key.toUtf8().constData());
	if ((isUrlRight("^srt?://\\w+", key) || isUrlRight("^rist?://\\w+", key) || isUrlRight("^rtmp[s]?://\\w+", key))) {
		int pos = key.lastIndexOf('/');
		if (pos >= 0 && pos + 1 < key.size()) {
			key = key.mid(pos + 1);
			PLS_INFO_KR("PLSRtmpChannelView", "extracted stream key after truncation: %s", key.toUtf8().constData());
		}
	}
	ui->StreamKeyEdit->setText(key);
	ui->StreamKeyEdit->setFocus();
	updateSaveBtnAvailable();
}

void PLSRtmpChannelView::updateSaveBtnAvailable()
{
	ui->SaveBtn->setEnabled(isInfoValid());
}
void PLSRtmpChannelView::languageChange()
{
	ui->UserIDEdit->setPlaceholderText(CHANNELS_TR(Optional));
	ui->UserPasswordEdit->setPlaceholderText(CHANNELS_TR(Optional));
}

void PLSRtmpChannelView::initCommbox()
{
	PLS_PERFORMANCE_FUNCTION();
	QStringList rtmpNames;
	rtmpNames += TR_SELECT;
	rtmpNames << mPlatforms << CHANNELS_TR(UserInputRTMP) << CHANNELS_TR(UserInputSRT) << CHANNELS_TR(UserInputRIST);
	for (QString &name : rtmpNames) {
		ui->PlatformCombbox->addItem(name == AFREECATV ? SOOP : name, name);
	}

	auto view = dynamic_cast<QListView *>(ui->PlatformCombbox->view());
	view->setRowHidden(0, true);
	ui->PlatformCombbox->setMaxVisibleItems(6);
	ui->PlatformCombbox->update();
}

bool PLSRtmpChannelView::verifyRename()
{
	const auto &infos = PLSCHANNELS_API->getAllChannelInfoReference();
	auto myName = ui->NameEdit->text();
	auto isSameName = [&](const QVariantMap &info) {
		auto name = getInfo(info, g_nickName);
		if (isEdit) {
			auto uuid = getInfo(info, g_channelUUID);
			auto oldUUid = getInfo(mOldData, g_channelUUID);
			return name == myName && uuid != oldUUid;
		}
		bool ret = (name == myName);
		return ret;
	};
	if (auto ite = std::find_if(infos.cbegin(), infos.cend(), isSameName); ite != infos.end()) {
		PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_CHANNEL_RTMP_EXIST_NAME, PLSErrKeyAllAlert, {}, PLSErrorHandler::ExtraData("RTMP channel duplicate stream name"), this);
		return false;
	}
	return true;
}

void PLSRtmpChannelView::ValidateNameEdit()
{
	static QRegularExpression regx("[\\r\\n]");
	auto txt = ui->NameEdit->text();
	if (txt.contains(regx)) {
		txt.remove(regx);
		QSignalBlocker bloker(ui->NameEdit);
		auto cursor = ui->NameEdit->cursorPosition();
		ui->NameEdit->setText(txt);
		ui->NameEdit->setCursorPosition(cursor);
	}
}

void PLSRtmpChannelView::IsHideSomeFrame(bool bShow)
{
	if ((m_type == SRT || m_type == RIST) && bShow) {
		ui->EditAreaFrame->show();
		ui->StreamKeyLabel->setText(tr("setting.channel.rtmp.streamkey"));
		ui->OpenLink->hide();
	} else if (m_type == OTHER && !bShow) {
		ui->EditAreaFrame->hide();
		ui->StreamKeyLabel->setText(QString(common::LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("setting.channel.rtmp.streamkey")));
		ui->OpenLink->show();
	} else {
		ui->EditAreaFrame->show();
		ui->StreamKeyLabel->setText(QString(common::LIVEINFO_STAR_HTML_TEMPLATE).arg(tr("setting.channel.rtmp.streamkey")));
		ui->OpenLink->show();
	}
#ifdef HOPCAST_NO_NAVER
	ui->OpenLink->hide(); // HopCast: the link pointed at PRISM's online guide
#endif
}

bool PLSRtmpChannelView::isInfoValid()
{
	auto url = ui->RTMPUrlEdit->text().trimmed();
	if (m_type == SRT || m_type == RIST) {
		if (ui->NameEdit->text().isEmpty() || ui->RTMPUrlEdit->text().isEmpty()) {
			return false;
		}
		if (m_type == SRT) {
			return isUrlRight("^srt?://\\w+", url);
		} else {
			return isUrlRight("^rist?://\\w+", url);
		}
	}

	ValidateNameEdit();
	if (ui->NameEdit->text().isEmpty() || ui->RTMPUrlEdit->text().isEmpty() || ui->StreamKeyEdit->text().isEmpty()) {
		return false;
	}

	if (!isUrlRight("^rtmp[s]?://\\w+", url)) {
		return false;
	}
	return true;
}

bool PLSRtmpChannelView::checkIsModified() const
{
	auto tmp = SaveResult();
	return tmp != mOldData;
}

void PLSRtmpChannelView::updateRtmpInfos()
{
	PLS_PERFORMANCE_FUNCTION();
	mRtmps = PLSCHANNELS_API->getRTMPInfos();
	mPlatforms = PLSCHANNELS_API->getRTMPsName();
#ifdef HOPCAST_NO_NAVER
	mPlatforms.clear(); // HopCast: no list of suggested platforms - just RTMP / SRT / RIST
#endif
}

bool PLSRtmpChannelView::isUrlRight(const QString &regular, const QString &url) const
{
	QRegularExpression reg(regular);
	reg.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
	auto matchRe = reg.match(url);
	return matchRe.hasMatch();
}

void PLSRtmpChannelView::UpdateServerList(const QString &channelName)
{
	PLS_PERFORMANCE_FUNCTION();
	auto bTwitchChannel = channelName == TWITCH;
	auto bYouTubeChannel = channelName == YOUTUBE;
	if (!bTwitchChannel && !bYouTubeChannel) {
		return;
	}
	QList<QPair<QString, QString>> tmpServer;
	QSignalBlocker block(ui->ServerComboBox);
	ui->ServerComboBox->clear();
	if (bTwitchChannel) {
		if (mTwitchServer.isEmpty()) {
			ui->ServerComboBox->addItem(QTStr("setting.output.server.auto"), mRtmps.value(TWITCH));
		}
		tmpServer = mTwitchServer;
	} else if (bYouTubeChannel) {
		tmpServer = mYoutubeRtmpServer;
	}
	for (auto pair : tmpServer) {
		ui->ServerComboBox->addItem(pair.first, pair.second);
	}
}

void PLSRtmpChannelView::setServerUI(const QString &channelName)
{
	auto bTwitchChannel = channelName == TWITCH;
	if (bTwitchChannel || channelName == YOUTUBE) {
		auto text = ui->RTMPUrlEdit->text();
		int index = ui->ServerComboBox->findData(text);
		if (bTwitchChannel) {
			bool bServerAuto = getInfo(mOldData, g_isTwitchRtmpServerAuto, false);
			if (index == 0 && !bServerAuto) {
				index = 1;
			} else if (index == -1) {
				index = 0;
			}
		}
		QSignalBlocker block(ui->ServerComboBox);
		ui->ServerComboBox->setCurrentIndex(index);
		ui->ServerFrame->show();
		ui->ServerComboBox->show();
	} else {
		ui->ServerFrame->hide();
		ui->ServerComboBox->hide();
	}
}

#ifdef HOPCAST_NO_NAVER
namespace {
constexpr qint64 kMaxIconBytes = 20 * 1024 * 1024; // keeps animated pictures from being huge

QString iconDir()
{
	return pls_get_user_path(QStringLiteral("PRISMLiveStudio/user/channel-icons/"));
}

// Only ever delete a picture that HopCast itself copied in.
void removeOwnedIcon(const QString &path)
{
	if (!path.isEmpty() && path.contains(QStringLiteral("/channel-icons/")))
		QFile::remove(path);
}
} // namespace

void PLSRtmpChannelView::initIconRow()
{
	auto *label = new QLabel(QStringLiteral("Icon"), this);
	label->setObjectName(QStringLiteral("NameLabel")); // same look as the other labels in this form
	label->setFont(ui->NameLabel->font());

	auto *row = new QWidget(this);
	auto *layout = new QHBoxLayout(row);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(10);

	m_iconPreview = new QLabel(row);
	m_iconPreview->setFixedSize(44, 44);
	m_iconPreview->setAlignment(Qt::AlignCenter);
	layout->addWidget(m_iconPreview);

	auto *choose = new QPushButton(QStringLiteral("Choose a picture..."), row);
	choose->setObjectName(QStringLiteral("CancelBtn")); // same look as the grey buttons here
	auto *clear = new QPushButton(QStringLiteral("Use the default"), row);
	clear->setObjectName(QStringLiteral("CancelBtn"));
	layout->addWidget(choose);
	layout->addWidget(clear);
	layout->addStretch(1);

	ui->gridLayout->addWidget(label, 6, 0);
	ui->gridLayout->addWidget(row, 6, 2);

	connect(choose, &QPushButton::clicked, this, [this]() {
		const QString file = QFileDialog::getOpenFileName(this, QStringLiteral("Choose a picture for this channel"), QDir::homePath(),
								  QStringLiteral("Pictures (*.png *.jpg *.jpeg *.gif *.webp *.bmp);;All files (*)"));
		if (file.isEmpty())
			return;
		QImageReader reader(file);
		if (!reader.canRead() || QFileInfo(file).size() > kMaxIconBytes) {
			PLSAlertView::warning(this, QStringLiteral("HopCast"),
					      QStringLiteral("That file can't be used as a picture (it must be a PNG, JPG, WebP or GIF, 20 MB or smaller)."));
			return;
		}
		m_iconChosenSource = file;
		m_iconCleared = false;
		refreshIconPreview();
		updateSaveBtnAvailable();
	});
	connect(clear, &QPushButton::clicked, this, [this]() {
		m_iconChosenSource.clear();
		m_iconCleared = true;
		refreshIconPreview();
		updateSaveBtnAvailable();
	});
	refreshIconPreview();
}

void PLSRtmpChannelView::refreshIconPreview()
{
	if (!m_iconPreview)
		return;
	QString path = m_iconCleared ? QString() : (!m_iconChosenSource.isEmpty() ? m_iconChosenSource : m_iconCurrent);
	QPixmap shown;
	if (!path.isEmpty() && QFileInfo::exists(path)) {
		QPixmap pix(path); // a still picture, or the first frame of an animated one
		if (!pix.isNull()) {
			const int side = 44 * devicePixelRatio();
			pix = pix.scaled(side, side, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
			shown = QPixmap(side, side);
			shown.fill(Qt::transparent);
			QPainter painter(&shown);
			painter.setRenderHint(QPainter::Antialiasing);
			QPainterPath circle;
			circle.addEllipse(0, 0, side, side);
			painter.setClipPath(circle);
			painter.drawPixmap((side - pix.width()) / 2, (side - pix.height()) / 2, pix);
			shown.setDevicePixelRatio(devicePixelRatio());
		}
	}
	if (shown.isNull()) {
		m_iconPreview->setPixmap(QPixmap());
		m_iconPreview->setText(QStringLiteral("R")); // what the default circle shows
	} else {
		m_iconPreview->setText(QString());
		m_iconPreview->setPixmap(shown);
	}
}

// Copies a newly chosen picture into HopCast's own folder (once) and records which one the channel uses.
void PLSRtmpChannelView::applyIcon(QVariantMap &data) const
{
	if (!m_iconChosenSource.isEmpty()) {
		QDir().mkpath(iconDir());
		const QString dest = iconDir() + QUuid::createUuid().toString(QUuid::WithoutBraces) + QLatin1Char('.') + QFileInfo(m_iconChosenSource).suffix().toLower();
		if (QFile::copy(m_iconChosenSource, dest)) {
			removeOwnedIcon(m_iconCurrent);
			m_iconCurrent = dest;
			m_iconChosenSource.clear();
		}
	} else if (m_iconCleared) {
		removeOwnedIcon(m_iconCurrent);
		m_iconCurrent.clear();
		m_iconCleared = false;
	}
	if (m_iconCurrent.isEmpty())
		data.remove(g_userIconCachePath);
	else
		data[g_userIconCachePath] = m_iconCurrent;
}
#endif
