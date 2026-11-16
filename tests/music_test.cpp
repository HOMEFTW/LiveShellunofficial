// Run the real decoder, music state and playback thread with filesystem / AudioOut adapters.
#include "../src/liveshell.hpp"
#include "../src/liveshell_music_wire.hpp"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

std::string fixture;
std::string local_path(const char* path) {
	const std::string s = path;
	return s.rfind("/data/LiveShell", 0) == 0 ? fixture + s.substr(15) : s;
}
FILE* test_fopen(const char* path, const char* mode) { return std::fopen(local_path(path).c_str(), mode); }
DIR* test_opendir(const char* path) { return ::opendir(local_path(path).c_str()); }
int test_stat(const char* path, struct stat* out) { return ::stat(local_path(path).c_str(), out); }
int test_unlink(const char* path) { return ::unlink(local_path(path).c_str()); }
int test_mkdir(const char* path, int) {
	std::error_code error;
	std::filesystem::create_directories(local_path(path), error);
	return error ? -1 : 0;
}
#define fopen test_fopen
#define opendir test_opendir
#define stat(path, out) test_stat(path, out)
#define unlink test_unlink
#define mkdir(path, mode) test_mkdir(path, mode)
#include "../src/liveshell_music.cpp"
#undef fopen
#undef opendir
#undef stat
#undef unlink
#undef mkdir

std::atomic<int> opens{0}, closes{0}, blocks{0};
std::atomic<bool> receiver_available{false};
int test_audio_init() { return 0; }
int test_audio_open(int user, int type, int, unsigned length, unsigned rate, unsigned format) {
	if (user != 255 || type != 0 || length != 1024 || rate != 48000 || format != 1) return -1;
	++opens;
	return 7;
}
int test_audio_output(int, const void* data) {
	if (data) {
		++blocks;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	return 0;
}
int test_audio_close(int) { ++closes; return 0; }
namespace liveshell {
void log(const char*, ...) {}
MusicAudioFunctions test_audio {test_audio_init, test_audio_open, test_audio_output, test_audio_close};
MusicAudioSink test_sink;
uint32_t test_serial = 0;
int music_output(const int16_t* pcm) {
	if (!receiver_available) return kMusicWaiting;
	MusicPacket packet;
	packet.serial = ++test_serial;
	memcpy(packet.pcm, pcm, sizeof(packet.pcm));
	return test_sink.receive(packet, sizeof(packet), true, test_audio);
}
void close_music_output() { test_sink.stop(test_audio); }
}

int checks = 0;
void check(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }
template<class F> bool until(F test) {
	for (int i = 0; i < 150; ++i) {
		if (test()) return true;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	return false;
}

int main(int argc, char** argv) {
	if (argc != 2) return 2;
	fixture = std::filesystem::absolute(argv[1]).string();
	using namespace liveshell;
	std::string error;
	check(music_problem("/data/LiveShell/music/a.mp3").empty(), "real MP3 decode");
	check(!music_problem("/data/LiveShell/music/bad.mp3").empty(), "reject invalid MP3");
	std::filesystem::remove(std::filesystem::path(fixture) / "music/bad.mp3");
	check(list_music().size() == 2, "music library excludes pictures / videos");
	check(next_music_track(list_music(), "/data/LiveShell/music/a.mp3") == "/data/LiveShell/music/b.mp3", "advance list");
	check(next_music_track(list_music(), "/data/LiveShell/music/b.mp3") == "/data/LiveShell/music/a.mp3", "wrap list");
	check(next_music_track({}, "missing").empty(), "empty music list");
	check(select_music("/data/LiveShell/music/a.mp3", error), "select global music");
	set_music_repeat(0);
	set_music_volume(60);
	check(start_music(), "start music service");
	check(until([] {return music_json().find("Paused") != std::string::npos;}), "paused without visible picture");
	check(opens == 0, "no output for videos / hidden pictures");
	music_visible(true);
	check(until([] {return music_json().find("Waiting for home-screen audio") != std::string::npos;}), "no false Playing before receiver confirms output");
	check(opens == 0 && blocks == 0, "background decoder creates no audio output of its own");
	receiver_available = true;
	check(until([] {return blocks >= 30;}), "single-track loops past EOF");
	check(opens == 1 && music_json().find("Playing") != std::string::npos, "single loop keeps output open");
	music_visible(false);
	check(until([] {return closes == 1;}), "hidden picture releases output");
	set_music_volume(0);
	music_visible(true);
	std::this_thread::sleep_for(std::chrono::milliseconds(70));
	check(opens == 1, "muted music stays stopped");
	set_music_volume(50);
	check(until([] {return opens == 2;}), "music resumes after unmute");
	set_music_repeat(1);
	check(until([] {return music_json().find("\"current\":\"/data/LiveShell/music/b.mp3\"") != std::string::npos;}), "list loop advances actual playback");
	check(until([] {return music_json().find("\"current\":\"/data/LiveShell/music/a.mp3\"") != std::string::npos;}), "list loop wraps actual playback");
	music_visible(false);
	check(select_music("", error), "no music selection");
	check(until([] {return music_json().find("\"status\":\"Off\"") != std::string::npos;}), "off stops music");
	stop_music();
	check(opens == closes, "all audio ports released");
	check(start_music(), "restart music service");
	check(music_json().find("\"repeat\":1") != std::string::npos && music_json().find("\"volume\":50") != std::string::npos, "settings survive restart");
	stop_music();
	check(delete_music("/data/LiveShell/music/b.mp3", error), "delete track");
	check(list_music().size() == 1, "deleted track leaves list");
	std::cout << checks << " music decoder / loop / pause / persistence checks passed\n";
}
