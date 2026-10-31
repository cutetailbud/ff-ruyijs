// Original zlib-rs C ABI; private prefix leaves existing MOZ_Z_ users unchanged.
#include "web_platform/compression_codec.h"
#if defined(__linux__)
#include <zlib.h>
#define RUYIJS_CS_Z_deflateInit2_ deflateInit2_
#define RUYIJS_CS_Z_inflateInit2_ inflateInit2_
#define RUYIJS_CS_Z_deflate deflate
#define RUYIJS_CS_Z_inflate inflate
#define RUYIJS_CS_Z_deflateEnd deflateEnd
#define RUYIJS_CS_Z_inflateEnd inflateEnd
#else
#include "../../vendor/compression-native/zlib-headers/zlib.h"
#endif
#include <new>
#include <limits>

extern "C" {
int RUYIJS_CS_Z_deflateInit2_(z_streamp,int,int,int,int,int,const char*,int);
int RUYIJS_CS_Z_inflateInit2_(z_streamp,int,const char*,int);
int RUYIJS_CS_Z_deflate(z_streamp,int);
int RUYIJS_CS_Z_inflate(z_streamp,int);
int RUYIJS_CS_Z_deflateEnd(z_streamp);
int RUYIJS_CS_Z_inflateEnd(z_streamp);
}
namespace ruyijs::web_platform {
#if defined(_WIN32)
static_assert(sizeof(z_stream)==88,"MSVC x64 original z_stream ABI");
#elif defined(__APPLE__)
static_assert(sizeof(z_stream)==112,"Darwin LP64 z_stream ABI");
#elif defined(__linux__)
static_assert(sizeof(z_stream)==112,"Linux LP64 z_stream ABI");
#else
#error unsupported z_stream ABI
#endif
void* OpenZlibCodec(CompressionFormat format,bool decompress) {
  auto* stream=new(std::nothrow) z_stream{};
  if (!stream) return nullptr;
  const int bits=format==CompressionFormat::Raw?-15:format==CompressionFormat::Gzip?31:15;
  const int status=decompress?RUYIJS_CS_Z_inflateInit2_(stream,bits,ZLIB_VERSION,sizeof(z_stream)):
      RUYIJS_CS_Z_deflateInit2_(stream,Z_DEFAULT_COMPRESSION,Z_DEFLATED,bits,8,Z_DEFAULT_STRATEGY,ZLIB_VERSION,sizeof(z_stream));
  if (status!=Z_OK) {delete stream;return nullptr;}
  return stream;
}
void CloseZlibCodec(void* handle,bool decompress) {
  auto* stream=static_cast<z_stream*>(handle);
  if (decompress) RUYIJS_CS_Z_inflateEnd(stream);else RUYIJS_CS_Z_deflateEnd(stream);
  delete stream;
}
CompressionStep StepZlib(CompressionCodec& codec,const uint8_t* input,size_t length,
    uint8_t* output,size_t capacity,bool flush) {
  CompressionStep result;
  auto* stream=static_cast<z_stream*>(codec.handle);
  if (length>std::numeric_limits<uInt>::max()) {result.error="Compression input exceeds the codec chunk limit";return result;}
  stream->next_in=const_cast<uint8_t*>(input);stream->avail_in=uInt(length);
  stream->next_out=output;stream->avail_out=uInt(capacity);
  const int status=codec.decompress?RUYIJS_CS_Z_inflate(stream,flush?Z_FINISH:Z_NO_FLUSH):
      RUYIJS_CS_Z_deflate(stream,flush?Z_FINISH:Z_NO_FLUSH);
  result.consumed=length-stream->avail_in;result.written=capacity-stream->avail_out;
  // JS allocation/enqueue follows this call. Keep no borrowed pointers in codec.
  stream->next_in=nullptr;stream->next_out=nullptr;
  if (status==Z_STREAM_END) codec.ended=true;
  if (codec.decompress && status==Z_DATA_ERROR) {result.error="The input data is corrupted: {0}";result.detail=stream->msg;}
  else if (codec.decompress && status==Z_MEM_ERROR) result.error="Out of memory";
  else if (codec.decompress && status==Z_NEED_DICT)
    result.error="The stream needs a preset dictionary but such setup is unsupported";
  else if (status!=Z_OK && status!=Z_STREAM_END && status!=Z_BUF_ERROR)
    result.error=codec.decompress?"Unexpected decompression error":"Unexpected compression error";
  result.again=result.written>0 && stream->avail_out==0 && (!codec.decompress || !codec.ended);
  return result;
}
}
