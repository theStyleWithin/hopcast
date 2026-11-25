// HopCast: checks the MP3 writer produces a valid MP3 of the right length, bitrate and channels, whatever the chunk sizes
// the audio arrives in. No audio libraries needed: the frame headers are read by hand. GPL-2.0-or-later.
#include "HopCastMp3.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static int bad = 0;
#define CHECK(c, msg) do { if (!(c)) { ++bad; printf("FAIL: %s\n", msg); } else printf("ok: %s\n", msg); fflush(stdout); } while (0)

struct Info { int frames = 0; int bitrate = 0; int sampleRate = 0; int channels = 0; bool valid = true; };

// MPEG-1 Layer III frame walker (the encoder is used at 32/44.1/48 kHz here)
static Info parse(const std::vector<unsigned char> &d)
{
	static const int bitrates[16] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
	static const int rates[4] = {44100, 48000, 32000, 0};
	Info info;
	size_t i = 0;
	// skip an ID3v2 tag if there is one
	if (d.size() > 10 && d[0] == 'I' && d[1] == 'D' && d[2] == '3')
		i = 10 + ((d[6] << 21) | (d[7] << 14) | (d[8] << 7) | d[9]);
	while (i + 4 <= d.size()) {
		if (d[i] != 0xFF || (d[i + 1] & 0xFE) != 0xFA) { // sync + MPEG-1 + Layer III
			info.valid = false;
			break;
		}
		const int br = bitrates[d[i + 2] >> 4];
		const int sr = rates[(d[i + 2] >> 2) & 3];
		const int pad = (d[i + 2] >> 1) & 1;
		if (!br || !sr) { info.valid = false; break; }
		const int len = 144 * br * 1000 / sr + pad;
		if (i + len > d.size()) break; // a cut-off last frame would be a fault
		if (info.frames == 0) { info.bitrate = br; info.sampleRate = sr; info.channels = ((d[i + 3] >> 6) == 3) ? 1 : 2; }
		++info.frames;
		i += len;
	}
	if (i != d.size()) info.valid = false;
	return info;
}

static std::vector<unsigned char> encode(int rate, int channels, int kbps, double seconds, const std::vector<size_t> &chunks)
{
	std::vector<unsigned char> out;
	hopcast::Mp3Writer w;
	if (!w.start(rate, channels, kbps, [&](const unsigned char *p, int n) { out.insert(out.end(), p, p + n); return true; }))
		return {};
	const size_t total = static_cast<size_t>(seconds * rate);
	std::vector<int16_t> pcm(total * channels);
	for (size_t f = 0; f < total; ++f) {
		const int16_t v = static_cast<int16_t>(12000 * std::sin(2 * 3.14159265358979 * 440.0 * f / rate));
		for (int c = 0; c < channels; ++c) pcm[f * channels + c] = v;
	}
	size_t pos = 0, k = 0;
	while (pos < total) {
		size_t n = chunks[k++ % chunks.size()];
		if (pos + n > total) n = total - pos;
		w.write(pcm.data() + pos * channels, n);
		pos += n;
	}
	w.finish();
	CHECK(w.framesIn() == total, "every input frame was counted");
	return out;
}

int main(int argc, char **argv)
{
	{
		auto mp3 = encode(48000, 2, 192, 5.0, {480, 1000, 1, 4800}); // awkward chunk sizes, like a real audio thread
		Info i = parse(mp3);
		CHECK(!mp3.empty() && i.valid, "48 kHz stereo: the whole output is valid MP3 frames");
		CHECK(i.bitrate == 192 && i.sampleRate == 48000 && i.channels == 2, "48 kHz stereo: 192 kbps, 48000 Hz, 2 channels");
		const double secs = i.frames * 1152.0 / 48000;
		printf("  length %.3f s, %d frames, %zu bytes\n", secs, i.frames, mp3.size());
		CHECK(secs >= 5.0 && secs < 5.2, "48 kHz stereo: about 5 seconds long");
		auto other = encode(48000, 2, 192, 5.0, {1152});
		CHECK(other.size() == mp3.size(), "the chunk sizes do not change the result");
		if (argc > 1) { // optional: keep the file so another tool (ffprobe, a player) can look at it
			if (FILE *f = fopen(argv[1], "wb")) { fwrite(mp3.data(), 1, mp3.size(), f); fclose(f); }
		}
	}
	{
		auto mp3 = encode(44100, 1, 128, 3.0, {441});
		Info i = parse(mp3);
		CHECK(!mp3.empty() && i.valid && i.bitrate == 128 && i.sampleRate == 44100 && i.channels == 1, "44.1 kHz mono 128 kbps is valid");
	}
	{
		hopcast::Mp3Writer w;
		CHECK(!w.start(96000, 2, 192, [](const unsigned char *, int) { return true; }), "an unsupported sample rate is refused");
		hopcast::Mp3Writer w2;
		CHECK(!w2.start(48000, 6, 192, [](const unsigned char *, int) { return true; }), "more than 2 channels is refused");
		hopcast::Mp3Writer w3;
		w3.start(48000, 2, 192, [](const unsigned char *, int) { return false; });
		std::vector<int16_t> pcm(2304 * 2, 100);
		w3.write(pcm.data(), 2304);
		CHECK(!w3.write(pcm.data(), 2304), "a sink that refuses data stops the writer (disk full)");
		w3.finish();
	}
	{
		hopcast::Mp3Writer w;
		std::vector<unsigned char> out;
		w.start(48000, 2, 192, [&](const unsigned char *p, int n) { out.insert(out.end(), p, p + n); return true; });
		CHECK(w.finish() && w.finish(), "finishing an empty or finished writer is safe");
	}
	return bad ? 1 : 0;
}
