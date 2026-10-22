// HopCast: stream to several servers at once, without a relay. GPL-2.0-or-later.
#include "HopCastMultiStream.h"
#include "HopCastNetLog.h"

#include <obs.hpp>
#include <util/base.h>

#include <QCoreApplication>
#include <QMetaObject>

#include <algorithm>
#include <cstring>
#include <memory>
#include <mutex>

namespace hopcast {

namespace {

using Notify = std::function<void(const QString &)>;

struct Extra {
	QString name;
	qint64 startedMs = 0; // when it was started, to pick out the connection lines that belong to its failure
	Notify notify;
	OBSService service;
	OBSOutput output;
	OBSSignal stopSignal; // declared last: disconnected first when this is destroyed
};

struct State {
	std::mutex mutex;
	std::vector<Destination> pending[2]; // [0] horizontal, [1] vertical
	std::vector<std::unique_ptr<Extra>> running;
};

/* Never destroyed: at exit libobs may already be gone, and releasing an output then would crash. */
State &state()
{
	static State *s = new State;
	return *s;
}

const char *outputTypeFor(obs_service_t *service)
{
	const char *protocol = obs_service_get_protocol(service);
	if (protocol) {
		if (!strcmp(protocol, "RTMP") || !strcmp(protocol, "RTMPS"))
			return "rtmp_output";
		if (!strcmp(protocol, "SRT") || !strcmp(protocol, "RIST"))
			return "ffmpeg_mpegts_muxer";
	}
	return obs_service_get_preferred_output_type(service);
}

QString describe(long long code, const QString &detail)
{
	QString text;
	switch (code) {
	case OBS_OUTPUT_BAD_PATH:
		text = QStringLiteral("the server address isn't valid");
		break;
	case OBS_OUTPUT_CONNECT_FAILED:
		text = QStringLiteral("couldn't connect to the server");
		break;
	case OBS_OUTPUT_INVALID_STREAM:
		text = QStringLiteral("the server didn't accept the stream key or channel");
		break;
	case OBS_OUTPUT_DISCONNECTED:
		text = QStringLiteral("the connection was lost");
		break;
	case OBS_OUTPUT_ENCODE_ERROR:
		text = QStringLiteral("an encoding error happened");
		break;
	default:
		text = QStringLiteral("an error happened");
		break;
	}
	if (!detail.isEmpty())
		text += QStringLiteral(" (") + detail + QLatin1Char(')');
	return text;
}

/* Runs on the GUI thread after an extra output has stopped; releasing it here, never inside libobs' own signal. */
void finish(Extra *raw, long long code, const QString &detail)
{
	std::unique_ptr<Extra> owned;
	{
		State &st = state();
		std::lock_guard<std::mutex> lock(st.mutex);
		auto it = std::find_if(st.running.begin(), st.running.end(), [raw](const std::unique_ptr<Extra> &e) { return e.get() == raw; });
		if (it == st.running.end())
			return;
		owned = std::move(*it);
		st.running.erase(it);
	}
	blog(LOG_INFO, "HopCast: extra destination '%s' stopped (code %lld)", qUtf8Printable(owned->name), code);
	if (code != OBS_OUTPUT_SUCCESS && owned->notify) {
		QString text = QStringLiteral("%1 stopped: %2.").arg(owned->name, describe(code, detail));
		// What the connection did, so a refused key can be told from a refused address (keys are hidden).
		QStringList seen = hopcast::recentConnectionLogSince(owned->startedMs, 3);
		for (QString &line : seen)
			line = line.mid(line.indexOf(QLatin1Char(' '), 24) + 1).left(120);
		if (!seen.isEmpty())
			text += QStringLiteral(" What HopCast saw: ") + seen.join(QStringLiteral(" | "));
		owned->notify(text);
	}
}

void onStop(void *data, calldata_t *params)
{
	auto *extra = static_cast<Extra *>(data);
	const long long code = calldata_int(params, "code");
	const char *lastError = calldata_string(params, "last_error");
	const QString detail = lastError ? QString::fromUtf8(lastError) : QString();
	QMetaObject::invokeMethod(qApp, [extra, code, detail]() { finish(extra, code, detail); }, Qt::QueuedConnection);
}

} // namespace

void setExtraDestinations(bool vertical, std::vector<Destination> list)
{
	State &st = state();
	std::lock_guard<std::mutex> lock(st.mutex);
	st.pending[vertical ? 1 : 0] = std::move(list);
}

void startExtraDestinations(bool vertical, obs_output_t *mainOutput, int maxRetries, int retryDelaySec, Notify notify)
{
	State &st = state();
	std::vector<Destination> list;
	{
		std::lock_guard<std::mutex> lock(st.mutex);
		list = std::move(st.pending[vertical ? 1 : 0]);
		st.pending[vertical ? 1 : 0].clear();
	}
	if (list.empty())
		return;
	if (!mainOutput || !obs_output_active(mainOutput)) {
		blog(LOG_WARNING, "HopCast: main stream isn't running; not starting %d extra destination(s)", static_cast<int>(list.size()));
		return;
	}

	obs_encoder_t *video = obs_output_get_video_encoder(mainOutput);
	obs_encoder_t *audio = obs_output_get_audio_encoder(mainOutput, 0);
	if (!video || !audio) {
		blog(LOG_WARNING, "HopCast: main stream has no encoders to share; not starting extra destinations");
		if (notify)
			notify(QStringLiteral("The extra destinations couldn't start (no encoder to share)."));
		return;
	}
	OBSDataAutoRelease mainSettings = obs_output_get_settings(mainOutput);

	int index = 0;
	for (const Destination &d : list) {
		++index;
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, "server", qUtf8Printable(d.server));
		obs_data_set_string(settings, "key", qUtf8Printable(d.key));
		if (!d.user.isEmpty() && !d.password.isEmpty()) {
			obs_data_set_bool(settings, "use_auth", true);
			obs_data_set_string(settings, "username", qUtf8Printable(d.user));
			obs_data_set_string(settings, "password", qUtf8Printable(d.password));
		}
		const QByteArray serviceName = QStringLiteral("hopcast_extra_service_%1").arg(index).toUtf8();
		OBSServiceAutoRelease service = obs_service_create("rtmp_custom", serviceName.constData(), settings, nullptr);
		if (!service) {
			blog(LOG_WARNING, "HopCast: couldn't create a service for '%s'", qUtf8Printable(d.name));
			if (notify)
				notify(QStringLiteral("%1 couldn't start: its server address isn't valid.").arg(d.name));
			continue;
		}

		const char *type = outputTypeFor(service);
		if (!type || obs_get_output_flags(type) == 0) {
			blog(LOG_WARNING, "HopCast: no output type for '%s'", qUtf8Printable(d.name));
			if (notify)
				notify(QStringLiteral("%1 couldn't start: this kind of server address isn't supported.").arg(d.name));
			continue;
		}

		const QByteArray outputName = QStringLiteral("hopcast_extra_output_%1").arg(index).toUtf8();
		OBSOutputAutoRelease output = obs_output_create(type, outputName.constData(), mainSettings, nullptr);
		if (!output) {
			blog(LOG_WARNING, "HopCast: couldn't create an output for '%s'", qUtf8Printable(d.name));
			if (notify)
				notify(QStringLiteral("%1 couldn't start.").arg(d.name));
			continue;
		}
		obs_output_set_video_encoder(output, video);
		obs_output_set_audio_encoder(output, audio, 0);
		obs_output_set_service(output, service);
		obs_output_set_reconnect_settings(output, maxRetries, retryDelaySec);

		auto extra = std::make_unique<Extra>();
		extra->name = d.name;
		extra->startedMs = hopcast::nowMs();
		extra->notify = notify;
		extra->service = service.Get();
		extra->output = output.Get();
		extra->stopSignal.Connect(obs_output_get_signal_handler(extra->output), "stop", onStop, extra.get());
		Extra *raw = extra.get();
		{
			std::lock_guard<std::mutex> lock(st.mutex);
			st.running.push_back(std::move(extra));
		}

		if (obs_output_start(raw->output)) {
			blog(LOG_INFO, "HopCast: extra destination '%s' started (%s)", qUtf8Printable(d.name), type);
			continue;
		}

		const char *error = obs_output_get_last_error(raw->output);
		const QString detail = error ? QString::fromUtf8(error) : QString();
		blog(LOG_WARNING, "HopCast: extra destination '%s' failed to start", qUtf8Printable(d.name));
		std::unique_ptr<Extra> failed;
		{
			std::lock_guard<std::mutex> lock(st.mutex);
			auto it = std::find_if(st.running.begin(), st.running.end(), [raw](const std::unique_ptr<Extra> &e) { return e.get() == raw; });
			if (it != st.running.end()) {
				failed = std::move(*it);
				st.running.erase(it);
			}
		}
		if (notify)
			notify(QStringLiteral("%1 couldn't start: %2.").arg(d.name, describe(OBS_OUTPUT_ERROR, detail)));
	}
}

void stopExtraDestinations()
{
	State &st = state();
	std::vector<OBSOutput> outputs;
	{
		std::lock_guard<std::mutex> lock(st.mutex);
		st.pending[0].clear();
		st.pending[1].clear();
		for (const auto &e : st.running)
			outputs.push_back(e->output);
	}
	for (OBSOutput &output : outputs)
		obs_output_stop(output);
}

} // namespace hopcast
