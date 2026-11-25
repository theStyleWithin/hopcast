#include "GoLivePannel.h"
#include <qscopeguard.h>
#include "ChannelCommonFunctions.h"
#include "LogPredefine.h"
#include "PLSAddingFrame.h"
#include "PLSChannelDataAPI.h"
#include "PLSChannelsVirualAPI.h"
#include "PLSPlatformApi.h"
#include "frontend-api.h"
#include "pls-channel-const.h"
#include "prism/PLSPlatformPrism.h"
#include "ui_GoLivePannel.h"
#ifdef HOPCAST_NO_NAVER
#include "../../hopcast/HopCastAudioRecord.h"
#include <QActionGroup>
#include <QHBoxLayout>
#include <QMenu>
#include <QStandardPaths>
#include <QToolButton>
#include <util/config-file.h>
#endif

using namespace ChannelData;

GoLivePannel::GoLivePannel(QWidget *parent) : QFrame(parent), ui(new Ui::GoLivePannel)
{

	ui->setupUi(this);
	pls_add_css(this, {"GoLivePannel"});

	ui->GoLiveShift->setProperty("subWindowLoadingName", "GoLive Button Loading");
	mBusyFrame = new PLSAddingFrame(ui->GoLiveShift);
	mBusyFrame->setObjectName("LoadingFrame");
	mBusyFrame->setContent("");
	mBusyFrame->setSourceFirstFile(g_goliveLoadingPixPath);
	auto *layout = new QHBoxLayout(ui->GoLiveShift);
	layout->addWidget(mBusyFrame);
	layout->setContentsMargins(0, 0, 0, 0);
	ui->GoLiveShift->setLayout(layout);
	mBusyFrame->hide();

	ui->Record->setProperty("subWindowLoadingName", "Rec Button Loading");
	m_busyFrameForRec = new PLSAddingFrame(ui->Record);
	m_busyFrameForRec->setAttribute(Qt::WA_NoMousePropagation);
	m_busyFrameForRec->setObjectName("LoadingFrame");
	m_busyFrameForRec->setContent("");
	m_busyFrameForRec->setSourceFirstFile(g_loadingPixPath);
	auto *recLayout = new QHBoxLayout(ui->Record);
	recLayout->addWidget(m_busyFrameForRec);
	recLayout->setContentsMargins(0, 0, 0, 0);
	ui->Record->setLayout(recLayout);
	m_busyFrameForRec->hide();
#ifdef HOPCAST_NO_NAVER
	setupRecordMode();
#endif

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::recordingChanged, this, &GoLivePannel::updateRecordButton);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::liveStateChanged, this, &GoLivePannel::updateGoliveButton);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::liveTypeChanged, this, [this]() {
		auto state = PLSCHANNELS_API->currentBroadcastState();
		updateGoliveButton(state);
	});

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStartBroadcast, ui->GoLiveShift, [this]() { toggleBroadcast(true); });
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStopBroadcast, ui->GoLiveShift, [this](DualOutputType type) {
		if (type == DualOutputType::All) {
			toggleBroadcast(false);
		} else {
			PLSBasic::instance()->StopStreaming(type);
		}
	});

	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStartRecord, ui->Record, [this]() { toggleRecord(true); });
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::toStopRecord, ui->Record, [this]() { toggleRecord(false); });
	pls_uistep_v2_enable(ui->Record, PLS_UI_STEPS_V2_SIGNAL_CLICKED, false);
	pls_uistep_v2_enable(ui->GoLiveShift, PLS_UI_STEPS_V2_SIGNAL_CLICKED, false);
	pls_uistep_v2_custom_button(ui->Record, PLS_UI_STEPS_V2_SIGNAL_TOGGLED);
	pls_uistep_v2_custom_button(ui->GoLiveShift, PLS_UI_STEPS_V2_SIGNAL_TOGGLED);
}

GoLivePannel::~GoLivePannel()
{
	delete ui;
}

