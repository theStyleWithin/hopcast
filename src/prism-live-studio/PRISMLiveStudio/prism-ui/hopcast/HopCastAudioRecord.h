// HopCast: "Record audio only (MP3)" for the Rec button. GPL-2.0-or-later.
#pragma once

#include <QString>

namespace hopcast {

// What the Rec button records: false = video (the usual recording), true = audio only as an MP3. Remembered between runs.
bool audioOnlyMode();
void setAudioOnlyMode(bool on);

bool audioRecording();

// Starts an MP3 of everything in the mixer's Track 1 (your game, your mic, the other sources that go to Track 1), written into
// `folder`. Returns false with a plain-words reason in `error` when it can't start. `path` is the file being written.
bool startAudioRecording(const QString &folder, QString *path, QString *error);

// Finishes the file and returns its path; empty when nothing was recording. `error` is set when the disk refused data on the
// way (for example it was full), in which case the file holds what was written up to that point.
QString stopAudioRecording(QString *error = nullptr);

} // namespace hopcast
