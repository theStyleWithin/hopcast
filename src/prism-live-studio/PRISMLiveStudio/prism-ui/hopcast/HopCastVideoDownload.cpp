// HopCast: download a video from a link (tray menu and Tools menu). GPL-2.0-or-later.
//
// How it works (the same idea Video Downloader by Unrud uses, written fresh here): the real work is done by yt-dlp, which
// knows how to read the links of thousands of sites. HopCast starts it as its own program, reads the progress lines it
// prints, and shows them. yt-dlp writes into a scratch folder first and only moves the finished file into your folder, so
// a cancelled or failed download leaves nothing half-done behind.
#include "HopCastVideoDownload.h"

#include <QApplication>
#include <QGuiApplication>
#include <memory>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#ifndef Q_OS_WIN
#include <csignal>
#include <unistd.h>
#endif

namespace {

constexpr int kQualities = 5;

struct Job {
	QString url;
	QString folder;
	QString scratch;
	int quality = 0; // 0 best, 1 up to 1080p, 2 up to 720p, 3 up to 480p, 4 sound only (MP3)
	int container = 0; // 0 the site's own (often WebM), 1 MP4, 2 MKV
	bool playlist = false;
	bool haveFfmpeg = false;
};

// The command line for one download. Everything is passed as separate arguments (no shell), the link comes after "--"
// so it can never be read as an option, and the user's own yt-dlp config file is ignored so it can't change what runs.
QStringList buildArgs(const Job &j)
{
	QStringList a;
	a << "--ignore-config" << "--no-quiet" << "--progress" << "--newline" << "--color" << "no_color";
	a << (j.playlist ? "--yes-playlist" : "--no-playlist");
	a << "--no-mtime" << "--retries" << "10" << "--fragment-retries" << "10" << "--socket-timeout" << "30";
	a << "-P" << j.folder << "-P" << (QStringLiteral("temp:") + j.scratch);
	a << "-o"
	  << (j.playlist ? QStringLiteral("%(playlist_title|Playlist)s/%(playlist_index|0)03d %(title).140B [%(id)s].%(ext)s")
			 : QStringLiteral("%(title).150B [%(id)s].%(ext)s"));
	a << "--progress-template"
	  << "download:HOPPROG %(progress.downloaded_bytes)s %(progress.total_bytes)s %(progress.total_bytes_estimate)s %(progress.speed)s %(progress.eta)s";
	a << "--print" << "after_move:HOPFILE %(filepath)s";

	static const int heights[] = {0, 1080, 720, 480};
	if (j.quality == 4) {
		if (j.haveFfmpeg)
			a << "-x" << "--audio-format" << "mp3" << "--audio-quality" << "192K";
		else
			a << "-f" << "ba/b"; // no ffmpeg to convert with: keep the original sound file
	} else {
		const int h = heights[j.quality];
		QString f;
		if (j.haveFfmpeg)
			f = h ? QStringLiteral("bv*[height<=%1]+ba/b[height<=%1]").arg(h) : QStringLiteral("bv*+ba/b");
		else if (j.container == 1) // no ffmpeg to convert with: at least ask for a stream that already is an MP4
			f = h ? QStringLiteral("b[height<=%1][ext=mp4]/b[height<=%1]/b").arg(h) : QStringLiteral("b[ext=mp4]/b");
		else
			f = h ? QStringLiteral("b[height<=%1]/b").arg(h) : QStringLiteral("b"); // one ready-made file, no joining needed
		a << "-f" << f;
		if (j.haveFfmpeg && j.container != 0) {
			const QString box = j.container == 1 ? QStringLiteral("mp4") : QStringLiteral("mkv");
			// MP4 is picked to play everywhere, so at the same size prefer the codecs every player knows (H.264 and AAC)
			if (j.container == 1)
				a << "-S" << "res,vcodec:h264,acodec:m4a";
			a << "--merge-output-format" << box << "--remux-video" << box;
		}
	}
	a << "--" << j.url;
	return a;
}

QString toolDir()
{
	QString d = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
	if (d.isEmpty())
		d = QDir::homePath();
	d += QStringLiteral("/HopCast/tools");
	QDir().mkpath(d);
	return d;
}

QString ownName()
{
#ifdef Q_OS_WIN
	return QStringLiteral("yt-dlp.exe");
#else
	return QStringLiteral("yt-dlp");
#endif
}

// The file name of the official stand-alone build for this computer, or empty if there isn't one.
QString assetName()
{
	const QString arch = QSysInfo::currentCpuArchitecture();
#ifdef Q_OS_WIN
	if (arch == QLatin1String("arm64"))
		return QStringLiteral("yt-dlp_arm64.exe");
	if (arch == QLatin1String("x86_64"))
		return QStringLiteral("yt-dlp.exe");
	return QStringLiteral("yt-dlp_x86.exe");
#else
	if (arch == QLatin1String("x86_64"))
		return QStringLiteral("yt-dlp_linux");
	if (arch == QLatin1String("arm64") || arch == QLatin1String("aarch64"))
		return QStringLiteral("yt-dlp_linux_aarch64");
	return QString();
#endif
}

struct Tool {
	QString path;
	bool own = false; // HopCast's own copy, which it can update
};

Tool findYtDlp()
{
	const QString own = toolDir() + QLatin1Char('/') + ownName();
	if (QFileInfo(own).isExecutable())
		return {own, true};
	const QString system = QStandardPaths::findExecutable(QStringLiteral("yt-dlp"));
	if (!system.isEmpty())
		return {system, false};
	return {};
}

QString findFfmpeg()
{
	return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
}

// Without the AppImage's own library paths, so yt-dlp and ffmpeg load the system's libraries instead of ours.
QProcessEnvironment cleanEnv()
{
	QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	for (const char *v : {"LD_LIBRARY_PATH", "LD_PRELOAD", "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QML2_IMPORT_PATH",
			      "QT_QPA_PLATFORM", "PYTHONHOME", "PYTHONPATH"})
		env.remove(QString::fromLatin1(v));
	env.insert(QStringLiteral("NO_COLOR"), QStringLiteral("1"));
	env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
	env.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
	return env;
}

QString sizeText(double bytes)
{
	const char *unit[] = {"B", "KB", "MB", "GB"};
	int u = 0;
	while (bytes >= 1024.0 && u < 3) {
		bytes /= 1024.0;
		++u;
	}
	return QString::number(bytes, 'f', u ? 1 : 0) + QLatin1Char(' ') + QLatin1String(unit[u]);
}

QString timeText(double seconds)
{
	const int s = int(seconds);
	if (s >= 3600)
		return QStringLiteral("%1 h %2 min").arg(s / 3600).arg((s / 60) % 60);
	if (s >= 60)
		return QStringLiteral("%1 min %2 s").arg(s / 60).arg(s % 60);
	return QStringLiteral("%1 s").arg(s);
}

// Turns yt-dlp's last error into a plain sentence.
QString plainError(const QString &raw)
{
	const QString r = raw.toLower();
	if (r.contains(QLatin1String("sign in")) || r.contains(QLatin1String("log in")) || r.contains(QLatin1String("login")) ||
	    r.contains(QLatin1String("private video")) || r.contains(QLatin1String("members-only")) || r.contains(QLatin1String("password")))
		return QStringLiteral("That video needs a login, and HopCast doesn't sign in to sites.");
	if (r.contains(QLatin1String("unsupported url")))
		return QStringLiteral("yt-dlp doesn't know how to get a video from that link.");
	if (r.contains(QLatin1String("video unavailable")) || r.contains(QLatin1String("this video is not available")) ||
	    r.contains(QLatin1String("has been removed")))
		return QStringLiteral("That video isn't available any more.");
	if (r.contains(QLatin1String("http error 404")))
		return QStringLiteral("The site says that page doesn't exist. Check the link.");
	if (r.contains(QLatin1String("unable to download webpage")) || r.contains(QLatin1String("name or service not known")) ||
	    r.contains(QLatin1String("network is unreachable")) || r.contains(QLatin1String("timed out")))
		return QStringLiteral("Couldn't reach the site. Check the link and your connection.");
	if (r.contains(QLatin1String("http error 403")) || r.contains(QLatin1String("http error 429")))
		return QStringLiteral("The site refused the download. Updating yt-dlp often fixes this.");
	return raw;
}

class VideoDownloadDialog : public QDialog {
public:
	explicit VideoDownloadDialog(QWidget *parent) : QDialog(parent), settings_(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("HopCast"), QStringLiteral("VideoDownload"))
	{
		setWindowTitle(QStringLiteral("Download a video"));
		setAttribute(Qt::WA_DeleteOnClose);
		setMinimumWidth(560);

		auto *root = new QVBoxLayout(this);

		root->addWidget(new QLabel(QStringLiteral("Link to the video")));
		url_ = new QLineEdit;
		url_->setObjectName(QStringLiteral("url"));
		url_->setPlaceholderText(QStringLiteral("https://..."));
		root->addWidget(url_);

		auto *row = new QHBoxLayout;
		row->addWidget(new QLabel(QStringLiteral("Quality")));
		quality_ = new QComboBox;
		quality_->setObjectName(QStringLiteral("quality"));
		quality_->addItems({QStringLiteral("Best available"), QStringLiteral("Up to 1080p"), QStringLiteral("Up to 720p"),
				    QStringLiteral("Up to 480p"), QStringLiteral("Sound only (MP3)")});
		quality_->setCurrentIndex(qBound(0, settings_.value(QStringLiteral("quality"), 0).toInt(), kQualities - 1));
		row->addWidget(quality_);
		row->addWidget(new QLabel(QStringLiteral("Save as")));
		container_ = new QComboBox;
		container_->setObjectName(QStringLiteral("container"));
		container_->addItems({QStringLiteral("Original (often WebM)"), QStringLiteral("MP4"), QStringLiteral("MKV")});
		container_->setToolTip(QStringLiteral("The file type of the finished video. MP4 and MKV need ffmpeg to convert."));
		container_->setCurrentIndex(qBound(0, settings_.value(QStringLiteral("container"), 0).toInt(), 2));
		row->addWidget(container_);
		row->addStretch(1);
		// saved the moment it changes, so a choice survives even if HopCast is closed with this window open
		connect(container_, &QComboBox::currentIndexChanged, this, [this](int i) { settings_.setValue(QStringLiteral("container"), i); settings_.sync(); });
		connect(quality_, &QComboBox::currentIndexChanged, this, [this](int i) { settings_.setValue(QStringLiteral("quality"), i); settings_.sync(); });
		playlist_ = new QCheckBox(QStringLiteral("If the link is a playlist, download all of it"));
		playlist_->setObjectName(QStringLiteral("playlist"));
		playlist_->setChecked(settings_.value(QStringLiteral("playlist"), false).toBool());
		row->addWidget(playlist_);
		root->addLayout(row);

		auto *where = new QHBoxLayout;
		where->addWidget(new QLabel(QStringLiteral("Save to")));
		folder_ = new QLineEdit;
		folder_->setObjectName(QStringLiteral("folder"));
		QString def = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
		if (def.isEmpty())
			def = QDir::homePath();
		folder_->setText(settings_.value(QStringLiteral("folder"), def + QStringLiteral("/HopCast")).toString());
		where->addWidget(folder_, 1);
		auto *browse = new QPushButton(QStringLiteral("Browse..."));
		connect(browse, &QPushButton::clicked, this, [this]() {
			const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("Save videos to"), folder_->text());
			if (!d.isEmpty())
				folder_->setText(d);
		});
		where->addWidget(browse);
		root->addLayout(where);