void GoLivePannel::changeEvent(QEvent *e)
{
	QFrame::changeEvent(e);
	switch (e->type()) {
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		break;
	default:
		break;
	}
}

void GoLivePannel::showEvent(QShowEvent *event)
{
	setRecTooltip();
	QFrame::showEvent(event);
}

void GoLivePannel::on_Record_toggled(bool isCheck)
{
	toggleRecord(isCheck);
}

void GoLivePannel::toggleRecord(bool isStart)
{
#ifdef HOPCAST_NO_NAVER
	// HopCast: with "Record audio only (MP3)" chosen, Rec makes an MP3 of the audio and leaves the video recording alone. A video
	// recording that is already running is stopped the normal way, whatever the choice is.
	if (hopcast::audioRecording() || (hopcast::audioOnlyMode() && !PLSCHANNELS_API->isInWholeRecording())) {
		toggleAudioRecord();
		return;
	}
#endif
	if (isEnteredRecord) {
		PRE_LOG_MSG_STEP(" Wait for record state changing .... new event is ignored!", g_recordStep, INFO)
		return;
	}

	bool bSame = PLSBasic::instance()->checkRecEncoder();
	if (bSame && mBusyFrame->isVisible()) {
		int state = PLSCHANNELS_API->currentBroadcastState();
		PLS_INFO("GoLivePannel", "current BroadcastState is %d", state);
		PLSErrorHandler::ExtraData extraData("click record button while golive button is busy");
		if (PLSCHANNELS_API->isRehearsaling()) {
			if (state == BroadcastGo || state == CanBroadcastState || state == StreamStarting) {
				PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_GOLIVE_BUSY_STARTING_REHEARSAL, PLSErrKeyAllAlert, {}, extraData);
			} else if (state == StopBroadcastGo || state == CanBroadcastStop || state == StreamStopping || state == StreamStopped) {
				PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_GOLIVE_BUSY_ENDING_REHEARSAL, PLSErrKeyAllAlert, {}, extraData);
			}
		} else {
			if (state == BroadcastGo || state == CanBroadcastState || state == StreamStarting) {
				PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_GOLIVE_BUSY_STARTING_STREAM, PLSErrKeyAllAlert, {}, extraData);
			} else if (state == StopBroadcastGo || state == CanBroadcastStop || state == StreamStopping || state == StreamStopped) {
				PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_GOLIVE_BUSY_ENDING_STREAM, PLSErrKeyAllAlert, {}, extraData);
			}
		}

		ui->Record->setDisabled(false);
		QSignalBlocker blocker(ui->Record);
		bool bCheck = ui->Record->isChecked();
		ui->Record->setChecked(!bCheck);
		return;
	}

	// Check encoder mismatch when streaming is active and trying to start recording
	if (isStart && !bSame && PLSCHANNELS_API->isLiving()) {
		PLS_INFO("GoLivePannel", "Encoder mismatch detected while streaming, showing warning");
		const bool encAdvanced = PLSBasic::instance()->isAdvancedOutputMode();
		PLSErrorHandler::ExtraData encExtra(encAdvanced ? "GoLive encoder mismatch streaming advanced" : "GoLive encoder mismatch streaming simple");
		const auto encCode = encAdvanced ? PLSErrorHandler::ALERT_GOLIVE_ENCODER_MISMATCH_STREAMING_ADVANCED : PLSErrorHandler::ALERT_GOLIVE_ENCODER_MISMATCH_STREAMING_SIMPLE;
		PLSErrorHandler::RetData encAlert = PLSErrorHandler::getAlertStringByPrismCode(encCode, PLSErrKeyAllAlert, {}, encExtra);
		pls_text_t message(encAlert.alertMsg, encAlert.alertMsgEnglish);

		PLSAlertView alert(pls_get_main_view(), PLSAlertView::Icon::Warning, tr("Alert.Title"), message, QString(),
				   {{PLSAlertView::Button::Yes, tr("Output.EncoderMismatch.ContinueRecording")}, {PLSAlertView::Button::No, tr("OK")}}, PLSAlertView::Button::No,
				   {{"disableExtralLink", true}});
		alert.setTextFormat(Qt::RichText);
		connect(&alert, &PLSAlertView::contentlinkActivated, this, [&alert](const QString &link) {
			if (link == "openOutputSettings") {
				alert.close();
				QMetaObject::invokeMethod(PLSBasic::Get(), "onPopupSettingView", Qt::QueuedConnection, Q_ARG(QString, "Output"), Q_ARG(QString, ""));
			}
		});
		auto result = static_cast<PLSAlertView::Button>(alert.exec());

		if (result != PLSAlertView::Button::Yes) {
			PLS_INFO("GoLivePannel", "User cancelled recording due to encoder mismatch");
			ui->Record->setDisabled(false);
			QSignalBlocker blocker(ui->Record);
			ui->Record->setChecked(false);
			return;
		}
		PLS_INFO("GoLivePannel", "User continued recording despite encoder mismatch");
	}

	HolderReleaser holder(&GoLivePannel::setEnteredRecord, this);
	bool isRecording = PLSCHANNELS_API->isRecording();
	int state = PLSCHANNELS_API->currentReocrdState();

	//state has been shifting to

	auto ignoreChange = [&]() {
		PRE_LOG_MSG_STEP(QString(" Record state is going to be %1 ,new event is ignored!").arg(isStart ? " ON " : "OFF"), g_recordStep, INFO)
		updateRecordButton(state);
		return;
	};
	if (isRecording == isStart) {
		ignoreChange();
		return;
	}
	QString msg = QString("Try to %1 record,current record state: ").arg(isStart ? "start " : " stop ") + RecordStatesMap[state];
	PRE_LOG_MSG_STEP(msg, g_recordStep, INFO)

	switch (state) {
	case RecordReady:
	case RecordStopped:
		PLSCHANNELS_API->setIsOnlyStopRecord(false);
		PLSCHANNELS_API->sigTrySetRecordState(CanRecord);
		break;
	case RecordStarted:
		PLSCHANNELS_API->setIsClickToStopRecord(true);
		PLSCHANNELS_API->setIsOnlyStopRecord(true);
		PLSCHANNELS_API->sigTrySetRecordState(RecordStopGo);
		break;
	default:
		ignoreChange();
		break;
	}
	holdOnRec(true);
}

