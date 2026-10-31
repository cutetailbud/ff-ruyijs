#if defined(__linux__)
// Firefox's Skia deliberately omits format decoders. Use bounded native codecs
// with an explicit RGBA -> sRGB -> premultiplied BGRA frame contract.
#ifdef RUYIJS_FIREFOX_SKIA
#include "web_media/image_codec.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSamplingOptions.h"
#include <webp/demux.h>
#include <lcms2.h>
#include <zlib.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <fstream>
#include <limits>
#include <memory>
namespace {
thread_local size_t codecBytes = 0;
constexpr size_t allocationLimit = 320 * 1024 * 1024;
union Block { size_t size; std::max_align_t alignment; };
void* CodecAlloc(size_t size) {
  if (size > allocationLimit - codecBytes) return nullptr;
  auto* value = static_cast<Block*>(malloc(sizeof(Block) + size));
  if (!value) return nullptr;
  value->size = size; codecBytes += size; return value + 1;
}
void CodecFree(void* data) {
  if (!data) return;
  auto* value = static_cast<Block*>(data) - 1;
  codecBytes -= value->size; free(value);
}
void* CodecResize(void* data, size_t size) {
  if (!data) return CodecAlloc(size);
  if (!size) { CodecFree(data); return nullptr; }
  auto* previous = static_cast<Block*>(data) - 1;
  const auto n = previous->size;
  if (size > allocationLimit - (codecBytes - n)) return nullptr;
  auto* value = static_cast<Block*>(realloc(previous, sizeof(Block) + size));
  if (!value) return nullptr;
  value->size = size; codecBytes = codecBytes - n + size; return value + 1;
}
}
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_GIF
#define STBI_ONLY_BMP
#define STBI_MAX_DIMENSIONS 32768
#define STBI_MALLOC(n) CodecAlloc(n)
#define STBI_REALLOC(p,n) CodecResize(p,n)
#define STBI_FREE(p) CodecFree(p)
#include <stb/stb_image.h>
namespace ruyijs::web_media {
namespace {
using Frame = domain::web_media::ImageFrame;
uint32_t BE(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
uint32_t LE(const uint8_t* p) { return uint32_t(p[3]) << 24 | uint32_t(p[2]) << 16 | uint32_t(p[1]) << 8 | p[0]; }
std::vector<uint8_t> Profile(const std::vector<uint8_t>& data) {
  if (data.size() > 8 && !memcmp(data.data(), "\x89PNG\r\n\x1a\n", 8)) {
    for (size_t pos = 8; pos + 12 <= data.size();) {
      const auto size = BE(data.data() + pos);
      if (size > data.size() - pos - 12) return {};
      if (!memcmp(data.data() + pos + 4, "iCCP", 4)) {
        size_t offset = pos + 8, end = offset + size;
        while (offset < end && data[offset]) ++offset;
        if (offset + 2 >= end || data[offset+1]) return {};
        offset += 2;
        std::vector<uint8_t> profile(16 * 1024 * 1024);
        uLongf length = profile.size();
        if (uncompress(profile.data(), &length, data.data() + offset, end - offset) != Z_OK) return {};
        profile.resize(length); return profile;
      }
      pos += size + 12;
    }
  } else if (data.size() > 4 && data[0] == 255 && data[1] == 216) {
    std::vector<std::vector<uint8_t>> pieces(256); unsigned total = 0;
    for (size_t pos = 2; pos + 4 <= data.size();) {
      if (data[pos] != 255 || data[pos+1] == 218) break;
      if (data[pos+1] == 255) { ++pos; continue; }
      const unsigned size = unsigned(data[pos+2]) * 256 + data[pos+3];
      if (size < 2 || size > data.size() - pos - 2) break;
      if (data[pos+1] == 226 && size >= 16 && !memcmp(data.data()+pos+4, "ICC_PROFILE\0", 12)) {
        const unsigned index = data[pos+16], count = data[pos+17];
        if (!index || index > count || !count || (total && total != count) || !pieces[index].empty()) return {};
        total = count; pieces[index].assign(data.begin()+pos+18, data.begin()+pos+size+2);
      }
      pos += size + 2;
    }
    std::vector<uint8_t> profile;
    for (unsigned i = 1; i <= total; ++i) {
      if (pieces[i].empty()) return {};
      profile.insert(profile.end(), pieces[i].begin(), pieces[i].end());
    }
    return profile;
  }
  return {};
}
void Convert(uint8_t* rgba, size_t count, const std::vector<uint8_t>& profile) {
  const auto* display = std::getenv("RUYIJS_COLOR_PROFILE");
  if (profile.empty() && (!display || !*display)) return;
  auto source = profile.empty() ? cmsCreate_sRGBProfile() : cmsOpenProfileFromMem(profile.data(), profile.size());
  auto target = display && *display ? cmsOpenProfileFromFile(display,"r") : cmsCreate_sRGBProfile();
  if (source && target) {
    auto transform = cmsCreateTransform(source, TYPE_RGBA_8, target, TYPE_RGBA_8, INTENT_PERCEPTUAL, cmsFLAGS_COPY_ALPHA);
    if (transform) { cmsDoTransform(transform, rgba, rgba, count); cmsDeleteTransform(transform); }
  }
  if (source) cmsCloseProfile(source);
  if (target) cmsCloseProfile(target);
}
bool Append(domain::web_media::ImageData& result, const uint8_t* rgba, int w, int h,
            uint32_t width, uint32_t height, uint64_t duration, const std::vector<uint8_t>& profile, bool noColor) {
  const auto n = size_t(w) * h;
  const uint32_t dw = width && height ? width : w, dh = width && height ? height : h;
  if (!dw || !dh || dw > 32768 || dh > 32768 || uint64_t(dw)*dh*(result.frames.size()+1) > domain::web_media::kImagePixelLimit) return false;
  std::vector<uint8_t> pixels(rgba, rgba + n*4);
  if (!noColor) Convert(pixels.data(), n, profile);
  for (size_t i = 0; i < pixels.size(); i += 4) {
    const auto r = pixels[i], g = pixels[i+1], b = pixels[i+2], a = pixels[i+3];
    pixels[i] = (unsigned(b)*a+127)/255; pixels[i+1] = (unsigned(g)*a+127)/255; pixels[i+2] = (unsigned(r)*a+127)/255;
  }
  Frame frame;
  frame.width = dw; frame.height = dh; frame.duration = duration;
  frame.timestamp = result.frames.empty() ? 0 : result.frames.back().timestamp + result.frames.back().duration;
  if (dw == uint32_t(w) && dh == uint32_t(h)) frame.pixels = std::move(pixels);
  else {
    frame.pixels.resize(size_t(dw)*dh*4);
    const auto info = SkImageInfo::Make(w,h,kBGRA_8888_SkColorType,kPremul_SkAlphaType);
    if (!SkPixmap(info,pixels.data(),w*4).scalePixels(
        SkPixmap(info.makeWH(dw,dh),frame.pixels.data(),dw*4),SkSamplingOptions(SkFilterMode::kLinear))) return false;
  }
  frame.opaque = true;
  for (size_t i = 3; i < frame.pixels.size(); i += 4) if (frame.pixels[i] != 255) { frame.opaque = false; break; }
  result.frames.push_back(std::move(frame)); return true;
}
}
bool ImageCodecSupports(const std::string& type) {
  return type == "image/avif" || type == "image/png" || type == "image/x-png" || type == "image/jpeg" ||
    type == "image/jpg" || type == "image/pjpeg" || type == "image/gif" || type == "image/webp" ||
    type == "image/bmp" || type == "image/x-ms-bmp" || type == "image/x-icon" || type == "image/vnd.microsoft.icon";
}
void ImageColorTransform(Frame* frame) {
  std::vector<uint8_t> rgba(frame->pixels);
  for (size_t i=0;i+3<rgba.size();i+=4) {
    const auto b=rgba[i],g=rgba[i+1],r=rgba[i+2],a=rgba[i+3];
    rgba[i]=a ? std::min(255u,unsigned(r)*255/a) : 0;
    rgba[i+1]=a ? std::min(255u,unsigned(g)*255/a) : 0;
    rgba[i+2]=a ? std::min(255u,unsigned(b)*255/a) : 0;
  }
  Convert(rgba.data(),rgba.size()/4,{});
  for (size_t i=0;i+3<rgba.size();i+=4) {
    const auto r=rgba[i],g=rgba[i+1],b=rgba[i+2],a=rgba[i+3];
    frame->pixels[i]=(unsigned(b)*a+127)/255;frame->pixels[i+1]=(unsigned(g)*a+127)/255;
    frame->pixels[i+2]=(unsigned(r)*a+127)/255;
  }
}
domain::web_media::ImageDecodeStatus DecodeImageBytes(const std::string& type, const std::vector<uint8_t>& data,
    uint32_t width, uint32_t height, bool noColor, domain::web_media::ImageData* output) {
  using Status = domain::web_media::ImageDecodeStatus;
  if (type == "image/avif") return DecodeAvifBytes(data,width,height,output);
  if (!ImageCodecSupports(type)) return Status::Unsupported;
  if (data.empty() || data.size() > domain::web_media::kEncodedImageLimit) return Status::InvalidData;
  try {
    domain::web_media::ImageData result;
    auto profile = noColor ? std::vector<uint8_t>() : Profile(data);
    const bool png = data.size() >= 8 && !memcmp(data.data(),"\x89PNG\r\n\x1a\n",8);
    const bool jpeg = data.size() >= 3 && data[0]==255 && data[1]==216 && data[2]==255;
    const bool gif = data.size() >= 6 && (!memcmp(data.data(),"GIF87a",6) || !memcmp(data.data(),"GIF89a",6));
    const bool bmp = data.size() >= 2 && data[0]=='B' && data[1]=='M';
    if (((type=="image/png" || type=="image/x-png") && !png) ||
        ((type=="image/jpeg" || type=="image/jpg" || type=="image/pjpeg") && !jpeg) ||
        (type=="image/gif" && !gif) || ((type=="image/bmp" || type=="image/x-ms-bmp") && !bmp)) return Status::InvalidData;
    if (type == "image/webp") {
      WebPData bytes{data.data(),data.size()}; WebPAnimDecoderOptions options;
      WebPAnimDecoderOptionsInit(&options); options.color_mode = MODE_RGBA;
      std::unique_ptr<WebPAnimDecoder, decltype(&WebPAnimDecoderDelete)> decoder(WebPAnimDecoderNew(&bytes,&options),WebPAnimDecoderDelete);
      WebPAnimInfo info{};
      if (!decoder || !WebPAnimDecoderGetInfo(decoder.get(),&info) || !info.frame_count || info.frame_count > 10000 ||
          uint64_t(info.canvas_width)*info.canvas_height*info.frame_count > domain::web_media::kImagePixelLimit) return Status::InvalidData;
      const auto* demux = WebPAnimDecoderGetDemuxer(decoder.get()); WebPChunkIterator icc{};
      if (!noColor && WebPDemuxGetChunk(demux,"ICCP",1,&icc)) {
        if (icc.chunk.size <= 16*1024*1024) profile.assign(icc.chunk.bytes,icc.chunk.bytes+icc.chunk.size);
        WebPDemuxReleaseChunkIterator(&icc);
      }
      result.repetitions = info.loop_count ? info.loop_count - 1 : std::numeric_limits<double>::infinity();
      int previous = 0;
      while (WebPAnimDecoderHasMoreFrames(decoder.get())) {
        uint8_t* rgba; int timestamp;
        if (!WebPAnimDecoderGetNext(decoder.get(),&rgba,&timestamp) ||
            !Append(result,rgba,info.canvas_width,info.canvas_height,width,height,uint64_t(timestamp-previous)*1000,profile,noColor)) return Status::InvalidData;
        previous = timestamp;
      }
    } else {
      const uint8_t* bytes = data.data(); size_t length = data.size(); std::vector<uint8_t> icon;
      if (type == "image/x-icon" || type == "image/vnd.microsoft.icon") {
        if (length < 22 || bytes[0] || bytes[1] || bytes[2] != 1 || bytes[3]) return Status::InvalidData;
        const auto offset = LE(bytes+18), size = LE(bytes+14);
        if (offset > length || size > length-offset) return Status::InvalidData;
        bytes += offset; length = size;
        if (length >= 40 && (LE(bytes)==40 || LE(bytes)==52 || LE(bytes)==56 || LE(bytes)==108 || LE(bytes)==124)) {
          const auto header = LE(bytes), colors = LE(bytes+32);
          const unsigned bits = unsigned(bytes[14]) + unsigned(bytes[15])*256;
          if (header > length || !bits || bits > 32) return Status::InvalidData;
          if (bits <= 8 && colors > (1u<<bits)) return Status::InvalidData;
          const auto palette = bits <= 8 ? (colors ? colors : 1u<<bits)*4 : 0;
          if (header + palette > length) return Status::InvalidData;
          icon.resize(14+length); icon[0]='B'; icon[1]='M';
          auto put = [&](size_t at,uint32_t value) { for (int i=0;i<4;++i) icon[at+i]=uint8_t(value>>(i*8)); };
          put(2,icon.size()); put(10,14+header+palette);
          memcpy(icon.data()+14,bytes,length); put(14+8,LE(bytes+8)/2);
          bytes=icon.data(); length=icon.size();
        }
      }
      int w=0,h=0,channels=0,frames=1; int* delays=nullptr;
      if (!stbi_info_from_memory(bytes,int(length),&w,&h,&channels) || w <= 0 || h <= 0 ||
          uint64_t(w)*h > domain::web_media::kImagePixelLimit) return Status::InvalidData;
      uint8_t* raw = type == "image/gif" ? stbi_load_gif_from_memory(bytes,int(length),&delays,&w,&h,&frames,&channels,4) :
          stbi_load_from_memory(bytes,int(length),&w,&h,&channels,4);
      std::unique_ptr<uint8_t,decltype(&CodecFree)> pixels(raw,CodecFree);
      std::unique_ptr<int,decltype(&CodecFree)> times(delays,CodecFree);
      if (!pixels || frames < 1 || frames > 10000 || uint64_t(w)*h*frames > domain::web_media::kImagePixelLimit) return Status::InvalidData;
      for (int i=0;i<frames;++i) if (!Append(result,raw+size_t(w)*h*4*i,w,h,width,height,delays ? uint64_t(delays[i])*1000 : 0,profile,noColor)) return Status::InvalidData;
    }
    result.animated=result.frames.size()>1;
    for (auto& frame:result.frames) frame.animated=result.animated;
    *output=std::move(result); return Status::Ok;
  } catch (const std::bad_alloc&) { return Status::OutOfMemory; }
}
}
#endif

#endif  // __linux__
