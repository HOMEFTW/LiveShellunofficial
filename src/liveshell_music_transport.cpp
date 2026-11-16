// AudioOut belongs to ShellUI: the background payload sends PCM and waits for its acknowledgment. GPL-3.0.
#include "liveshell.hpp"
#include "liveshell_music_wire.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace liveshell {
namespace {
int g_audio_socket = -1;
MusicPacket g_audio_packet;
bool g_audio_pending = false;
uint32_t g_audio_serial = 0;

bool audio_connect() {
	if (g_audio_socket >= 0) return true;
	g_audio_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (g_audio_socket < 0) return false;
	sockaddr_in peer {};
	peer.sin_family = AF_INET;
	peer.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	peer.sin_port = htons(kMusicPort);
	timeval timeout {0, 250000};
	setsockopt(g_audio_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	if (connect(g_audio_socket, reinterpret_cast<sockaddr*>(&peer), sizeof(peer)) != 0) {
		close(g_audio_socket);
		g_audio_socket = -1;
		return false;
	}
	if (g_audio_serial == 0) g_audio_serial = (static_cast<uint32_t>(getpid()) << 16) | 1;
	return true;
}
} // namespace

int music_output(const int16_t* pcm) {
	if (!audio_connect()) return kMusicWaiting;
	if (!g_audio_pending) {
		g_audio_packet.serial = ++g_audio_serial;
		g_audio_packet.frames = kMusicFrames;
		memcpy(g_audio_packet.pcm, pcm, sizeof(g_audio_packet.pcm));
		g_audio_pending = true;
	}
	if (send(g_audio_socket, &g_audio_packet, sizeof(g_audio_packet), 0) != sizeof(g_audio_packet)) {
		close(g_audio_socket);
		g_audio_socket = -1;
		return kMusicWaiting;
	}
	MusicAck ack;
	for (int i = 0; i < 4; ++i) {
		const ssize_t n = recv(g_audio_socket, &ack, sizeof(ack), 0);
		if (n < 0) {
			// A rest-mode socket or an absent receiver can reconnect on the next attempt.
			if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
				close(g_audio_socket);
				g_audio_socket = -1;
			}
			return kMusicWaiting;
		}
		if (n == sizeof(ack) && ack.magic == kMusicMagic && ack.serial == g_audio_packet.serial) {
			if (ack.result == kMusicPlayed) g_audio_pending = false;
			return ack.result;
		}
	}
	return kMusicWaiting;
}

void close_music_output() {
	if (g_audio_socket >= 0) {
		MusicPacket stop;
		stop.serial = ++g_audio_serial;
		stop.frames = 0;
		send(g_audio_socket, &stop, kMusicHeader, 0);
		close(g_audio_socket);
		g_audio_socket = -1;
	}
	g_audio_pending = false;
}
} // namespace liveshell