void GoLivePannel::updateRecordButton(int state)
{
	QString msg = QString(" try to update RECButton ,now state is %1 ").arg(RecordStatesMap[state]);
	PRE_LOG_MSG_STEP(QString(msg), g_recordStep, INFO);
	switch (state) {
	case RecordStarted:
		if (!ui->Record->isChecked()) {
			QSignalBlocker blocker(ui->Record);
			ui->Record->setChecked(true);
		}
		holdOnRec(false);
		ui->Record->setText(QTStr("Channels.STOP"));
		ui->Record->setDisabled(false);
		ui->Record->setToolTip(QTStr("Channels.stopRec.tooltip"));
		break;
	case RecordReady:
	case CanRecord:
	case RecordStopped: {
		if (ui->Record->isChecked()) {
			QSignalBlocker blocker(ui->Record);
			ui->Record->setChecked(false);
		}
		holdOnRec(false);
		ui->Record->setText(QTStr("Channels.REC"));
		ui->Record->setDisabled(false);
		setRecTooltip();
	} break;

	case RecordStarting:
	case RecordStopping:
	case RecordStopGo:
		break;
	default:
		break;
	}
}

void GoLivePannel::holdOnAll(bool holdOn)
{
	mBusyFrame->setVisible(holdOn);
	if (holdOn) {
		mBusyFrame->start(200);

	} else {
		mBusyFrame->stop();
	}
}

