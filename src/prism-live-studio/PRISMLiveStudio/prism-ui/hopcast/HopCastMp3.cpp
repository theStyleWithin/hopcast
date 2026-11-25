// HopCast: MP3 writer around the Shine encoder. GPL-2.0-or-later.
#include "HopCastMp3.h"

#include "shine/layer3.h"

#include <algorithm>

namespace hopcast {

Mp3Writer::~Mp3Writer()
{
	if (enc_) {
		shine_close(enc_);
		enc_ = nullptr;
	}
}

bool Mp3Writer::start(int sampleRate, int channels, int bitrateKbps, Sink sink)
{
	if (enc_ || channels < 1 || channels > 2 || !sink)
		return false;
	if (shine_check_config(sampleRate, bitrateKbps) < 0)
		return false;

	shine_config_t config;
	shine_set_config_mpeg_defaults(&config.mpeg);
	config.wave.channels = channels;
	config.wave.samplerate = sampleRate;
	config.mpeg.bitrate = bitrateKbps;
	config.mpeg.mode = channels == 1 ? SHINE_MONO : SHINE_JOINT_STEREO;

	enc_ = shine_initialise(&config);
	if (!enc_)
		return false;

	channels_ = channels;
	blockFrames_ = shine_samples_per_pass(enc_);
	pending_.clear();
	pending_.reserve(static_cast<size_t>(blockFrames_) * static_cast<size_t>(channels_) * 2);
	sink_ = std::move(sink);
	framesIn_ = 0;
	ok_ = true;
	return true;
}

bool Mp3Writer::encodeBlock(const int16_t *block)
{
	int written = 0;
	const unsigned char *data = shine_encode_buffer_interleaved(enc_, block, &written);
	if (data && written > 0 && !sink_(data, written))
		ok_ = false;
	return ok_;
}

bool Mp3Writer::write(const int16_t *interleaved, size_t frames)
{
	if (!enc_ || !ok_)
		return false;
	framesIn_ += frames;
	const size_t blockSamples = static_cast<size_t>(blockFrames_) * static_cast<size_t>(channels_);
	pending_.insert(pending_.end(), interleaved, interleaved + frames * static_cast<size_t>(channels_));

	size_t used = 0;
	while (ok_ && pending_.size() - used >= blockSamples) {
		encodeBlock(pending_.data() + used);
		used += blockSamples;
	}
	pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(used));
	return ok_;
}

bool Mp3Writer::finish()
{
	if (!enc_)
		return ok_;
	const size_t blockSamples = static_cast<size_t>(blockFrames_) * static_cast<size_t>(channels_);
	if (ok_ && !pending_.empty()) {
		pending_.resize(blockSamples, 0); // the last, partly filled block is finished with silence
		encodeBlock(pending_.data());
		pending_.clear();
	}
	if (ok_) {
		int written = 0;
		const unsigned char *data = shine_flush(enc_, &written);
		if (data && written > 0 && !sink_(data, written))
			ok_ = false;
	}
	shine_close(enc_);
	enc_ = nullptr;
	return ok_;
}

} // namespace hopcast
