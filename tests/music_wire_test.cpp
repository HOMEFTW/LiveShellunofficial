#include "../src/liveshell_music_wire.hpp"
#include <iostream>
#include <stdexcept>

int checks = 0, opens = 0, closes = 0, blocks = 0, open_result = 7, output_result = 0;
void check(bool ok, const char* why) { ++checks; if (!ok) throw std::runtime_error(why); }
int audio_init() { return -1; } // ShellUI has already initialized it: Open still decides usability
int audio_open(int user, int type, int, unsigned len, unsigned rate, unsigned format) {
	check(user == 255 && type == 0, "audible ShellUI MAIN port, not background BGM");
	check(len == 1024 && rate == 48000 && format == 1, "PCM output format");
	++opens;
	return open_result;
}
int audio_output(int, const void* pcm) {
	if (pcm) {
		check(reinterpret_cast<uintptr_t>(pcm) % 16 == 0, "aligned native PCM buffer");
		++blocks;
	}
	return output_result;
}
int audio_close(int) { ++closes; return 0; }

int main(int, char**) {
	using namespace liveshell;
	MusicAudioFunctions api {audio_init, audio_open, audio_output, audio_close};
	MusicAudioSink sink;
	MusicPacket packet;
	packet.serial = 1;
	check(valid_music_packet(packet, sizeof(packet)), "full PCM packet");
	check(!valid_music_packet(packet, sizeof(packet) - 1), "truncated PCM rejected");
	check(!valid_music_packet(packet, 0), "empty datagram rejected");
	packet.magic = 0;
	check(sink.receive(packet, sizeof(packet), true, api) == kMusicBadPacket, "foreign packet rejected");
	packet.magic = kMusicMagic;
	packet.frames = 4096;
	check(!valid_music_packet(packet, sizeof(packet)), "oversized frame count rejected");
	packet.frames = kMusicFrames;
	check(sink.receive(packet, sizeof(packet), false, api) == kMusicPaused && opens == 0, "hidden image does not open output");
	check(sink.receive(packet, sizeof(packet), true, {}) == kMusicUnavailable, "missing native audio reports failure");
	check(sink.receive(packet, sizeof(packet), true, api) == kMusicPlayed && blocks == 1, "play acknowledged only after output");
	check(sink.receive(packet, sizeof(packet), true, api) == kMusicPlayed && blocks == 1, "retry cannot play the same block twice");
	packet.serial++;
	check(sink.receive(packet, sizeof(packet), true, api) == kMusicPlayed && blocks == 2, "new block advances");
	packet.frames = 0;
	check(sink.receive(packet, kMusicHeader, true, api) == kMusicPaused && closes == 1, "stop releases native port");
	packet.frames = kMusicFrames;
	packet.serial++;
	open_result = -20;
	check(sink.receive(packet, sizeof(packet), true, api) == -20, "native open error reaches sender");
	open_result = 7;
	output_result = -30;
	check(sink.receive(packet, sizeof(packet), true, api) == -30 && closes == 2, "native output error releases port");
	output_result = 0;
	check(sink.receive(packet, sizeof(packet), true, api) == kMusicPlayed, "native output can recover");
	sink.stop(api);
	check(closes == 3, "idle receiver releases last port");
	std::cout << checks << " music channel / native output checks passed\n";
}
