// HopCast: import an OBS profile. GPL-2.0-or-later.
#include "HopCastProfileImport.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QUrl>
#include <private/qzipreader_p.h>

#include "ChannelCommonFunctions.h"
#include "PLSAlertView.h"
#include "PLSChannelDataAPI.h"
#include "libutils-api.h"
#include "pls-channel-const.h"

using namespace ChannelData;

namespace {

struct ObsProfile {
	QString name;
	QString server, key, user, password;
	int width = 0, height = 0, fps = 0, bitrate = 0, keyint = 0;
	QByteArray basicIni, streamEncoder;
};

const QStringList kFiles = {QStringLiteral("basic.ini"), QStringLiteral("service.json"),
			    QStringLiteral("streamEncoder.json")};

// "Section" -> "Key" -> value, from OBS's basic.ini (keys may be indented).
QMap<QString, QMap<QString, QString>> parseIni(const QByteArray &data)
{
	QMap<QString, QMap<QString, QString>> out;
	QString section;
	for (const QByteArray &raw : data.split('\n')) {
		const QString line = QString::fromUtf8(raw).trimmed();
		if (line.isEmpty() || line.startsWith(QLatin1Char(';')))
			continue;
		if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
			section = line.mid(1, line.size() - 2);
			continue;
		}
		const int eq = line.indexOf(QLatin1Char('='));
		if (eq > 0)
			out[section][line.left(eq).trimmed()] = line.mid(eq + 1).trimmed();
	}
	return out;
}

// The profile's files, from a zip (only the three known names, wherever they sit inside it: a zip may
// hold them in a folder or loose) or from a folder.
bool readFiles(const QString &path, QMap<QString, QByteArray> &files, QString &baseName, QString &error)
{
	const QFileInfo info(path);
	if (info.isDir()) {
		baseName = info.fileName();
		for (const QString &f : kFiles) {
			QFile file(info.filePath() + QLatin1Char('/') + f);
			if (file.open(QIODevice::ReadOnly))
				files[f] = file.read(1 << 20);
		}
		return true;
	}

	baseName = info.completeBaseName();
	QZipReader zip(path, QIODevice::ReadOnly);
	if (!zip.isReadable()) {
		error = QStringLiteral("That file isn't a readable .zip.");
		return false;
	}
	for (const QZipReader::FileInfo &fi : zip.fileInfoList()) {
		if (!fi.isFile || fi.size > (1 << 20))
			continue;
		const QString leaf = QFileInfo(fi.filePath).fileName();
		if (kFiles.contains(leaf) && !files.contains(leaf)) {
			files[leaf] = zip.fileData(fi.filePath);
			if (files.size() == 1 && baseName == info.completeBaseName()) {
				const QString dir = QFileInfo(fi.filePath).path();
				if (dir != QLatin1String(".") && !dir.contains(QLatin1Char('/')))
					baseName = dir; // OBS names a profile after its folder
			}
		}
	}
	return true;
}

bool parseProfile(const QMap<QString, QByteArray> &files, const QString &baseName, ObsProfile &p, QString &error)
{
	if (!files.contains(QStringLiteral("service.json"))) {
		error = QStringLiteral("There's no service.json in it, so there's no server address or stream key to import.");
		return false;
	}
	const QJsonObject root = QJsonDocument::fromJson(files[QStringLiteral("service.json")]).object();
	const QJsonObject s = root.value(QStringLiteral("settings")).toObject();
	p.server = s.value(QStringLiteral("server")).toString().trimmed();
	p.key = s.value(QStringLiteral("key")).toString().trimmed();
	if (s.value(QStringLiteral("use_auth")).toBool()) {
		p.user = s.value(QStringLiteral("username")).toString();
		p.password = s.value(QStringLiteral("password")).toString();
	}
	static const QRegularExpression url(QStringLiteral("^(rtmps?|srt|rist)://"),
					    QRegularExpression::CaseInsensitiveOption);
	if (!url.match(p.server).hasMatch()) {
		error = QStringLiteral("It uses OBS's built-in service list instead of a server address "
				       "(rtmp://, rtmps://, srt:// or rist://), which HopCast doesn't have. "
				       "Add the channel by hand with its server address and stream key.");
		return false;
	}
	if (p.key.isEmpty()) {
		error = QStringLiteral("The profile has no stream key in it.");
		return false;
	}

	const auto ini = parseIni(files.value(QStringLiteral("basic.ini")));
	p.name = ini.value(QStringLiteral("General")).value(QStringLiteral("Name"), baseName);
	if (p.name.isEmpty())
		p.name = baseName;
	const auto video = ini.value(QStringLiteral("Video"));
	p.width = video.value(QStringLiteral("OutputCX")).toInt();
	p.height = video.value(QStringLiteral("OutputCY")).toInt();
	const int num = video.value(QStringLiteral("FPSNum")).toInt();
	const int den = video.value(QStringLiteral("FPSDen"), QStringLiteral("1")).toInt();
	p.fps = num > 0 && den > 0 ? num / den : video.value(QStringLiteral("FPSCommon")).toInt();
	const QJsonObject enc = QJsonDocument::fromJson(files.value(QStringLiteral("streamEncoder.json"))).object();
	p.bitrate = enc.value(QStringLiteral("bitrate")).toInt();
	p.keyint = enc.value(QStringLiteral("keyint_sec")).toInt();
	p.basicIni = files.value(QStringLiteral("basic.ini"));
	p.streamEncoder = files.value(QStringLiteral("streamEncoder.json"));
	return true;
}

