// HopCast: audio-only recording (MP3). GPL-2.0-or-later.
//
// Taps the same audio the normal recording uses (the mixer's Track 1), makes 16-bit stereo of it, and gives it to a small
// worker thread that encodes MP3 with Shine and writes the file. The audio thread itself only copies the samples into a
// queue, so a slow disk or a busy computer can't make the sound stutter in the stream or the game.
#include "HopCastAudioRecord.h"
#include "HopCastMp3.h"

#include <obs.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSettings>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

namespace hopcast {

namespace {

constexpr int kBitrateKbps = 192;
constexpr size_t kMaxQueuedChunks = 4000; // about 85 seconds of audio if the disk stalls; after that chunks are dropped

struct Recorder {
	std::mutex m;
	std::condition_variable cv;
	std::deque<std::vector<int16_t>> queue;
	std::thread worker;
	QFile file;
	Mp3Writer writer;
	QString path;
	audio_t *audio = nullptr;
	bool running = false;   // between start and stop
	bool finishing = false; // the worker should drain the queue and end the file
	std::atomic<bool> failed{false};
	std::atomic<uint64_t> dropped{0};
};

Recorder g;

void onAudio(void *, size_t, struct audio_data *data)
{
	if (!data || !data->data[0] || data->frames == 0)
		return;
	const int16_t *samples = reinterpret_cast<const int16_t *>(data->data[0]);
	std::vector<int16_t> copy(samples, samples + static_cast<size_t>(data->frames) * 2);
	{
		std::lock_guard<std::mutex> lock(g.m);
		if (g.queue.size() >= kMaxQueuedChunks) {
			g.dropped++;
			return;
		}
		g.queue.push_back(std::move(copy));
	}
	g.cv.notify_one();
}

void workerMain()
{
	for (;;) {
		std::vector<int16_t> chunk;
		{
			std::unique_lock<std::mutex> lock(g.m);
			g.cv.wait(lock, [] { return !g.queue.empty() || g.finishing; });
			if (g.queue.empty())
				break; // finishing, and everything has been written
			chunk = std::move(g.queue.front());
			g.queue.pop_front();
		}
		if (!g.failed && !g.writer.write(chunk.data(), chunk.size() / 2))
			g.failed = true;
	}
	if (!g.writer.finish())
		g.failed = true;
	g.file.close();
}

QString freeFileName(const QString &folder)
{
	const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH-mm-ss"));
	QString base = QDir(folder).filePath(QStringLiteral("HopCast audio ") + stamp);
	QString candidate = base + QStringLiteral(".mp3");
	for (int n = 2; QFile::exists(candidate); ++n)
		candidate = base + QStringLiteral(" (%1).mp3").arg(n);
	return candidate;
}

QSettings openSettings()
{
	return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("HopCast"), QStringLiteral("Recording"));
}

} // namespace

bool audioOnlyMode()
{
	return openSettings().value(QStringLiteral("audioOnly"), false).toBool();
}

void setAudioOnlyMode(bool on)
{
	QSettings s = openSettings();
	s.setValue(QStringLiteral("audioOnly"), on);
	s.sync();
}

bool audioRecording()
{
	return g.running;
}

bool startAudioRecording(const QString &folder, QString *path, QString *error)
{
	auto fail = [&](const QString &why) {
		if (error)
			*error = why;
		return false;
	};
	if (g.running)
		return fail(QStringLiteral("An audio recording is already running."));

	audio_t *audio = obs_get_audio();
	const audio_output_info *info = audio ? audio_output_get_info(audio) : nullptr;
	if (!audio || !info)
		return fail(QStringLiteral("HopCast's audio isn't running yet."));
	const int rate = static_cast<int>(info->samples_per_sec);

	if (!QDir().mkpath(folder))
		return fail(QStringLiteral("The recording folder can't be created: ") + folder);
	g.path = freeFileName(folder);
	g.file.setFileName(g.path);
	if (!g.file.open(QIODevice::WriteOnly))
		return fail(QStringLiteral("The file can't be written: ") + g.path + QStringLiteral(" (") + g.file.errorString() + QLatin1Char(')'));

	const bool started = g.writer.start(rate, 2, kBitrateKbps, [](const unsigned char *data, int size) {
		return g.file.write(reinterpret_cast<const char *>(data), size) == size;
	});
	if (!started) {
		g.file.close();
		g.file.remove();
		return fail(QStringLiteral("The MP3 encoder doesn't support this audio setting (%1 Hz). Use 44.1 or 48 kHz in Settings > Audio.").arg(rate));
	}

	{
		std::lock_guard<std::mutex> lock(g.m);
		g.queue.clear();
		g.finishing = false;
	}
	g.failed = false;
	g.dropped = 0;

	audio_convert_info conversion = {};
	conversion.samples_per_sec = info->samples_per_sec;
	conversion.format = AUDIO_FORMAT_16BIT;
	conversion.speakers = SPEAKERS_STEREO;
	conversion.allow_clipping = false;

	g.worker = std::thread(workerMain);
	if (!audio_output_connect(audio, 0, &conversion, onAudio, nullptr)) {
		{
			std::lock_guard<std::mutex> lock(g.m);
			g.finishing = true;
		}
		g.cv.notify_one();
		g.worker.join();
		g.file.remove();
		return fail(QStringLiteral("HopCast couldn't connect to its audio."));
	}
	g.audio = audio;
	g.running = true;

	static bool quitHooked = false;
	if (!quitHooked) {
		quitHooked = true;
		// closing HopCast while recording must still leave a finished, playable file
		QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, []() { stopAudioRecording(); });
	}
	if (path)
		*path = g.path;
	return true;
}

QString stopAudioRecording(QString *error)
{
	if (!g.running)
		return QString();
	audio_output_disconnect(g.audio, 0, onAudio, nullptr);
	{
		std::lock_guard<std::mutex> lock(g.m);
		g.finishing = true;
	}
	g.cv.notify_one();
	if (g.worker.joinable())
		g.worker.join();
	g.running = false;
	g.audio = nullptr;

	if (error) {
		error->clear();
		if (g.failed)
			*error = QStringLiteral("The disk stopped taking data, so the file ends early.");
		else if (g.dropped)
			*error = QStringLiteral("The computer was too busy for a moment and a little audio was skipped.");
	}
	return g.path;
}

} // namespace hopcast