bool GoLivePannel::confirmToContinue() const
{
	bool isClosing = pls_is_main_window_closing();
	if (isClosing) {
		return true;
	}
	if (pls_previous_broadcast_control_by() == ControlSrcType::RemoteControl) {
		return true;
	}
	//only check naver shopping
	auto selected = PLSCHANNELS_API->getCurrentSelectedPlatformChannels(NAVER_SHOPPING_LIVE, NoType);
	if (selected.isEmpty()) {
		return true;
	}

	if (PLS_PLATFORM_API->getLiveEndType() == EndLiveType::MQTT_END_LIVE) {
		return true;
	}
	auto islastMuted = pls_mixer_is_all_mute();
	if (!islastMuted) {
		pls_mixer_mute_all(true);
	}

	quint64 time = 10 * 1000;
	const bool rehearsing = PLSCHANNELS_API->isRehearsaling();
	PLSErrorHandler::ExtraData countdownExtra(rehearsing ? "GoLive end rehearsal countdown confirm" : "GoLive end live countdown confirm");
	const auto countdownCode = rehearsing ? PLSErrorHandler::ALERT_GOLIVE_END_REHEARSAL_CONFIRM : PLSErrorHandler::ALERT_GOLIVE_END_LIVE_CONFIRM;
	PLSErrorHandler::RetData countdownAlert = PLSErrorHandler::getAlertStringByPrismCode(countdownCode, PLSErrKeyAllAlert, {}, countdownExtra);
	pls_text_t msg(countdownAlert.alertMsg, countdownAlert.alertMsgEnglish);
	pls_language_key_t toEndLive(rehearsing ? ("yes.endRehearsal") : ("yes.endlive"));
	pls_language_key_t notEnd(rehearsing ? ("no.continue.rehearsal") : ("no.continue.live"));
	//according ux, continue left,end right,so use cancel and apply .

	auto ret = PLSAlertView::questionWithCountdownView(pls_get_main_view(), tr("Confirm"), msg, "", {{PLSAlertView::Button::Cancel, notEnd}, {PLSAlertView::Button::Apply, toEndLive}},
							   PLSAlertView::Button::Apply, time, 170);

	if (islastMuted != pls_mixer_is_all_mute()) {
		pls_mixer_mute_all(islastMuted);
	}
	if (ret.button == PLSAlertView::Button::Apply) {
		return true;
	}
	return false;
}

void GoLivePannel::holdOnRec(bool holdOn)
{
	m_busyFrameForRec->setVisible(holdOn);
	if (holdOn) {
		m_busyFrameForRec->start(200);
	} else {
		m_busyFrameForRec->stop();
	}
}

void GoLivePannel::on_GoLiveShift_toggled(bool isGolive)
{
	ui->GoLiveShift->setDisabled(true);
	holdOnAll(true);
	toggleBroadcast(isGolive);
}

