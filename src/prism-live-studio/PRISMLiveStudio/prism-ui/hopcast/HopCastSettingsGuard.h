// HopCast: keeps the user's settings safe. Automatic snapshots, a check at startup that puts back any
// settings file that went missing, empty or corrupt, and a snapshot before the first run of a new
// version. GPL-2.0-or-later.
#pragma once

#include <QString>
#include <QStringList>

namespace hopcast {

// Before the app reads any settings: applies a restore the user asked for, repairs damaged settings
// files from the newest good snapshot, and snapshots the settings before the first run of a new version.
void settingsGuardBeforeStartup(const QString &configRoot, const QString &version);

// Once the app has started fine: remembers the version, starts the periodic and on-exit snapshots,
// and says what (if anything) was repaired.
void settingsGuardAfterStartup(const QString &version);

// Snapshot now (false if nothing changed since the last one).
bool settingsBackupNow();

// Snapshot names, newest first, for the restore menu.
QStringList settingsBackupNames();

// Restores the chosen snapshot the next time HopCast starts (the running app would overwrite it).
bool settingsScheduleRestore(const QString &name);

} // namespace hopcast
