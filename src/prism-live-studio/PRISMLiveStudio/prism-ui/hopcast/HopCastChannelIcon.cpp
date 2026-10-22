// HopCast: pictures for channels. GPL-2.0-or-later.
#include "HopCastChannelIcon.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QUuid>

#include "PLSAlertView.h"
#include "frontend-api.h"

namespace {
constexpr qint64 kMaxBytes = 20 * 1024 * 1024; // keeps animated pictures from being huge

QString folder()
{
	return pls_get_user_path(QStringLiteral("PRISMLiveStudio/user/channel-icons/"));
}
} // namespace

namespace hopcast {

bool pickChannelPicture(QWidget *parent, QString &chosenFile)
{
	const QString file = QFileDialog::getOpenFileName(parent, QStringLiteral("Choose a picture for this channel"), QDir::homePath(),
							  QStringLiteral("Pictures (*.png *.jpg *.jpeg *.gif *.webp *.bmp);;All files (*)"));
	if (file.isEmpty())
		return false;
	QImageReader reader(file);
	if (!reader.canRead() || QFileInfo(file).size() > kMaxBytes) {
		PLSAlertView::warning(parent, QStringLiteral("HopCast"),
				      QStringLiteral("That file can't be used as a picture (it must be a PNG, JPG, WebP or GIF, 20 MB or smaller)."));
		return false;
	}
	chosenFile = file;
	return true;
}

QString storeChannelPicture(const QString &sourceFile)
{
	QDir().mkpath(folder());
	const QString dest = folder() + QUuid::createUuid().toString(QUuid::WithoutBraces) + QLatin1Char('.') + QFileInfo(sourceFile).suffix().toLower();
	return QFile::copy(sourceFile, dest) ? dest : QString();
}

void removeChannelPicture(const QString &path)
{
	if (!path.isEmpty() && path.contains(QStringLiteral("/channel-icons/")))
		QFile::remove(path);
}

} // namespace hopcast
