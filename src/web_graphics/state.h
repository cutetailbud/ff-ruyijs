#ifndef RUYIJS_WEB_GRAPHICS_STATE_H
#define RUYIJS_WEB_GRAPHICS_STATE_H
#include <cstdint>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "js/RootingAPI.h"
#include "runtime/web_kit.h"
#include "web_graphics/model.h"

namespace ruyijs {
class PageHelpers;
namespace browser_environment { class Environment; }
namespace web_graphics {

// The WebGL object interfaces this domain creates. A created object carries its
// owner context and kind in reserved slots, so the context recognises its own
// objects without a second registry.
enum ObjectKind : int32_t {
  kBufferObject = 0, kTextureObject, kFramebufferObject, kRenderbufferObject,
  kProgramObject, kShaderObject, kUniformLocationObject, kQueryObject, kSamplerObject,
  kSyncObject, kTransformFeedbackObject, kVertexArrayObject, kObjectKindCount,
};

struct ParameterStateValue {
  int32_t name = 0;
  ParameterKind kind = ParameterKind::Number;
  int count = 0;
  double values[4] = {};
};

// One vertex attribute array binding: the pointer set by vertexAttribPointer and
// whether enableVertexAttribArray turned the array on (off in WebGL's initial state).
struct AttributeArray {
  int32_t size = 4, type = 0x1406, stride = 0, offset = 0;
  bool normalized = false, enabled = false;
};
// One uniform of the current program: the value uniform* stored by name, so
// getUniformLocation only has to hand back the name.
struct UniformSlot {
  std::string name;
  float value[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  int32_t count = 0;
  bool set = false;
};

// One WebGLRenderingContext: the single WebGL error, the two buffer binding
// points, the canvas it belongs to and the drawing buffer the software
// rasterizer fills. Created on the first successful getContext("webgl") and
// owned by the realm until teardown.
struct Context final {
  JS::PersistentRootedObject canvas, wrapper;
  JS::PersistentRootedObject arrayBinding, elementBinding;
  JS::PersistentRootedObject drawFramebuffer, readFramebuffer;
  JS::PersistentRootedObject extensions;
  // State backed by the existing Context owner; entries use WebGL pname enums.
  std::array<ParameterStateValue, 96> parameterState{};
  int32_t parameterStateCount = 0;
  int32_t error = 0;
  int32_t version = 1;
  int32_t drawingColorSpace = 0, unpackColorSpace = 0;
  // The linked program drawArrays executes, and the uniform values by name.
  JS::PersistentRootedObject currentProgram;
  std::array<AttributeArray, 16> attributeArrays{};
  std::vector<UniformSlot> uniforms;
  // The drawing buffer: premultiplied RGBA8, row 0 is the bottom row, exactly
  // the bytes readPixels reads and the surface handed to the canvas encoder.
  std::vector<uint8_t> drawingBuffer;
  int32_t drawingWidth = 0, drawingHeight = 0;
  // The canvas pixel store the drawing buffer is handed to (see surface.cpp).
  JS::PersistentRootedObject surfaceContext, surfaceImage, surfacePixels;
  int32_t surfaceWidth = 0, surfaceHeight = 0;
  // getContextAttributes(): fixed by the options of the getContext call that
  // created the context (CanvasRenderingContextHelper::GetOrCreateContext).
  Attributes attributes;
  explicit Context(JSContext* cx, int32_t v)
      : canvas(cx), wrapper(cx), arrayBinding(cx), elementBinding(cx), extensions(cx),
        drawFramebuffer(cx), readFramebuffer(cx), currentProgram(cx), surfaceContext(cx),
        surfaceImage(cx), surfacePixels(cx), version(v) {}
};

struct State {
  State(JSContext* cx, JS::HandleObject page, PageHelpers& helpers,
        browser_environment::Environment& environment);
  ~State();
  WebFace& Face(int32_t kind);
  Context* Find(JSObject* canvas, int32_t version) const;
  Context* FindOrCreate(JSObject* canvas, int32_t version);
  JSContext* cx;
  browser_environment::Environment* environment;
  WebKit kit;
  WebFace context, buffer, texture, framebuffer, renderbuffer, program, shader, uniform;
  WebFace context2, query, sampler, sync, transformFeedback, vertexArray, activeInfo, precision, event;
  JS::PersistentRootedObject eventMessages, eventFactory;
  JS::PersistentRootedObject extensionPrototypes;
  Model model;
  std::vector<std::unique_ptr<Context>> contexts;
};

extern const JSClass kContextClass, kContext2Class, kBufferClass, kTextureClass, kFramebufferClass,
    kRenderbufferClass, kProgramClass, kShaderClass, kUniformLocationClass;
bool InstallPublicSurface(State&);
bool InstallResultInterfaces(State&);
bool InstallExtensions(State&);
bool InstallContextEvent(State&);
bool PublicCall(JSContext*, unsigned, JS::Value*);
bool StateCall(JSContext*, unsigned, JS::Value*);
bool ClearCall(JSContext*, unsigned, JS::Value*);
bool FramebufferCall(JSContext*, unsigned, JS::Value*);
bool GetParameterState(const Context&, int32_t, ParameterStateValue*) noexcept;
void SetParameterState(Context&, int32_t, ParameterKind, int, const double*) noexcept;
bool ContextAttribute(JSContext*, unsigned, JS::Value*);
bool SetContextColorSpace(JSContext*, unsigned, JS::Value*);
bool GetShaderPrecision(JSContext*, unsigned, JS::Value*);
bool ExtraObjectCall(JSContext*, unsigned, JS::Value*);
bool NewExtension(State&, Context&, JS::HandleString, JS::MutableHandleValue);
// Whether getExtension(name) already returned an object for this context.
bool ExtensionEnabled(State&, Context&, const char* name, bool* enabled);
bool DefaultParameter(State&, Context&, int32_t, JS::MutableHandleValue, bool*);

// context.cpp: the WebGLRenderingContext constructor and its member table.
bool ContextCtor(JSContext*, unsigned, JS::Value*);
bool InstallContext(State&);
// params.cpp: getParameter/getSupportedExtensions/getExtension/getError.
bool GetParameter(JSContext*, unsigned, JS::Value*);
bool GetSupportedExtensions(JSContext*, unsigned, JS::Value*);
bool GetExtension(JSContext*, unsigned, JS::Value*);
bool GetError(JSContext*, unsigned, JS::Value*);
// objects.cpp: the WebGL object interfaces and the entry points that create,
// delete and query them, plus buffer binding and uniform lookup.
bool InstallObjects(State&);
JSObject* NewObject(State& state, int32_t kind, Context& context, JS::MutableHandleObject out);
bool CreateBuffer(JSContext*, unsigned, JS::Value*);
bool CreateTexture(JSContext*, unsigned, JS::Value*);
bool CreateFramebuffer(JSContext*, unsigned, JS::Value*);
bool CreateRenderbuffer(JSContext*, unsigned, JS::Value*);
bool CreateProgram(JSContext*, unsigned, JS::Value*);
bool CreateShader(JSContext*, unsigned, JS::Value*);
bool DeleteBuffer(JSContext*, unsigned, JS::Value*);
bool DeleteTexture(JSContext*, unsigned, JS::Value*);
bool DeleteFramebuffer(JSContext*, unsigned, JS::Value*);
bool DeleteRenderbuffer(JSContext*, unsigned, JS::Value*);
bool DeleteProgram(JSContext*, unsigned, JS::Value*);
bool DeleteShader(JSContext*, unsigned, JS::Value*);
bool IsBuffer(JSContext*, unsigned, JS::Value*);
bool IsTexture(JSContext*, unsigned, JS::Value*);
bool IsFramebuffer(JSContext*, unsigned, JS::Value*);
bool IsRenderbuffer(JSContext*, unsigned, JS::Value*);
bool IsProgram(JSContext*, unsigned, JS::Value*);
bool IsShader(JSContext*, unsigned, JS::Value*);
bool BindBuffer(JSContext*, unsigned, JS::Value*);
bool GetUniformLocation(JSContext*, unsigned, JS::Value*);
// draw_calls.cpp: the shader/program/buffer/attribute/uniform/draw entry points
// and the two parameter queries the draw probe reads.
bool DrawCall(JSContext*, unsigned, JS::Value*);
// draw.cpp: the software rasterizer that fills the drawing buffer.
void ResizeDrawingBuffer(Context&, int32_t width, int32_t height);
bool DrawTriangleStrip(State&, Context&, int32_t first, int32_t count);
// surface.cpp: hands the drawing buffer to the canvas pixel store the canvas PNG
// encoder reads, so canvas.toDataURL() of a WebGL canvas encodes it.
bool SyncCanvasSurface(State&, Context&);
// environment.cpp: the "webgl" profile group's configure and describe ports.
bool GraphicsPort(void* context, JS::HandleObject group);
bool GraphicsDescribe(void* context, JS::MutableHandleValue out);
}  // namespace web_graphics
}  // namespace ruyijs
#endif
