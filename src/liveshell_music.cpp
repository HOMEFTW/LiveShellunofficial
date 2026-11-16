// Global MP3 background music for static wallpapers. GPL-3.0.
// dr_mp3 decodes on the payload thread; PCM is played by SceShellUI, which owns audible home-screen output.
#include "liveshell.hpp"
#include "liveshell_music_wire.hpp"
#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_SIMD
#include "dr_mp3.h"

#include <atomic>
#include <ctime>
#include <dirent.h>
#include <mutex>
#include <pthread.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

namespace liveshell {
namespace {

constexpr const char* kMusicConfig = "/data/LiveShell/music.txt";
std::mutex g_music_lock;
std::string g_music_path, g_music_status = "Off";
int g_music_volume = 60;
int g_music_repeat = 0;
unsigned g_music_revision = 0;
std::atomic<bool> g_music_running{false};
std::atomic<time_t> g_visible_until{0};
pthread_t g_music_thread;

void save_music() { // caller holds g_music_lock
	if (FILE* f = fopen(kMusicConfig, "w")) {
		fprintf(f, "%s\n%d\n%d\n", g_music_path.c_str(), g_music_volume, g_music_repeat);
		fclose(f);
	}
}

void music_status(const std::string& status) {
	std::lock_guard<std::mutex> lock(g_music_lock);
	g_music_status = status;
}

std::string music_quote(const std::string& s) {
	std::string out = "\"";
	for (const unsigned char c : s) {
		if (c == '"' || c == '\\') {
			out += '\\';
		}
		if (c < 32) {
			char escaped[7];
			snprintf(escaped, sizeof(escaped), "\\u%04x", c);
			out += escaped;
		} else {
			out += c;
		}
	}
	return out + '"';
}

// Buffered, looping PCM input. Mono becomes stereo before resampling to AudioOut's 48 kHz.
struct MusicInput {
	drmp3 decoder {};
	drmp3_int16 pcm[2048 * 2] {};
	size_t at = 0, count = 0;
	bool opened = false;

