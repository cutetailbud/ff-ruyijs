#if defined(__linux__)
#include "web_media/image_animation.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSamplingOptions.h"
#include <algorithm>
#include <cstring>

namespace ruyijs::web_media {
bool ComposeImageAnimation(const std::string& mime, const ImageAnimation& input, uint32_t width, uint32_t height,
                          domain::web_media::ImageData* out) {
  const uint64_t pixels = uint64_t(input.width) * input.height;
  if (!pixels || pixels > domain::web_media::kImagePixelLimit ||
      input.patches.size() > domain::web_media::kImagePixelLimit / pixels) return false;
  std::vector<uint8_t> canvas(size_t(pixels) * 4), previous;
  domain::web_media::ImageData result;
  result.animated = true;
  result.repetitions = input.repetitions;
  uint64_t timestamp = 0;
  for (const auto& patch : input.patches) {
    if (patch.x > input.width || patch.width > input.width - patch.x ||
        patch.y > input.height || patch.height > input.height - patch.y) return false;
    domain::web_media::ImageData decoded;
    if (DecodeImageBytes(mime, patch.encoded, 0, 0, false, &decoded) != domain::web_media::ImageDecodeStatus::Ok ||
        decoded.frames.empty()) return !input.complete;
    const auto& image = decoded.frames.front();
    if (image.width != patch.width || image.height != patch.height) return false;
    if (patch.disposal == 2) previous = canvas;
    for (uint32_t y = 0; y < image.height; ++y) for (uint32_t x = 0; x < image.width; ++x) {
      const auto* source = &image.pixels[(size_t(y) * image.width + x) * 4];
      auto* target = &canvas[(size_t(y + patch.y) * input.width + x + patch.x) * 4];
      for (unsigned c = 0; c < 4; ++c)
        target[c] = uint8_t(patch.over ? std::min(255u, uint32_t(source[c]) +
            (uint32_t(target[c]) * (255 - source[3]) + 127) / 255) : source[c]);
    }
    domain::web_media::ImageFrame frame;
    frame.width = width && height ? width : input.width;
    frame.height = width && height ? height : input.height;
    if (uint64_t(frame.width) * frame.height * input.patches.size() > domain::web_media::kImagePixelLimit) return false;
    frame.pixels = canvas;
    if (frame.width != input.width || frame.height != input.height) {
      const auto native = SkImageInfo::Make(int(input.width), int(input.height), kBGRA_8888_SkColorType, kPremul_SkAlphaType);
      const auto resized = native.makeWH(int(frame.width), int(frame.height));
      frame.pixels.resize(resized.computeMinByteSize());
      if (!SkPixmap(native, canvas.data(), native.minRowBytes()).scalePixels(
          SkPixmap(resized, frame.pixels.data(), resized.minRowBytes()), SkSamplingOptions(SkFilterMode::kLinear))) return false;
    }
    frame.timestamp = timestamp;
    frame.duration = patch.duration;
    timestamp += patch.duration;
    frame.animated = true;
    frame.opaque = true;
    for (size_t i = 3; i < frame.pixels.size(); i += 4) if (frame.pixels[i] != 255) { frame.opaque = false; break; }
    result.frames.push_back(std::move(frame));
    if (patch.disposal == 1) for (uint32_t y = 0; y < patch.height; ++y)
      std::memset(&canvas[(size_t(y + patch.y) * input.width + patch.x) * 4], 0, size_t(patch.width) * 4);
    else if (patch.disposal == 2) canvas.swap(previous);
  }
  *out = std::move(result);
  return true;
}
}

#endif  // __linux__
