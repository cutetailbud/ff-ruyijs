#pragma once
#if defined(__linux__)
class SkTypeface;
using IDWriteFontFace = SkTypeface;
#else
struct IDWriteFontFace;
#endif
