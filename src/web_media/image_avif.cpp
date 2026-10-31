// Source-owned libavif parses the container; dav1d performs actual AV1 decoding.
#include "web_media/image_codec.h"
#if defined(__linux__)
#include <avif/avif.h>
#else
#include "../../vendor/image-native/libavif/include/avif/avif.h"
#endif
#include <cmath>
#include <limits>
#include <memory>
#include <new>
namespace ruyijs::web_media {
domain::web_media::ImageDecodeStatus DecodeAvifBytes(const std::vector<uint8_t>& bytes,
    uint32_t width, uint32_t height, domain::web_media::ImageData* out) {
  using Status=domain::web_media::ImageDecodeStatus;
  std::unique_ptr<avifDecoder,decltype(&avifDecoderDestroy)> decoder(avifDecoderCreate(),avifDecoderDestroy);
  if(!decoder)return Status::OutOfMemory;
  decoder->maxThreads=2;decoder->imageSizeLimit=uint32_t(domain::web_media::kImagePixelLimit);
  decoder->imageCountLimit=10000;decoder->codecChoice=AVIF_CODEC_CHOICE_DAV1D;
  if(avifDecoderSetIOMemory(decoder.get(),bytes.data(),bytes.size())!=AVIF_RESULT_OK||
      avifDecoderParse(decoder.get())!=AVIF_RESULT_OK)return Status::InvalidData;
  out->animated=decoder->imageCount>1;
  out->repetitions=!out->animated||decoder->repetitionCount<0?std::numeric_limits<double>::infinity():decoder->repetitionCount;
  uint64_t total=0;
  try {
    for(int index=0;index<decoder->imageCount;++index) {
      if(avifDecoderNextImage(decoder.get())!=AVIF_RESULT_OK)return Status::InvalidData;
      if(width&&height&&avifImageScale(decoder->image,width,height,&decoder->diag)!=AVIF_RESULT_OK)return Status::InvalidData;
      const auto* image=decoder->image;
      total+=uint64_t(image->width)*image->height;if(total>domain::web_media::kImagePixelLimit)return Status::InvalidData;
      domain::web_media::ImageFrame frame;
      frame.width=image->width;frame.height=image->height;frame.opaque=!image->alphaPlane;
      frame.animated=out->animated;frame.timestamp=uint64_t(std::round(decoder->imageTiming.pts*1000000));
      frame.duration=uint64_t(std::round(decoder->imageTiming.duration*1000000));
      frame.pixels.resize(size_t(frame.width)*frame.height*4);
      avifRGBImage rgb;avifRGBImageSetDefaults(&rgb,image);
      rgb.depth=8;rgb.format=AVIF_RGB_FORMAT_BGRA;rgb.alphaPremultiplied=AVIF_TRUE;
      rgb.pixels=frame.pixels.data();rgb.rowBytes=frame.width*4;
      if(avifImageYUVToRGB(image,&rgb)!=AVIF_RESULT_OK)return Status::InvalidData;
      out->frames.push_back(std::move(frame));
    }
  } catch(const std::bad_alloc&) {return Status::OutOfMemory;}
  return Status::Ok;
}
}
