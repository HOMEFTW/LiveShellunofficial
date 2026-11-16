// PCM channel between the payload decoder and SceShellUI's audible AudioOut. GPL-3.0.
#pragma once
#include <cstddef>
#include <cstdint>

namespace liveshell {
constexpr int kMusicPort = 9056; // 9052..9055 are reserved by the existing research features
constexpr uint32_t kMusicMagic = 0x324d534c; // LSM2
constexpr unsigned kMusicFrames = 1024;
constexpr int kMusicPlayed = 0, kMusicPaused = 1, kMusicWaiting = 2;
constexpr int kMusicUnavailable = -1, kMusicBadPacket = -2;

struct MusicPacket {
	uint32_t magic = kMusicMagic;
	uint32_t serial = 0;
	uint32_t frames = kMusicFrames; // zero = release audio output
	uint32_t reserved = 0;
	alignas(16) int16_t pcm[kMusicFrames * 2] {};
};

struct MusicAck {
	uint32_t magic = kMusicMagic;
	uint32_t serial = 0;
	int32_t result = kMusicWaiting;
};

constexpr size_t kMusicHeader = offsetof(MusicPacket, pcm);

inline bool valid_music_packet(const MusicPacket& p, size_t bytes) {
	return bytes >= kMusicHeader && p.magic == kMusicMagic && (p.frames == 0 || p.frames == kMusicFrames) &&
	       bytes == kMusicHeader + p.frames * sizeof(int16_t) * 2;
}

struct MusicAudioFunctions {
	int (*init)() = nullptr;
	int (*open)(int32_t, int32_t, int32_t, uint32_t, uint32_t, uint32_t) = nullptr;
	int (*output)(int32_t, const void*) = nullptr;
	int (*close)(int32_t) = nullptr;
	bool ready() const { return init && open && output && close; }
};

// Runs only on the receiver's audio thread. Shared with host tests; platform APIs are supplied by the caller.
class MusicAudioSink {
	int port_ = -1;
	bool initialized_ = false;
	uint32_t last_serial_ = 0;
	bool have_serial_ = false;
	int last_result_ = kMusicWaiting;

public:
	void stop(const MusicAudioFunctions& api) {
		if (port_ >= 0 && api.ready()) {
			api.output(port_, nullptr);
			api.close(port_);
		}
		port_ = -1;
		have_serial_ = false;
	}

	int receive(const MusicPacket& p, size_t bytes, bool visible, const MusicAudioFunctions& api) {
		if (!valid_music_packet(p, bytes)) return kMusicBadPacket;
		if (p.frames == 0 || !visible) {
			stop(api);
			return kMusicPaused;
		}
		if (!api.ready()) return kMusicUnavailable;
		if (have_serial_ && p.serial == last_serial_) return last_result_; // a lost acknowledgment: do not play twice
		if (!initialized_) {
			api.init(); // ShellUI can have initialized AudioOut already; Open returns the usable result.
			initialized_ = true;
		}
		if (port_ < 0) {
			port_ = api.open(0xff, 0, 0, kMusicFrames, 48000, 1); // ShellUI, main port, S16 stereo
			if (port_ < 0) return port_;
		}
		const int result = api.output(port_, p.pcm);
		if (result < 0) {
			stop(api);
			return result;
		}
		last_serial_ = p.serial, have_serial_ = true, last_result_ = kMusicPlayed;
		return kMusicPlayed;
	}
};
} // namespace liveshell
