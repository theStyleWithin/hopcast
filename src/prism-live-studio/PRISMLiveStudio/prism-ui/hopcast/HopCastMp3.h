// HopCast: writes an MP3 stream with the Shine encoder (see shine/README.hopcast). GPL-2.0-or-later.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

struct shine_global_flags;

namespace hopcast {

// Turns 16-bit PCM into MP3 data and hands every finished piece to `sink` (for example "append to a file"). It has no Qt or
// libobs in it, so it is the same on every platform and can be tested on its own.
class Mp3Writer {
public:
	using Sink = std::function<bool(const unsigned char *data, int size)>;

	Mp3Writer() = default;
	~Mp3Writer();
	Mp3Writer(const Mp3Writer &) = delete;
	Mp3Writer &operator=(const Mp3Writer &) = delete;

	// channels is 1 or 2, bitrate is in kbps (for example 192). False when the encoder doesn't support those settings.
	bool start(int sampleRate, int channels, int bitrateKbps, Sink sink);

	// Interleaved 16-bit samples, any number of frames per call. False once the sink has refused data.
	bool write(const int16_t *interleaved, size_t frames);

	// Encodes what is left (padded with silence) and ends the stream. Safe to call twice.
	bool finish();

	uint64_t framesIn() const { return framesIn_; }
	bool running() const { return enc_ != nullptr; }

private:
	bool encodeBlock(const int16_t *block);

	shine_global_flags *enc_ = nullptr;
	int channels_ = 2;
	int blockFrames_ = 1152;
	std::vector<int16_t> pending_;
	Sink sink_;
	uint64_t framesIn_ = 0;
	bool ok_ = true;
};

} // namespace hopcast