void GoLivePannel::toggleBroadcast(bool toStart)
{
	auto cleanup = qScopeGuard([]() { pls_set_broadcast_control(ControlSrcType::None); });
	if (isEnteredGolive) {
		PRE_LOG_MSG_STEP("ignore toggle broadcasting,it has entered ", g_LiveStep, INFO)
		return;
	}

	bool bSame = PLSBasic::instance()->checkRecEncoder();
	if (bSame && !ui->Record->isEnabled()) {
		int state = PLSCHANNELS_API->currentReocrdState();
		PLS_INFO("GoLivePannel", "current Reocrd is %d", state);
		if (state == RecordStarting) {
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_GOLIVE_BUSY_STARTING_REC, PLSErrKeyAllAlert, {},
							      PLSErrorHandler::ExtraData("click golive button while record is starting"));
		} else if (state == RecordStopGo || state == RecordStopping) {
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_GOLIVE_BUSY_ENDING_REC, PLSErrKeyAllAlert, {},
							      PLSErrorHandler::ExtraData("click golive button while record is ending"));
		}

		ui->GoLiveShift->setDisabled(false);
		QSignalBlocker blocker(ui->GoLiveShift);
		ui->GoLiveShift->setChecked(!toStart);
		holdOnAll(false);
		return;
	}

	// Check encoder mismatch when recording is active (or will start with streaming) and trying to start streaming
	bool recordWhenStreaming = config_get_bool(App()->GetUserConfig(), "BasicWindow", "RecordWhenStreaming");
	if (toStart && !bSame && (PLSCHANNELS_API->isRecording() || recordWhenStreaming)) {
		PLS_INFO("GoLivePannel", "Encoder mismatch detected while recording, showing warning");
		const bool encAdvanced = PLSBasic::instance()->isAdvancedOutputMode();
		PLSErrorHandler::ExtraData encExtra(encAdvanced ? "GoLive encoder mismatch recording advanced" : "GoLive encoder mismatch recording simple");
		const auto encCode = encAdvanced ? PLSErrorHandler::ALERT_GOLIVE_ENCODER_MISMATCH_RECORDING_ADVANCED : PLSErrorHandler::ALERT_GOLIVE_ENCODER_MISMATCH_RECORDING_SIMPLE;
		PLSErrorHandler::RetData encAlert = PLSErrorHandler::getAlertStringByPrismCode(encCode, PLSErrKeyAllAlert, {}, encExtra);
		pls_text_t message(encAlert.alertMsg, encAlert.alertMsgEnglish);

		PLSAlertView alert(pls_get_main_view(), PLSAlertView::Icon::Warning, tr("Alert.Title"), message, QString(),
				   {{PLSAlertView::Button::Yes, tr("Output.EncoderMismatch.ContinueStreaming")}, {PLSAlertView::Button::No, tr("OK")}}, PLSAlertView::Button::No,
				   {{"disableExtralLink", true}});
		alert.setTextFormat(Qt::RichText);
		connect(&alert, &PLSAlertView::contentlinkActivated, this, [&alert](const QString &link) {
			if (link == "openOutputSettings") {
				alert.close();
				QMetaObject::invokeMethod(PLSBasic::Get(), "onPopupSettingView", Qt::QueuedConnection, Q_ARG(QString, "Output"), Q_ARG(QString, ""));
			}
		});
		auto result = static_cast<PLSAlertView::Button>(alert.exec());

		if (result != PLSAlertView::Button::Yes) {
			PLS_INFO("GoLivePannel", "User cancelled streaming due to encoder mismatch");
			ui->GoLiveShift->setDisabled(false);
			QSignalBlocker blocker(ui->GoLiveShift);
			ui->GoLiveShift->setChecked(false);
			holdOnAll(false);
			return;
		}
		PLS_INFO("GoLivePannel", "User continued streaming despite encoder mismatch");
	}

	HolderReleaser holder(&GoLivePannel::setEnteredGolive, this);

	int state = PLSCHANNELS_API->currentBroadcastState();

	if (state == StreamStopping) {
		PLSErrorHandler::RetData retData =
			PLSErrorHandler::showAlertByPrismCode(PLSErrorHandler::ALERT_GOLIVE_FORCE_STOP_CONFIRM, PLSErrKeyAllAlert, {}, PLSErrorHandler::ExtraData("GoLive force stop confirm"));
		ui->GoLiveShift->setDisabled(false);

		if (retData.clickedBtn == QDialogButtonBox::Yes && PLSCHANNELS_API->currentBroadcastState() == StreamStopping) {
			PLSBasic::instance()->ForceStopStreaming();
			QSignalBlocker blocker(ui->GoLiveShift);
			auto bCheck = ui->GoLiveShift->isChecked();
			ui->GoLiveShift->setChecked(!bCheck);
		}
		return;
	}

	// to end
	if (!toStart && !confirmToContinue()) {
		//issue3760 navershopping click the finsh button to end end,wait 10s,BroadcastState maybe modify when mqtt force end
		state = PLSCHANNELS_API->currentBroadcastState();
		updateGoliveButton(state);
		holdOnAll(false);
		return;
	}
	state = PLSCHANNELS_API->currentBroadcastState();

	auto ignoreChange = [&]() {
		PRE_LOG_MSG_STEP("ignore toggle broadcasting,state is going to be ", g_LiveStep, INFO)
		updateGoliveButton(state);
		return;
	};

	//state has been shifting to
	if (toStart == PLSCHANNELS_API->isLiving()) {
		ignoreChange();
		return;
	}

	QString msg = QString(" try to change broadcast ,now state is %1 ").arg(LiveStatesMap[state]);
	PRE_LOG_MSG_STEP(QString(msg), g_LiveStep, INFO)

	switch (state) {
	case ReadyState:
		PLSCHANNELS_API->sigTrySetBroadcastState(BroadcastGo);
		break;

	case StreamStarted:
		PLSCHANNELS_API->setIsOnlyStopRecord(false);
		PLSCHANNELS_API->sigTrySetBroadcastState(StopBroadcastGo);
		break;
	default:
		ignoreChange();
		break;
	}
}

