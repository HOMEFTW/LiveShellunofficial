// Shared wallpaper formats and image geometry. No PS5 dependencies; also used by host tests. GPL-3.0.
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace liveshell::media {

constexpr long kMaxImageBytes = 32L * 1024 * 1024;
constexpr unsigned kMaxImageSide = 4096;

inline std::string extension(const std::string& path) {
	const size_t slash = path.find_last_of("/\\"), dot = path.rfind('.');
	if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
		return {};
	}
	std::string ext = path.substr(dot);
	for (char& c : ext) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return ext;
}

inline bool is_image(const std::string& path) {
	const std::string ext = extension(path);
	return ext == ".png" || ext == ".jpg" || ext == ".jpeg";
}

inline bool is_wallpaper(const std::string& path) {
	return extension(path) == ".mp4" || is_image(path);
}

struct ImageInfo {
	unsigned w = 0, h = 0;
	const char* mime = "";
	std::string error;
};

// Reads headers only. ImageBox performs the actual decode; malformed image data can still fail there.
inline ImageInfo image_info(const std::string& path) {
	ImageInfo info;
	FILE* f = std::fopen(path.c_str(), "rb");
	if (f == nullptr) {
		info.error = "Cannot read the picture";
		return info;
	}
	unsigned char b[33] = {};
	const bool sized = std::fseek(f, 0, SEEK_END) == 0;
	const long size = sized ? std::ftell(f) : -1;
	std::rewind(f);
	const size_t n = std::fread(b, 1, sizeof(b), f);
	info.error = "Not a supported PNG or JPEG picture";
	if (size <= 0 || size > kMaxImageBytes) {
		info.error = "Pictures must be at most 32 MB";
	} else if (n >= 33 && std::memcmp(b, "\x89PNG\r\n\x1a\n", 8) == 0 &&
	           b[8] == 0 && b[9] == 0 && b[10] == 0 && b[11] == 13 && std::memcmp(b + 12, "IHDR", 4) == 0) {
		auto be32 = [&](int at) {
			return (static_cast<unsigned>(b[at]) << 24) | (static_cast<unsigned>(b[at + 1]) << 16) |
			       (static_cast<unsigned>(b[at + 2]) << 8) | b[at + 3];
		};
		info.w = be32(16), info.h = be32(20), info.mime = "image/png";
		const bool colour = b[25] == 0 || b[25] == 2 || b[25] == 3 || b[25] == 4 || b[25] == 6;
		info.error = b[24] == 8 && colour && b[26] == 0 && b[27] == 0 && b[28] == 0
		                 ? "" : "Use an 8-bit, non-interlaced PNG";
	} else if (n >= 4 && b[0] == 0xff && b[1] == 0xd8) {
		std::fseek(f, 2, SEEK_SET);
		while (std::ftell(f) < size) {
			if (std::fgetc(f) != 0xff) {
				break;
			}
			int marker;
			do { marker = std::fgetc(f); } while (marker == 0xff);
			if (marker < 0 || marker == 0xda || marker == 0xd9) {
				break;
			}
			const int hi = std::fgetc(f), lo = std::fgetc(f);
			if (hi < 0 || lo < 0) {
				break;
			}
			const int len = (hi << 8) | lo;
			if (len < 2 || len > size - std::ftell(f) + 2) {
				break;
			}
			if (marker == 0xc0 || marker == 0xc2) { // baseline / progressive JPEG
				unsigned char sof[6];
				if (len >= 8 && std::fread(sof, 1, 6, f) == 6 && len == 8 + 3 * sof[5]) {
					info.h = (sof[1] << 8) | sof[2], info.w = (sof[3] << 8) | sof[4];
					info.mime = "image/jpeg";
					info.error = sof[0] == 8 && (sof[5] == 1 || sof[5] == 3)
					                 ? "" : "Use an 8-bit RGB or grayscale JPEG";
				}
				break;
			}
			if (std::fseek(f, len - 2, SEEK_CUR) != 0) {
				break;
			}
		}
	}
	std::fclose(f);
	if (info.error.empty() && (info.w == 0 || info.h == 0 || info.w > kMaxImageSide || info.h > kMaxImageSide)) {
		info.error = "Pictures must be between 1 and 4096 pixels on each side";
	}
	return info;
}

struct Rect { float x, y, w, h; };

inline Rect cover(unsigned width, unsigned height, float x, float y, float w, float h) {
	const float scale = std::max(w / static_cast<float>(width), h / static_cast<float>(height));
	const float iw = width * scale, ih = height * scale;
	return {x + (w - iw) * 0.5f, y + (h - ih) * 0.5f, iw, ih};
}

} // namespace liveshell::media