		tools_ = new QLabel;
		tools_->setWordWrap(true);
		tools_->setObjectName(QStringLiteral("tools"));
		root->addWidget(tools_);

		bar_ = new QProgressBar;
		bar_->setRange(0, 1000);
		bar_->setValue(0);
		bar_->setTextVisible(false);
		root->addWidget(bar_);

		status_ = new QLabel(QStringLiteral("Paste a link and press Download."));
		status_->setObjectName(QStringLiteral("status"));
		status_->setWordWrap(true);
		status_->setTextInteractionFlags(Qt::TextSelectableByMouse);
		root->addWidget(status_);

		log_ = new QPlainTextEdit;
		log_->setReadOnly(true);
		log_->setMaximumBlockCount(400);
		log_->setPlaceholderText(QStringLiteral("What yt-dlp said shows here."));
		log_->setFixedHeight(110);
		root->addWidget(log_);

		auto *buttons = new QHBoxLayout;
		get_ = new QPushButton;
		get_->setObjectName(QStringLiteral("get"));
		connect(get_, &QPushButton::clicked, this, [this]() { toolButton(); });
		buttons->addWidget(get_);
		buttons->addStretch(1);
		show_ = new QPushButton(QStringLiteral("Show in folder"));
		show_->setObjectName(QStringLiteral("show"));
		show_->setEnabled(false);
		connect(show_, &QPushButton::clicked, this, [this]() {
			if (!lastFile_.isEmpty())
				QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(lastFile_).absolutePath()));
		});
		buttons->addWidget(show_);
		go_ = new QPushButton(QStringLiteral("Download"));
		go_->setObjectName(QStringLiteral("download"));
		go_->setDefault(true);
		connect(go_, &QPushButton::clicked, this, [this]() { startDownload(); });
		buttons->addWidget(go_);
		cancel_ = new QPushButton(QStringLiteral("Close"));
		cancel_->setObjectName(QStringLiteral("cancel"));
		connect(cancel_, &QPushButton::clicked, this, [this]() {
			if (busy_)
				cancelWork();
			else
				close();
		});
		buttons->addWidget(cancel_);
		root->addLayout(buttons);

		// a link already on the clipboard is a good first guess
		const QString clip = QGuiApplication::clipboard() ? QGuiApplication::clipboard()->text().trimmed() : QString();
		if (looksLikeLink(clip))
			url_->setText(clip);

		refreshTools();
		setBusy(false);
	}

	~VideoDownloadDialog() override { killTree(); }

	// for tests
	QString lastFile() const { return lastFile_; }
	bool isBusy() const { return busy_; }