void GoLivePannel::updateGoliveButton(int state)
{
	QString msg = QString(" try to update GoliveButton ,now state is %1 ").arg(LiveStatesMap[state]);
	PRE_LOG_MSG_STEP(QString(msg), g_LiveStep, INFO);
	switch (state) {
	case StreamStarted:
		if (!ui->GoLiveShift->isChecked()) {
			QSignalBlocker blocker(ui->GoLiveShift);
			ui->GoLiveShift->setChecked(true);
		}
		ui->GoLiveShift->setText(PLSCHANNELS_API->isRehearsaling() ? finisheRehearsalText : finishLiveText);
		ui->GoLiveShift->setDisabled(false);
		emit PLSMainView::instance() -> onGolivePending(false);
		break;
	case ReadyState:
		if (ui->GoLiveShift->isChecked()) {
			QSignalBlocker blocker(ui->GoLiveShift);
			ui->GoLiveShift->setChecked(false);
		}
		ui->GoLiveShift->setText(goliveText);
		ui->GoLiveShift->setDisabled(false);
		emit PLSMainView::instance() -> onGolivePending(false);
		break;
	case BroadcastGo:
	case CanBroadcastState:
	case StreamStarting:
	case StopBroadcastGo:
	case CanBroadcastStop:
		ui->GoLiveShift->setDisabled(true);
		emit PLSMainView::instance() -> onGolivePending(true);
		break;
	case StreamStopping:
		ui->GoLiveShift->setDisabled(false);
		emit PLSMainView::instance() -> onGolivePending(true);
		break;
	default:
		break;
	}
}

void GoLivePannel::setRecTooltip()
{
	pls_check_app_exiting();
	int state = PLSCHANNELS_API->currentReocrdState();
	if (state == RecordStarted) {
		ui->Record->setToolTip(QTStr("Channels.stopRec.tooltip"));
	} else {
		bool bSame = PLSBasic::instance()->checkRecEncoder();
		if (bSame) {
			ui->Record->setToolTip(QTStr("Channels.pauseRec.tooltip"));
		} else {
			ui->Record->setToolTip("");
		}
	}
}

#ifdef HOPCAST_NO_NAVER
namespace {
// The folder the normal recordings go to (Settings > Output > Recording), so the MP3s land beside the videos.
QString hopcastRecordingFolder()
{
	config_t *cfg = PLSBasic::instance()->Config();
	const bool advanced = cfg && QString::fromUtf8(config_get_string(cfg, "Output", "Mode")).compare(QStringLiteral("Advanced"), Qt::CaseInsensitive) == 0;
	const char *path = cfg ? config_get_string(cfg, advanced ? "AdvOut" : "SimpleOutput", advanced ? "RecFilePath" : "FilePath") : nullptr;
	QString folder = path ? QString::fromUtf8(path) : QString();
	if (folder.isEmpty())
		folder = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
	return folder;
}
} // namespace