	bool frame(float* stereo, bool loop = true) {
		if (at == count) {
			count = drmp3_read_pcm_frames_s16(&decoder, 2048, pcm);
			if (count == 0 && loop && drmp3_seek_to_pcm_frame(&decoder, 0)) {
				count = drmp3_read_pcm_frames_s16(&decoder, 2048, pcm);
			}
			at = 0;
			if (count == 0) {
				return false;
			}
		}
		stereo[0] = pcm[at * decoder.channels];
		stereo[1] = pcm[at * decoder.channels + (decoder.channels == 1 ? 0 : 1)];
		++at;
		return true;
	}
};

std::string next_music_track(const std::vector<Video>& tracks, const std::string& current) {
	if (tracks.empty()) {
		return {};
	}
	size_t i = 0;
	while (i < tracks.size() && tracks[i].path != current) {
		++i;
	}
	return tracks[i < tracks.size() ? (i + 1) % tracks.size() : 0].path;
}

void* music_thread(void*) {
	MusicInput input;
	std::string loaded;
	unsigned revision = ~0u;
	bool failed = false;
	bool have_output = false, ended = false;
	float left[2] {}, right[2] {};
	double phase = 0.0;
	int16_t output[kMusicFrames * 2];
	int last_output_result = kMusicWaiting;
	while (g_music_running.load()) {
		std::string path;
		int volume, repeat;
		unsigned wanted;
		{
			std::lock_guard<std::mutex> lock(g_music_lock);
			path = g_music_path, volume = g_music_volume, wanted = g_music_revision, repeat = g_music_repeat;
		}
		if (wanted != revision) {
			close_music_output();
			have_output = false;
			if (input.opened) {
				drmp3_uninit(&input.decoder);
			}
			input = MusicInput {};
			loaded = path, revision = wanted, failed = false, phase = 0.0;
			if (!path.empty()) {
				input.opened = drmp3_init_file(&input.decoder, path.c_str(), nullptr);
				failed = !input.opened || input.decoder.channels > 2 || input.decoder.channels == 0 ||
				         input.decoder.sampleRate < 8000 || input.decoder.sampleRate > 48000;
				if (!failed) {
					failed = !input.frame(left) || !input.frame(right);
				}
				if (failed) {
					log("music: cannot decode %s", path.c_str());
					music_status("Could not decode this MP3");
				}
			}
		}
		const bool visible = time(nullptr) < g_visible_until.load();
		if (loaded.empty() || failed || !visible || volume == 0) {
			close_music_output();
			have_output = false;
			if (!failed) {
				music_status(loaded.empty() ? "Off" : volume == 0 ? "Muted" : "Paused (pictures only)");
			}
			usleep(50 * 1000);
			continue;
		}
		if (!have_output) {
			const double step = static_cast<double>(input.decoder.sampleRate) / 48000.0;
			const float gain = static_cast<float>(volume) / 100.0f;
			ended = false;
			std::fill(std::begin(output), std::end(output), 0);
			for (unsigned i = 0; i < kMusicFrames; ++i) {
				for (int c = 0; c < 2; ++c) {
					output[i * 2 + c] = static_cast<int16_t>((left[c] + (right[c] - left[c]) * phase) * gain);
				}
				phase += step;
				while (phase >= 1.0) {
					phase -= 1.0;
					left[0] = right[0], left[1] = right[1];
					if (!input.frame(right, repeat == 0)) {
						ended = true;
						break;
					}
				}
				if (ended) {
					break;
				}
			}
			have_output = true;
		}
		const int result = music_output(output);
		if (result != kMusicPlayed) {
			if (result == kMusicWaiting) music_status("Waiting for home-screen audio");
			else if (result == kMusicPaused) music_status("Paused (pictures only)");
			else {
				char message[96];
				snprintf(message, sizeof(message), "Home-screen audio error (0x%08x)", static_cast<unsigned>(result));
				music_status(message);
			}
			if (result != last_output_result) log("music: ShellUI output result %#x", static_cast<unsigned>(result));
			last_output_result = result;
			usleep(result < 0 ? 250000 : 50000);
			continue; // keep this PCM block until the receiver accepts it; do not advance silently
		}
		last_output_result = result;
		music_status("Playing");
		have_output = false;
		if (ended) {
			if (repeat == 0) {
				failed = true;
				music_status("Could not loop this MP3; select another track");
				continue;
			}
			const auto tracks = list_music();
			const std::string next = next_music_track(tracks, loaded);
			std::lock_guard<std::mutex> lock(g_music_lock);
			if (revision == g_music_revision) {
				g_music_path = next;
				++g_music_revision;
				save_music();
			}
		}
	}
	close_music_output();
	if (input.opened) {
		drmp3_uninit(&input.decoder);
	}
	return nullptr;
}

} // namespace

std::vector<Video> list_music() {
	std::vector<Video> tracks;
	auto scan = [&](const std::string& dir, const std::string& where) {
		if (DIR* d = opendir(dir.c_str())) {
			while (dirent* e = readdir(d)) {
				if (e->d_name[0] == '.' || media::extension(e->d_name) != ".mp3") {
					continue;
				}
				const std::string path = dir + "/" + e->d_name;
				struct stat st {};
				if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
					tracks.push_back({path, e->d_name, where});
				}
			}
			closedir(d);
		}
	};
	auto subdir = [](const std::string& dir, const char* name) {
		std::string found;
		if (DIR* d = opendir(dir.c_str())) {
			while (dirent* e = readdir(d)) {
				if (strcasecmp(e->d_name, name) == 0) {
					found = dir + "/" + e->d_name;
					break;
				}
			}
			closedir(d);
		}
		return found;
	};
	scan("/data/LiveShell/music", "Internal");
	for (int i = 0; i < 8; ++i) {
		const std::string root = subdir("/mnt/usb" + std::to_string(i), "liveshell");
		if (!root.empty()) {
			const std::string music = subdir(root, "music");
			if (!music.empty()) {
				scan(music, "USB " + std::to_string(i + 1));
			}
		}
	}
	std::sort(tracks.begin(), tracks.end(), [](const Video& a, const Video& b) {
		return a.name != b.name ? a.name < b.name : a.path < b.path;
	});
	return tracks;
}