protected:
	void closeEvent(QCloseEvent *e) override
	{
		cancelWork();
		settings_.setValue(QStringLiteral("folder"), folder_->text());
		settings_.setValue(QStringLiteral("quality"), quality_->currentIndex());
		settings_.setValue(QStringLiteral("container"), container_->currentIndex());
		settings_.setValue(QStringLiteral("playlist"), playlist_->isChecked());
		QDialog::closeEvent(e);
	}

private:
	static bool looksLikeLink(const QString &s)
	{
		const QUrl u(s, QUrl::StrictMode);
		return u.isValid() && (u.scheme() == QLatin1String("http") || u.scheme() == QLatin1String("https")) && !u.host().isEmpty() &&
		       !s.contains(QLatin1Char(' '));
	}

	void say(const QString &text) { status_->setText(text); }

	void note(const QString &line)
	{
		if (!line.trimmed().isEmpty())
			log_->appendPlainText(line);
	}

	void setBusy(bool busy)
	{
		busy_ = busy;
		go_->setEnabled(!busy);
		url_->setEnabled(!busy);
		quality_->setEnabled(!busy);
		container_->setEnabled(!busy);
		playlist_->setEnabled(!busy);
		folder_->setEnabled(!busy);
		get_->setEnabled(!busy && !assetName().isEmpty());
		cancel_->setText(busy ? QStringLiteral("Cancel") : QStringLiteral("Close"));
	}

	void refreshTools()
	{
		const Tool t = findYtDlp();
		const QString ff = findFfmpeg();
		QString text;
		if (t.path.isEmpty()) {
			text = QStringLiteral("yt-dlp (the program that does the downloading) isn't installed. ");
			get_->setText(QStringLiteral("Get yt-dlp"));
			get_->setToolTip(QStringLiteral("Downloads the official yt-dlp release once and checks its checksum before using it."));
			get_->setVisible(!assetName().isEmpty());
			if (assetName().isEmpty())
				text += QStringLiteral("There's no ready-made copy for this computer; install yt-dlp with your package manager.");
			else
				text += QStringLiteral("\"Get yt-dlp\" downloads the official release (about 35 MB) once and checks its checksum first.");
		} else {
			text = QStringLiteral("yt-dlp: ") + t.path;
			get_->setText(QStringLiteral("Update yt-dlp"));
			get_->setToolTip(QStringLiteral("Sites change often; updating fixes most downloads that stop working."));
			get_->setVisible(t.own);
		}
		text += QLatin1Char('\n');
		text += ff.isEmpty() ? QStringLiteral("ffmpeg: not found. Without it HopCast saves one ready-made file (up to about 720p) and keeps sound-only downloads in their original format.")
				     : QStringLiteral("ffmpeg: ") + ff;
		tools_->setText(text);
	}

	void toolButton()
	{
		const Tool t = findYtDlp();
		if (t.path.isEmpty())
			fetchTool();
		else if (t.own)
			updateTool(t.path);
	}

	// ---- getting yt-dlp (once), checking the official checksum before it is ever run

	void fetchTool()
	{
		const QString asset = assetName();
		if (asset.isEmpty())
			return;
		setBusy(true);
		bar_->setValue(0);
		say(QStringLiteral("Getting the official yt-dlp..."));
		const QString base = QStringLiteral("https://github.com/yt-dlp/yt-dlp/releases/latest/download/");

		auto request = [](const QString &u) {
			QNetworkRequest r{QUrl(u)};
			r.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
			r.setTransferTimeout(60000);
			return r;
		};

		QNetworkReply *sums = net_.get(request(base + QStringLiteral("SHA2-256SUMS")));
		connect(sums, &QNetworkReply::finished, this, [this, sums, asset, base, request]() {
			sums->deleteLater();
			if (sums->error() != QNetworkReply::NoError) {
				finishTool(QStringLiteral("Couldn't get the checksum list: ") + sums->errorString());
				return;
			}
			QString expected;
			for (const QString &line : QString::fromUtf8(sums->readAll()).split(QLatin1Char('\n'))) {
				const QStringList parts = line.simplified().split(QLatin1Char(' '));
				if (parts.size() == 2 && parts[1] == asset)
					expected = parts[0].toLower();
			}
			if (expected.size() != 64) {
				finishTool(QStringLiteral("The official checksum list doesn't include ") + asset);
				return;
			}
			QNetworkReply *file = net_.get(request(base + asset));
			connect(file, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
				if (total > 0)
					bar_->setValue(int(got * 1000 / total));
				say(QStringLiteral("Getting yt-dlp: %1%2").arg(sizeText(double(got)), total > 0 ? QStringLiteral(" of ") + sizeText(double(total)) : QString()));
			});
			connect(file, &QNetworkReply::finished, this, [this, file, expected]() {
				file->deleteLater();
				if (file->error() != QNetworkReply::NoError) {
					finishTool(QStringLiteral("Couldn't download yt-dlp: ") + file->errorString());
					return;
				}
				const QByteArray data = file->readAll();
				const QString actual = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
				if (actual != expected) {
					finishTool(QStringLiteral("The downloaded file didn't match the official checksum, so it was thrown away."));
					return;
				}
				const QString target = toolDir() + QLatin1Char('/') + ownName();
				const QString partial = target + QStringLiteral(".part");
				QFile out(partial);
				if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(data) != data.size()) {
					finishTool(QStringLiteral("Couldn't save yt-dlp to ") + target);
					return;
				}
				out.close();
				out.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup |
						   QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther);
				QFile::remove(target);
				if (!QFile::rename(partial, target)) {
					finishTool(QStringLiteral("Couldn't put yt-dlp in place at ") + target);
					return;
				}
				finishTool(QString());
			});
		});
	}

	void finishTool(const QString &error)
	{
		bar_->setValue(error.isEmpty() ? 1000 : 0);
		say(error.isEmpty() ? QStringLiteral("yt-dlp is ready. Paste a link and press Download.") : error);
		setBusy(false);
		refreshTools();
	}

	void updateTool(const QString &exe)
	{
		setBusy(true);
		bar_->setValue(0);
		say(QStringLiteral("Updating yt-dlp..."));
		auto *p = new QProcess(this);
		p->setProcessEnvironment(cleanEnv());
		p->setProcessChannelMode(QProcess::MergedChannels);
		connect(p, &QProcess::readyRead, this, [this, p]() { note(QString::fromUtf8(p->readAll()).trimmed()); });
		connect(p, &QProcess::errorOccurred, this, [this, p](QProcess::ProcessError e) {
			if (e == QProcess::FailedToStart) {
				p->deleteLater();
				finishTool(QStringLiteral("Couldn't run yt-dlp to update it."));
			}
		});
		connect(p, &QProcess::finished, this, [this, p](int code, QProcess::ExitStatus) {
			p->deleteLater();
			finishTool(code == 0 ? QString() : QStringLiteral("The update didn't work (see below). You can also remove ") + toolDir() + QStringLiteral(" and get yt-dlp again."));
			if (code == 0)
				say(QStringLiteral("yt-dlp is up to date."));
		});
		p->start(exe, {QStringLiteral("-U"), QStringLiteral("--ignore-config")});
	}

	// ---- one download

	void startDownload()
	{
		const QString link = url_->text().trimmed();
		if (!looksLikeLink(link)) {
			say(QStringLiteral("That doesn't look like a web link. It should start with http:// or https://"));
			return;
		}
		const Tool t = findYtDlp();
		if (t.path.isEmpty()) {
			say(QStringLiteral("yt-dlp isn't installed yet. Press \"Get yt-dlp\" first."));
			return;
		}
		const QString folder = QDir::cleanPath(folder_->text().trimmed());
		if (folder.isEmpty() || !QDir().mkpath(folder)) {
			say(QStringLiteral("Couldn't use that folder."));
			return;
		}
		scratch_ = std::make_unique<QTemporaryDir>(QDir::tempPath() + QStringLiteral("/hopcast-video-XXXXXX"));
		if (!scratch_->isValid()) {
			say(QStringLiteral("Couldn't make a scratch folder."));
			return;
		}

		Job job;
		job.url = link;
		job.folder = folder;
		job.scratch = scratch_->path();
		job.quality = quality_->currentIndex();
		job.container = job.quality == 4 ? 0 : container_->currentIndex();
		job.playlist = playlist_->isChecked();
		job.haveFfmpeg = !findFfmpeg().isEmpty();

		files_.clear();
		lastFile_.clear();
		cancelled_ = false;
		lastError_.clear();
		buffer_.clear();
		log_->clear();
		show_->setEnabled(false);
		bar_->setValue(0);
		say(QStringLiteral("Starting..."));
		setBusy(true);

		proc_ = new QProcess(this);
		proc_->setProcessEnvironment(cleanEnv());
		proc_->setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
		proc_->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) { a->flags |= 0x08000000; }); // no console window
