#include "../src/liveshell_media.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

int checks = 0;
void check(bool ok, const char* message) {
	++checks;
	if (!ok) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
	if (argc != 2) return 2;
	using namespace liveshell::media;
	const std::string dir = argv[1];
	check(is_image("/pictures/Wall.JPEG"), "uppercase JPEG");
	check(is_image("wall.PNG"), "uppercase PNG");
	check(!is_image("wall.png.part"), "partial uploads excluded");
	check(!is_image("folder.jpg/video.mp4"), "directory extension ignored");
	check(is_wallpaper("clip.MP4"), "video compatibility");
	check(!is_wallpaper("song.mp3"), "music excluded from wallpapers");
	for (const char* file : {"rgb.png", "gray.png", "rgba.png", "rgb.jpg", "progressive.jpeg"}) {
		const auto info = image_info(dir + "/" + file);
		check(info.error.empty(), file);
		check(info.w == 32 && info.h == 16, "image dimensions");
	}
	for (const char* file : {"zero.png", "huge.png", "depth16.png", "interlaced.png", "bad.png", "truncated.jpg", "bad_segment.jpg", "cmyk.jpg", "empty.png", "oversized.png"}) {
		check(!image_info(dir + "/" + file).error.empty(), file);
	}
	check(!image_info(dir + "/missing.jpg").error.empty(), "missing picture");
	const auto wide = cover(3840, 1080, 0, 0, 1920, 1080);
	check(wide.w == 3840 && wide.h == 1080 && wide.x == -960 && wide.y == 0, "wide centered crop");
	const auto tall = cover(1080, 1920, 0, 0, 1920, 1080);
	check(tall.x == 0 && tall.y < 0 && tall.w == 1920 && std::abs(tall.w / tall.h - 1080.0f / 1920) < 0.001f, "portrait keeps aspect");
	const auto effect = cover(32, 16, -20, -10, 1960, 1100);
	check(effect.w >= 1960 && effect.h >= 1100 && std::abs(effect.w / effect.h - 2.0f) < 0.001f, "effects keep aspect");
	std::cout << checks << " media header / geometry checks passed\n";
}