std::string music_problem(const std::string& path) {
	struct stat st {};
	if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > kMaxMusicBytes) {
		return "Use an MP3 file up to 64 MB";
	}
	drmp3 decoder;
	if (!drmp3_init_file(&decoder, path.c_str(), nullptr)) {
		return "Cannot decode this MP3";
	}
	int16_t sample[2];
	const bool ok = decoder.channels >= 1 && decoder.channels <= 2 && decoder.sampleRate >= 8000 && decoder.sampleRate <= 48000 &&
	                drmp3_read_pcm_frames_s16(&decoder, 1, sample) == 1;
	drmp3_uninit(&decoder);
	return ok ? "" : "Use a mono or stereo MP3 with a sample rate from 8 to 48 kHz";
}

bool select_music(const std::string& path, std::string& error) {
	if (!path.empty() && !(error = music_problem(path)).empty()) {
		return false;
	}
	std::lock_guard<std::mutex> lock(g_music_lock);
	g_music_path = path;
	++g_music_revision;
	save_music();
	return true;
}

bool delete_music(const std::string& path, std::string& error) {
	std::lock_guard<std::mutex> lock(g_music_lock);
	if (unlink(path.c_str()) != 0) {
		error = "Could not delete the music file";
		return false;
	}
	if (path == g_music_path) {
		g_music_path.clear();
		++g_music_revision;
		save_music();
	}
	return true;
}

void set_music_volume(int percent) {
	std::lock_guard<std::mutex> lock(g_music_lock);
	g_music_volume = std::max(0, std::min(100, percent));
	save_music();
}

void set_music_repeat(int mode) {
	std::lock_guard<std::mutex> lock(g_music_lock);
	g_music_repeat = mode == 1 ? 1 : 0;
	save_music();
}

std::string music_json() {
	std::string json;
	{
		std::lock_guard<std::mutex> lock(g_music_lock);
		json = "{\"current\":" + music_quote(g_music_path) + ",\"status\":" + music_quote(g_music_status) +
		       ",\"volume\":" + std::to_string(g_music_volume) + ",\"repeat\":" + std::to_string(g_music_repeat) + ",\"tracks\":[";
	}
	bool first = true;
	for (const Video& v : list_music()) {
		struct stat st {};
		stat(v.path.c_str(), &st);
		json += std::string(first ? "" : ",") + "{\"path\":" + music_quote(v.path) + ",\"name\":" + music_quote(v.name) +
		        ",\"where\":" + music_quote(v.where) + ",\"size\":" + std::to_string(st.st_size) + "}";
		first = false;
	}
	return json + "]}";
}

void music_visible(bool visible) {
	g_visible_until.store(visible ? time(nullptr) + 4 : 0);
}

bool start_music() {
	mkdir("/data/LiveShell/pictures", 0777);
	mkdir("/data/LiveShell/music", 0777);
	{
		std::lock_guard<std::mutex> lock(g_music_lock);
		if (FILE* f = fopen(kMusicConfig, "r")) {
			char path[1024] {};
			if (fgets(path, sizeof(path), f)) {
				path[strcspn(path, "\r\n")] = 0;
				g_music_path = path;
			}
			int volume;
			if (fscanf(f, "%d", &volume) == 1) {
				g_music_volume = std::max(0, std::min(100, volume));
			}
			int repeat;
			if (fscanf(f, "%d", &repeat) == 1) {
				g_music_repeat = repeat == 1 ? 1 : 0;
			}
			fclose(f);
		}
	}
	g_music_running.store(true);
	if (pthread_create(&g_music_thread, nullptr, music_thread, nullptr) != 0) {
		g_music_running.store(false);
		music_status("Music service could not start");
		return false;
	}
	return true;
}

void stop_music() {
	if (g_music_running.exchange(false)) {
		pthread_join(g_music_thread, nullptr);
	}
}

} // namespace liveshell