#else
		proc_->setChildProcessModifier([]() { ::setsid(); }); // its own process group, so Cancel can stop yt-dlp and ffmpeg together
#endif
		connect(proc_, &QProcess::readyRead, this, [this]() { readOutput(); });
		connect(proc_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
			if (e == QProcess::FailedToStart)
				workEnded(-1);
		});
		connect(proc_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus st) { workEnded(st == QProcess::NormalExit ? code : -1); });
		proc_->start(t.path, buildArgs(job));
	}

	void readOutput()
	{
		buffer_ += proc_->readAll();
		int cut;
		while ((cut = buffer_.indexOf('\n')) >= 0 || (cut = buffer_.indexOf('\r')) >= 0) {
			const QString line = QString::fromUtf8(buffer_.left(cut)).trimmed();
			buffer_.remove(0, cut + 1);
			handleLine(line);
		}
	}

	void handleLine(const QString &line)
	{
		if (line.isEmpty())
			return;
		if (line.startsWith(QLatin1String("HOPPROG "))) {
			const QStringList f = line.mid(8).split(QLatin1Char(' '));
			if (f.size() < 5)
				return;
			auto num = [](const QString &s) { bool ok = false; const double v = s.toDouble(&ok); return ok ? v : -1.0; };
			const double got = num(f[0]);
			const double total = num(f[1]) > 0 ? num(f[1]) : num(f[2]);
			const double speed = num(f[3]);
			const double eta = num(f[4]);
			if (total > 0 && got >= 0)
				bar_->setValue(int(qBound(0.0, got / total, 1.0) * 1000));
			QString text = QStringLiteral("Downloading: ") + sizeText(qMax(0.0, got));
			if (total > 0)
				text += QStringLiteral(" of ") + sizeText(total);
			if (speed > 0)
				text += QStringLiteral(", ") + sizeText(speed) + QStringLiteral("/s");
			if (eta >= 0 && total > 0)
				text += QStringLiteral(", ") + timeText(eta) + QStringLiteral(" left");
			say(text);
			return;
		}
		if (line.startsWith(QLatin1String("HOPFILE "))) {
			lastFile_ = line.mid(8);
			files_ << lastFile_;
			note(QStringLiteral("Saved: ") + lastFile_);
			return;
		}
		note(line);
		if (line.startsWith(QLatin1String("ERROR:")))
			lastError_ = line.mid(6).trimmed();
		else if (line.startsWith(QLatin1String("[Merger]")))
			say(QStringLiteral("Putting the picture and sound together..."));
		else if (line.startsWith(QLatin1String("[ExtractAudio]")))
			say(QStringLiteral("Making the MP3..."));
		else if (line.startsWith(QLatin1String("[download] Downloading item")))
			say(line.mid(11));
	}

	void workEnded(int code)
	{
		if (!busy_ || !proc_)
			return;
		if (!buffer_.isEmpty()) {
			handleLine(QString::fromUtf8(buffer_).trimmed());
			buffer_.clear();
		}
		proc_->deleteLater();
		proc_ = nullptr;
		scratch_.reset(); // whatever was half done is removed with the scratch folder
		if (cancelled_) {
			bar_->setValue(0);
			say(QStringLiteral("Cancelled. Nothing was saved."));
		} else if (code == 0 && !files_.isEmpty()) {
			bar_->setValue(1000);
			say(files_.size() == 1 ? QStringLiteral("Saved: ") + files_.first()
					       : QStringLiteral("Saved %1 files in %2").arg(files_.size()).arg(QFileInfo(lastFile_).absolutePath()));
			show_->setEnabled(true);
		} else {
			bar_->setValue(0);
			const QString why = lastError_.isEmpty() ? QStringLiteral("yt-dlp stopped without saving anything (see below).") : plainError(lastError_);
			say(QStringLiteral("Couldn't download: ") + why);
		}
		setBusy(false);
		refreshTools();
	}

	void cancelWork()
	{
		if (!busy_ || !proc_)
			return;
		cancelled_ = true;
		killTree();
	}

	void killTree()
	{
		if (!proc_ || proc_->state() == QProcess::NotRunning)
			return;
#ifdef Q_OS_WIN
		QProcess::execute(QStringLiteral("taskkill"), {QStringLiteral("/F"), QStringLiteral("/T"), QStringLiteral("/PID"), QString::number(proc_->processId())});
		proc_->kill();
#else
		const pid_t pid = pid_t(proc_->processId());
		if (pid > 1) {
			::kill(-pid, SIGTERM);
			QPointer<QProcess> guard(proc_);
			QTimer::singleShot(3000, qApp, [guard, pid]() {
				if (guard && guard->state() != QProcess::NotRunning)
					::kill(-pid, SIGKILL);
			});
		}
#endif
	}

	QSettings settings_;
	QLineEdit *url_ = nullptr;
	QComboBox *quality_ = nullptr;
	QCheckBox *playlist_ = nullptr;
	QComboBox *container_ = nullptr;
	QLineEdit *folder_ = nullptr;
	QLabel *tools_ = nullptr;
	QProgressBar *bar_ = nullptr;
	QLabel *status_ = nullptr;
	QPlainTextEdit *log_ = nullptr;
	QPushButton *get_ = nullptr;
	QPushButton *show_ = nullptr;
	QPushButton *go_ = nullptr;
	QPushButton *cancel_ = nullptr;
	QNetworkAccessManager net_;
	QProcess *proc_ = nullptr;
	std::unique_ptr<QTemporaryDir> scratch_;
	QByteArray buffer_;
	QStringList files_;
	QString lastFile_;
	QString lastError_;
	bool busy_ = false;
	bool cancelled_ = false;
};

QPointer<VideoDownloadDialog> g_open;

} // namespace

namespace hopcast {

void showVideoDownloader(QWidget *parent)
{
	if (g_open) {
		g_open->show();
		g_open->raise();
		g_open->activateWindow();
		return;
	}
	g_open = new VideoDownloadDialog(parent);
	g_open->show();
	g_open->raise();
	g_open->activateWindow();
}

} // namespace hopcast