QString freeChannelName(const QString &wanted)
{
	const auto &infos = PLSCHANNELS_API->getAllChannelInfoReference();
	auto taken = [&](const QString &n) {
		for (const auto &info : infos)
			if (getInfo(info, g_nickName) == n)
				return true;
		return false;
	};
	QString name = wanted.left(100);
	for (int i = 2; taken(name); ++i)
		name = wanted.left(94) + QStringLiteral(" (%1)").arg(i);
	return name;
}

// Saves basic.ini and streamEncoder.json (not service.json) as a profile, unless one by that name exists.
bool saveProfile(const ObsProfile &p, QString &profileName)
{
	QString dirName = p.name;
	dirName.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
	const QString dir = pls_get_user_path("PRISMLiveStudio/basic/profiles/") + dirName;
	if (p.basicIni.isEmpty() || dirName.isEmpty() || QFileInfo::exists(dir + QStringLiteral("/basic.ini")))
		return false;
	QDir().mkpath(dir);
	QFile ini(dir + QStringLiteral("/basic.ini"));
	if (!ini.open(QIODevice::WriteOnly))
		return false;
	ini.write(p.basicIni);
	ini.close();
	if (!p.streamEncoder.isEmpty()) {
		QFile enc(dir + QStringLiteral("/streamEncoder.json"));
		if (enc.open(QIODevice::WriteOnly))
			enc.write(p.streamEncoder);
	}
	profileName = dirName;
	return true;
}

} // namespace

namespace hopcast {

void importObsProfile(QWidget *parent, const QString &path)
{
	const QString title = QStringLiteral("HopCast");
	QMap<QString, QByteArray> files;
	QString baseName, error;
	ObsProfile p;
	if (!readFiles(path, files, baseName, error) || !parseProfile(files, baseName, p, error)) {
		PLSAlertView::warning(parent, title, QStringLiteral("Couldn't import that profile.\n\n") + error);
		return;
	}

	QVariantMap info = createDefaultChannelInfoMap(SELECT_TYPE, RTMPType);
	const QString name = freeChannelName(p.name);
	info[g_nickName] = name;
	info[g_displayLine1] = name;
	info[g_channelName] = CUSTOM_RTMP;
	info[g_channelRtmpUrl] = p.server;
	info[g_streamKey] = p.key;
	info[g_rtmpUserID] = p.user;
	info[g_password] = p.password;
	info[g_channelStatus] = QVariant::fromValue(Valid);
	PLSCHANNELS_API->addChannelInfo(info);

	QString profileName;
	const bool savedProfile = saveProfile(p, profileName);

	QString msg = QStringLiteral("Added the channel \"%1\" (%2).").arg(name, QUrl(p.server).host());
	if (p.width > 0 && p.fps > 0)
		msg += QStringLiteral("\n\nThe profile asks for %1x%2 at %3 fps%4%5.")
			       .arg(p.width)
			       .arg(p.height)
			       .arg(p.fps)
			       .arg(p.bitrate > 0 ? QStringLiteral(", %1 kbps").arg(p.bitrate) : QString())
			       .arg(p.keyint > 0 ? QStringLiteral(", a keyframe every %1 s").arg(p.keyint) : QString());
	if (savedProfile)
		msg += QStringLiteral("\nIts settings are in the Profile menu as \"%1\".").arg(profileName);
	PLSAlertView::information(parent, title, msg);
}

} // namespace hopcast
