// HopCast: pictures for channels (PNG, JPG, WebP or GIF, animated too), kept in HopCast's own folder.
// GPL-2.0-or-later.
#pragma once

#include <QString>

class QWidget;

namespace hopcast {

// Asks for a picture file and checks it can be used (readable, 20 MB or smaller). False if cancelled or unusable
// (an unusable file is explained to the user).
bool pickChannelPicture(QWidget *parent, QString &chosenFile);

// Copies a picture into HopCast's channel-icons folder under a new name; returns the new path (empty on failure).
QString storeChannelPicture(const QString &sourceFile);

// Deletes a picture, but only one inside HopCast's own channel-icons folder.
void removeChannelPicture(const QString &path);

} // namespace hopcast
