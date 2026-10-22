#include "PLSPlatformRtmp.h"

#include "../PLSPlatformApi.h"
#include "PLSChannelDataAPI.h"
#ifdef HOPCAST_NO_NAVER
#include <liblog.h>
#include <frontend-api.h>
#include <PLSAlertView.h>
#include <QUrl>
#endif

PLSServiceType PLSPlatformRtmp::getServiceType() const
{
	return PLSServiceType::ST_CUSTOM;
}

void PLSPlatformRtmp::onPrepareLive(bool value)
{
	if (!value) {
		prepareLiveCallback(value);
		return;
	}

#ifdef HOPCAST_NO_NAVER
	/* HopCast: say so plainly when the channel has no server address or stream key (instead of
	 * the server's confusing "could not access the channel or stream key"), and log what is
	 * used - the host and the key's length, never the key itself. */
	{
		QString server = getInitData().value(ChannelData::g_channelRtmpUrl).toString().trimmed();
		const QString key = getInitData().value(ChannelData::g_streamKey).toString().trimmed();
		PLS_INFO(MODULE_PlatformService, "HopCast: going live with '%s': server host=%s, stream key length=%d", qUtf8Printable(getChannelName()),
			 qUtf8Printable(QUrl(server).host()), static_cast<int>(key.size()));
		if (server.isEmpty() || key.isEmpty()) {
			PLS_ERROR(MODULE_PlatformService, "HopCast: channel has no %s saved; not going live", server.isEmpty() ? "server address" : "stream key");
			pls_async_call_mt(this, [] {
				PLSAlertView::warning(pls_get_main_view(), QTStr("Alert.Title"),
						      QObject::tr("This channel has no stream key (or server address) saved.\n\n"
								  "Open the channel, enter the server address and stream key again, then save it."));
			});
			prepareLiveCallback(false);
			return;
		}
		setStreamServer(server.toStdString());
		setStreamKey(key.toStdString());
		prepareLiveCallback(true);
		return;
	}
#endif
	setStreamServer(getInitData().value(ChannelData::g_channelRtmpUrl).toString().toStdString());
	setStreamKey(getInitData().value(ChannelData::g_streamKey).toString().toStdString());

	prepareLiveCallback(true);
}

void PLSPlatformRtmp::onAlLiveStarted(bool value)
{
	if (value && getChannelName() == FACEBOOK) {
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("MQTT.Facebook.OnBroadcast"));
	} else if (value && getChannelName() == BAND) {
		pls_toast_message(pls_toast_info_type::PLS_TOAST_NOTICE, QTStr("Live.Check.Band.Rtmp.Living.Notice"));
	}
}

QString PLSPlatformRtmp::getShareUrlEnc()
{
	return mySharedData().m_strStreamServer.c_str();
}

QJsonObject PLSPlatformRtmp::getLiveStartParams()
{
	QJsonObject platform;

	platform["id"] = mySharedData().m_mapInitData[ChannelData::g_rtmpUserID].toString();
	platform["password"] = mySharedData().m_mapInitData[ChannelData::g_password].toString();
	platform["simulcastChannel"] = mySharedData().m_mapInitData[ChannelData::g_nickName].toString();

	return platform;
}