void GoLivePannel::setupRecordMode()
{
	m_recordMode = new QToolButton(this);
	m_recordMode->setObjectName(QStringLiteral("RecordMode"));
	m_recordMode->setFocusPolicy(Qt::NoFocus);
	m_recordMode->setCursor(Qt::PointingHandCursor);
	m_recordMode->setPopupMode(QToolButton::InstantPopup);
	m_recordMode->setStyleSheet(QStringLiteral("QToolButton { color: #e6e6e6; background: #3a3a3a; border: none; border-radius: 4px; padding: 0 7px; min-height: 26px; font-size: 13px; }"
						   "QToolButton:hover { background: #4a4a4a; } QToolButton:disabled { color: #777777; } QToolButton::menu-indicator { image: none; }"));

	auto *menu = new QMenu(m_recordMode);
	auto *group = new QActionGroup(menu);
	auto *video = menu->addAction(QStringLiteral("Record video"));
	auto *audio = menu->addAction(QStringLiteral("Record audio only (MP3)"));
	for (QAction *a : {video, audio}) {
		a->setCheckable(true);
		group->addAction(a);
	}
	connect(menu, &QMenu::aboutToShow, this, [video, audio]() {
		video->setChecked(!hopcast::audioOnlyMode());
		audio->setChecked(hopcast::audioOnlyMode());
	});
	connect(video, &QAction::triggered, this, [this]() {
		hopcast::setAudioOnlyMode(false);
		updateRecordMode();
	});
	connect(audio, &QAction::triggered, this, [this]() {
		hopcast::setAudioOnlyMode(true);
		updateRecordMode();
	});
	m_recordMode->setMenu(menu);

	if (auto *row = qobject_cast<QHBoxLayout *>(layout()))
		row->addWidget(m_recordMode);
	connect(PLSCHANNELS_API, &PLSChannelDataAPI::recordingChanged, this, [this](int) { updateRecordMode(); });
	updateRecordMode();
}

void GoLivePannel::updateRecordMode()
{
	if (!m_recordMode)
		return;
	const bool audio = hopcast::audioOnlyMode();
	const bool busy = hopcast::audioRecording() || PLSCHANNELS_API->isInWholeRecording();
	m_recordMode->setText(audio ? QStringLiteral("\u266A \u25BE") : QStringLiteral("\u25BE")); // "note arrow" in audio mode
	m_recordMode->setEnabled(!busy);
	m_recordMode->setToolTip(audio ? QStringLiteral("Rec records audio only (MP3). Click to change.") : QStringLiteral("Rec records video. Click to choose audio only (MP3)."));
}

void GoLivePannel::toggleAudioRecord()
{
	QString message;
	QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information;
	if (hopcast::audioRecording()) {
		QString problem;
		const QString saved = hopcast::stopAudioRecording(&problem);
		message = QStringLiteral("Audio saved: ") + saved;
		if (!problem.isEmpty()) {
			message += QStringLiteral(" (") + problem + QLatin1Char(')');
			icon = QSystemTrayIcon::Warning;
		}
	} else {
		QString path, error;
		if (hopcast::startAudioRecording(hopcastRecordingFolder(), &path, &error)) {
			message = QStringLiteral("Recording audio (MP3): ") + path;
		} else {
			message = QStringLiteral("Audio recording didn't start. ") + error;
			icon = QSystemTrayIcon::Warning;
		}
	}

	// Rec shows "STOP" while the audio recording runs (without starting the video recording logic)
	const bool running = hopcast::audioRecording();
	{
		QSignalBlocker blocker(ui->Record);
		ui->Record->setChecked(running);
	}
	ui->Record->setText(running ? QTStr("Channels.STOP") : QTStr("Channels.REC"));
	ui->Record->setDisabled(false);
	updateRecordMode();
	PLSBasic::instance()->SysTrayNotify(message, icon, QStringLiteral("HopCast"));
}
#endif
