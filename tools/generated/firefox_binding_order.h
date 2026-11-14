// Generated from the donor's dom/bindings/*Binding.cpp; do not edit.
#pragma once
#include <cstddef>
namespace ruyijs {
struct BindingOrderEntry { const char* name; const char* const* members; size_t count; size_t constants; };
inline constexpr const char* kBindingOrder0[] = {"drawArraysInstancedANGLE", "drawElementsInstancedANGLE", "vertexAttribDivisorANGLE", "VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE"};
inline constexpr const char* kBindingOrder1[] = {"INVISIBLE", "VISIBLE", "IRREGULAR_AREA", "APZ_AWARE_LISTENERS", "INACTIVE_SCROLLFRAME", "PAN_X_DISABLED", "PAN_Y_DISABLED", "PINCH_ZOOM_DISABLED", "DOUBLE_TAP_ZOOM_DISABLED", "SCROLLBAR", "SCROLLBAR_THUMB", "SCROLLBAR_VERTICAL", "REQUIRES_TARGET_CONFIRMATION"};
inline constexpr const char* kBindingOrder2[] = {"abort", "signal"};
inline constexpr const char* kBindingOrder3[] = {"throwIfAborted", "aborted", "reason", "onabort"};
inline constexpr const char* kBindingOrder4[] = {"startContainer", "startOffset", "endContainer", "endOffset", "collapsed"};
inline constexpr const char* kBindingOrder5[] = {"is", "has", "get", "computedRole", "states", "attributes", "DOMNode", "role", "roleDescription", "label", "current", "autocomplete", "keyShortcuts", "modal", "multiline", "multiselectable", "orientation", "readOnly", "required", "sort", "placeholder", "valueMax", "valueMin", "valueNow", "valueText", "checked", "disabled", "expanded", "hasPopUp", "hidden", "invalid", "pressed", "selected", "atomic", "busy", "live", "relevant", "activeDescendant", "details", "errorMessage", "colCount", "colIndex", "colSpan", "level", "posInSet", "rowCount", "rowIndex", "rowSpan", "setSize"};
inline constexpr const char* kBindingOrder6[] = {"uninstall", "setEnabled", "id", "version", "type", "name", "description", "isEnabled", "isActive", "canUninstall"};
inline constexpr const char* kBindingOrder7[] = {"id"};
inline constexpr const char* kBindingOrder8[] = {"install", "cancel", "state", "error", "progress", "maxProgress"};
inline constexpr const char* kBindingOrder9[] = {"getAddonByID", "createInstall", "sendAbuseReport"};
inline constexpr const char* kBindingOrder10[] = {"getFloatFrequencyData", "getByteFrequencyData", "getFloatTimeDomainData", "getByteTimeDomainData", "fftSize", "frequencyBinCount", "minDecibels", "maxDecibels", "smoothingTimeConstant"};
inline constexpr const char* kBindingOrder11[] = {"cancel", "finish", "play", "pause", "updatePlaybackRate", "reverse", "persist", "commitStyles", "id", "effect", "timeline", "startTime", "currentTime", "rangeStart", "rangeEnd", "overallProgress", "playbackRate", "playState", "pending", "replaceState", "ready", "finished", "onfinish", "oncancel", "onremove"};
inline constexpr const char* kBindingOrder12[] = {"getTiming", "getComputedTiming", "updateTiming"};
inline constexpr const char* kBindingOrder13[] = {"animationName", "elapsedTime", "pseudoElement", "animation"};
inline constexpr const char* kBindingOrder14[] = {"currentTime", "timelineTime"};
inline constexpr const char* kBindingOrder15[] = {"currentTime", "duration"};
inline constexpr const char* kBindingOrder16[] = {"root"};
inline constexpr const char* kBindingOrder17[] = {"localName", "value", "name", "namespaceURI", "prefix", "specified", "ownerElement"};
inline constexpr const char* kBindingOrder18[] = {"getChannelData", "copyFromChannel", "copyToChannel", "sampleRate", "length", "duration", "numberOfChannels"};
inline constexpr const char* kBindingOrder19[] = {"start", "buffer", "playbackRate", "detune", "loop", "loopStart", "loopEnd"};
inline constexpr const char* kBindingOrder20[] = {"getOutputTimestamp", "suspend", "close", "createMediaElementSource", "createMediaStreamSource", "createMediaStreamTrackSource", "createMediaStreamDestination", "baseLatency", "outputLatency"};
inline constexpr const char* kBindingOrder21[] = {"allocationSize", "copyTo", "clone", "close", "format", "sampleRate", "numberOfFrames", "numberOfChannels", "duration", "timestamp"};
inline constexpr const char* kBindingOrder22[] = {"configure", "decode", "flush", "reset", "close", "state", "decodeQueueSize", "ondequeue"};
inline constexpr const char* kBindingOrder23[] = {"maxChannelCount"};
inline constexpr const char* kBindingOrder24[] = {"configure", "encode", "flush", "reset", "close", "mozRequestDebugInfo", "state", "encodeQueueSize", "ondequeue"};
inline constexpr const char* kBindingOrder25[] = {"setPosition", "setOrientation"};
inline constexpr const char* kBindingOrder26[] = {"connect", "disconnect", "context", "numberOfInputs", "numberOfOutputs", "channelCount", "channelCountMode", "channelInterpretation"};
inline constexpr const char* kBindingOrder27[] = {"setValueAtTime", "linearRampToValueAtTime", "exponentialRampToValueAtTime", "setTargetAtTime", "setValueCurveAtTime", "cancelScheduledValues", "value", "defaultValue", "minValue", "maxValue"};
inline constexpr const char* kBindingOrder28[] = {"entries", "keys", "values", "forEach", "has", "get", "size"};
inline constexpr const char* kBindingOrder29[] = {"playbackTime", "inputBuffer", "outputBuffer"};
inline constexpr const char* kBindingOrder30[] = {"start", "stop", "onended"};
inline constexpr const char* kBindingOrder31[] = {"type", "state", "onstatechange"};
inline constexpr const char* kBindingOrder32[] = {"id", "kind", "label", "language", "enabled"};
inline constexpr const char* kBindingOrder33[] = {"getTrackById", "@@iterator", "length", "onchange", "onaddtrack", "onremovetrack"};
inline constexpr const char* kBindingOrder34[] = {"port"};
inline constexpr const char* kBindingOrder35[] = {"registerProcessor", "currentFrame", "currentTime", "sampleRate", "port"};
inline constexpr const char* kBindingOrder36[] = {"parameters", "port", "onprocessorerror"};
inline constexpr const char* kBindingOrder37[] = {"port"};
inline constexpr const char* kBindingOrder38[] = {"authenticatorData", "signature", "userHandle"};
inline constexpr const char* kBindingOrder39[] = {"getTransports", "getAuthenticatorData", "getPublicKey", "getPublicKeyAlgorithm", "attestationObject"};
inline constexpr const char* kBindingOrder40[] = {"clientDataJSON"};
inline constexpr const char* kBindingOrder41[] = {"visible"};
inline constexpr const char* kBindingOrder42[] = {"resume", "createBuffer", "decodeAudioData", "createBufferSource", "createConstantSource", "createScriptProcessor", "createAnalyser", "createGain", "createDelay", "createBiquadFilter", "createIIRFilter", "createWaveShaper", "createPanner", "createStereoPanner", "createConvolver", "createChannelSplitter", "createChannelMerger", "createDynamicsCompressor", "createOscillator", "createPeriodicWave", "destination", "sampleRate", "currentTime", "listener", "state", "audioWorklet", "onstatechange"};
inline constexpr const char* kBindingOrder43[] = {"charging", "chargingTime", "dischargingTime", "level", "onchargingchange", "onchargingtimechange", "ondischargingtimechange", "onlevelchange"};
inline constexpr const char* kBindingOrder44[] = {"returnValue"};
inline constexpr const char* kBindingOrder45[] = {"getFrequencyResponse", "type", "frequency", "detune", "Q", "gain"};
inline constexpr const char* kBindingOrder46[] = {"slice", "stream", "text", "arrayBuffer", "bytes", "size", "type"};
inline constexpr const char* kBindingOrder47[] = {"data"};
inline constexpr const char* kBindingOrder48[] = {"postMessage", "close", "name", "onmessage", "onmessageerror"};
inline constexpr const char* kBindingOrder49[] = {"getAllBrowsingContextsInSubtree", "setScreenAreaOverride", "resetScreenAreaOverride", "setOrientationOverride", "resetOrientationOverride", "setMaxTouchPointsOverride", "resetMaxTouchPointsOverride", "setRDMPaneMaxTouchPoints", "setGeolocationServiceOverride", "resetNavigationRateLimit", "name", "parent", "top", "children", "docShell", "embedderElement", "id", "opener", "group", "window", "currentWindowContext", "parentWindowContext", "topWindowContext", "ancestorsAreCurrent", "customPlatform", "customUserAgent", "embedderElementType", "createdDynamically", "isInBFCache", "isDiscarded", "sandboxFlags", "isActive", "forceOffline", "isAppTab", "hasSiblings", "inRDMPane", "fullZoom", "textZoom", "overrideDPPX", "suspendMediaWhenInactive", "authorStyleDisabledDefault", "useGlobalHistory", "watchedByDevTools", "serviceWorkersTestingEnabled", "mediumOverride", "languageOverride", "timezoneOverride", "prefersColorSchemeOverride", "prefersReducedMotionOverride", "forcedColorsOverride", "prefersContrastOverride", "animationsPlayBackRateMultiplier", "browserId", "displayMode", "touchEventsOverride", "targetTopLevelLinkClicksToBlank", "allowJavascript", "forceDesktopViewport", "defaultLoadFlags", "historyID", "childSessionHistory", "childOffset", "isDocumentPiP", "associatedWindow", "topWindow", "topFrameElement", "isContent", "usePrivateBrowsing", "useRemoteTabs", "useRemoteSubframes", "useTrackingProtection", "originAttributes"};
inline constexpr const char* kBindingOrder50[] = {"getToplevels", "id"};
inline constexpr const char* kBindingOrder51[] = {"highWaterMark", "size"};
inline constexpr const char* kBindingOrder52[] = {"toJSON", "documentURL", "referrer", "blockedURL", "effectiveDirective", "originalPolicy", "sourceFile", "sample", "disposition", "statusCode", "lineNumber", "columnNumber"};
inline constexpr const char* kBindingOrder53[] = {"animationName"};
inline constexpr const char* kBindingOrder54[] = {"colorSpace", "channels", "alpha"};
inline constexpr const char* kBindingOrder55[] = {"conditionText"};
inline constexpr const char* kBindingOrder56[] = {"containerName", "containerQuery", "conditions"};
inline constexpr const char* kBindingOrder57[] = {"name", "system", "negative", "prefix", "suffix", "range", "pad", "fallback", "symbols", "additiveSymbols", "speakAs"};
inline constexpr const char* kBindingOrder58[] = {"name", "query"};
inline constexpr const char* kBindingOrder59[] = {"propertyDefinition"};
inline constexpr const char* kBindingOrder60[] = {"@@iterator", "fontFamily", "font-family", "src", "fontStyle", "font-style", "fontWeight", "font-weight", "fontWidth", "font-width", "font-stretch", "fontStretch", "fontDisplay", "font-display", "unicodeRange", "unicode-range", "fontFeatureSettings", "font-feature-settings", "fontVariationSettings", "font-variation-settings", "fontLanguageOverride", "font-language-override", "ascentOverride", "ascent-override", "descentOverride", "descent-override", "lineGapOverride", "line-gap-override", "sizeAdjust", "size-adjust"};
inline constexpr const char* kBindingOrder61[] = {"style"};
inline constexpr const char* kBindingOrder62[] = {"fontFamily", "valueText"};
inline constexpr const char* kBindingOrder63[] = {"name", "fontFamily", "basePalette", "overrideColors"};
inline constexpr const char* kBindingOrder64[] = {"insertRule", "deleteRule", "cssRules"};
inline constexpr const char* kBindingOrder65[] = {"h", "s", "l", "alpha"};
inline constexpr const char* kBindingOrder66[] = {"h", "w", "b", "alpha"};
inline constexpr const char* kBindingOrder67[] = {"href", "media", "styleSheet", "layerName", "supportsText"};
inline constexpr const char* kBindingOrder68[] = {"keyText", "style"};
inline constexpr const char* kBindingOrder69[] = {"appendRule", "deleteRule", "findRule", "@@iterator", "name", "cssRules", "length"};
inline constexpr const char* kBindingOrder70[] = {"value"};
inline constexpr const char* kBindingOrder71[] = {"l", "c", "h", "alpha"};
inline constexpr const char* kBindingOrder72[] = {"l", "a", "b", "alpha"};
inline constexpr const char* kBindingOrder73[] = {"name"};
inline constexpr const char* kBindingOrder74[] = {"nameList"};
inline constexpr const char* kBindingOrder75[] = {"name", "style"};
inline constexpr const char* kBindingOrder76[] = {"lower", "value", "upper"};
inline constexpr const char* kBindingOrder77[] = {"value"};
inline constexpr const char* kBindingOrder78[] = {"values"};
inline constexpr const char* kBindingOrder79[] = {"values"};
inline constexpr const char* kBindingOrder80[] = {"value"};
inline constexpr const char* kBindingOrder81[] = {"values"};
inline constexpr const char* kBindingOrder82[] = {"values"};
inline constexpr const char* kBindingOrder83[] = {"operator"};
inline constexpr const char* kBindingOrder84[] = {"matrix"};
inline constexpr const char* kBindingOrder85[] = {"media"};
inline constexpr const char* kBindingOrder86[] = {"namespaceURI", "prefix"};
inline constexpr const char* kBindingOrder87[] = {"style"};
inline constexpr const char* kBindingOrder88[] = {"@@iterator", "keys", "values", "entries", "forEach", "length"};
inline constexpr const char* kBindingOrder89[] = {"add", "sub", "mul", "div", "min", "max", "equals", "to", "toSum", "type"};
inline constexpr const char* kBindingOrder90[] = {"l", "c", "h", "alpha"};
inline constexpr const char* kBindingOrder91[] = {"l", "a", "b", "alpha"};
inline constexpr const char* kBindingOrder92[] = {"@@iterator", "pageOrientation", "page-orientation", "size", "marginBottom", "margin-bottom", "marginLeft", "margin-left", "marginRight", "margin-right", "marginTop", "margin-top", "margin", "marginBlock", "margin-block", "marginInline", "margin-inline", "paddingBlock", "padding-block", "paddingInline", "padding-inline"};
inline constexpr const char* kBindingOrder93[] = {"selectorText", "style"};
inline constexpr const char* kBindingOrder94[] = {"length"};
inline constexpr const char* kBindingOrder95[] = {"@@iterator", "positionArea", "position-area", "alignSelf", "align-self", "justifySelf", "justify-self", "positionAnchor", "position-anchor", "maxBlockSize", "max-block-size", "maxHeight", "max-height", "maxInlineSize", "max-inline-size", "maxWidth", "max-width", "bottom", "insetBlockEnd", "inset-block-end", "insetBlockStart", "inset-block-start", "insetInlineEnd", "inset-inline-end", "insetInlineStart", "inset-inline-start", "left", "right", "top", "marginBlockEnd", "margin-block-end", "marginBlockStart", "margin-block-start", "marginBottom", "margin-bottom", "marginInlineEnd", "margin-inline-end", "marginInlineStart", "margin-inline-start", "marginLeft", "margin-left", "marginRight", "margin-right", "marginTop", "margin-top", "blockSize", "block-size", "height", "inlineSize", "inline-size", "minBlockSize", "min-block-size", "minHeight", "min-height", "minInlineSize", "min-inline-size", "minWidth", "min-width", "width", "margin", "marginBlock", "margin-block", "marginInline", "margin-inline", "paddingBlock", "padding-block", "paddingInline", "padding-inline", "inset", "insetBlock", "inset-block", "insetInline", "inset-inline", "placeSelf", "place-self", "webkitAlignSelf", "WebkitAlignSelf", "-webkit-align-self", "MozMarginStart", "-moz-margin-start", "MozMarginEnd", "-moz-margin-end"};
inline constexpr const char* kBindingOrder96[] = {"name", "style"};
inline constexpr const char* kBindingOrder97[] = {"name", "syntax", "inherits", "initialValue"};
inline constexpr const char* kBindingOrder98[] = {"type", "element"};
inline constexpr const char* kBindingOrder99[] = {"r", "g", "b", "alpha"};
inline constexpr const char* kBindingOrder100[] = {"x", "y", "z", "angle"};
inline constexpr const char* kBindingOrder101[] = {"type", "cssText", "parentRule", "parentStyleSheet", "STYLE_RULE", "CHARSET_RULE", "IMPORT_RULE", "MEDIA_RULE", "FONT_FACE_RULE", "PAGE_RULE", "NAMESPACE_RULE", "KEYFRAMES_RULE", "KEYFRAME_RULE", "COUNTER_STYLE_RULE", "SUPPORTS_RULE", "FONT_FEATURE_VALUES_RULE"};
inline constexpr const char* kBindingOrder102[] = {"item", "@@iterator", "length"};
inline constexpr const char* kBindingOrder103[] = {"x", "y", "z"};
inline constexpr const char* kBindingOrder104[] = {"start", "end"};
inline constexpr const char* kBindingOrder105[] = {"ax", "ay"};
inline constexpr const char* kBindingOrder106[] = {"ax"};
inline constexpr const char* kBindingOrder107[] = {"ay"};
inline constexpr const char* kBindingOrder108[] = {"item", "getPropertyValue", "getPropertyPriority", "setProperty", "removeProperty", "@@iterator", "cssText", "length", "parentRule"};
inline constexpr const char* kBindingOrder109[] = {"@@iterator", "alignItems", "align-items", "alignmentBaseline", "alignment-baseline", "backfaceVisibility", "backface-visibility", "baselineSource", "baseline-source", "borderCollapse", "border-collapse", "borderImageRepeat", "border-image-repeat", "boxDecorationBreak", "box-decoration-break", "boxSizing", "box-sizing", "breakInside", "break-inside", "captionSide", "caption-side", "clear", "columnFill", "column-fill", "columnSpan", "column-span", "contain", "containerType", "container-type", "contentVisibility", "content-visibility", "direction", "display", "dominantBaseline", "dominant-baseline", "emptyCells", "empty-cells", "fieldSizing", "field-sizing", "flexDirection", "flex-direction", "flexWrap", "flex-wrap", "cssFloat", "float", "fontKerning", "font-kerning", "fontLanguageOverride", "font-language-override", "fontOpticalSizing", "font-optical-sizing", "fontSynthesisStyle", "font-synthesis-style", "fontVariantCaps", "font-variant-caps", "fontVariantEastAsian", "font-variant-east-asian", "fontVariantEmoji", "font-variant-emoji", "fontVariantLigatures", "font-variant-ligatures", "fontVariantNumeric", "font-variant-numeric", "fontVariantPosition", "font-variant-position", "forcedColorAdjust", "forced-color-adjust", "gridAutoFlow", "grid-auto-flow", "hyphens", "imageOrientation", "image-orientation", "imageRendering", "image-rendering", "imeMode", "ime-mode", "isolation", "justifyItems", "justify-items", "lineBreak", "line-break", "listStylePosition", "list-style-position", "marginTrim", "margin-trim", "maskType", "mask-type", "mathShift", "math-shift", "mathStyle", "math-style", "mixBlendMode", "mix-blend-mode", "MozBoxAlign", "-moz-box-align", "MozBoxCollapse", "-moz-box-collapse", "MozBoxDirection", "-moz-box-direction", "MozBoxOrient", "-moz-box-orient", "MozBoxPack", "-moz-box-pack", "MozControlCharacterVisibility", "-moz-control-character-visibility", "MozFloatEdge", "-moz-float-edge", "MozImageDecoding", "-moz-image-decoding", "MozOrient", "-moz-orient", "MozOsxFontSmoothing", "-moz-osx-font-smoothing", "WebkitFontSmoothing", "-webkit-font-smoothing", "webkitFontSmoothing", "MozTextSizeAdjust", "-moz-text-size-adjust", "MozTheme", "-moz-theme", "MozUserFocus", "-moz-user-focus", "MozWindowDragging", "-moz-window-dragging", "MozWindowShadow", "-moz-window-shadow", "objectFit", "object-fit", "outlineStyle", "outline-style", "overflowAnchor", "overflow-anchor", "overflowWrap", "overflow-wrap", "paintOrder", "paint-order", "pointerEvents", "pointer-events", "position", "positionArea", "position-area", "positionTryOrder", "position-try-order", "positionVisibility", "position-visibility", "printColorAdjust", "print-color-adjust", "resize", "rubyAlign", "ruby-align", "rubyPosition", "ruby-position", "scrollBehavior", "scroll-behavior", "scrollSnapAlign", "scroll-snap-align", "scrollSnapStop", "scroll-snap-stop", "scrollSnapType", "scroll-snap-type", "scrollbarGutter", "scrollbar-gutter", "scrollbarWidth", "scrollbar-width", "shapeRendering", "shape-rendering", "strokeLinecap", "stroke-linecap", "strokeLinejoin", "stroke-linejoin", "tableLayout", "table-layout", "textAlign", "text-align", "textAlignLast", "text-align-last", "textAnchor", "text-anchor", "textAutospace", "text-autospace", "textBoxEdge", "text-box-edge", "textBoxTrim", "text-box-trim", "textCombineUpright", "text-combine-upright", "textDecorationLine", "text-decoration-line", "textDecorationSkipInk", "text-decoration-skip-ink", "textDecorationStyle", "text-decoration-style", "textEmphasisPosition", "text-emphasis-position", "textJustify", "text-justify", "textOrientation", "text-orientation", "textRendering", "text-rendering", "textTransform", "text-transform", "textUnderlinePosition", "text-underline-position", "textWrapMode", "text-wrap-mode", "textWrapStyle", "text-wrap-style", "touchAction", "touch-action", "transformBox", "transform-box", "transformStyle", "transform-style", "unicodeBidi", "unicode-bidi", "userSelect", "user-select", "vectorEffect", "vector-effect", "visibility", "webkitTextSecurity", "WebkitTextSecurity", "-webkit-text-security", "whiteSpaceCollapse", "white-space-collapse", "wordBreak", "word-break", "writingMode", "writing-mode", "appearance", "MozDefaultAppearance", "-moz-default-appearance", "MozForceBrokenImageIcon", "-moz-force-broken-image-icon", "MozSubtreeHiddenOnlyVisually", "-moz-subtree-hidden-only-visually", "breakAfter", "break-after", "breakBefore", "break-before", "colorInterpolation", "color-interpolation", "colorInterpolationFilters", "color-interpolation-filters", "alignContent", "align-content", "justifyContent", "justify-content", "clipRule", "clip-rule", "fillRule", "fill-rule", "alignSelf", "align-self", "justifySelf", "justify-self", "fontSynthesisPosition", "font-synthesis-position", "fontSynthesisSmallCaps", "font-synthesis-small-caps", "fontSynthesisWeight", "font-synthesis-weight", "overflowBlock", "overflow-block", "overflowInline", "overflow-inline", "overflowX", "overflow-x", "overflowY", "overflow-y", "overscrollBehaviorBlock", "overscroll-behavior-block", "overscrollBehaviorInline", "overscroll-behavior-inline", "overscrollBehaviorX", "overscroll-behavior-x", "overscrollBehaviorY", "overscroll-behavior-y", "borderBlockEndStyle", "border-block-end-style", "borderBlockStartStyle", "border-block-start-style", "borderBottomStyle", "border-bottom-style", "borderInlineEndStyle", "border-inline-end-style", "borderInlineStartStyle", "border-inline-start-style", "borderLeftStyle", "border-left-style", "borderRightStyle", "border-right-style", "borderTopStyle", "border-top-style", "columnRuleStyle", "column-rule-style", "accentColor", "accent-color", "anchorName", "anchor-name", "animationComposition", "animation-composition", "animationDelay", "animation-delay", "animationDirection", "animation-direction", "animationDuration", "animation-duration", "animationFillMode", "animation-fill-mode", "animationIterationCount", "animation-iteration-count", "animationName", "animation-name", "animationPlayState", "animation-play-state", "animationRangeEnd", "animation-range-end", "animationRangeStart", "animation-range-start", "animationTimeline", "animation-timeline", "animationTimingFunction", "animation-timing-function", "aspectRatio", "aspect-ratio", "backdropFilter", "backdrop-filter", "backgroundAttachment", "background-attachment", "backgroundBlendMode", "background-blend-mode", "backgroundClip", "background-clip", "backgroundImage", "background-image", "backgroundOrigin", "background-origin", "backgroundPositionX", "background-position-x", "backgroundPositionY", "background-position-y", "backgroundRepeat", "background-repeat", "backgroundSize", "background-size", "baselineShift", "baseline-shift", "borderImageOutset", "border-image-outset", "borderImageSlice", "border-image-slice", "borderImageWidth", "border-image-width", "borderSpacing", "border-spacing", "boxShadow", "box-shadow", "caretColor", "caret-color", "clip", "clipPath", "clip-path", "color", "colorScheme", "color-scheme", "columnCount", "column-count", "containerName", "container-name", "content", "counterIncrement", "counter-increment", "counterReset", "counter-reset", "counterSet", "counter-set", "cursor", "d", "filter", "fitTolerance", "fit-tolerance", "flow-tolerance", "flowTolerance", "flexBasis", "flex-basis", "fontFamily", "font-family", "fontFeatureSettings", "font-feature-settings", "fontPalette", "font-palette", "fontSize", "font-size", "fontSizeAdjust", "font-size-adjust", "fontStyle", "font-style", "fontVariantAlternates", "font-variant-alternates", "fontVariationSettings", "font-variation-settings", "fontWeight", "font-weight", "fontWidth", "font-width", "gridTemplateAreas", "grid-template-areas", "hyphenateCharacter", "hyphenate-character", "hyphenateLimitChars", "hyphenate-limit-chars", "initialLetter", "initial-letter", "letterSpacing", "letter-spacing", "lineClamp", "line-clamp", "lineHeight", "line-height", "linkParameters", "link-parameters", "listStyleType", "list-style-type", "maskClip", "mask-clip", "maskComposite", "mask-composite", "maskImage", "mask-image", "maskMode", "mask-mode", "maskOrigin", "mask-origin", "maskPositionX", "mask-position-x", "maskPositionY", "mask-position-y", "maskRepeat", "mask-repeat", "maskSize", "mask-size", "mathDepth", "math-depth", "MozContextProperties", "-moz-context-properties", "offsetAnchor", "offset-anchor", "offsetPath", "offset-path", "offsetPosition", "offset-position", "offsetRotate", "offset-rotate", "outlineOffset", "outline-offset", "overflowClipMargin", "overflow-clip-margin", "page", "perspective", "positionAnchor", "position-anchor", "positionTryFallbacks", "position-try-fallbacks", "quotes", "rotate", "scale", "scrollTimelineAxis", "scroll-timeline-axis", "scrollTimelineName", "scroll-timeline-name", "scrollbarColor", "scrollbar-color", "shapeOutside", "shape-outside", "strokeDasharray", "stroke-dasharray", "strokeDashoffset", "stroke-dashoffset", "strokeWidth", "stroke-width", "tabSize", "tab-size", "textDecorationInset", "text-decoration-inset", "textDecorationThickness", "text-decoration-thickness", "textEmphasisStyle", "text-emphasis-style", "textIndent", "text-indent", "textOverflow", "text-overflow", "textShadow", "text-shadow", "textUnderlineOffset", "text-underline-offset", "transformOrigin", "transform-origin", "transitionBehavior", "transition-behavior", "transitionDelay", "transition-delay", "transitionDuration", "transition-duration", "transitionProperty", "transition-property", "transitionTimingFunction", "transition-timing-function", "translate", "viewTimelineAxis", "view-timeline-axis", "viewTimelineInset", "view-timeline-inset", "viewTimelineName", "view-timeline-name", "viewTransitionClass", "view-transition-class", "viewTransitionName", "view-transition-name", "webkitTextStrokeWidth", "WebkitTextStrokeWidth", "-webkit-text-stroke-width", "willChange", "will-change", "wordSpacing", "word-spacing", "zIndex", "z-index", "zoom", "objectPosition", "object-position", "perspectiveOrigin", "perspective-origin", "fill", "stroke", "gridTemplateColumns", "grid-template-columns", "gridTemplateRows", "grid-template-rows", "borderImageSource", "border-image-source", "listStyleImage", "list-style-image", "gridAutoColumns", "grid-auto-columns", "gridAutoRows", "grid-auto-rows", "fillOpacity", "fill-opacity", "strokeOpacity", "stroke-opacity", "anchorScope", "anchor-scope", "timelineScope", "timeline-scope", "MozScrollbarInsetBlock", "-moz-scrollbar-inset-block", "MozScrollbarInsetInline", "-moz-scrollbar-inset-inline", "MozWindowTransform", "-moz-window-transform", "transform", "columnWidth", "column-width", "MozLineScrollAmount", "-moz-line-scroll-amount", "columnGap", "column-gap", "rowGap", "row-gap", "MozBoxOrdinalGroup", "-moz-box-ordinal-group", "order", "markerEnd", "marker-end", "markerMid", "marker-mid", "markerStart", "marker-start", "containIntrinsicBlockSize", "contain-intrinsic-block-size", "containIntrinsicHeight", "contain-intrinsic-height", "containIntrinsicInlineSize", "contain-intrinsic-inline-size", "containIntrinsicWidth", "contain-intrinsic-width", "gridColumnEnd", "grid-column-end", "gridColumnStart", "grid-column-start", "gridRowEnd", "grid-row-end", "gridRowStart", "grid-row-start", "maxBlockSize", "max-block-size", "maxHeight", "max-height", "maxInlineSize", "max-inline-size", "maxWidth", "max-width", "flexGrow", "flex-grow", "flexShrink", "flex-shrink", "MozBoxFlex", "-moz-box-flex", "strokeMiterlimit", "stroke-miterlimit", "cx", "cy", "offsetDistance", "offset-distance", "x", "y", "floodOpacity", "flood-opacity", "MozWindowOpacity", "-moz-window-opacity", "opacity", "shapeImageThreshold", "shape-image-threshold", "stopOpacity", "stop-opacity", "borderBottomLeftRadius", "border-bottom-left-radius", "borderBottomRightRadius", "border-bottom-right-radius", "borderEndEndRadius", "border-end-end-radius", "borderEndStartRadius", "border-end-start-radius", "borderStartEndRadius", "border-start-end-radius", "borderStartStartRadius", "border-start-start-radius", "borderTopLeftRadius", "border-top-left-radius", "borderTopRightRadius", "border-top-right-radius", "cornerBottomLeftShape", "corner-bottom-left-shape", "cornerBottomRightShape", "corner-bottom-right-shape", "cornerEndEndShape", "corner-end-end-shape", "cornerEndStartShape", "corner-end-start-shape", "cornerStartEndShape", "corner-start-end-shape", "cornerStartStartShape", "corner-start-start-shape", "cornerTopLeftShape", "corner-top-left-shape", "cornerTopRightShape", "corner-top-right-shape", "bottom", "insetBlockEnd", "inset-block-end", "insetBlockStart", "inset-block-start", "insetInlineEnd", "inset-inline-end", "insetInlineStart", "inset-inline-start", "left", "right", "top", "marginBlockEnd", "margin-block-end", "marginBlockStart", "margin-block-start", "marginBottom", "margin-bottom", "marginInlineEnd", "margin-inline-end", "marginInlineStart", "margin-inline-start", "marginLeft", "margin-left", "marginRight", "margin-right", "marginTop", "margin-top", "blockSize", "block-size", "height", "inlineSize", "inline-size", "minBlockSize", "min-block-size", "minHeight", "min-height", "minInlineSize", "min-inline-size", "minWidth", "min-width", "width", "MozWindowInputRegionMargin", "-moz-window-input-region-margin", "scrollMarginBlockEnd", "scroll-margin-block-end", "scrollMarginBlockStart", "scroll-margin-block-start", "scrollMarginBottom", "scroll-margin-bottom", "scrollMarginInlineEnd", "scroll-margin-inline-end", "scrollMarginInlineStart", "scroll-margin-inline-start", "scrollMarginLeft", "scroll-margin-left", "scrollMarginRight", "scroll-margin-right", "scrollMarginTop", "scroll-margin-top", "borderBlockEndWidth", "border-block-end-width", "borderBlockStartWidth", "border-block-start-width", "borderBottomWidth", "border-bottom-width", "borderInlineEndWidth", "border-inline-end-width", "borderInlineStartWidth", "border-inline-start-width", "borderLeftWidth", "border-left-width", "borderRightWidth", "border-right-width", "borderTopWidth", "border-top-width", "columnRuleWidth", "column-rule-width", "outlineWidth", "outline-width", "paddingBlockEnd", "padding-block-end", "paddingBlockStart", "padding-block-start", "paddingBottom", "padding-bottom", "paddingInlineEnd", "padding-inline-end", "paddingInlineStart", "padding-inline-start", "paddingLeft", "padding-left", "paddingRight", "padding-right", "paddingTop", "padding-top", "r", "shapeMargin", "shape-margin", "rx", "ry", "scrollPaddingBlockEnd", "scroll-padding-block-end", "scrollPaddingBlockStart", "scroll-padding-block-start", "scrollPaddingBottom", "scroll-padding-bottom", "scrollPaddingInlineEnd", "scroll-padding-inline-end", "scrollPaddingInlineStart", "scroll-padding-inline-start", "scrollPaddingLeft", "scroll-padding-left", "scrollPaddingRight", "scroll-padding-right", "scrollPaddingTop", "scroll-padding-top", "backgroundColor", "background-color", "borderBlockEndColor", "border-block-end-color", "borderBlockStartColor", "border-block-start-color", "borderBottomColor", "border-bottom-color", "borderInlineEndColor", "border-inline-end-color", "borderInlineStartColor", "border-inline-start-color", "borderLeftColor", "border-left-color", "borderRightColor", "border-right-color", "borderTopColor", "border-top-color", "columnRuleColor", "column-rule-color", "floodColor", "flood-color", "lightingColor", "lighting-color", "outlineColor", "outline-color", "stopColor", "stop-color", "textDecorationColor", "text-decoration-color", "textEmphasisColor", "text-emphasis-color", "webkitTextFillColor", "WebkitTextFillColor", "-webkit-text-fill-color", "webkitTextStrokeColor", "WebkitTextStrokeColor", "-webkit-text-stroke-color", "margin", "marginBlock", "margin-block", "marginInline", "margin-inline", "scrollMargin", "scroll-margin", "scrollMarginBlock", "scroll-margin-block", "scrollMarginInline", "scroll-margin-inline", "padding", "paddingBlock", "padding-block", "paddingInline", "padding-inline", "scrollPadding", "scroll-padding", "scrollPaddingBlock", "scroll-padding-block", "scrollPaddingInline", "scroll-padding-inline", "borderColor", "border-color", "borderStyle", "border-style", "borderWidth", "border-width", "borderBlockColor", "border-block-color", "borderInlineColor", "border-inline-color", "borderBlockWidth", "border-block-width", "borderInlineWidth", "border-inline-width", "borderBlockStyle", "border-block-style", "borderInlineStyle", "border-inline-style", "borderTop", "border-top", "borderRight", "border-right", "borderBottom", "border-bottom", "borderLeft", "border-left", "borderBlockStart", "border-block-start", "borderBlockEnd", "border-block-end", "borderInlineStart", "border-inline-start", "borderInlineEnd", "border-inline-end", "borderBlock", "border-block", "borderInline", "border-inline", "borderImage", "border-image", "border", "overflow", "overscrollBehavior", "overscroll-behavior", "container", "verticalAlign", "vertical-align", "pageBreakBefore", "page-break-before", "pageBreakAfter", "page-break-after", "pageBreakInside", "page-break-inside", "offset", "webkitPerspective", "WebkitPerspective", "-webkit-perspective", "webkitTransform", "WebkitTransform", "-webkit-transform", "background", "inset", "insetBlock", "inset-block", "insetInline", "inset-inline", "containIntrinsicSize", "contain-intrinsic-size", "flexFlow", "flex-flow", "flex", "gap", "gridRow", "grid-row", "gridColumn", "grid-column", "gridArea", "grid-area", "gridTemplate", "grid-template", "grid", "placeContent", "place-content", "placeSelf", "place-self", "placeItems", "place-items", "positionTry", "position-try", "marker", "transition", "animation", "scrollTimeline", "scroll-timeline", "viewTimeline", "view-timeline", "animationRange", "animation-range", "listStyle", "list-style", "backgroundPosition", "background-position", "mask", "maskPosition", "mask-position", "borderRadius", "border-radius", "cornerShape", "corner-shape", "cornerTopShape", "corner-top-shape", "cornerRightShape", "corner-right-shape", "cornerBottomShape", "corner-bottom-shape", "cornerLeftShape", "corner-left-shape", "cornerBlockStartShape", "corner-block-start-shape", "cornerBlockEndShape", "corner-block-end-shape", "cornerInlineStartShape", "corner-inline-start-shape", "cornerInlineEndShape", "corner-inline-end-shape", "outline", "columns", "columnRule", "column-rule", "textDecoration", "text-decoration", "font", "fontVariant", "font-variant", "fontSynthesis", "font-synthesis", "textBox", "text-box", "textEmphasis", "text-emphasis", "textWrap", "text-wrap", "whiteSpace", "white-space", "webkitTextStroke", "WebkitTextStroke", "-webkit-text-stroke", "webkitLineClamp", "WebkitLineClamp", "-webkit-line-clamp", "MozAppearance", "-moz-appearance", "all", "webkitBoxFlex", "WebkitBoxFlex", "-webkit-box-flex", "webkitBoxOrdinalGroup", "WebkitBoxOrdinalGroup", "-webkit-box-ordinal-group", "webkitAlignContent", "WebkitAlignContent", "-webkit-align-content", "webkitAlignItems", "WebkitAlignItems", "-webkit-align-items", "webkitAlignSelf", "WebkitAlignSelf", "-webkit-align-self", "MozAnimationDelay", "-moz-animation-delay", "webkitAnimationDelay", "WebkitAnimationDelay", "-webkit-animation-delay", "MozAnimationDirection", "-moz-animation-direction", "webkitAnimationDirection", "WebkitAnimationDirection", "-webkit-animation-direction", "MozAnimationDuration", "-moz-animation-duration", "webkitAnimationDuration", "WebkitAnimationDuration", "-webkit-animation-duration", "MozAnimationFillMode", "-moz-animation-fill-mode", "webkitAnimationFillMode", "WebkitAnimationFillMode", "-webkit-animation-fill-mode", "MozAnimationIterationCount", "-moz-animation-iteration-count", "webkitAnimationIterationCount", "WebkitAnimationIterationCount", "-webkit-animation-iteration-count", "MozAnimationName", "-moz-animation-name", "webkitAnimationName", "WebkitAnimationName", "-webkit-animation-name", "MozAnimationPlayState", "-moz-animation-play-state", "webkitAnimationPlayState", "WebkitAnimationPlayState", "-webkit-animation-play-state", "MozAnimationTimingFunction", "-moz-animation-timing-function", "webkitAnimationTimingFunction", "WebkitAnimationTimingFunction", "-webkit-animation-timing-function", "webkitAppearance", "WebkitAppearance", "-webkit-appearance", "webkitBackgroundSize", "WebkitBackgroundSize", "-webkit-background-size", "webkitBoxShadow", "WebkitBoxShadow", "-webkit-box-shadow", "webkitClipPath", "WebkitClipPath", "-webkit-clip-path", "gridColumnGap", "grid-column-gap", "webkitFilter", "WebkitFilter", "-webkit-filter", "webkitFlexBasis", "WebkitFlexBasis", "-webkit-flex-basis", "webkitFlexGrow", "WebkitFlexGrow", "-webkit-flex-grow", "webkitFlexShrink", "WebkitFlexShrink", "-webkit-flex-shrink", "MozFontFeatureSettings", "-moz-font-feature-settings", "webkitFontFeatureSettings", "WebkitFontFeatureSettings", "-webkit-font-feature-settings", "MozFontLanguageOverride", "-moz-font-language-override", "fontStretch", "font-stretch", "webkitJustifyContent", "WebkitJustifyContent", "-webkit-justify-content", "webkitOrder", "WebkitOrder", "-webkit-order", "wordWrap", "word-wrap", "MozPerspective", "-moz-perspective", "MozPerspectiveOrigin", "-moz-perspective-origin", "webkitPerspectiveOrigin", "WebkitPerspectiveOrigin", "-webkit-perspective-origin", "colorAdjust", "color-adjust", "gridRowGap", "grid-row-gap", "MozTransform", "-moz-transform", "MozTransformOrigin", "-moz-transform-origin", "webkitTransformOrigin", "WebkitTransformOrigin", "-webkit-transform-origin", "MozTransformStyle", "-moz-transform-style", "webkitTransformStyle", "WebkitTransformStyle", "-webkit-transform-style", "MozTransitionDelay", "-moz-transition-delay", "webkitTransitionDelay", "WebkitTransitionDelay", "-webkit-transition-delay", "MozTransitionDuration", "-moz-transition-duration", "webkitTransitionDuration", "WebkitTransitionDuration", "-webkit-transition-duration", "MozTransitionProperty", "-moz-transition-property", "webkitTransitionProperty", "WebkitTransitionProperty", "-webkit-transition-property", "MozTransitionTimingFunction", "-moz-transition-timing-function", "webkitTransitionTimingFunction", "WebkitTransitionTimingFunction", "-webkit-transition-timing-function", "MozUserSelect", "-moz-user-select", "webkitUserSelect", "WebkitUserSelect", "-webkit-user-select", "webkitMaskImage", "WebkitMaskImage", "-webkit-mask-image", "webkitMaskPositionX", "WebkitMaskPositionX", "-webkit-mask-position-x", "webkitMaskPositionY", "WebkitMaskPositionY", "-webkit-mask-position-y", "webkitMaskRepeat", "WebkitMaskRepeat", "-webkit-mask-repeat", "webkitMaskSize", "WebkitMaskSize", "-webkit-mask-size", "MozTabSize", "-moz-tab-size", "MozBorderStartColor", "-moz-border-start-color", "MozBorderEndColor", "-moz-border-end-color", "MozBorderStartStyle", "-moz-border-start-style", "MozBorderEndStyle", "-moz-border-end-style", "MozBorderStartWidth", "-moz-border-start-width", "MozBorderEndWidth", "-moz-border-end-width", "webkitBorderTopLeftRadius", "WebkitBorderTopLeftRadius", "-webkit-border-top-left-radius", "webkitBorderTopRightRadius", "WebkitBorderTopRightRadius", "-webkit-border-top-right-radius", "webkitBorderBottomRightRadius", "WebkitBorderBottomRightRadius", "-webkit-border-bottom-right-radius", "webkitBorderBottomLeftRadius", "WebkitBorderBottomLeftRadius", "-webkit-border-bottom-left-radius", "MozMarginStart", "-moz-margin-start", "MozMarginEnd", "-moz-margin-end", "MozPaddingStart", "-moz-padding-start", "MozPaddingEnd", "-moz-padding-end", "webkitBackgroundClip", "WebkitBackgroundClip", "-webkit-background-clip", "webkitBackgroundOrigin", "WebkitBackgroundOrigin", "-webkit-background-origin", "MozBackfaceVisibility", "-moz-backface-visibility", "webkitBackfaceVisibility", "WebkitBackfaceVisibility", "-webkit-backface-visibility", "MozHyphens", "-moz-hyphens", "webkitTextSizeAdjust", "WebkitTextSizeAdjust", "-webkit-text-size-adjust", "webkitFlexDirection", "WebkitFlexDirection", "-webkit-flex-direction", "webkitFlexWrap", "WebkitFlexWrap", "-webkit-flex-wrap", "MozBoxSizing", "-moz-box-sizing", "webkitBoxSizing", "WebkitBoxSizing", "-webkit-box-sizing", "webkitMaskClip", "WebkitMaskClip", "-webkit-mask-clip", "webkitMaskOrigin", "WebkitMaskOrigin", "-webkit-mask-origin", "webkitMaskComposite", "WebkitMaskComposite", "-webkit-mask-composite", "webkitBoxAlign", "WebkitBoxAlign", "-webkit-box-align", "webkitBoxDirection", "WebkitBoxDirection", "-webkit-box-direction", "webkitBoxOrient", "WebkitBoxOrient", "-webkit-box-orient", "webkitBoxPack", "WebkitBoxPack", "-webkit-box-pack", "MozBorderStart", "-moz-border-start", "MozBorderEnd", "-moz-border-end", "MozBorderImage", "-moz-border-image", "webkitBorderImage", "WebkitBorderImage", "-webkit-border-image", "webkitFlexFlow", "WebkitFlexFlow", "-webkit-flex-flow", "webkitFlex", "WebkitFlex", "-webkit-flex", "gridGap", "grid-gap", "MozTransition", "-moz-transition", "webkitTransition", "WebkitTransition", "-webkit-transition", "MozAnimation", "-moz-animation", "webkitAnimation", "WebkitAnimation", "-webkit-animation", "webkitMask", "WebkitMask", "-webkit-mask", "webkitMaskPosition", "WebkitMaskPosition", "-webkit-mask-position", "webkitBorderRadius", "WebkitBorderRadius", "-webkit-border-radius"};
inline constexpr const char* kBindingOrder110[] = {"selectorText", "style", "styleMap"};
inline constexpr const char* kBindingOrder111[] = {"insertRule", "deleteRule", "replace", "replaceSync", "removeRule", "addRule", "ownerRule", "cssRules", "rules"};
inline constexpr const char* kBindingOrder112[] = {"toString"};
inline constexpr const char* kBindingOrder113[] = {"toMatrix", "toString", "is2D"};
inline constexpr const char* kBindingOrder114[] = {"toMatrix", "@@iterator", "keys", "values", "entries", "forEach", "length", "is2D"};
inline constexpr const char* kBindingOrder115[] = {"transitionProperty"};
inline constexpr const char* kBindingOrder116[] = {"x", "y", "z"};
inline constexpr const char* kBindingOrder117[] = {"value", "unit"};
inline constexpr const char* kBindingOrder118[] = {"@@iterator", "keys", "values", "entries", "forEach", "length"};
inline constexpr const char* kBindingOrder119[] = {"variable", "fallback"};
inline constexpr const char* kBindingOrder120[] = {"navigation", "types"};
inline constexpr const char* kBindingOrder121[] = {"match", "matchAll", "add", "addAll", "put", "delete", "keys"};
inline constexpr const char* kBindingOrder122[] = {"match", "has", "open", "delete", "keys"};
inline constexpr const char* kBindingOrder123[] = {"phase"};
inline constexpr const char* kBindingOrder124[] = {"getWindowGlobals", "notifyStartDelayedAutoplayMedia", "loadURI", "fixupAndLoadURIString", "print", "goBack", "goForward", "goToIndex", "reload", "stop", "resetScalingZoom", "clearRestoreState", "startApzAutoscroll", "stopApzAutoscroll", "serviceWorkersDisabledByPolicy", "downloadFolderOverride", "currentWindowGlobal", "topChromeWindow", "currentRemoteType", "embedderWindowGlobal", "secureBrowserUI", "webProgress", "sessionHistory", "activeSessionHistoryEntry", "mediaController", "currentURI", "forceAppWindowActive", "isActive", "touchEventsOverride", "targetTopLevelLinkClicksToBlank", "crossGroupOpener", "isReplaced", "mostRecentLoadingSessionHistoryEntry", "scopedPrefs", "isUnderHiddenEmbedderElement", "canOpenModalPicker"};
inline constexpr const char* kBindingOrder125[] = {"requestFrame", "canvas"};
inline constexpr const char* kBindingOrder126[] = {"addColorStop"};
inline constexpr const char* kBindingOrder127[] = {"setTransform"};
inline constexpr const char* kBindingOrder128[] = {"getContextAttributes", "getDebugInfo", "drawWindow", "drawImage", "beginPath", "fill", "stroke", "clip", "isPointInPath", "isPointInStroke", "createLinearGradient", "createRadialGradient", "createConicGradient", "createPattern", "createImageData", "getImageData", "putImageData", "setLineDash", "getLineDash", "closePath", "moveTo", "lineTo", "quadraticCurveTo", "bezierCurveTo", "arcTo", "rect", "roundRect", "arc", "ellipse", "clearRect", "fillRect", "strokeRect", "save", "restore", "reset", "isContextLost", "fillText", "strokeText", "measureText", "scale", "rotate", "translate", "transform", "getTransform", "setTransform", "resetTransform", "drawFocusIfNeeded", "canvas", "globalAlpha", "globalCompositeOperation", "strokeStyle", "fillStyle", "filter", "imageSmoothingEnabled", "lineWidth", "lineCap", "lineJoin", "miterLimit", "lineDashOffset", "shadowOffsetX", "shadowOffsetY", "shadowBlur", "shadowColor", "lang", "font", "textAlign", "textBaseline", "direction", "letterSpacing", "fontKerning", "fontStretch", "fontVariantCaps", "textRendering", "wordSpacing", "DRAWWINDOW_DRAW_CARET", "DRAWWINDOW_DO_NOT_FLUSH", "DRAWWINDOW_DRAW_VIEW", "DRAWWINDOW_USE_WIDGET_LAYERS", "DRAWWINDOW_ASYNC_DECODE_IMAGES"};
inline constexpr const char* kBindingOrder129[] = {"getClientRect", "offsetNode", "offset"};
inline constexpr const char* kBindingOrder130[] = {"collapsed", "boundingClientRect", "reason", "caretVisible", "caretVisuallyVisible", "selectionVisible", "selectionEditable", "selectedTextContent", "clientX", "clientY"};
inline constexpr const char* kBindingOrder131[] = {"cancel", "redirectTo", "upgradeToSecure", "suspend", "resume", "matches", "registerTraceableChannel", "errorCheck", "getRequestHeaders", "getRequestHeader", "getResponseHeaders", "setRequestHeader", "setResponseHeader", "id", "channel", "contentType", "method", "type", "suspended", "finalURI", "finalURL", "statusCode", "statusLine", "errorString", "onerror", "onstart", "onstop", "proxyInfo", "remoteAddress", "loadInfo", "isServiceWorkerScript", "originURL", "documentURL", "originURI", "documentURI", "canModify", "frameId", "parentFrameId", "documentInnerWindowId", "parentDocumentInnerWindowId", "browserElement", "frameAncestors", "urlClassification", "thirdParty", "requestSize", "responseSize"};
inline constexpr const char* kBindingOrder132[] = {"rangeStart", "rangeEnd"};
inline constexpr const char* kBindingOrder133[] = {"substringData", "appendData", "insertData", "deleteData", "replaceData", "before", "after", "replaceWith", "remove", "data", "length", "previousElementSibling", "nextElementSibling"};
inline constexpr const char* kBindingOrder134[] = {"getReports", "isRecordingEnabled", "setRecordingEnabled", "flushActiveReports"};
inline constexpr const char* kBindingOrder135[] = {"canGo", "go", "reload", "count", "index"};
inline constexpr const char* kBindingOrder136[] = {"loadFrameScript", "removeDelayedFrameScript", "getDelayedFrameScripts"};
inline constexpr const char* kBindingOrder137[] = {"loadFrameScript", "removeDelayedFrameScript", "getDelayedFrameScripts"};
inline constexpr const char* kBindingOrder138[] = {"append", "remove", "@@iterator"};
inline constexpr const char* kBindingOrder139[] = {"postMessage", "url", "frameType", "type", "id"};
inline constexpr const char* kBindingOrder140[] = {"get", "matchAll", "openWindow", "claim"};
inline constexpr const char* kBindingOrder141[] = {"read", "readText", "write", "writeText"};
inline constexpr const char* kBindingOrder142[] = {"clipboardData"};
inline constexpr const char* kBindingOrder143[] = {"getType", "presentationStyle", "types"};
inline constexpr const char* kBindingOrder144[] = {"wasClean", "code", "reason"};
inline constexpr const char* kBindingOrder145[] = {"requestClose", "close", "destroy", "oncancel", "onclose"};
inline constexpr const char* kBindingOrder146[] = {"init", "getInitProgress", "queryNodes", "getQueryProgress", "getPathToRoot", "getNodeAdjacents", "sampleNodes", "MAX_QUERY_RESULTS", "SAMPLE_COUNT"};
inline constexpr const char* kBindingOrder147[] = {"GARBAGE", "INCREMENTAL_ROOT", "ROOT", "SOFT_ROOT", "CC_MANAGED", "GC_MARKED", "GC_GRAY"};
inline constexpr const char* kBindingOrder148[] = {"command", "source"};
inline constexpr const char* kBindingOrder149[] = {"initCompositionEvent", "data"};
inline constexpr const char* kBindingOrder150[] = {"readable", "writable"};
inline constexpr const char* kBindingOrder151[] = {"toJSON", "url", "connection", "allowlist", "disposition"};
inline constexpr const char* kBindingOrder152[] = {"assert", "clear", "count", "countReset", "debug", "error", "info", "log", "table", "trace", "warn", "dir", "dirxml", "group", "groupCollapsed", "groupEnd", "time", "timeLog", "timeEnd", "exception", "timeStamp", "profile", "profileEnd", "shouldLog"};
inline constexpr const char* kBindingOrder153[] = {"offset"};
inline constexpr const char* kBindingOrder154[] = {"addMessageListener", "removeMessageListener", "dump", "atob", "btoa", "sendAsyncMessage", "sendSyncMessage", "content", "docShell", "tabEventTarget", "processMessageManager", "remoteType"};
inline constexpr const char* kBindingOrder155[] = {"addMessageListener", "removeMessageListener", "dump", "atob", "btoa", "sendAsyncMessage", "sendSyncMessage", "initialProcessData", "sharedData", "processMessageManager", "remoteType"};
inline constexpr const char* kBindingOrder156[] = {"skipped"};
inline constexpr const char* kBindingOrder157[] = {"buffer", "normalize"};
inline constexpr const char* kBindingOrder158[] = {"changed", "deleted"};
inline constexpr const char* kBindingOrder159[] = {"get", "getAll", "set", "delete", "onchange"};
inline constexpr const char* kBindingOrder160[] = {"subscribe", "getSubscriptions", "unsubscribe"};
inline constexpr const char* kBindingOrder161[] = {"highWaterMark", "size"};
inline constexpr const char* kBindingOrder162[] = {"windowID", "innerWindowID", "callID", "isSecure"};
inline constexpr const char* kBindingOrder163[] = {"id", "type"};
inline constexpr const char* kBindingOrder164[] = {"get", "create", "store", "preventSilentAccess"};
inline constexpr const char* kBindingOrder165[] = {"getRandomValues", "randomUUID", "subtle"};
inline constexpr const char* kBindingOrder166[] = {"type", "extractable", "algorithm", "usages"};
inline constexpr const char* kBindingOrder167[] = {"define", "get", "getName", "whenDefined", "upgrade", "initialize"};
inline constexpr const char* kBindingOrder168[] = {"initCustomEvent", "detail"};
inline constexpr const char* kBindingOrder169[] = {"add", "delete", "clear", "entries", "keys", "values", "forEach", "has", "size"};
inline constexpr const char* kBindingOrder170[] = {"name", "message", "code", "result", "filename", "lineNumber", "columnNumber", "data", "stack", "INDEX_SIZE_ERR", "DOMSTRING_SIZE_ERR", "HIERARCHY_REQUEST_ERR", "WRONG_DOCUMENT_ERR", "INVALID_CHARACTER_ERR", "NO_DATA_ALLOWED_ERR", "NO_MODIFICATION_ALLOWED_ERR", "NOT_FOUND_ERR", "NOT_SUPPORTED_ERR", "INUSE_ATTRIBUTE_ERR", "INVALID_STATE_ERR", "SYNTAX_ERR", "INVALID_MODIFICATION_ERR", "NAMESPACE_ERR", "INVALID_ACCESS_ERR", "VALIDATION_ERR", "TYPE_MISMATCH_ERR", "SECURITY_ERR", "NETWORK_ERR", "ABORT_ERR", "URL_MISMATCH_ERR", "QUOTA_EXCEEDED_ERR", "TIMEOUT_ERR", "INVALID_NODE_TYPE_ERR", "DATA_CLONE_ERR"};
inline constexpr const char* kBindingOrder171[] = {"hasFeature", "createDocumentType", "createDocument", "createHTMLDocument"};
inline constexpr const char* kBindingOrder172[] = {"connectRoot", "disconnectRoot", "pauseObserving", "resumeObserving", "setAttributes", "getAttributes", "setArgs", "translateFragment", "translateElements", "translateRoots"};
inline constexpr const char* kBindingOrder173[] = {"multiplySelf", "preMultiplySelf", "translateSelf", "scaleSelf", "scale3dSelf", "rotateSelf", "rotateFromVectorSelf", "rotateAxisAngleSelf", "skewXSelf", "skewYSelf", "invertSelf", "setMatrixValue", "a", "b", "c", "d", "e", "f", "m11", "m12", "m13", "m14", "m21", "m22", "m23", "m24", "m31", "m32", "m33", "m34", "m41", "m42", "m43", "m44"};
inline constexpr const char* kBindingOrder174[] = {"translate", "scale", "scaleNonUniform", "scale3d", "rotate", "rotateFromVector", "rotateAxisAngle", "skewX", "skewY", "multiply", "flipX", "flipY", "inverse", "transformPoint", "toFloat32Array", "toFloat64Array", "toJSON", "toString", "a", "b", "c", "d", "e", "f", "m11", "m12", "m13", "m14", "m21", "m22", "m23", "m24", "m31", "m32", "m33", "m34", "m41", "m42", "m43", "m44", "is2D", "isIdentity"};
inline constexpr const char* kBindingOrder175[] = {"parseFromString", "parseFromSafeString", "forceEnableDTD"};
inline constexpr const char* kBindingOrder176[] = {"x", "y", "z", "w"};
inline constexpr const char* kBindingOrder177[] = {"matrixTransform", "toJSON", "x", "y", "z", "w"};
inline constexpr const char* kBindingOrder178[] = {"getBounds", "toJSON", "p1", "p2", "p3", "p4"};
inline constexpr const char* kBindingOrder179[] = {"x", "y", "width", "height"};
inline constexpr const char* kBindingOrder180[] = {"item", "@@iterator", "length"};
inline constexpr const char* kBindingOrder181[] = {"toJSON", "x", "y", "width", "height", "top", "right", "bottom", "left"};
inline constexpr const char* kBindingOrder182[] = {"item", "contains", "@@iterator", "length"};
inline constexpr const char* kBindingOrder183[] = {"item", "contains", "add", "remove", "replace", "toggle", "supports", "@@iterator", "keys", "values", "entries", "forEach", "toString", "length", "value"};
inline constexpr const char* kBindingOrder184[] = {"setDragImage", "getData", "setData", "clearData", "addElement", "dropEffect", "effectAllowed", "items", "types", "files", "mozCursor", "mozUserCancelled", "mozSourceNode"};
inline constexpr const char* kBindingOrder185[] = {"getAsString", "getAsFile", "webkitGetAsEntry", "kind", "type"};
inline constexpr const char* kBindingOrder186[] = {"add", "remove", "clear", "@@iterator", "length"};
inline constexpr const char* kBindingOrder187[] = {"type", "global"};
inline constexpr const char* kBindingOrder188[] = {"connect", "disconnect", "addListener", "removeListener"};
inline constexpr const char* kBindingOrder189[] = {"readable", "writable"};
inline constexpr const char* kBindingOrder190[] = {"postMessage", "close", "requestAnimationFrame", "cancelAnimationFrame", "name", "onmessage", "onmessageerror", "onrtctransform"};
inline constexpr const char* kBindingOrder191[] = {"delayTime"};
inline constexpr const char* kBindingOrder192[] = {"toJSON", "id", "anticipatedRemoval", "message", "sourceFile", "lineNumber", "columnNumber"};
inline constexpr const char* kBindingOrder193[] = {"x", "y", "z"};
inline constexpr const char* kBindingOrder194[] = {"value"};
inline constexpr const char* kBindingOrder195[] = {"initDeviceMotionEvent", "acceleration", "accelerationIncludingGravity", "rotationRate", "interval"};
inline constexpr const char* kBindingOrder196[] = {"initDeviceOrientationEvent", "alpha", "beta", "gamma", "absolute"};
inline constexpr const char* kBindingOrder197[] = {"alpha", "beta", "gamma"};
inline constexpr const char* kBindingOrder198[] = {"toJSON", "protocol", "data"};
inline constexpr const char* kBindingOrder199[] = {"getFilesAndDirectories", "getFiles", "name", "path"};
inline constexpr const char* kBindingOrder200[] = {"getElementsByTagName", "getElementsByTagNameNS", "getElementsByClassName", "createElement", "createElementNS", "createDocumentFragment", "createTextNode", "createComment", "createProcessingInstruction", "importNode", "adoptNode", "createEvent", "createRange", "createNodeIterator", "createTreeWalker", "createCDATASection", "createAttribute", "createAttributeNS", "getElementsByName", "open", "close", "write", "writeln", "hasFocus", "execCommand", "queryCommandEnabled", "queryCommandIndeterm", "queryCommandState", "queryCommandSupported", "queryCommandValue", "releaseCapture", "mozSetImageElement", "clear", "captureEvents", "releaseEvents", "exitFullscreen", "mozCancelFullScreen", "exitPictureInPicture", "exitPointerLock", "addCertException", "reloadWithHttpsOnlyException", "getFailedCertSecurityInfo", "getNetErrorInfo", "enableStyleSheetsForSet", "caretPositionFromPoint", "caretRangeFromPoint", "createTouch", "createTouchList", "blockUnblockOnload", "getSelection", "hasStorageAccess", "requestStorageAccess", "requestStorageAccessForOrigin", "startViewTransition", "ariaNotify", "elementFromPoint", "elementsFromPoint", "getAnimations", "getBoxQuads", "convertQuadFromNode", "convertRectFromNode", "convertPointFromNode", "getElementById", "prepend", "append", "replaceChildren", "moveBefore", "querySelector", "querySelectorAll", "createExpression", "createNSResolver", "evaluate", "implementation", "URL", "documentURI", "compatMode", "characterSet", "charset", "inputEncoding", "contentType", "doctype", "documentElement", "domain", "referrer", "cookie", "lastModified", "readyState", "title", "dir", "body", "head", "images", "embeds", "plugins", "links", "forms", "scripts", "defaultView", "designMode", "onreadystatechange", "mozSyntheticDocument", "currentScript", "fgColor", "linkColor", "vlinkColor", "alinkColor", "bgColor", "anchors", "applets", "all", "fullscreen", "mozFullScreen", "fullscreenEnabled", "mozFullScreenEnabled", "onfullscreenchange", "onfullscreenerror", "pictureInPictureEnabled", "onpointerlockchange", "onpointerlockerror", "hidden", "visibilityState", "onvisibilitychange", "selectedStyleSheetSet", "lastStyleSheetSet", "preferredStyleSheetSet", "styleSheetSets", "scrollingElement", "timeline", "rootElement", "inAndroidPipMode", "l10n", "hasPendingL10nMutations", "featurePolicy", "permissionsPolicy", "fragmentDirective", "activeViewTransition", "activeElement", "styleSheets", "pointerLockElement", "fullscreenElement", "mozFullScreenElement", "customElementRegistry", "pictureInPictureElement", "adoptedStyleSheets", "fonts", "onabort", "onblur", "onfocus", "oncancel", "onauxclick", "onbeforeinput", "onbeforematch", "onbeforetoggle", "oncanplay", "oncanplaythrough", "onchange", "onclick", "onclose", "oncontentvisibilityautostatechange", "oncontextlost", "oncontextmenu", "oncommand", "oncontextrestored", "oncopy", "oncuechange", "oncut", "ondblclick", "ondrag", "ondragend", "ondragenter", "ondragexit", "ondragleave", "ondragover", "ondragstart", "ondrop", "ondurationchange", "onemptied", "onended", "onformdata", "oninput", "oninvalid", "onkeydown", "onkeypress", "onkeyup", "onload", "onloadeddata", "onloadedmetadata", "onloadstart", "onmousedown", "onmouseenter", "onmouseleave", "onmousemove", "onmouseout", "onmouseover", "onmouseup", "onwheel", "onpaste", "onpause", "onplay", "onplaying", "onprogress", "onratechange", "onreset", "onresize", "onscroll", "onscrollend", "onsecuritypolicyviolation", "onseeked", "onseeking", "onselect", "onslotchange", "onstalled", "onsubmit", "onsuspend", "ontimeupdate", "onvolumechange", "onwaiting", "onselectstart", "onselectionchange", "ontoggle", "onpointercancel", "onpointerdown", "onpointerup", "onpointermove", "onpointerout", "onpointerover", "onpointerenter", "onpointerleave", "onpointerrawupdate", "ongotpointercapture", "onlostpointercapture", "onmozfullscreenchange", "onmozfullscreenerror", "onanimationcancel", "onanimationend", "onanimationiteration", "onanimationstart", "ontransitioncancel", "ontransitionend", "ontransitionrun", "ontransitionstart", "onwebkitanimationend", "onwebkitanimationiteration", "onwebkitanimationstart", "onwebkittransitionend", "onerror", "children", "firstElementChild", "lastElementChild", "childElementCount", "ontouchstart", "ontouchend", "ontouchmove", "ontouchcancel"};
inline constexpr const char* kBindingOrder201[] = {"getElementById", "prepend", "append", "replaceChildren", "moveBefore", "querySelector", "querySelectorAll", "children", "firstElementChild", "lastElementChild", "childElementCount"};
inline constexpr const char* kBindingOrder202[] = {"connectRoot", "ready"};
inline constexpr const char* kBindingOrder203[] = {"requestWindow", "window", "onenter"};
inline constexpr const char* kBindingOrder204[] = {"window"};
inline constexpr const char* kBindingOrder205[] = {"before", "after", "replaceWith", "remove", "name", "publicId", "systemId"};
inline constexpr const char* kBindingOrder206[] = {"getRetainedSize", "getImmediatelyDominated", "getImmediateDominator", "root"};
inline constexpr const char* kBindingOrder207[] = {"initDragEvent", "dataTransfer"};
inline constexpr const char* kBindingOrder208[] = {"threshold", "knee", "ratio", "reduction", "attack", "release"};
inline constexpr const char* kBindingOrder209[] = {"MIN_EXT", "MAX_EXT"};
inline constexpr const char* kBindingOrder210[] = {"clipControlEXT", "LOWER_LEFT_EXT", "UPPER_LEFT_EXT", "NEGATIVE_ONE_TO_ONE_EXT", "ZERO_TO_ONE_EXT", "CLIP_ORIGIN_EXT", "CLIP_DEPTH_MODE_EXT"};
inline constexpr const char* kBindingOrder211[] = {"RGBA16F_EXT", "RGB16F_EXT", "FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE_EXT", "UNSIGNED_NORMALIZED_EXT"};
inline constexpr const char* kBindingOrder212[] = {"DEPTH_CLAMP_EXT"};
inline constexpr const char* kBindingOrder213[] = {"createQueryEXT", "deleteQueryEXT", "isQueryEXT", "beginQueryEXT", "endQueryEXT", "queryCounterEXT", "getQueryEXT", "getQueryObjectEXT", "QUERY_COUNTER_BITS_EXT", "CURRENT_QUERY_EXT", "QUERY_RESULT_EXT", "QUERY_RESULT_AVAILABLE_EXT", "TIME_ELAPSED_EXT", "TIMESTAMP_EXT", "GPU_DISJOINT_EXT"};
inline constexpr const char* kBindingOrder214[] = {"polygonOffsetClampEXT", "POLYGON_OFFSET_CLAMP_EXT"};
inline constexpr const char* kBindingOrder215[] = {"SRGB_EXT", "SRGB_ALPHA_EXT", "SRGB8_ALPHA8_EXT", "FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT"};
inline constexpr const char* kBindingOrder216[] = {"COMPRESSED_RGBA_BPTC_UNORM_EXT", "COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT", "COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT", "COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT"};
inline constexpr const char* kBindingOrder217[] = {"COMPRESSED_RED_RGTC1_EXT", "COMPRESSED_SIGNED_RED_RGTC1_EXT", "COMPRESSED_RED_GREEN_RGTC2_EXT", "COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT"};
inline constexpr const char* kBindingOrder218[] = {"TEXTURE_MAX_ANISOTROPY_EXT", "MAX_TEXTURE_MAX_ANISOTROPY_EXT"};
inline constexpr const char* kBindingOrder219[] = {"MIRROR_CLAMP_TO_EDGE_EXT"};
inline constexpr const char* kBindingOrder220[] = {"R16_EXT", "RG16_EXT", "RGB16_EXT", "RGBA16_EXT", "R16_SNORM_EXT", "RG16_SNORM_EXT", "RGB16_SNORM_EXT", "RGBA16_SNORM_EXT"};
inline constexpr const char* kBindingOrder221[] = {"updateText", "updateSelection", "updateControlBounds", "updateSelectionBounds", "updateCharacterBounds", "attachedElements", "characterBounds", "text", "selectionStart", "selectionEnd", "characterBoundsRangeStart", "ontextupdate", "ontextformatupdate", "oncharacterboundsupdate", "oncompositionstart", "oncompositionend"};
inline constexpr const char* kBindingOrder222[] = {"getAttributeNames", "getAttribute", "getAttributeNS", "toggleAttribute", "setAttribute", "setAttributeNS", "removeAttribute", "removeAttributeNS", "hasAttribute", "hasAttributeNS", "hasAttributes", "closest", "matches", "webkitMatchesSelector", "getElementsByTagName", "getElementsByTagNameNS", "getElementsByClassName", "insertAdjacentElement", "insertAdjacentText", "mozMatchesSelector", "setPointerCapture", "releasePointerCapture", "hasPointerCapture", "setCapture", "releaseCapture", "getAttributeNode", "setAttributeNode", "removeAttributeNode", "getAttributeNodeNS", "setAttributeNodeNS", "getTransformToAncestor", "getTransformToParent", "getTransformToViewport", "getClientRects", "getBoundingClientRect", "checkVisibility", "scrollIntoView", "scroll", "scrollTo", "scrollBy", "insertAdjacentHTML", "attachShadow", "requestFullscreen", "mozRequestFullScreen", "requestPointerLock", "setHTML", "setHTMLUnsafe", "getHTML", "computedStyleMap", "ariaNotify", "animate", "getAnimations", "before", "after", "replaceWith", "remove", "getBoxQuads", "convertQuadFromNode", "convertRectFromNode", "convertPointFromNode", "prepend", "append", "replaceChildren", "moveBefore", "querySelector", "querySelectorAll", "namespaceURI", "prefix", "localName", "tagName", "id", "className", "classList", "part", "attributes", "scrollTop", "scrollLeft", "scrollWidth", "scrollHeight", "clientTop", "clientLeft", "clientWidth", "clientHeight", "scrollTopMax", "scrollLeftMax", "currentCSSZoom", "innerHTML", "outerHTML", "shadowRoot", "openOrClosedShadowRoot", "assignedSlot", "slot", "customElementRegistry", "onfullscreenchange", "onfullscreenerror", "ariaActiveDescendantElement", "role", "ariaAtomic", "ariaAutoComplete", "ariaBrailleLabel", "ariaBrailleRoleDescription", "ariaBusy", "ariaChecked", "ariaColCount", "ariaColIndex", "ariaColIndexText", "ariaColSpan", "ariaControlsElements", "ariaCurrent", "ariaDescribedByElements", "ariaDescription", "ariaDetailsElements", "ariaDisabled", "ariaErrorMessageElements", "ariaExpanded", "ariaFlowToElements", "ariaHasPopup", "ariaHidden", "ariaInvalid", "ariaKeyShortcuts", "ariaLabel", "ariaLabelledByElements", "ariaLevel", "ariaLive", "ariaModal", "ariaMultiLine", "ariaMultiSelectable", "ariaOrientation", "ariaOwnsElements", "ariaPlaceholder", "ariaPosInSet", "ariaPressed", "ariaReadOnly", "ariaRelevant", "ariaRequired", "ariaRoleDescription", "ariaRowCount", "ariaRowIndex", "ariaRowIndexText", "ariaRowSpan", "ariaSelected", "ariaSetSize", "ariaSort", "ariaValueMax", "ariaValueMin", "ariaValueNow", "ariaValueText", "previousElementSibling", "nextElementSibling", "children", "firstElementChild", "lastElementChild", "childElementCount"};
inline constexpr const char* kBindingOrder223[] = {"setFormValue", "setValidity", "checkValidity", "reportValidity", "shadowRoot", "form", "willValidate", "validity", "validationMessage", "labels", "states", "ariaActiveDescendantElement", "role", "ariaAtomic", "ariaAutoComplete", "ariaBrailleLabel", "ariaBrailleRoleDescription", "ariaBusy", "ariaChecked", "ariaColCount", "ariaColIndex", "ariaColIndexText", "ariaColSpan", "ariaControlsElements", "ariaCurrent", "ariaDescribedByElements", "ariaDescription", "ariaDetailsElements", "ariaDisabled", "ariaErrorMessageElements", "ariaExpanded", "ariaFlowToElements", "ariaHasPopup", "ariaHidden", "ariaInvalid", "ariaKeyShortcuts", "ariaLabel", "ariaLabelledByElements", "ariaLevel", "ariaLive", "ariaModal", "ariaMultiLine", "ariaMultiSelectable", "ariaOrientation", "ariaOwnsElements", "ariaPlaceholder", "ariaPosInSet", "ariaPressed", "ariaReadOnly", "ariaRelevant", "ariaRequired", "ariaRoleDescription", "ariaRowCount", "ariaRowIndex", "ariaRowIndexText", "ariaRowSpan", "ariaSelected", "ariaSetSize", "ariaSort", "ariaValueMax", "ariaValueMin", "ariaValueNow", "ariaValueText"};
inline constexpr const char* kBindingOrder224[] = {"copyTo", "type", "timestamp", "duration", "byteLength"};
inline constexpr const char* kBindingOrder225[] = {"copyTo", "type", "timestamp", "duration", "byteLength"};
inline constexpr const char* kBindingOrder226[] = {"message", "filename", "lineno", "colno", "error"};
inline constexpr const char* kBindingOrder227[] = {"composedPath", "stopPropagation", "stopImmediatePropagation", "preventDefault", "initEvent", "type", "target", "srcElement", "currentTarget", "eventPhase", "bubbles", "cancelable", "returnValue", "defaultPrevented", "composed", "timeStamp", "cancelBubble", "originalTarget", "explicitOriginalTarget", "NONE", "CAPTURING_PHASE", "AT_TARGET", "BUBBLING_PHASE", "ALT_MASK", "CONTROL_MASK", "SHIFT_MASK", "META_MASK"};
inline constexpr const char* kBindingOrder228[] = {"event", "targetType"};
inline constexpr const char* kBindingOrder229[] = {"entries", "keys", "values", "forEach", "has", "get", "size"};
inline constexpr const char* kBindingOrder230[] = {"close", "url", "withCredentials", "readyState", "onopen", "onmessage", "onerror", "CONNECTING", "OPEN", "CLOSED"};
inline constexpr const char* kBindingOrder231[] = {"addEventListener", "removeEventListener", "dispatchEvent"};
inline constexpr const char* kBindingOrder232[] = {"toString", "name", "message", "result", "filename", "lineNumber", "columnNumber", "data", "stack"};
inline constexpr const char* kBindingOrder233[] = {"changed", "deleted"};
inline constexpr const char* kBindingOrder234[] = {"waitUntil"};
inline constexpr const char* kBindingOrder235[] = {"data", "origin", "lastEventId", "source", "ports"};
inline constexpr const char* kBindingOrder236[] = {"create", "get", "getAll", "clear", "clearAll", "onAlarm"};
inline constexpr const char* kBindingOrder237[] = {"mockExtensionAPI", "alarms", "browserSettings", "dns", "proxy", "publicSuffix", "runtime", "scripting", "test"};
inline constexpr const char* kBindingOrder238[] = {"allowPopupsForUserEvents", "cacheEnabled", "closeTabsByDoubleClick", "contextMenuShowEvent", "ftpProtocolEnabled", "homepageOverride", "imageAnimationBehavior", "newTabPageOverride", "newTabPosition", "openBookmarksInNewTabs", "openSearchResultsInNewTabs", "openUrlbarResultsInNewTabs", "webNotificationsDisabled", "overrideDocumentColors", "overrideContentColorScheme", "useDocumentFonts", "zoomFullPage", "zoomSiteSpecific", "verticalTabs", "colorManagement"};
inline constexpr const char* kBindingOrder239[] = {"mode", "useNativeSRGB", "useWebRenderCompositor"};
inline constexpr const char* kBindingOrder240[] = {"resolve"};
inline constexpr const char* kBindingOrder241[] = {"addListener", "removeListener", "hasListener", "hasListeners"};
inline constexpr const char* kBindingOrder242[] = {"denies", "deny", "except", "source"};
inline constexpr const char* kBindingOrder243[] = {"methodSyncWithReturn", "methodNoReturn", "methodAsync", "methodAmbiguousArgsAsync", "methodReturnsPort", "propertyAsErrorObject", "propertyAsString", "onTestEvent"};
inline constexpr const char* kBindingOrder244[] = {"disconnect", "postMessage", "name", "sender", "error", "onDisconnect", "onMessage"};
inline constexpr const char* kBindingOrder245[] = {"onRequest", "onError", "settings"};
inline constexpr const char* kBindingOrder246[] = {"isKnownSuffix", "getKnownSuffix", "getDomain"};
inline constexpr const char* kBindingOrder247[] = {"openOptionsPage", "getManifest", "getURL", "setUninstallURL", "reload", "connect", "connectNative", "sendMessage", "sendNativeMessage", "getBrowserInfo", "getPlatformInfo", "onStartup", "onInstalled", "onUpdateAvailable", "onConnect", "onConnectExternal", "onMessage", "onMessageExternal", "lastError", "id"};
inline constexpr const char* kBindingOrder248[] = {"executeScript", "insertCSS", "removeCSS", "registerContentScripts", "getRegisteredContentScripts", "unregisterContentScripts", "updateContentScripts"};
inline constexpr const char* kBindingOrder249[] = {"get", "set", "clear", "onChange"};
inline constexpr const char* kBindingOrder250[] = {"withHandlingUserInput", "notifyFail", "notifyPass", "log", "sendMessage", "fail", "succeed", "assertTrue", "assertFalse", "assertDeepEq", "assertEq", "assertRejects", "assertThrows", "onMessage"};
inline constexpr const char* kBindingOrder251[] = {"AddSearchProvider", "IsSearchProviderInstalled"};
inline constexpr const char* kBindingOrder252[] = {"respondWith", "request", "preloadResponse", "clientId", "resultingClientId", "handled"};
inline constexpr const char* kBindingOrder253[] = {"state", "onstatechange", "onrequestprogress", "onresponseprogress"};
inline constexpr const char* kBindingOrder254[] = {"name", "lastModified", "webkitRelativePath"};
inline constexpr const char* kBindingOrder255[] = {"item", "@@iterator", "length"};
inline constexpr const char* kBindingOrder256[] = {"readAsArrayBuffer", "readAsBinaryString", "readAsText", "readAsDataURL", "abort", "readyState", "result", "error", "onloadstart", "onprogress", "onload", "onabort", "onerror", "onloadend", "EMPTY", "LOADING", "DONE"};
inline constexpr const char* kBindingOrder257[] = {"readAsArrayBuffer", "readAsBinaryString", "readAsText", "readAsDataURL"};
inline constexpr const char* kBindingOrder258[] = {"name", "root"};
inline constexpr const char* kBindingOrder259[] = {"createReader", "getFile", "getDirectory"};
inline constexpr const char* kBindingOrder260[] = {"getFileHandle", "getDirectoryHandle", "removeEntry", "resolve", "values", "entries", "keys"};
inline constexpr const char* kBindingOrder261[] = {"next"};
inline constexpr const char* kBindingOrder262[] = {"next"};
inline constexpr const char* kBindingOrder263[] = {"readEntries"};
inline constexpr const char* kBindingOrder264[] = {"getParent", "isFile", "isDirectory", "name", "fullPath", "filesystem"};
inline constexpr const char* kBindingOrder265[] = {"file"};
inline constexpr const char* kBindingOrder266[] = {"getFile", "createWritable", "createSyncAccessHandle"};
inline constexpr const char* kBindingOrder267[] = {"move", "isSameEntry", "kind", "name"};
inline constexpr const char* kBindingOrder268[] = {"read", "write", "truncate", "getSize", "flush", "close"};
inline constexpr const char* kBindingOrder269[] = {"write", "seek", "truncate"};
inline constexpr const char* kBindingOrder270[] = {"getLines", "mainAxisDirection", "crossAxisDirection"};
inline constexpr const char* kBindingOrder271[] = {"node", "frameRect", "mainBaseSize", "mainDeltaSize", "mainMinSize", "mainMaxSize", "crossMinSize", "crossMaxSize", "clampState"};
inline constexpr const char* kBindingOrder272[] = {"getItems", "growthState", "crossStart", "crossSize", "firstBaselineOffset", "lastBaselineOffset"};
inline constexpr const char* kBindingOrder273[] = {"addResource", "hasMessage", "getMessage", "formatPattern", "locales"};
inline constexpr const char* kBindingOrder274[] = {"next", "values"};
inline constexpr const char* kBindingOrder275[] = {"next", "values"};
inline constexpr const char* kBindingOrder276[] = {"textElements"};
inline constexpr const char* kBindingOrder277[] = {"relatedTarget"};
inline constexpr const char* kBindingOrder278[] = {"load", "family", "style", "weight", "width", "stretch", "unicodeRange", "variant", "featureSettings", "variationSettings", "display", "ascentOverride", "descentOverride", "lineGapOverride", "sizeAdjust", "status", "loaded"};
inline constexpr const char* kBindingOrder279[] = {"add", "has", "delete", "clear", "entries", "values", "forEach", "load", "check", "size", "onloading", "onloadingdone", "onloadingerror", "ready", "status"};
inline constexpr const char* kBindingOrder280[] = {"next"};
inline constexpr const char* kBindingOrder281[] = {"fontfaces"};
inline constexpr const char* kBindingOrder282[] = {"append", "delete", "get", "getAll", "has", "set", "entries", "keys", "values", "forEach"};
inline constexpr const char* kBindingOrder283[] = {"formData"};
inline constexpr const char* kBindingOrder284[] = {"next"};
inline constexpr const char* kBindingOrder285[] = {"browsingContextId", "isTopFrame", "childID"};
inline constexpr const char* kBindingOrder286[] = {"requestUpdatePosition", "requestTabStateFlush", "requestEpochUpdate", "requestSHistoryUpdate", "printPreview", "exitPrintPreview", "startPersistence", "docShell", "remoteTab", "loadContext", "browsingContext", "depthTooGreat", "isRemoteFrame", "messageManager", "ownerElement", "childID", "lazyWidth", "lazyHeight", "isDead"};
inline constexpr const char* kBindingOrder287[] = {"requestAdapter", "getPreferredCanvasFormat", "wgslLanguageFeatures"};
inline constexpr const char* kBindingOrder288[] = {"requestDevice", "features", "limits", "info", "missingFeatures"};
inline constexpr const char* kBindingOrder289[] = {"vendor", "architecture", "device", "description", "subgroupMinSize", "subgroupMaxSize", "isFallbackAdapter"};
inline constexpr const char* kBindingOrder290[] = {"label"};
inline constexpr const char* kBindingOrder291[] = {"label"};
inline constexpr const char* kBindingOrder292[] = {"mapAsync", "getMappedRange", "unmap", "destroy", "size", "usage", "mapState", "label"};
inline constexpr const char* kBindingOrder293[] = {"MAP_READ", "MAP_WRITE", "COPY_SRC", "COPY_DST", "INDEX", "VERTEX", "UNIFORM", "STORAGE", "INDIRECT", "QUERY_RESOLVE"};
inline constexpr const char* kBindingOrder294[] = {"configure", "unconfigure", "getConfiguration", "getCurrentTexture", "canvas"};
inline constexpr const char* kBindingOrder295[] = {"RED", "GREEN", "BLUE", "ALPHA", "ALL"};
inline constexpr const char* kBindingOrder296[] = {"label"};
inline constexpr const char* kBindingOrder297[] = {"beginRenderPass", "beginComputePass", "copyBufferToBuffer", "copyBufferToTexture", "copyTextureToBuffer", "copyTextureToTexture", "clearBuffer", "resolveQuerySet", "finish", "pushDebugGroup", "popDebugGroup", "insertDebugMarker", "label"};
inline constexpr const char* kBindingOrder298[] = {"messages"};
inline constexpr const char* kBindingOrder299[] = {"message", "type", "lineNum", "linePos", "offset", "length"};
inline constexpr const char* kBindingOrder300[] = {"setPipeline", "dispatchWorkgroups", "dispatchWorkgroupsIndirect", "end", "setBindGroup", "pushDebugGroup", "popDebugGroup", "insertDebugMarker", "label"};
inline constexpr const char* kBindingOrder301[] = {"getBindGroupLayout", "label"};
inline constexpr const char* kBindingOrder302[] = {"destroy", "createBuffer", "createTexture", "importExternalTexture", "createSampler", "createBindGroupLayout", "createPipelineLayout", "createBindGroup", "createShaderModule", "createComputePipeline", "createRenderPipeline", "createComputePipelineAsync", "createRenderPipelineAsync", "createCommandEncoder", "createRenderBundleEncoder", "createQuerySet", "pushErrorScope", "popErrorScope", "features", "limits", "adapterInfo", "queue", "lost", "onuncapturederror", "label"};
inline constexpr const char* kBindingOrder303[] = {"reason", "message"};
inline constexpr const char* kBindingOrder304[] = {"message"};
inline constexpr const char* kBindingOrder305[] = {"label"};
inline constexpr const char* kBindingOrder306[] = {"READ", "WRITE"};
inline constexpr const char* kBindingOrder307[] = {"reason"};
inline constexpr const char* kBindingOrder308[] = {"label"};
inline constexpr const char* kBindingOrder309[] = {"destroy", "type", "count", "label"};
inline constexpr const char* kBindingOrder310[] = {"submit", "onSubmittedWorkDone", "writeBuffer", "writeTexture", "copyExternalImageToTexture", "label"};
inline constexpr const char* kBindingOrder311[] = {"label"};
inline constexpr const char* kBindingOrder312[] = {"finish", "setBindGroup", "pushDebugGroup", "popDebugGroup", "insertDebugMarker", "setPipeline", "setIndexBuffer", "setVertexBuffer", "draw", "drawIndexed", "drawIndirect", "drawIndexedIndirect", "label"};
inline constexpr const char* kBindingOrder313[] = {"setViewport", "setScissorRect", "setBlendConstant", "setStencilReference", "beginOcclusionQuery", "endOcclusionQuery", "executeBundles", "end", "setBindGroup", "pushDebugGroup", "popDebugGroup", "insertDebugMarker", "setPipeline", "setIndexBuffer", "setVertexBuffer", "draw", "drawIndexed", "drawIndirect", "drawIndexedIndirect", "label"};
inline constexpr const char* kBindingOrder314[] = {"getBindGroupLayout", "label"};
inline constexpr const char* kBindingOrder315[] = {"label"};
inline constexpr const char* kBindingOrder316[] = {"getCompilationInfo", "label"};
inline constexpr const char* kBindingOrder317[] = {"VERTEX", "FRAGMENT", "COMPUTE"};
inline constexpr const char* kBindingOrder318[] = {"entries", "keys", "values", "forEach", "has", "size"};
inline constexpr const char* kBindingOrder319[] = {"maxTextureDimension1D", "maxTextureDimension2D", "maxTextureDimension3D", "maxTextureArrayLayers", "maxBindGroups", "maxBindGroupsPlusVertexBuffers", "maxBindingsPerBindGroup", "maxDynamicUniformBuffersPerPipelineLayout", "maxDynamicStorageBuffersPerPipelineLayout", "maxSampledTexturesPerShaderStage", "maxSamplersPerShaderStage", "maxStorageBuffersInVertexStage", "maxStorageBuffersInFragmentStage", "maxStorageBuffersPerShaderStage", "maxStorageTexturesInVertexStage", "maxStorageTexturesInFragmentStage", "maxStorageTexturesPerShaderStage", "maxUniformBuffersPerShaderStage", "maxUniformBufferBindingSize", "maxStorageBufferBindingSize", "minUniformBufferOffsetAlignment", "minStorageBufferOffsetAlignment", "maxVertexBuffers", "maxBufferSize", "maxVertexAttributes", "maxVertexBufferArrayStride", "maxInterStageShaderVariables", "maxColorAttachments", "maxColorAttachmentBytesPerSample", "maxComputeWorkgroupStorageSize", "maxComputeInvocationsPerWorkgroup", "maxComputeWorkgroupSizeX", "maxComputeWorkgroupSizeY", "maxComputeWorkgroupSizeZ", "maxComputeWorkgroupsPerDimension"};
inline constexpr const char* kBindingOrder320[] = {"createView", "destroy", "width", "height", "depthOrArrayLayers", "mipLevelCount", "sampleCount", "dimension", "format", "usage", "label"};
inline constexpr const char* kBindingOrder321[] = {"COPY_SRC", "COPY_DST", "TEXTURE_BINDING", "STORAGE_BINDING", "RENDER_ATTACHMENT", "TRANSIENT_ATTACHMENT"};
inline constexpr const char* kBindingOrder322[] = {"label"};
inline constexpr const char* kBindingOrder323[] = {"error"};
inline constexpr const char* kBindingOrder324[] = {"gain"};
inline constexpr const char* kBindingOrder325[] = {"id", "index", "mapping", "hand", "connected", "buttons", "axes", "timestamp", "pose", "hapticActuators", "lightIndicators", "touchEvents"};
inline constexpr const char* kBindingOrder326[] = {"axis", "value"};
inline constexpr const char* kBindingOrder327[] = {"pressed", "touched", "value"};
inline constexpr const char* kBindingOrder328[] = {"button"};
inline constexpr const char* kBindingOrder329[] = {"gamepad"};
inline constexpr const char* kBindingOrder330[] = {"pulse", "type"};
inline constexpr const char* kBindingOrder331[] = {"setColor", "type"};
inline constexpr const char* kBindingOrder332[] = {"hasOrientation", "hasPosition", "position", "linearVelocity", "linearAcceleration", "orientation", "angularVelocity", "angularAcceleration"};
inline constexpr const char* kBindingOrder333[] = {"addGamepad", "removeGamepad", "newButtonEvent", "newButtonValueEvent", "newAxisMoveEvent", "newPoseMove", "newTouch", "noMapping", "standardMapping", "noHand", "leftHand", "rightHand"};
inline constexpr const char* kBindingOrder334[] = {"touchId", "surfaceId", "position", "surfaceDimensions"};
inline constexpr const char* kBindingOrder335[] = {"getCurrentPosition", "watchPosition", "clearWatch"};
inline constexpr const char* kBindingOrder336[] = {"toJSON", "latitude", "longitude", "altitude", "accuracy", "altitudeAccuracy", "heading", "speed"};
inline constexpr const char* kBindingOrder337[] = {"toJSON", "coords", "timestamp"};
inline constexpr const char* kBindingOrder338[] = {"code", "message", "PERMISSION_DENIED", "POSITION_UNAVAILABLE", "TIMEOUT"};
inline constexpr const char* kBindingOrder339[] = {"getConstraints", "getAudioOutputOptions", "type", "windowID", "innerWindowID", "callID", "rawID", "mediaSource", "devices", "isSecure", "isHandlingUserInput"};
inline constexpr const char* kBindingOrder340[] = {"set"};
inline constexpr const char* kBindingOrder341[] = {"add"};
inline constexpr const char* kBindingOrder342[] = {"accumulateSamples", "accumulateSingleSample"};
inline constexpr const char* kBindingOrder343[] = {"set"};
inline constexpr const char* kBindingOrder344[] = {"add"};
inline constexpr const char* kBindingOrder345[] = {"get"};
inline constexpr const char* kBindingOrder346[] = {"record"};
inline constexpr const char* kBindingOrder347[] = {"accumulate"};
inline constexpr const char* kBindingOrder348[] = {"addToNumerator"};
inline constexpr const char* kBindingOrder349[] = {"set"};
inline constexpr const char* kBindingOrder350[] = {"set"};
inline constexpr const char* kBindingOrder351[] = {"addToNumerator", "addToDenominator"};
inline constexpr const char* kBindingOrder352[] = {"set"};
inline constexpr const char* kBindingOrder353[] = {"add", "set"};
inline constexpr const char* kBindingOrder354[] = {"set"};
inline constexpr const char* kBindingOrder355[] = {"start", "stop", "cancel", "setRaw"};
inline constexpr const char* kBindingOrder356[] = {"start", "stopAndAccumulate", "cancel", "accumulateSamples", "accumulateSingleSample"};
inline constexpr const char* kBindingOrder357[] = {"set"};
inline constexpr const char* kBindingOrder358[] = {"set", "generateAndSet"};
inline constexpr const char* kBindingOrder359[] = {"rows", "cols", "areas"};
inline constexpr const char* kBindingOrder360[] = {"name", "type", "rowStart", "rowEnd", "columnStart", "columnEnd"};
inline constexpr const char* kBindingOrder361[] = {"lines", "tracks"};
inline constexpr const char* kBindingOrder362[] = {"names", "start", "breadth", "type", "number", "negativeNumber"};
inline constexpr const char* kBindingOrder363[] = {"item", "@@iterator", "length"};
inline constexpr const char* kBindingOrder364[] = {"start", "breadth", "type", "state"};
inline constexpr const char* kBindingOrder365[] = {"item", "@@iterator", "length"};
inline constexpr const char* kBindingOrder366[] = {"namedItem", "item", "@@iterator", "length"};
inline constexpr const char* kBindingOrder367[] = {"toString", "download", "ping", "rel", "referrerPolicy", "relList", "text", "coords", "charset", "name", "rev", "shape", "href", "target", "origin", "protocol", "username", "password", "host", "hostname", "port", "pathname", "search", "hash", "hreflang", "type"};
inline constexpr const char* kBindingOrder368[] = {"toString", "alt", "coords", "shape", "download", "ping", "rel", "referrerPolicy", "relList", "noHref", "href", "target", "origin", "protocol", "username", "password", "host", "hostname", "port", "pathname", "search", "hash", "hreflang", "type"};
inline constexpr const char* kBindingOrder369[] = {"clear"};
inline constexpr const char* kBindingOrder370[] = {"href", "target"};
inline constexpr const char* kBindingOrder371[] = {"text", "link", "vLink", "aLink", "bgColor", "background", "onafterprint", "onbeforeprint", "onbeforeunload", "onhashchange", "onlanguagechange", "onmessage", "onmessageerror", "onoffline", "ononline", "onpagehide", "onpagereveal", "onpageshow", "onpageswap", "onpopstate", "onrejectionhandled", "onstorage", "onunhandledrejection", "onunload", "ongamepadconnected", "ongamepaddisconnected"};
inline constexpr const char* kBindingOrder372[] = {"checkValidity", "reportValidity", "setCustomValidity", "disabled", "form", "formAction", "formEnctype", "formMethod", "formNoValidate", "formTarget", "name", "type", "value", "willValidate", "validity", "validationMessage", "labels", "commandForElement", "command", "popoverTargetElement", "popoverTargetAction"};
inline constexpr const char* kBindingOrder373[] = {"getContext", "toDataURL", "toBlob", "captureStream", "transferControlToOffscreen", "width", "height", "mozOpaque", "mozPrintCallback"};
inline constexpr const char* kBindingOrder374[] = {"item", "namedItem", "@@iterator", "length"};
inline constexpr const char* kBindingOrder375[] = {"compact"};
inline constexpr const char* kBindingOrder376[] = {"value"};
inline constexpr const char* kBindingOrder377[] = {"options"};
inline constexpr const char* kBindingOrder378[] = {"name", "open"};
inline constexpr const char* kBindingOrder379[] = {"show", "showModal", "close", "requestClose", "closedBy", "open", "returnValue"};
inline constexpr const char* kBindingOrder380[] = {"compact"};
inline constexpr const char* kBindingOrder381[] = {"align"};
inline constexpr const char* kBindingOrder382[] = {"click", "attachInternals", "showPopover", "hidePopover", "togglePopover", "focus", "blur", "title", "lang", "translate", "dir", "innerText", "outerText", "hidden", "inert", "accessKey", "accessKeyLabel", "draggable", "contentEditable", "isContentEditable", "editContext", "popover", "spellcheck", "inputMode", "enterKeyHint", "autocapitalize", "autocorrect", "nonce", "headingOffset", "headingReset", "containerTiming", "containerTimingIgnore", "style", "attributeStyleMap", "offsetParent", "offsetTop", "offsetLeft", "offsetWidth", "offsetHeight", "onabort", "onblur", "onfocus", "oncancel", "onauxclick", "onbeforeinput", "onbeforematch", "onbeforetoggle", "oncanplay", "oncanplaythrough", "onchange", "onclick", "onclose", "oncontentvisibilityautostatechange", "oncontextlost", "oncontextmenu", "oncommand", "oncontextrestored", "oncopy", "oncuechange", "oncut", "ondblclick", "ondrag", "ondragend", "ondragenter", "ondragexit", "ondragleave", "ondragover", "ondragstart", "ondrop", "ondurationchange", "onemptied", "onended", "onformdata", "oninput", "oninvalid", "onkeydown", "onkeypress", "onkeyup", "onload", "onloadeddata", "onloadedmetadata", "onloadstart", "onmousedown", "onmouseenter", "onmouseleave", "onmousemove", "onmouseout", "onmouseover", "onmouseup", "onwheel", "onpaste", "onpause", "onplay", "onplaying", "onprogress", "onratechange", "onreset", "onresize", "onscroll", "onscrollend", "onsecuritypolicyviolation", "onseeked", "onseeking", "onselect", "onslotchange", "onstalled", "onsubmit", "onsuspend", "ontimeupdate", "onvolumechange", "onwaiting", "onselectstart", "onselectionchange", "ontoggle", "onpointercancel", "onpointerdown", "onpointerup", "onpointermove", "onpointerout", "onpointerover", "onpointerenter", "onpointerleave", "onpointerrawupdate", "ongotpointercapture", "onlostpointercapture", "onmozfullscreenchange", "onmozfullscreenerror", "onanimationcancel", "onanimationend", "onanimationiteration", "onanimationstart", "ontransitioncancel", "ontransitionend", "ontransitionrun", "ontransitionstart", "onwebkitanimationend", "onwebkitanimationiteration", "onwebkitanimationstart", "onwebkittransitionend", "dataset", "autofocus", "tabIndex", "onerror", "ontouchstart", "ontouchend", "ontouchmove", "ontouchcancel"};
inline constexpr const char* kBindingOrder383[] = {"getSVGDocument", "src", "type", "width", "height", "align", "name"};
inline constexpr const char* kBindingOrder384[] = {"checkValidity", "reportValidity", "setCustomValidity", "disabled", "form", "name", "type", "elements", "willValidate", "validity", "validationMessage"};
inline constexpr const char* kBindingOrder385[] = {"color", "face", "size"};
inline constexpr const char* kBindingOrder386[] = {"namedItem", "@@iterator"};
inline constexpr const char* kBindingOrder387[] = {"submit", "requestSubmit", "reset", "checkValidity", "reportValidity", "@@iterator", "acceptCharset", "action", "autocomplete", "enctype", "encoding", "method", "name", "noValidate", "target", "rel", "relList", "elements", "length"};
inline constexpr const char* kBindingOrder388[] = {"name", "scrolling", "src", "frameBorder", "longDesc", "noResize", "contentDocument", "contentWindow", "marginHeight", "marginWidth"};
inline constexpr const char* kBindingOrder389[] = {"cols", "rows", "onafterprint", "onbeforeprint", "onbeforeunload", "onhashchange", "onlanguagechange", "onmessage", "onmessageerror", "onoffline", "ononline", "onpagehide", "onpagereveal", "onpageshow", "onpageswap", "onpopstate", "onrejectionhandled", "onstorage", "onunhandledrejection", "onunload", "ongamepadconnected", "ongamepaddisconnected"};
inline constexpr const char* kBindingOrder390[] = {"align", "color", "noShade", "size", "width"};
inline constexpr const char* kBindingOrder391[] = {"align"};
inline constexpr const char* kBindingOrder392[] = {"version"};
inline constexpr const char* kBindingOrder393[] = {"getSVGDocument", "src", "srcdoc", "name", "sandbox", "allowFullscreen", "width", "height", "referrerPolicy", "loading", "contentDocument", "contentWindow", "align", "scrolling", "frameBorder", "longDesc", "marginHeight", "marginWidth", "featurePolicy", "permissionsPolicy", "allow"};
inline constexpr const char* kBindingOrder394[] = {"decode", "alt", "src", "srcset", "crossOrigin", "useMap", "referrerPolicy", "isMap", "width", "height", "decoding", "loading", "fetchPriority", "naturalWidth", "naturalHeight", "complete", "name", "align", "hspace", "vspace", "longDesc", "border", "sizes", "currentSrc", "lowsrc", "x", "y"};
inline constexpr const char* kBindingOrder395[] = {"stepUp", "stepDown", "checkValidity", "reportValidity", "setCustomValidity", "select", "setRangeText", "setSelectionRange", "showPicker", "openDateTimePicker", "closeDateTimePicker", "setOpenState", "setFocusState", "updateValidityState", "getStep", "getStepBase", "getColor", "setUserInputColor", "setUserInput", "accept", "alpha", "alt", "autocomplete", "capture", "defaultChecked", "checked", "colorSpace", "dirName", "disabled", "form", "files", "formAction", "formEnctype", "formMethod", "formNoValidate", "formTarget", "height", "indeterminate", "list", "max", "maxLength", "min", "minLength", "multiple", "name", "pattern", "placeholder", "readOnly", "required", "size", "src", "step", "type", "defaultValue", "value", "valueAsDate", "valueAsNumber", "width", "willValidate", "validity", "validationMessage", "labels", "selectionStart", "selectionEnd", "selectionDirection", "align", "useMap", "textLength", "webkitEntries", "webkitdirectory", "popoverTargetElement", "popoverTargetAction"};
inline constexpr const char* kBindingOrder396[] = {"value", "type"};
inline constexpr const char* kBindingOrder397[] = {"form", "htmlFor", "control"};
inline constexpr const char* kBindingOrder398[] = {"form", "align"};
inline constexpr const char* kBindingOrder399[] = {"disabled", "href", "crossOrigin", "rel", "relList", "media", "hreflang", "type", "referrerPolicy", "sizes", "imageSrcset", "imageSizes", "blocking", "fetchPriority", "charset", "rev", "target", "integrity", "as", "sheet"};
inline constexpr const char* kBindingOrder400[] = {"name", "areas"};
inline constexpr const char* kBindingOrder401[] = {"start", "stop", "behavior", "bgColor", "direction", "height", "hspace", "loop", "scrollAmount", "scrollDelay", "trueSpeed", "vspace", "width"};
inline constexpr const char* kBindingOrder402[] = {"load", "canPlayType", "fastSeek", "updateCueDisplay", "play", "pause", "addTextTrack", "mozRequestDebugInfo", "mozRequestDebugLog", "mozGetMetadata", "setMediaKeys", "seekToNextFrame", "setVisible", "hasSuspendTaint", "setSinkId", "captureStream", "mozCaptureStream", "mozCaptureStreamUntilEnded", "error", "src", "currentSrc", "crossOrigin", "networkState", "preload", "buffered", "readyState", "seeking", "currentTime", "duration", "paused", "defaultPlaybackRate", "playbackRate", "played", "seekable", "ended", "autoplay", "loop", "controls", "volume", "muted", "defaultMuted", "loading", "audioTracks", "videoTracks", "textTracks", "mozMediaSourceObject", "srcObject", "preservesPitch", "mozFragmentEnd", "mediaKeys", "onencrypted", "onwaitingforkey", "computedVolume", "computedMuted", "sinkId", "allowedToPlay", "mozAudioCaptured", "NETWORK_EMPTY", "NETWORK_IDLE", "NETWORK_LOADING", "NETWORK_NO_SOURCE", "HAVE_NOTHING", "HAVE_METADATA", "HAVE_CURRENT_DATA", "HAVE_FUTURE_DATA", "HAVE_ENOUGH_DATA"};
inline constexpr const char* kBindingOrder403[] = {"compact"};
inline constexpr const char* kBindingOrder404[] = {"name", "httpEquiv", "content", "media", "scheme"};
inline constexpr const char* kBindingOrder405[] = {"value", "min", "max", "low", "high", "optimum", "labels"};
inline constexpr const char* kBindingOrder406[] = {"cite", "dateTime"};
inline constexpr const char* kBindingOrder407[] = {"reversed", "start", "type", "compact"};
inline constexpr const char* kBindingOrder408[] = {"checkValidity", "reportValidity", "setCustomValidity", "getSVGDocument", "data", "type", "name", "useMap", "form", "width", "height", "contentDocument", "contentWindow", "willValidate", "validity", "validationMessage", "align", "archive", "code", "declare", "hspace", "standby", "vspace", "codeBase", "codeType", "border"};
inline constexpr const char* kBindingOrder409[] = {"disabled", "label"};
inline constexpr const char* kBindingOrder410[] = {"disabled", "form", "label", "defaultSelected", "selected", "value", "text", "index"};
inline constexpr const char* kBindingOrder411[] = {"add", "remove", "@@iterator", "length", "selectedIndex"};
inline constexpr const char* kBindingOrder412[] = {"checkValidity", "reportValidity", "setCustomValidity", "htmlFor", "form", "name", "type", "defaultValue", "value", "willValidate", "validity", "validationMessage", "labels"};
inline constexpr const char* kBindingOrder413[] = {"align"};
inline constexpr const char* kBindingOrder414[] = {"name", "value", "type", "valueType"};
inline constexpr const char* kBindingOrder415[] = {"width"};
inline constexpr const char* kBindingOrder416[] = {"value", "max", "position", "labels"};
inline constexpr const char* kBindingOrder417[] = {"cite"};
inline constexpr const char* kBindingOrder418[] = {"src", "type", "noModule", "charset", "async", "defer", "crossOrigin", "referrerPolicy", "text", "blocking", "fetchPriority", "event", "htmlFor", "integrity", "innerText", "textContent"};
inline constexpr const char* kBindingOrder419[] = {"item", "namedItem", "add", "remove", "checkValidity", "reportValidity", "setCustomValidity", "showPicker", "@@iterator", "autocomplete", "disabled", "form", "multiple", "name", "required", "size", "type", "options", "length", "selectedOptions", "selectedIndex", "value", "willValidate", "validity", "validationMessage", "labels"};
inline constexpr const char* kBindingOrder420[] = {"assignedNodes", "assignedElements", "assign", "name"};
inline constexpr const char* kBindingOrder421[] = {"src", "type", "srcset", "sizes", "media", "width", "height"};
inline constexpr const char* kBindingOrder422[] = {"disabled", "media", "type", "blocking", "sheet"};
inline constexpr const char* kBindingOrder423[] = {"align"};
inline constexpr const char* kBindingOrder424[] = {"colSpan", "rowSpan", "headers", "cellIndex", "abbr", "scope", "align", "axis", "height", "width", "ch", "chOff", "noWrap", "vAlign", "bgColor"};
inline constexpr const char* kBindingOrder425[] = {"span", "align", "ch", "chOff", "vAlign", "width"};
inline constexpr const char* kBindingOrder426[] = {"createCaption", "deleteCaption", "createTHead", "deleteTHead", "createTFoot", "deleteTFoot", "createTBody", "insertRow", "deleteRow", "caption", "tHead", "tFoot", "tBodies", "rows", "align", "border", "frame", "rules", "summary", "width", "bgColor", "cellPadding", "cellSpacing"};
inline constexpr const char* kBindingOrder427[] = {"insertCell", "deleteCell", "rowIndex", "sectionRowIndex", "cells", "align", "ch", "chOff", "vAlign", "bgColor"};
inline constexpr const char* kBindingOrder428[] = {"insertRow", "deleteRow", "rows", "align", "ch", "chOff", "vAlign"};
inline constexpr const char* kBindingOrder429[] = {"content", "htmlFor", "shadowRootMode", "shadowRootDelegatesFocus", "shadowRootClonable", "shadowRootSerializable", "shadowRootReferenceTarget", "shadowRootSlotAssignment", "shadowRootCustomElementRegistry"};
inline constexpr const char* kBindingOrder430[] = {"checkValidity", "reportValidity", "setCustomValidity", "select", "setRangeText", "setSelectionRange", "setUserInput", "autocomplete", "cols", "dirName", "disabled", "form", "maxLength", "minLength", "name", "placeholder", "readOnly", "required", "rows", "wrap", "type", "defaultValue", "value", "textLength", "willValidate", "validity", "validationMessage", "labels", "selectionStart", "selectionEnd", "selectionDirection"};
inline constexpr const char* kBindingOrder431[] = {"dateTime"};
inline constexpr const char* kBindingOrder432[] = {"text"};
inline constexpr const char* kBindingOrder433[] = {"kind", "src", "srclang", "label", "default", "readyState", "track", "NONE", "LOADING", "LOADED", "ERROR"};
inline constexpr const char* kBindingOrder434[] = {"compact", "type"};
inline constexpr const char* kBindingOrder435[] = {"cloneElementVisually", "stopCloningElementVisually", "getVideoPlaybackQuality", "requestPictureInPicture", "requestVideoFrameCallback", "cancelVideoFrameCallback", "width", "height", "videoWidth", "videoHeight", "poster", "mozParsedFrames", "mozDecodedFrames", "mozPresentedFrames", "mozPaintedFrames", "mozFrameDelay", "mozHasAudio", "isCloningElementVisually", "onenterpictureinpicture", "onleavepictureinpicture", "disablePictureInPicture"};
inline constexpr const char* kBindingOrder436[] = {"oldURL", "newURL"};
inline constexpr const char* kBindingOrder437[] = {"append", "delete", "get", "getSetCookie", "has", "set", "entries", "keys", "values", "forEach"};
inline constexpr const char* kBindingOrder438[] = {"next"};
inline constexpr const char* kBindingOrder439[] = {"takeCensus", "describeNode", "computeDominatorTree", "computeShortestPaths", "creationTime"};
inline constexpr const char* kBindingOrder440[] = {"entries", "keys", "values", "forEach", "has", "clear", "delete", "add", "priority", "type", "size"};
inline constexpr const char* kBindingOrder441[] = {"next"};
inline constexpr const char* kBindingOrder442[] = {"highlightsFromPoint", "entries", "keys", "values", "forEach", "has", "clear", "delete", "get", "set", "size"};
inline constexpr const char* kBindingOrder443[] = {"next"};
inline constexpr const char* kBindingOrder444[] = {"go", "back", "forward", "pushState", "replaceState", "length", "scrollRestoration", "state"};
inline constexpr const char* kBindingOrder445[] = {"advance", "continue", "continuePrimaryKey", "update", "delete", "source", "direction", "key", "primaryKey", "request"};
inline constexpr const char* kBindingOrder446[] = {"value"};
inline constexpr const char* kBindingOrder447[] = {"transaction", "createObjectStore", "deleteObjectStore", "close", "name", "version", "objectStoreNames", "onabort", "onclose", "onerror", "onversionchange"};
inline constexpr const char* kBindingOrder448[] = {"open", "deleteDatabase", "databases", "cmp"};
inline constexpr const char* kBindingOrder449[] = {"get", "getKey", "getAll", "mozGetAll", "getAllKeys", "mozGetAllKeys", "getAllRecords", "count", "openCursor", "openKeyCursor", "name", "objectStore", "keyPath", "multiEntry", "unique", "locale", "isAutoLocale"};
inline constexpr const char* kBindingOrder450[] = {"includes", "lower", "upper", "lowerOpen", "upperOpen"};
inline constexpr const char* kBindingOrder451[] = {"put", "add", "delete", "clear", "get", "getKey", "getAll", "mozGetAll", "getAllKeys", "getAllRecords", "count", "openCursor", "openKeyCursor", "createIndex", "index", "deleteIndex", "name", "keyPath", "indexNames", "transaction", "autoIncrement"};
inline constexpr const char* kBindingOrder452[] = {"onblocked", "onupgradeneeded"};
inline constexpr const char* kBindingOrder453[] = {"key", "primaryKey", "value"};
inline constexpr const char* kBindingOrder454[] = {"result", "error", "source", "transaction", "readyState", "onsuccess", "onerror"};
inline constexpr const char* kBindingOrder455[] = {"objectStore", "commit", "abort", "objectStoreNames", "mode", "durability", "db", "error", "onabort", "oncomplete", "onerror"};
inline constexpr const char* kBindingOrder456[] = {"oldVersion", "newVersion"};
inline constexpr const char* kBindingOrder457[] = {"getFrequencyResponse"};
inline constexpr const char* kBindingOrder458[] = {"token", "isAutoSelected", "configURL"};
inline constexpr const char* kBindingOrder459[] = {"timeRemaining", "didTimeout"};
inline constexpr const char* kBindingOrder460[] = {"close", "width", "height"};
inline constexpr const char* kBindingOrder461[] = {"transferFromImageBitmap", "transferImageBitmap", "canvas"};
inline constexpr const char* kBindingOrder462[] = {"takePhoto", "videoStreamTrack", "onphoto", "onerror"};
inline constexpr const char* kBindingOrder463[] = {"code", "message", "FRAME_GRAB_ERROR", "SETTINGS_ERROR", "PHOTO_ERROR", "ERROR_UNKNOWN"};
inline constexpr const char* kBindingOrder464[] = {"imageCaptureError"};
inline constexpr const char* kBindingOrder465[] = {"width", "height", "data", "colorSpace"};
inline constexpr const char* kBindingOrder466[] = {"decode", "reset", "close", "type", "complete", "completed", "tracks"};
inline constexpr const char* kBindingOrder467[] = {"shrinkToFit", "restoreImage", "imageIsOverflowing", "imageIsResized"};
inline constexpr const char* kBindingOrder468[] = {"animated", "frameCount", "repetitionCount", "selected"};
inline constexpr const char* kBindingOrder469[] = {"@@iterator", "ready", "length", "selectedIndex", "selectedTrack"};
inline constexpr const char* kBindingOrder470[] = {"run", "release", "startProfiling", "endProfiling", "inputNames", "outputNames"};
inline constexpr const char* kBindingOrder471[] = {"getTargetRanges", "isComposing", "inputType", "data", "dataTransfer"};
inline constexpr const char* kBindingOrder472[] = {"nextToken", "lineNumber", "columnNumber"};
inline constexpr const char* kBindingOrder473[] = {"getNameString", "getVariationAxes", "getVariationInstances", "getFeatures", "fromFontGroup", "fromLanguagePrefs", "fromSystemFallback", "name", "CSSFamilyName", "CSSGeneric", "ranges", "rule", "srcIndex", "URI", "localName", "format", "metadata", "NAME_ID_COPYRIGHT", "NAME_ID_FAMILY", "NAME_ID_SUBFAMILY", "NAME_ID_UNIQUE", "NAME_ID_FULL", "NAME_ID_VERSION", "NAME_ID_POSTSCRIPT", "NAME_ID_TRADEMARK", "NAME_ID_MANUFACTURER", "NAME_ID_DESIGNER", "NAME_ID_DESCRIPTION", "NAME_ID_VENDOR_URL", "NAME_ID_DESIGNER_URL", "NAME_ID_LICENSE", "NAME_ID_LICENSE_URL", "NAME_ID_TYPOGRAPHIC_FAMILY", "NAME_ID_TYPOGRAPHIC_SUBFAMILY", "NAME_ID_COMPATIBLE_FULL", "NAME_ID_SAMPLE_TEXT"};
inline constexpr const char* kBindingOrder474[] = {"GRID_NONE", "GRID_CONTAINER", "GRID_SUBGRID_ROW", "GRID_SUBGRID_COL"};
inline constexpr const char* kBindingOrder475[] = {"toJSON", "documentURL", "blockedURL", "destination", "reportOnly", "reason"};
inline constexpr const char* kBindingOrder476[] = {"observe", "unobserve", "disconnect", "takeRecords", "root", "rootMargin", "scrollMargin", "thresholds"};
inline constexpr const char* kBindingOrder477[] = {"time", "rootBounds", "boundingClientRect", "intersectionRect", "isIntersecting", "intersectionRatio", "target"};
inline constexpr const char* kBindingOrder478[] = {"getDisplayNames", "isAppLocaleRTL"};
inline constexpr const char* kBindingOrder479[] = {"sendAsyncMessage", "sendQuery", "manager", "name"};
inline constexpr const char* kBindingOrder480[] = {"sendAsyncMessage", "sendQuery", "manager", "name"};
inline constexpr const char* kBindingOrder481[] = {"sendAsyncMessage", "sendQuery", "manager", "windowContext", "document", "browsingContext", "docShell", "contentWindow", "name"};
inline constexpr const char* kBindingOrder482[] = {"sendAsyncMessage", "sendQuery", "manager", "windowContext", "browsingContext", "name"};
inline constexpr const char* kBindingOrder483[] = {"DOM_VK_CANCEL", "DOM_VK_HELP", "DOM_VK_BACK_SPACE", "DOM_VK_TAB", "DOM_VK_CLEAR", "DOM_VK_RETURN", "DOM_VK_SHIFT", "DOM_VK_CONTROL", "DOM_VK_ALT", "DOM_VK_PAUSE", "DOM_VK_CAPS_LOCK", "DOM_VK_KANA", "DOM_VK_HANGUL", "DOM_VK_EISU", "DOM_VK_JUNJA", "DOM_VK_FINAL", "DOM_VK_HANJA", "DOM_VK_KANJI", "DOM_VK_ESCAPE", "DOM_VK_CONVERT", "DOM_VK_NONCONVERT", "DOM_VK_ACCEPT", "DOM_VK_MODECHANGE", "DOM_VK_SPACE", "DOM_VK_PAGE_UP", "DOM_VK_PAGE_DOWN", "DOM_VK_END", "DOM_VK_HOME", "DOM_VK_LEFT", "DOM_VK_UP", "DOM_VK_RIGHT", "DOM_VK_DOWN", "DOM_VK_SELECT", "DOM_VK_PRINT", "DOM_VK_EXECUTE", "DOM_VK_PRINTSCREEN", "DOM_VK_INSERT", "DOM_VK_DELETE", "DOM_VK_0", "DOM_VK_1", "DOM_VK_2", "DOM_VK_3", "DOM_VK_4", "DOM_VK_5", "DOM_VK_6", "DOM_VK_7", "DOM_VK_8", "DOM_VK_9", "DOM_VK_COLON", "DOM_VK_SEMICOLON", "DOM_VK_LESS_THAN", "DOM_VK_EQUALS", "DOM_VK_GREATER_THAN", "DOM_VK_QUESTION_MARK", "DOM_VK_AT", "DOM_VK_A", "DOM_VK_B", "DOM_VK_C", "DOM_VK_D", "DOM_VK_E", "DOM_VK_F", "DOM_VK_G", "DOM_VK_H", "DOM_VK_I", "DOM_VK_J", "DOM_VK_K", "DOM_VK_L", "DOM_VK_M", "DOM_VK_N", "DOM_VK_O", "DOM_VK_P", "DOM_VK_Q", "DOM_VK_R", "DOM_VK_S", "DOM_VK_T", "DOM_VK_U", "DOM_VK_V", "DOM_VK_W", "DOM_VK_X", "DOM_VK_Y", "DOM_VK_Z", "DOM_VK_WIN", "DOM_VK_CONTEXT_MENU", "DOM_VK_SLEEP", "DOM_VK_NUMPAD0", "DOM_VK_NUMPAD1", "DOM_VK_NUMPAD2", "DOM_VK_NUMPAD3", "DOM_VK_NUMPAD4", "DOM_VK_NUMPAD5", "DOM_VK_NUMPAD6", "DOM_VK_NUMPAD7", "DOM_VK_NUMPAD8", "DOM_VK_NUMPAD9", "DOM_VK_MULTIPLY", "DOM_VK_ADD", "DOM_VK_SEPARATOR", "DOM_VK_SUBTRACT", "DOM_VK_DECIMAL", "DOM_VK_DIVIDE", "DOM_VK_F1", "DOM_VK_F2", "DOM_VK_F3", "DOM_VK_F4", "DOM_VK_F5", "DOM_VK_F6", "DOM_VK_F7", "DOM_VK_F8", "DOM_VK_F9", "DOM_VK_F10", "DOM_VK_F11", "DOM_VK_F12", "DOM_VK_F13", "DOM_VK_F14", "DOM_VK_F15", "DOM_VK_F16", "DOM_VK_F17", "DOM_VK_F18", "DOM_VK_F19", "DOM_VK_F20", "DOM_VK_F21", "DOM_VK_F22", "DOM_VK_F23", "DOM_VK_F24", "DOM_VK_NUM_LOCK", "DOM_VK_SCROLL_LOCK", "DOM_VK_WIN_OEM_FJ_JISHO", "DOM_VK_WIN_OEM_FJ_MASSHOU", "DOM_VK_WIN_OEM_FJ_TOUROKU", "DOM_VK_WIN_OEM_FJ_LOYA", "DOM_VK_WIN_OEM_FJ_ROYA", "DOM_VK_CIRCUMFLEX", "DOM_VK_EXCLAMATION", "DOM_VK_DOUBLE_QUOTE", "DOM_VK_HASH", "DOM_VK_DOLLAR", "DOM_VK_PERCENT", "DOM_VK_AMPERSAND", "DOM_VK_UNDERSCORE", "DOM_VK_OPEN_PAREN", "DOM_VK_CLOSE_PAREN", "DOM_VK_ASTERISK", "DOM_VK_PLUS", "DOM_VK_PIPE", "DOM_VK_HYPHEN_MINUS", "DOM_VK_OPEN_CURLY_BRACKET", "DOM_VK_CLOSE_CURLY_BRACKET", "DOM_VK_TILDE", "DOM_VK_VOLUME_MUTE", "DOM_VK_VOLUME_DOWN", "DOM_VK_VOLUME_UP", "DOM_VK_COMMA", "DOM_VK_PERIOD", "DOM_VK_SLASH", "DOM_VK_BACK_QUOTE", "DOM_VK_OPEN_BRACKET", "DOM_VK_BACK_SLASH", "DOM_VK_CLOSE_BRACKET", "DOM_VK_QUOTE", "DOM_VK_META", "DOM_VK_ALTGR", "DOM_VK_WIN_ICO_HELP", "DOM_VK_WIN_ICO_00", "DOM_VK_PROCESSKEY", "DOM_VK_WIN_ICO_CLEAR", "DOM_VK_WIN_OEM_RESET", "DOM_VK_WIN_OEM_JUMP", "DOM_VK_WIN_OEM_PA1", "DOM_VK_WIN_OEM_PA2", "DOM_VK_WIN_OEM_PA3", "DOM_VK_WIN_OEM_WSCTRL", "DOM_VK_WIN_OEM_CUSEL", "DOM_VK_WIN_OEM_ATTN", "DOM_VK_WIN_OEM_FINISH", "DOM_VK_WIN_OEM_COPY", "DOM_VK_WIN_OEM_AUTO", "DOM_VK_WIN_OEM_ENLW", "DOM_VK_WIN_OEM_BACKTAB", "DOM_VK_ATTN", "DOM_VK_CRSEL", "DOM_VK_EXSEL", "DOM_VK_EREOF", "DOM_VK_PLAY", "DOM_VK_ZOOM", "DOM_VK_PA1", "DOM_VK_WIN_OEM_CLEAR"};
inline constexpr const char* kBindingOrder484[] = {"getModifierState", "initKeyboardEvent", "charCode", "keyCode", "altKey", "ctrlKey", "shiftKey", "metaKey", "location", "repeat", "isComposing", "key", "code", "DOM_KEY_LOCATION_STANDARD", "DOM_KEY_LOCATION_LEFT", "DOM_KEY_LOCATION_RIGHT", "DOM_KEY_LOCATION_NUMPAD", "DOM_VK_CANCEL", "DOM_VK_HELP", "DOM_VK_BACK_SPACE", "DOM_VK_TAB", "DOM_VK_CLEAR", "DOM_VK_RETURN", "DOM_VK_SHIFT", "DOM_VK_CONTROL", "DOM_VK_ALT", "DOM_VK_PAUSE", "DOM_VK_CAPS_LOCK", "DOM_VK_KANA", "DOM_VK_HANGUL", "DOM_VK_EISU", "DOM_VK_JUNJA", "DOM_VK_FINAL", "DOM_VK_HANJA", "DOM_VK_KANJI", "DOM_VK_ESCAPE", "DOM_VK_CONVERT", "DOM_VK_NONCONVERT", "DOM_VK_ACCEPT", "DOM_VK_MODECHANGE", "DOM_VK_SPACE", "DOM_VK_PAGE_UP", "DOM_VK_PAGE_DOWN", "DOM_VK_END", "DOM_VK_HOME", "DOM_VK_LEFT", "DOM_VK_UP", "DOM_VK_RIGHT", "DOM_VK_DOWN", "DOM_VK_SELECT", "DOM_VK_PRINT", "DOM_VK_EXECUTE", "DOM_VK_PRINTSCREEN", "DOM_VK_INSERT", "DOM_VK_DELETE", "DOM_VK_0", "DOM_VK_1", "DOM_VK_2", "DOM_VK_3", "DOM_VK_4", "DOM_VK_5", "DOM_VK_6", "DOM_VK_7", "DOM_VK_8", "DOM_VK_9", "DOM_VK_COLON", "DOM_VK_SEMICOLON", "DOM_VK_LESS_THAN", "DOM_VK_EQUALS", "DOM_VK_GREATER_THAN", "DOM_VK_QUESTION_MARK", "DOM_VK_AT", "DOM_VK_A", "DOM_VK_B", "DOM_VK_C", "DOM_VK_D", "DOM_VK_E", "DOM_VK_F", "DOM_VK_G", "DOM_VK_H", "DOM_VK_I", "DOM_VK_J", "DOM_VK_K", "DOM_VK_L", "DOM_VK_M", "DOM_VK_N", "DOM_VK_O", "DOM_VK_P", "DOM_VK_Q", "DOM_VK_R", "DOM_VK_S", "DOM_VK_T", "DOM_VK_U", "DOM_VK_V", "DOM_VK_W", "DOM_VK_X", "DOM_VK_Y", "DOM_VK_Z", "DOM_VK_WIN", "DOM_VK_CONTEXT_MENU", "DOM_VK_SLEEP", "DOM_VK_NUMPAD0", "DOM_VK_NUMPAD1", "DOM_VK_NUMPAD2", "DOM_VK_NUMPAD3", "DOM_VK_NUMPAD4", "DOM_VK_NUMPAD5", "DOM_VK_NUMPAD6", "DOM_VK_NUMPAD7", "DOM_VK_NUMPAD8", "DOM_VK_NUMPAD9", "DOM_VK_MULTIPLY", "DOM_VK_ADD", "DOM_VK_SEPARATOR", "DOM_VK_SUBTRACT", "DOM_VK_DECIMAL", "DOM_VK_DIVIDE", "DOM_VK_F1", "DOM_VK_F2", "DOM_VK_F3", "DOM_VK_F4", "DOM_VK_F5", "DOM_VK_F6", "DOM_VK_F7", "DOM_VK_F8", "DOM_VK_F9", "DOM_VK_F10", "DOM_VK_F11", "DOM_VK_F12", "DOM_VK_F13", "DOM_VK_F14", "DOM_VK_F15", "DOM_VK_F16", "DOM_VK_F17", "DOM_VK_F18", "DOM_VK_F19", "DOM_VK_F20", "DOM_VK_F21", "DOM_VK_F22", "DOM_VK_F23", "DOM_VK_F24", "DOM_VK_NUM_LOCK", "DOM_VK_SCROLL_LOCK", "DOM_VK_WIN_OEM_FJ_JISHO", "DOM_VK_WIN_OEM_FJ_MASSHOU", "DOM_VK_WIN_OEM_FJ_TOUROKU", "DOM_VK_WIN_OEM_FJ_LOYA", "DOM_VK_WIN_OEM_FJ_ROYA", "DOM_VK_CIRCUMFLEX", "DOM_VK_EXCLAMATION", "DOM_VK_DOUBLE_QUOTE", "DOM_VK_HASH", "DOM_VK_DOLLAR", "DOM_VK_PERCENT", "DOM_VK_AMPERSAND", "DOM_VK_UNDERSCORE", "DOM_VK_OPEN_PAREN", "DOM_VK_CLOSE_PAREN", "DOM_VK_ASTERISK", "DOM_VK_PLUS", "DOM_VK_PIPE", "DOM_VK_HYPHEN_MINUS", "DOM_VK_OPEN_CURLY_BRACKET", "DOM_VK_CLOSE_CURLY_BRACKET", "DOM_VK_TILDE", "DOM_VK_VOLUME_MUTE", "DOM_VK_VOLUME_DOWN", "DOM_VK_VOLUME_UP", "DOM_VK_COMMA", "DOM_VK_PERIOD", "DOM_VK_SLASH", "DOM_VK_BACK_QUOTE", "DOM_VK_OPEN_BRACKET", "DOM_VK_BACK_SLASH", "DOM_VK_CLOSE_BRACKET", "DOM_VK_QUOTE", "DOM_VK_META", "DOM_VK_ALTGR", "DOM_VK_WIN_ICO_HELP", "DOM_VK_WIN_ICO_00", "DOM_VK_PROCESSKEY", "DOM_VK_WIN_ICO_CLEAR", "DOM_VK_WIN_OEM_RESET", "DOM_VK_WIN_OEM_JUMP", "DOM_VK_WIN_OEM_PA1", "DOM_VK_WIN_OEM_PA2", "DOM_VK_WIN_OEM_PA3", "DOM_VK_WIN_OEM_WSCTRL", "DOM_VK_WIN_OEM_CUSEL", "DOM_VK_WIN_OEM_ATTN", "DOM_VK_WIN_OEM_FINISH", "DOM_VK_WIN_OEM_COPY", "DOM_VK_WIN_OEM_AUTO", "DOM_VK_WIN_OEM_ENLW", "DOM_VK_WIN_OEM_BACKTAB", "DOM_VK_ATTN", "DOM_VK_CRSEL", "DOM_VK_EXSEL", "DOM_VK_EREOF", "DOM_VK_PLAY", "DOM_VK_ZOOM", "DOM_VK_PA1", "DOM_VK_WIN_OEM_CLEAR"};
inline constexpr const char* kBindingOrder485[] = {"getKeyframes", "setKeyframes", "target", "pseudoElement", "iterationComposite", "composite"};
inline constexpr const char* kBindingOrder486[] = {"hasFile", "fetchFile", "fetchFileSync", "name", "metaSource", "locales", "prePath", "index"};
inline constexpr const char* kBindingOrder487[] = {"ERROR_UNKNOWN", "ERROR_FORBIDDEN_TYPE", "ERROR_NAMED_ELEMENT_MISSING", "ERROR_NAMED_ELEMENT_TYPE_MISMATCH", "ERROR_TRANSLATED_ELEMENT_DISCONNECTED", "ERROR_TRANSLATED_ELEMENT_DISALLOWED_DOM"};
inline constexpr const char* kBindingOrder488[] = {"getAvailableLocales", "registerSources", "updateSources", "removeSources", "hasSource", "getSourceNames", "clearSources", "generateBundlesSync", "generateBundles"};
inline constexpr const char* kBindingOrder489[] = {"toJSON", "renderTime", "loadTime", "size", "id", "url", "element", "paintTime", "presentationTime"};
inline constexpr const char* kBindingOrder490[] = {"open", "listen"};
inline constexpr const char* kBindingOrder491[] = {"initialize", "formatChat", "createGenerationStream"};
inline constexpr const char* kBindingOrder492[] = {"addResourceIds", "removeResourceIds", "formatValue", "formatValues", "formatMessages", "setAsync", "formatValueSync", "formatValuesSync", "formatMessagesSync"};
inline constexpr const char* kBindingOrder493[] = {"name", "mode"};
inline constexpr const char* kBindingOrder494[] = {"request", "query"};
inline constexpr const char* kBindingOrder495[] = {"inputs", "outputs", "onstatechange", "sysexEnabled"};
inline constexpr const char* kBindingOrder496[] = {"port"};
inline constexpr const char* kBindingOrder497[] = {"onmidimessage"};
inline constexpr const char* kBindingOrder498[] = {"entries", "keys", "values", "forEach", "has", "get", "size"};
inline constexpr const char* kBindingOrder499[] = {"data"};
inline constexpr const char* kBindingOrder500[] = {"send", "clear"};
inline constexpr const char* kBindingOrder501[] = {"entries", "keys", "values", "forEach", "has", "get", "size"};
inline constexpr const char* kBindingOrder502[] = {"open", "close", "id", "manufacturer", "name", "version", "type", "state", "connection", "onstatechange"};
inline constexpr const char* kBindingOrder503[] = {"deleteState", "generateIdentity", "generateCredential", "generateKeyPackage", "groupCreate", "groupGet", "groupJoin", "getGroupIdFromMessage", "getGroupEpochFromMessage"};
inline constexpr const char* kBindingOrder504[] = {"deleteState", "add", "proposeAdd", "remove", "proposeRemove", "close", "details", "send", "receive", "hasPendingProposals", "clearPendingProposals", "hasPendingCommit", "clearPendingCommit", "applyPendingCommit", "exportSecret", "groupId", "clientId"};
inline constexpr const char* kBindingOrder505[] = {"getParameter", "EXTENSIONS", "WSI_INFO", "UNPACK_REQUIRE_FASTPATH", "DOES_INDEX_VALIDATION", "CONTEXT_TYPE"};
inline constexpr const char* kBindingOrder506[] = {"matches", "glob"};
inline constexpr const char* kBindingOrder507[] = {"matches", "matchesCookie", "subsumes", "subsumesDomain", "overlaps", "pattern", "matchesAllWebUrls"};
inline constexpr const char* kBindingOrder508[] = {"matches", "matchesCookie", "subsumes", "subsumesDomain", "overlaps", "overlapsAll", "patterns", "matchesAllWebUrls"};
inline constexpr const char* kBindingOrder509[] = {"href", "target", "download", "ping", "rel", "relList", "referrerPolicy", "origin", "protocol", "username", "password", "host", "hostname", "port", "pathname", "search", "hash", "hreflang", "type"};
inline constexpr const char* kBindingOrder510[] = {"focus", "blur", "nonce", "style", "attributeStyleMap", "onabort", "onblur", "onfocus", "oncancel", "onauxclick", "onbeforeinput", "onbeforematch", "onbeforetoggle", "oncanplay", "oncanplaythrough", "onchange", "onclick", "onclose", "oncontentvisibilityautostatechange", "oncontextlost", "oncontextmenu", "oncommand", "oncontextrestored", "oncopy", "oncuechange", "oncut", "ondblclick", "ondrag", "ondragend", "ondragenter", "ondragexit", "ondragleave", "ondragover", "ondragstart", "ondrop", "ondurationchange", "onemptied", "onended", "onformdata", "oninput", "oninvalid", "onkeydown", "onkeypress", "onkeyup", "onload", "onloadeddata", "onloadedmetadata", "onloadstart", "onmousedown", "onmouseenter", "onmouseleave", "onmousemove", "onmouseout", "onmouseover", "onmouseup", "onwheel", "onpaste", "onpause", "onplay", "onplaying", "onprogress", "onratechange", "onreset", "onresize", "onscroll", "onscrollend", "onsecuritypolicyviolation", "onseeked", "onseeking", "onselect", "onslotchange", "onstalled", "onsubmit", "onsuspend", "ontimeupdate", "onvolumechange", "onwaiting", "onselectstart", "onselectionchange", "ontoggle", "onpointercancel", "onpointerdown", "onpointerup", "onpointermove", "onpointerout", "onpointerover", "onpointerenter", "onpointerleave", "onpointerrawupdate", "ongotpointercapture", "onlostpointercapture", "onmozfullscreenchange", "onmozfullscreenerror", "onanimationcancel", "onanimationend", "onanimationiteration", "onanimationstart", "ontransitioncancel", "ontransitionend", "ontransitionrun", "ontransitionstart", "onwebkitanimationend", "onwebkitanimationiteration", "onwebkitanimationstart", "onwebkittransitionend", "dataset", "autofocus", "tabIndex", "onerror", "ontouchstart", "ontouchend", "ontouchmove", "ontouchcancel"};
inline constexpr const char* kBindingOrder511[] = {"decodingInfo", "encodingInfo"};
inline constexpr const char* kBindingOrder512[] = {"getMetadata", "focus", "play", "pause", "resume", "stop", "mute", "unmute", "prevTrack", "nextTrack", "seekBackward", "seekForward", "skipAd", "seekTo", "id", "isActive", "isAudible", "isMuted", "isPlaying", "isAnyMediaBeingControlled", "playbackState", "effectiveAudioSessionType", "supportedKeys", "onactivated", "ondeactivated", "onaudiblechange", "oneffectiveaudiosessiontypechange", "onmetadatachange", "onplaybackstatechange", "onpositionstatechange", "onsupportedkeyschange"};
inline constexpr const char* kBindingOrder513[] = {"toJSON", "deviceId", "kind", "label", "groupId"};
inline constexpr const char* kBindingOrder514[] = {"getSupportedConstraints", "enumerateDevices", "getUserMedia", "getDisplayMedia", "selectAudioOutput", "ondevicechange"};
inline constexpr const char* kBindingOrder515[] = {"mediaElement"};
inline constexpr const char* kBindingOrder516[] = {"initDataType", "initData"};
inline constexpr const char* kBindingOrder517[] = {"code", "message", "MEDIA_ERR_ABORTED", "MEDIA_ERR_NETWORK", "MEDIA_ERR_DECODE", "MEDIA_ERR_SRC_NOT_SUPPORTED"};
inline constexpr const char* kBindingOrder518[] = {"systemCode"};
inline constexpr const char* kBindingOrder519[] = {"messageType", "message"};
inline constexpr const char* kBindingOrder520[] = {"generateRequest", "load", "update", "close", "remove", "error", "sessionId", "expiration", "closed", "keyStatuses", "onkeystatuseschange", "onmessage"};
inline constexpr const char* kBindingOrder521[] = {"has", "get", "entries", "keys", "values", "forEach", "size"};
inline constexpr const char* kBindingOrder522[] = {"next"};
inline constexpr const char* kBindingOrder523[] = {"getConfiguration", "createMediaKeys", "keySystem"};
inline constexpr const char* kBindingOrder524[] = {"createSession", "setServerCertificate", "getStatusForPolicy", "keySystem"};
inline constexpr const char* kBindingOrder525[] = {"item", "deleteMedium", "appendMedium", "@@iterator", "toString", "mediaText", "length"};
inline constexpr const char* kBindingOrder526[] = {"title", "artist", "album", "artwork"};
inline constexpr const char* kBindingOrder527[] = {"addListener", "removeListener", "media", "matches", "onchange"};
inline constexpr const char* kBindingOrder528[] = {"media", "matches"};
inline constexpr const char* kBindingOrder529[] = {"start", "stop", "pause", "resume", "requestData", "stream", "mimeType", "state", "onstart", "onstop", "ondataavailable", "onpause", "onresume", "onerror", "videoBitsPerSecond", "audioBitsPerSecond"};
inline constexpr const char* kBindingOrder530[] = {"error"};
inline constexpr const char* kBindingOrder531[] = {"setActionHandler", "setPositionState", "metadata", "playbackState"};
inline constexpr const char* kBindingOrder532[] = {"addSourceBuffer", "removeSourceBuffer", "endOfStream", "setLiveSeekableRange", "clearLiveSeekableRange", "sourceBuffers", "activeSourceBuffers", "readyState", "duration", "onsourceopen", "onsourceended", "onsourceclose"};
inline constexpr const char* kBindingOrder533[] = {"getAudioTracks", "getVideoTracks", "getTracks", "getTrackById", "addTrack", "removeTrack", "clone", "id", "active", "onaddtrack", "onremovetrack"};
inline constexpr const char* kBindingOrder534[] = {"stream"};
inline constexpr const char* kBindingOrder535[] = {"mediaStream"};
inline constexpr const char* kBindingOrder536[] = {"name", "message", "constraint"};
inline constexpr const char* kBindingOrder537[] = {"stream"};
inline constexpr const char* kBindingOrder538[] = {"clone", "stop", "getCapabilities", "getConstraints", "getSettings", "applyConstraints", "kind", "id", "label", "enabled", "muted", "onmute", "onunmute", "readyState", "onended"};
inline constexpr const char* kBindingOrder539[] = {"track"};
inline constexpr const char* kBindingOrder540[] = {"complete", "methodName", "validationURL"};
inline constexpr const char* kBindingOrder541[] = {"broadcastAsyncMessage", "getChildAt", "releaseCachedProcesses", "childCount"};
inline constexpr const char* kBindingOrder542[] = {"port1", "port2"};
inline constexpr const char* kBindingOrder543[] = {"initMessageEvent", "data", "origin", "lastEventId", "source", "ports"};
inline constexpr const char* kBindingOrder544[] = {"addMessageListener", "removeMessageListener"};
inline constexpr const char* kBindingOrder545[] = {"postMessage", "start", "close", "onmessage", "onmessageerror"};
inline constexpr const char* kBindingOrder546[] = {"sendAsyncMessage", "processMessageManager", "remoteType"};
inline constexpr const char* kBindingOrder547[] = {"type", "description", "suffixes", "enabledPlugin"};
inline constexpr const char* kBindingOrder548[] = {"item", "namedItem", "@@iterator", "length"};
inline constexpr const char* kBindingOrder549[] = {"registerTool", "unregisterTool", "getTools", "invokeTool"};
inline constexpr const char* kBindingOrder550[] = {"requestUserInteraction"};
inline constexpr const char* kBindingOrder551[] = {"initMouseEvent", "getModifierState", "initNSMouseEvent", "screenX", "screenY", "pageX", "pageY", "clientX", "clientY", "x", "y", "offsetX", "offsetY", "ctrlKey", "shiftKey", "altKey", "metaKey", "button", "buttons", "relatedTarget", "movementX", "movementY", "mozPressure", "mozInputSource", "MOZ_SOURCE_UNKNOWN", "MOZ_SOURCE_MOUSE", "MOZ_SOURCE_PEN", "MOZ_SOURCE_ERASER", "MOZ_SOURCE_CURSOR", "MOZ_SOURCE_TOUCH", "MOZ_SOURCE_KEYBOARD"};
inline constexpr const char* kBindingOrder552[] = {"initMouseScrollEvent", "axis", "HORIZONTAL_AXIS", "VERTICAL_AXIS"};
inline constexpr const char* kBindingOrder553[] = {"done", "context"};
inline constexpr const char* kBindingOrder554[] = {"matchesURI", "matchesWindowGlobal", "allFrames", "checkPermissions", "matchAboutBlank", "matchOriginAsFallback", "frameID", "matches", "excludeMatches", "isUserScript", "originAttributesPatterns", "extension"};
inline constexpr const char* kBindingOrder555[] = {"observe", "disconnect"};
inline constexpr const char* kBindingOrder556[] = {"has", "get", "entries", "keys", "values", "forEach"};
inline constexpr const char* kBindingOrder557[] = {"changedKeys"};
inline constexpr const char* kBindingOrder558[] = {"next"};
inline constexpr const char* kBindingOrder559[] = {"@@iterator", "length"};
inline constexpr const char* kBindingOrder560[] = {"@@iterator", "length"};
inline constexpr const char* kBindingOrder561[] = {"set", "delete", "flush"};
inline constexpr const char* kBindingOrder562[] = {"observe", "disconnect", "takeRecords"};
inline constexpr const char* kBindingOrder563[] = {"type", "target", "addedNodes", "removedNodes", "previousSibling", "nextSibling", "attributeName", "attributeNamespace", "oldValue"};
inline constexpr const char* kBindingOrder564[] = {"getNamedItem", "setNamedItem", "removeNamedItem", "item", "getNamedItemNS", "setNamedItemNS", "removeNamedItemNS", "@@iterator", "length"};
inline constexpr const char* kBindingOrder565[] = {"intercept", "scroll", "navigationType", "destination", "canIntercept", "userInitiated", "hashChange", "signal", "formData", "downloadRequest", "info", "hasUAVisualTransition", "sourceElement"};
inline constexpr const char* kBindingOrder566[] = {"entries", "updateCurrentEntry", "navigate", "reload", "traverseTo", "back", "forward", "currentEntry", "transition", "activation", "canGoBack", "canGoForward", "onnavigate", "onnavigatesuccess", "onnavigateerror", "oncurrententrychange"};
inline constexpr const char* kBindingOrder567[] = {"from", "entry", "navigationType"};
inline constexpr const char* kBindingOrder568[] = {"navigationType", "from"};
inline constexpr const char* kBindingOrder569[] = {"getState", "url", "key", "id", "index", "sameDocument"};
inline constexpr const char* kBindingOrder570[] = {"getState", "url", "key", "id", "index", "sameDocument", "ondispose"};
inline constexpr const char* kBindingOrder571[] = {"redirect", "addHandler"};
inline constexpr const char* kBindingOrder572[] = {"enable", "disable", "setHeaderValue", "getState"};
inline constexpr const char* kBindingOrder573[] = {"navigationType", "from", "committed", "finished"};
inline constexpr const char* kBindingOrder574[] = {"vibrate", "javaEnabled", "getGamepads", "requestGamepadServiceTest", "getVRDisplays", "requestVRServiceTest", "requestMIDIAccess", "mozGetUserMedia", "sendBeacon", "requestMediaKeySystemAccess", "share", "canShare", "getAutoplayPolicy", "registerProtocolHandler", "taintEnabled", "permissions", "mimeTypes", "plugins", "pdfViewerEnabled", "doNotTrack", "maxTouchPoints", "mediaCapabilities", "oscpu", "vendor", "vendorSub", "productSub", "cookieEnabled", "buildID", "mozAddonManager", "connection", "activeVRDisplays", "xr", "serial", "mediaDevices", "serviceWorker", "mozTCPSocket", "credentials", "clipboard", "testTrialGatedAttribute", "mediaSession", "audioSession", "userActivation", "wakeLock", "login", "modelContext", "globalPrivacyControl", "webdriver", "hardwareConcurrency", "gpu", "geolocation", "appCodeName", "appName", "appVersion", "platform", "userAgent", "product", "language", "languages", "locks", "onLine", "storage"};
inline constexpr const char* kBindingOrder575[] = {"setStatus"};
inline constexpr const char* kBindingOrder576[] = {"type", "ontypechange"};
inline constexpr const char* kBindingOrder577[] = {"getRootNode", "hasChildNodes", "insertBefore", "appendChild", "replaceChild", "removeChild", "normalize", "cloneNode", "isSameNode", "isEqualNode", "compareDocumentPosition", "contains", "lookupPrefix", "lookupNamespaceURI", "isDefaultNamespace", "nodeType", "nodeName", "baseURI", "isConnected", "ownerDocument", "parentNode", "parentElement", "childNodes", "firstChild", "lastChild", "previousSibling", "nextSibling", "nodeValue", "textContent", "containingShadowRoot", "accessibleNode", "ELEMENT_NODE", "ATTRIBUTE_NODE", "TEXT_NODE", "CDATA_SECTION_NODE", "ENTITY_REFERENCE_NODE", "ENTITY_NODE", "PROCESSING_INSTRUCTION_NODE", "COMMENT_NODE", "DOCUMENT_NODE", "DOCUMENT_TYPE_NODE", "DOCUMENT_FRAGMENT_NODE", "NOTATION_NODE", "DOCUMENT_POSITION_DISCONNECTED", "DOCUMENT_POSITION_PRECEDING", "DOCUMENT_POSITION_FOLLOWING", "DOCUMENT_POSITION_CONTAINS", "DOCUMENT_POSITION_CONTAINED_BY", "DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC"};
inline constexpr const char* kBindingOrder578[] = {"FILTER_ACCEPT", "FILTER_REJECT", "FILTER_SKIP", "SHOW_ALL", "SHOW_ELEMENT", "SHOW_ATTRIBUTE", "SHOW_TEXT", "SHOW_CDATA_SECTION", "SHOW_ENTITY_REFERENCE", "SHOW_ENTITY", "SHOW_PROCESSING_INSTRUCTION", "SHOW_COMMENT", "SHOW_DOCUMENT", "SHOW_DOCUMENT_TYPE", "SHOW_DOCUMENT_FRAGMENT", "SHOW_NOTATION"};
inline constexpr const char* kBindingOrder579[] = {"nextNode", "previousNode", "detach", "root", "referenceNode", "pointerBeforeReferenceNode", "whatToShow", "filter"};
inline constexpr const char* kBindingOrder580[] = {"item", "@@iterator", "keys", "values", "entries", "forEach", "length"};
inline constexpr const char* kBindingOrder581[] = {"close", "onclick", "onshow", "onerror", "onclose", "title", "dir", "lang", "body", "tag", "icon", "navigate", "requireInteraction", "silent", "vibrate", "data", "actions"};
inline constexpr const char* kBindingOrder582[] = {"notification", "action"};
inline constexpr const char* kBindingOrder583[] = {"enableiOES", "disableiOES", "blendEquationiOES", "blendEquationSeparateiOES", "blendFunciOES", "blendFuncSeparateiOES", "colorMaskiOES"};
inline constexpr const char* kBindingOrder584[] = {"FRAGMENT_SHADER_DERIVATIVE_HINT_OES"};
inline constexpr const char* kBindingOrder585[] = {"HALF_FLOAT_OES"};
inline constexpr const char* kBindingOrder586[] = {"createVertexArrayOES", "deleteVertexArrayOES", "isVertexArrayOES", "bindVertexArrayOES", "VERTEX_ARRAY_BINDING_OES"};
inline constexpr const char* kBindingOrder587[] = {"framebufferTextureMultiviewOVR", "FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR", "FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR", "MAX_VIEWS_OVR", "FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_OVR"};
inline constexpr const char* kBindingOrder588[] = {"renderedBuffer"};
inline constexpr const char* kBindingOrder589[] = {"startRendering", "length", "oncomplete"};
inline constexpr const char* kBindingOrder590[] = {"getContext", "transferToImageBitmap", "convertToBlob", "toBlob", "width", "height", "oncontextlost", "oncontextrestored"};
inline constexpr const char* kBindingOrder591[] = {"commit", "drawImage", "beginPath", "fill", "stroke", "clip", "isPointInPath", "isPointInStroke", "createLinearGradient", "createRadialGradient", "createConicGradient", "createPattern", "createImageData", "getImageData", "putImageData", "setLineDash", "getLineDash", "closePath", "moveTo", "lineTo", "quadraticCurveTo", "bezierCurveTo", "arcTo", "rect", "roundRect", "arc", "ellipse", "clearRect", "fillRect", "strokeRect", "save", "restore", "reset", "isContextLost", "fillText", "strokeText", "measureText", "scale", "rotate", "translate", "transform", "getTransform", "setTransform", "resetTransform", "canvas", "globalAlpha", "globalCompositeOperation", "strokeStyle", "fillStyle", "filter", "imageSmoothingEnabled", "lineWidth", "lineCap", "lineJoin", "miterLimit", "lineDashOffset", "shadowOffsetX", "shadowOffsetY", "shadowBlur", "shadowColor", "lang", "font", "textAlign", "textBaseline", "direction", "letterSpacing", "fontKerning", "fontStretch", "fontVariantCaps", "textRendering", "wordSpacing"};
inline constexpr const char* kBindingOrder592[] = {"setPeriodicWave", "type", "frequency", "detune"};
inline constexpr const char* kBindingOrder593[] = {"viewTransition"};
inline constexpr const char* kBindingOrder594[] = {"activation", "viewTransition"};
inline constexpr const char* kBindingOrder595[] = {"persisted"};
inline constexpr const char* kBindingOrder596[] = {"clientRect", "reason"};
inline constexpr const char* kBindingOrder597[] = {"item", "@@iterator", "length"};
inline constexpr const char* kBindingOrder598[] = {"registerPaint"};
inline constexpr const char* kBindingOrder599[] = {"setPosition", "setOrientation", "panningModel", "positionX", "positionY", "positionZ", "orientationX", "orientationY", "orientationZ", "distanceModel", "refDistance", "maxDistance", "rolloffFactor", "coneInnerAngle", "coneOuterAngle", "coneOuterGain"};
inline constexpr const char* kBindingOrder600[] = {"loadProcessScript", "removeDelayedProcessScript", "getDelayedProcessScripts", "initialProcessData", "sharedData"};
inline constexpr const char* kBindingOrder601[] = {"addPath", "closePath", "moveTo", "lineTo", "quadraticCurveTo", "bezierCurveTo", "arcTo", "rect", "roundRect", "arc", "ellipse"};
inline constexpr const char* kBindingOrder602[] = {"toJSON", "country", "addressLine", "region", "regionCode", "city", "dependentLocality", "postalCode", "sortingCode", "organization", "recipient", "phone"};
inline constexpr const char* kBindingOrder603[] = {"methodName", "methodDetails"};
inline constexpr const char* kBindingOrder604[] = {"show", "abort", "canMakePayment", "id", "shippingAddress", "shippingOption", "shippingType", "onmerchantvalidation", "onshippingaddresschange", "onshippingoptionchange", "onpaymentmethodchange"};
inline constexpr const char* kBindingOrder605[] = {"updateWith"};
inline constexpr const char* kBindingOrder606[] = {"toJSON", "complete", "retry", "requestId", "methodName", "details", "shippingAddress", "shippingOption", "payerName", "payerEmail", "payerPhone", "onpayerdetailchange"};
inline constexpr const char* kBindingOrder607[] = {"initialize", "createOffer", "createAnswer", "setLocalDescription", "setRemoteDescription", "getStats", "getRemoteStreams", "addTransceiver", "getTransceivers", "closeStreams", "enablePacketDump", "disablePacketDump", "addIceCandidate", "close", "setConfiguration", "restartIce", "restartIceNoRenegotiationNeeded", "pluginCrash", "onSetDescriptionSuccess", "onSetDescriptionError", "createDataChannel", "chain", "updateNegotiationNeeded", "createdSender", "certificate", "fingerprint", "currentLocalDescription", "pendingLocalDescription", "currentRemoteDescription", "pendingRemoteDescription", "currentOfferer", "pendingOfferer", "iceConnectionState", "iceGatheringState", "connectionState", "signalingState", "id", "peerIdentity", "privacyRequested", "duplicateFingerprintQuirk", "sctp"};
inline constexpr const char* kBindingOrder608[] = {"onCreateOfferSuccess", "onCreateOfferError", "onCreateAnswerSuccess", "onCreateAnswerError", "onSetDescriptionSuccess", "onSetDescriptionError", "onAddIceCandidateSuccess", "onAddIceCandidateError", "onIceCandidate", "onIceCandidateError", "notifyDataChannel", "onStateChange", "fireTrackEvent", "fireStreamEvent", "fireNegotiationNeededEvent", "onPacket"};
inline constexpr const char* kBindingOrder609[] = {"now", "toJSON", "getEntries", "getEntriesByType", "getEntriesByName", "clearResourceTimings", "setResourceTimingBufferSize", "mark", "clearMarks", "measure", "clearMeasures", "timeOrigin", "timing", "navigation", "onresourcetimingbufferfull", "mozMemory", "eventCounts", "interactionCount"};
inline constexpr const char* kBindingOrder610[] = {"toJSON", "identifier", "intersectionRect", "size", "firstRenderTime", "lastPaintedElement", "rootElement", "paintTime", "presentationTime"};
inline constexpr const char* kBindingOrder611[] = {"toJSON", "name", "entryType", "startTime", "duration"};
inline constexpr const char* kBindingOrder612[] = {"name", "entryType", "startTime", "duration", "epoch", "origin"};
inline constexpr const char* kBindingOrder613[] = {"toJSON", "processingStart", "processingEnd", "cancelable", "target", "interactionId"};
inline constexpr const char* kBindingOrder614[] = {"detail"};
inline constexpr const char* kBindingOrder615[] = {"detail"};
inline constexpr const char* kBindingOrder616[] = {"toJSON", "type", "redirectCount", "TYPE_NAVIGATE", "TYPE_RELOAD", "TYPE_BACK_FORWARD", "TYPE_RESERVED"};
inline constexpr const char* kBindingOrder617[] = {"toJSON", "unloadEventStart", "unloadEventEnd", "domInteractive", "domContentLoadedEventStart", "domContentLoadedEventEnd", "domComplete", "loadEventStart", "loadEventEnd", "type", "redirectCount"};
inline constexpr const char* kBindingOrder618[] = {"observe", "disconnect", "takeRecords"};
inline constexpr const char* kBindingOrder619[] = {"getEntries", "getEntriesByType", "getEntriesByName"};
inline constexpr const char* kBindingOrder620[] = {"toJSON", "paintTime", "presentationTime"};
inline constexpr const char* kBindingOrder621[] = {"toJSON", "initiatorType", "deliveryType", "nextHopProtocol", "workerStart", "redirectStart", "redirectEnd", "fetchStart", "domainLookupStart", "domainLookupEnd", "connectStart", "connectEnd", "secureConnectionStart", "requestStart", "finalResponseHeadersStart", "firstInterimResponseStart", "responseStart", "responseEnd", "transferSize", "encodedBodySize", "decodedBodySize", "responseStatus", "contentType", "serverTiming", "renderBlockingStatus"};
inline constexpr const char* kBindingOrder622[] = {"toJSON", "name", "duration", "description"};
inline constexpr const char* kBindingOrder623[] = {"toJSON", "navigationStart", "unloadEventStart", "unloadEventEnd", "redirectStart", "redirectEnd", "fetchStart", "domainLookupStart", "domainLookupEnd", "connectStart", "connectEnd", "secureConnectionStart", "requestStart", "responseStart", "responseEnd", "domLoading", "domInteractive", "domContentLoadedEventStart", "domContentLoadedEventEnd", "domComplete", "loadEventStart", "loadEventEnd", "timeToNonBlankPaint", "timeToContentfulPaint", "timeToFirstInteractive"};
inline constexpr const char* kBindingOrder624[] = {"name", "state", "onchange"};
inline constexpr const char* kBindingOrder625[] = {"query"};
inline constexpr const char* kBindingOrder626[] = {"allowsFeature", "features", "allowedFeatures", "getAllowlistForFeature"};
inline constexpr const char* kBindingOrder627[] = {"featureId", "sourceFile", "lineNumber", "columnNumber", "disposition"};
inline constexpr const char* kBindingOrder628[] = {"pictureInPictureWindow"};
inline constexpr const char* kBindingOrder629[] = {"width", "height", "onresize"};
inline constexpr const char* kBindingOrder630[] = {"id", "parentId", "itemType", "url", "guid", "parentGuid", "source", "isTagging"};
inline constexpr const char* kBindingOrder631[] = {"index", "title", "tags", "dateAdded", "frecency", "hidden", "visitCount", "lastVisitDate", "targetFolderItemId", "targetFolderGuid", "targetFolderTitle"};
inline constexpr const char* kBindingOrder632[] = {"lastModified"};
inline constexpr const char* kBindingOrder633[] = {"keyword"};
inline constexpr const char* kBindingOrder634[] = {"index", "oldParentGuid", "oldIndex", "title", "tags", "frecency", "hidden", "visitCount", "dateAdded", "lastVisitDate"};
inline constexpr const char* kBindingOrder635[] = {"index", "title", "isDescendantRemoval"};
inline constexpr const char* kBindingOrder636[] = {"tags"};
inline constexpr const char* kBindingOrder637[] = {"dateAdded"};
inline constexpr const char* kBindingOrder638[] = {"title"};
inline constexpr const char* kBindingOrder639[] = {"type"};
inline constexpr const char* kBindingOrder640[] = {"entries", "keys", "values", "forEach", "has", "get", "size"};
inline constexpr const char* kBindingOrder641[] = {"url", "pageGuid", "faviconUrl"};
inline constexpr const char* kBindingOrder642[] = {"url", "visitId", "visitTime", "referringVisitId", "transitionType", "pageGuid", "frecency", "hidden", "visitCount", "typedCount", "lastKnownTitle", "userContextId"};
inline constexpr const char* kBindingOrder643[] = {"url", "pageGuid", "reason", "transitionType", "isRemovedFromStore", "isPartialVisistsRemoval", "REASON_DELETED", "REASON_EXPIRED"};
inline constexpr const char* kBindingOrder644[] = {"url", "pageGuid", "title"};
inline constexpr const char* kBindingOrder645[] = {"item", "namedItem", "@@iterator", "description", "filename", "name", "length"};
inline constexpr const char* kBindingOrder646[] = {"refresh", "item", "namedItem", "@@iterator", "length"};
inline constexpr const char* kBindingOrder647[] = {"pluginID", "pluginDumpID", "pluginName", "pluginFilename", "submittedCrashReport", "gmpPlugin"};
inline constexpr const char* kBindingOrder648[] = {"getCoalescedEvents", "getPredictedEvents", "pointerId", "width", "height", "pressure", "tangentialPressure", "tiltX", "tiltY", "twist", "altitudeAngle", "azimuthAngle", "pointerType", "isPrimary", "persistentDeviceId"};
inline constexpr const char* kBindingOrder649[] = {"state", "hasUAVisualTransition"};
inline constexpr const char* kBindingOrder650[] = {"requestingWindow", "popupWindowURI", "popupWindowName", "popupWindowFeatures"};
inline constexpr const char* kBindingOrder651[] = {"isAnchored", "alignmentPosition", "popupAlignment", "alignmentOffset"};
inline constexpr const char* kBindingOrder652[] = {"duration", "playbackRate", "position"};
inline constexpr const char* kBindingOrder653[] = {"executeInGlobal", "url", "hasReturnValue"};
inline constexpr const char* kBindingOrder654[] = {"loadProcessScript", "removeDelayedProcessScript", "getDelayedProcessScripts", "osPid", "isInProcess", "processParent"};
inline constexpr const char* kBindingOrder655[] = {"target", "sheet"};
inline constexpr const char* kBindingOrder656[] = {"add"};
inline constexpr const char* kBindingOrder657[] = {"lengthComputable", "loaded", "total"};
inline constexpr const char* kBindingOrder658[] = {"promise", "reason"};
inline constexpr const char* kBindingOrder659[] = {"getClientExtensionResults", "toJSON", "rawId", "response", "authenticatorAttachment"};
inline constexpr const char* kBindingOrder660[] = {"data"};
inline constexpr const char* kBindingOrder661[] = {"subscribe", "getSubscription", "permissionState"};
inline constexpr const char* kBindingOrder662[] = {"subscribe", "getSubscription", "permissionState"};
inline constexpr const char* kBindingOrder663[] = {"arrayBuffer", "blob", "bytes", "json", "text"};
inline constexpr const char* kBindingOrder664[] = {"getKey", "unsubscribe", "toJSON", "endpoint", "options", "expirationTime"};
inline constexpr const char* kBindingOrder665[] = {"newSubscription", "oldSubscription"};
inline constexpr const char* kBindingOrder666[] = {"applicationServerKey"};
inline constexpr const char* kBindingOrder667[] = {"quota", "requested"};
inline constexpr const char* kBindingOrder668[] = {"getFingerprints", "expires"};
inline constexpr const char* kBindingOrder669[] = {"insertDTMF", "ontonechange", "canInsertDTMF", "toneBuffer"};
inline constexpr const char* kBindingOrder670[] = {"tone"};
inline constexpr const char* kBindingOrder671[] = {"close", "send", "label", "negotiated", "ordered", "maxPacketLifeTime", "maxRetransmits", "protocol", "id", "readyState", "bufferedAmount", "bufferedAmountLowThreshold", "onopen", "onerror", "onclosing", "onclose", "onmessage", "onbufferedamountlow", "binaryType"};
inline constexpr const char* kBindingOrder672[] = {"channel"};
inline constexpr const char* kBindingOrder673[] = {"getRemoteCertificates", "iceTransport", "state", "onstatechange", "onerror"};
inline constexpr const char* kBindingOrder674[] = {"getMetadata", "timestamp", "data"};
inline constexpr const char* kBindingOrder675[] = {"getMetadata", "type", "timestamp", "data"};
inline constexpr const char* kBindingOrder676[] = {"errorDetail", "sdpLineNumber", "sctpCauseCode", "receivedAlert", "sentAlert"};
inline constexpr const char* kBindingOrder677[] = {"error"};
inline constexpr const char* kBindingOrder678[] = {"toJSON", "candidate", "sdpMid", "sdpMLineIndex", "foundation", "component", "priority", "address", "protocol", "port", "type", "tcpType", "relatedAddress", "relatedPort", "usernameFragment"};
inline constexpr const char* kBindingOrder679[] = {"local", "remote"};
inline constexpr const char* kBindingOrder680[] = {"getSelectedCandidatePair", "role", "state", "gatheringState", "onstatechange", "ongatheringstatechange", "onselectedcandidatepairchange"};
inline constexpr const char* kBindingOrder681[] = {"register"};
inline constexpr const char* kBindingOrder682[] = {"setIdentityProvider", "getIdentityAssertion", "createOffer", "createAnswer", "setLocalDescription", "setRemoteDescription", "addIceCandidate", "restartIce", "getConfiguration", "setConfiguration", "getLocalStreams", "getRemoteStreams", "addStream", "addTrack", "removeTrack", "addTransceiver", "getSenders", "getReceivers", "getTransceivers", "close", "getStats", "createDataChannel", "localDescription", "currentLocalDescription", "pendingLocalDescription", "remoteDescription", "currentRemoteDescription", "pendingRemoteDescription", "signalingState", "canTrickleIceCandidates", "iceGatheringState", "iceConnectionState", "connectionState", "peerIdentity", "idpLoginUrl", "onnegotiationneeded", "onicecandidate", "onicecandidateerror", "onsignalingstatechange", "onaddstream", "onaddtrack", "ontrack", "oniceconnectionstatechange", "onicegatheringstatechange", "onconnectionstatechange", "sctp", "ondatachannel"};
inline constexpr const char* kBindingOrder683[] = {"address", "port", "url", "errorCode", "errorText"};
inline constexpr const char* kBindingOrder684[] = {"candidate"};
inline constexpr const char* kBindingOrder685[] = {"getParameters", "getContributingSources", "getSynchronizationSources", "getStats", "track", "transport", "jitterBufferTarget", "transform"};
inline constexpr const char* kBindingOrder686[] = {"generateKeyFrame", "sendKeyFrameRequest", "readable", "writable", "options"};
inline constexpr const char* kBindingOrder687[] = {"setParameters", "getParameters", "replaceTrack", "setStreams", "getStats", "track", "transport", "dtmf", "transform"};
inline constexpr const char* kBindingOrder688[] = {"stop", "setCodecPreferences", "mid", "sender", "receiver", "stopped", "direction", "currentDirection"};
inline constexpr const char* kBindingOrder689[] = {"transport", "state", "maxMessageSize", "maxChannels", "onstatechange"};
inline constexpr const char* kBindingOrder690[] = {"toJSON", "type", "sdp"};
inline constexpr const char* kBindingOrder691[] = {"entries", "keys", "values", "forEach", "has", "get", "size"};
inline constexpr const char* kBindingOrder692[] = {"receiver", "track", "streams", "transceiver"};
inline constexpr const char* kBindingOrder693[] = {"transformer"};
inline constexpr const char* kBindingOrder694[] = {"@@iterator", "value"};
inline constexpr const char* kBindingOrder695[] = {"setStart", "setEnd", "setStartBefore", "setStartAfter", "setEndBefore", "setEndAfter", "collapse", "selectNode", "selectNodeContents", "compareBoundaryPoints", "deleteContents", "extractContents", "cloneContents", "insertNode", "surroundContents", "cloneRange", "detach", "isPointInRange", "comparePoint", "intersectsNode", "createContextualFragment", "getClientRects", "getBoundingClientRect", "toString", "commonAncestorContainer", "START_TO_START", "START_TO_END", "END_TO_END", "END_TO_START"};
inline constexpr const char* kBindingOrder696[] = {"close", "enqueue", "error", "byobRequest", "desiredSize"};
inline constexpr const char* kBindingOrder697[] = {"cancel", "getReader", "pipeThrough", "pipeTo", "tee", "values", "locked"};
inline constexpr const char* kBindingOrder698[] = {"next", "return"};
inline constexpr const char* kBindingOrder699[] = {"read", "releaseLock", "cancel", "closed"};
inline constexpr const char* kBindingOrder700[] = {"respond", "respondWithNewView", "view"};
inline constexpr const char* kBindingOrder701[] = {"close", "enqueue", "error", "desiredSize"};
inline constexpr const char* kBindingOrder702[] = {"read", "releaseLock", "cancel", "closed"};
inline constexpr const char* kBindingOrder703[] = {"requestingWindow", "redirectURI"};
inline constexpr const char* kBindingOrder704[] = {"toJSON", "type", "url", "body"};
inline constexpr const char* kBindingOrder705[] = {"toJSON"};
inline constexpr const char* kBindingOrder706[] = {"observe", "disconnect", "takeRecords"};
inline constexpr const char* kBindingOrder707[] = {"clone", "arrayBuffer", "blob", "bytes", "formData", "json", "text", "method", "url", "headers", "destination", "referrer", "referrerPolicy", "mode", "credentials", "cache", "redirect", "integrity", "keepalive", "signal", "duplex", "body", "bodyUsed"};
inline constexpr const char* kBindingOrder708[] = {"observe", "unobserve", "disconnect"};
inline constexpr const char* kBindingOrder709[] = {"target", "contentRect", "borderBoxSize", "contentBoxSize", "devicePixelContentBoxSize"};
inline constexpr const char* kBindingOrder710[] = {"inlineSize", "blockSize"};
inline constexpr const char* kBindingOrder711[] = {"clone", "arrayBuffer", "blob", "bytes", "formData", "json", "text", "type", "url", "redirected", "status", "ok", "statusText", "headers", "body", "bodyUsed"};
inline constexpr const char* kBindingOrder712[] = {"target", "download", "ping", "rel", "referrerPolicy", "relList", "origin", "protocol", "username", "password", "host", "hostname", "port", "pathname", "search", "hash", "hreflang", "type", "href"};
inline constexpr const char* kBindingOrder713[] = {"newValueSpecifiedUnits", "convertToSpecifiedUnits", "unitType", "value", "valueInSpecifiedUnits", "valueAsString", "SVG_ANGLETYPE_UNKNOWN", "SVG_ANGLETYPE_UNSPECIFIED", "SVG_ANGLETYPE_DEG", "SVG_ANGLETYPE_RAD", "SVG_ANGLETYPE_GRAD"};
inline constexpr const char* kBindingOrder714[] = {"baseVal", "animVal"};
inline constexpr const char* kBindingOrder715[] = {"baseVal", "animVal"};
inline constexpr const char* kBindingOrder716[] = {"baseVal", "animVal"};
inline constexpr const char* kBindingOrder717[] = {"baseVal", "animVal"};
inline constexpr const char* kBindingOrder718[] = {"baseVal", "animVal"};
inline constexpr const char* kBindingOrder719[] = {"baseVal", "animVal"};
inline constexpr const char* kBindingOrder720[] = {"baseVal", "animVal"};
inline constexpr const char* kBindingOrder721[] = {"baseVal", "animVal"};
inline constexpr const char* kBindingOrder722[] = {"baseVal", "animVal"};
inline constexpr const char* kBindingOrder723[] = {"baseVal", "animVal"};
inline constexpr const char* kBindingOrder724[] = {"baseVal", "animVal"};
inline constexpr const char* kBindingOrder725[] = {"baseVal", "animVal"};
inline constexpr const char* kBindingOrder726[] = {"getStartTime", "getCurrentTime", "getSimpleDuration", "beginElement", "beginElementAt", "endElement", "endElementAt", "targetElement", "onbegin", "onend", "onrepeat", "requiredExtensions", "systemLanguage"};
inline constexpr const char* kBindingOrder727[] = {"cx", "cy", "r"};
inline constexpr const char* kBindingOrder728[] = {"clipPathUnits", "transform"};
inline constexpr const char* kBindingOrder729[] = {"type", "tableValues", "slope", "intercept", "amplitude", "exponent", "offset", "SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN", "SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY", "SVG_FECOMPONENTTRANSFER_TYPE_TABLE", "SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE", "SVG_FECOMPONENTTRANSFER_TYPE_LINEAR", "SVG_FECOMPONENTTRANSFER_TYPE_GAMMA"};
inline constexpr const char* kBindingOrder730[] = {"focus", "blur", "id", "className", "ownerSVGElement", "viewportElement", "nonce", "style", "attributeStyleMap", "onabort", "onblur", "onfocus", "oncancel", "onauxclick", "onbeforeinput", "onbeforematch", "onbeforetoggle", "oncanplay", "oncanplaythrough", "onchange", "onclick", "onclose", "oncontentvisibilityautostatechange", "oncontextlost", "oncontextmenu", "oncommand", "oncontextrestored", "oncopy", "oncuechange", "oncut", "ondblclick", "ondrag", "ondragend", "ondragenter", "ondragexit", "ondragleave", "ondragover", "ondragstart", "ondrop", "ondurationchange", "onemptied", "onended", "onformdata", "oninput", "oninvalid", "onkeydown", "onkeypress", "onkeyup", "onload", "onloadeddata", "onloadedmetadata", "onloadstart", "onmousedown", "onmouseenter", "onmouseleave", "onmousemove", "onmouseout", "onmouseover", "onmouseup", "onwheel", "onpaste", "onpause", "onplay", "onplaying", "onprogress", "onratechange", "onreset", "onresize", "onscroll", "onscrollend", "onsecuritypolicyviolation", "onseeked", "onseeking", "onselect", "onslotchange", "onstalled", "onsubmit", "onsuspend", "ontimeupdate", "onvolumechange", "onwaiting", "onselectstart", "onselectionchange", "ontoggle", "onpointercancel", "onpointerdown", "onpointerup", "onpointermove", "onpointerout", "onpointerover", "onpointerenter", "onpointerleave", "onpointerrawupdate", "ongotpointercapture", "onlostpointercapture", "onmozfullscreenchange", "onmozfullscreenerror", "onanimationcancel", "onanimationend", "onanimationiteration", "onanimationstart", "ontransitioncancel", "ontransitionend", "ontransitionrun", "ontransitionstart", "onwebkitanimationend", "onwebkitanimationiteration", "onwebkitanimationstart", "onwebkittransitionend", "dataset", "autofocus", "tabIndex", "onerror", "ontouchstart", "ontouchend", "ontouchmove", "ontouchcancel"};
inline constexpr const char* kBindingOrder731[] = {"cx", "cy", "rx", "ry"};
inline constexpr const char* kBindingOrder732[] = {"in1", "in2", "mode", "x", "y", "width", "height", "result", "SVG_FEBLEND_MODE_UNKNOWN", "SVG_FEBLEND_MODE_NORMAL", "SVG_FEBLEND_MODE_MULTIPLY", "SVG_FEBLEND_MODE_SCREEN", "SVG_FEBLEND_MODE_DARKEN", "SVG_FEBLEND_MODE_LIGHTEN", "SVG_FEBLEND_MODE_OVERLAY", "SVG_FEBLEND_MODE_COLOR_DODGE", "SVG_FEBLEND_MODE_COLOR_BURN", "SVG_FEBLEND_MODE_HARD_LIGHT", "SVG_FEBLEND_MODE_SOFT_LIGHT", "SVG_FEBLEND_MODE_DIFFERENCE", "SVG_FEBLEND_MODE_EXCLUSION", "SVG_FEBLEND_MODE_HUE", "SVG_FEBLEND_MODE_SATURATION", "SVG_FEBLEND_MODE_COLOR", "SVG_FEBLEND_MODE_LUMINOSITY"};
inline constexpr const char* kBindingOrder733[] = {"in1", "type", "values", "x", "y", "width", "height", "result", "SVG_FECOLORMATRIX_TYPE_UNKNOWN", "SVG_FECOLORMATRIX_TYPE_MATRIX", "SVG_FECOLORMATRIX_TYPE_SATURATE", "SVG_FECOLORMATRIX_TYPE_HUEROTATE", "SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA"};
inline constexpr const char* kBindingOrder734[] = {"in1", "x", "y", "width", "height", "result"};
inline constexpr const char* kBindingOrder735[] = {"in1", "in2", "operator", "k1", "k2", "k3", "k4", "x", "y", "width", "height", "result", "SVG_FECOMPOSITE_OPERATOR_UNKNOWN", "SVG_FECOMPOSITE_OPERATOR_OVER", "SVG_FECOMPOSITE_OPERATOR_IN", "SVG_FECOMPOSITE_OPERATOR_OUT", "SVG_FECOMPOSITE_OPERATOR_ATOP", "SVG_FECOMPOSITE_OPERATOR_XOR", "SVG_FECOMPOSITE_OPERATOR_ARITHMETIC", "SVG_FECOMPOSITE_OPERATOR_LIGHTER"};
inline constexpr const char* kBindingOrder736[] = {"in1", "orderX", "orderY", "kernelMatrix", "divisor", "bias", "targetX", "targetY", "edgeMode", "kernelUnitLengthX", "kernelUnitLengthY", "preserveAlpha", "x", "y", "width", "height", "result", "SVG_EDGEMODE_UNKNOWN", "SVG_EDGEMODE_DUPLICATE", "SVG_EDGEMODE_WRAP", "SVG_EDGEMODE_NONE"};
inline constexpr const char* kBindingOrder737[] = {"in1", "surfaceScale", "diffuseConstant", "kernelUnitLengthX", "kernelUnitLengthY", "x", "y", "width", "height", "result"};
inline constexpr const char* kBindingOrder738[] = {"in1", "in2", "scale", "xChannelSelector", "yChannelSelector", "x", "y", "width", "height", "result", "SVG_CHANNEL_UNKNOWN", "SVG_CHANNEL_R", "SVG_CHANNEL_G", "SVG_CHANNEL_B", "SVG_CHANNEL_A"};
inline constexpr const char* kBindingOrder739[] = {"azimuth", "elevation"};
inline constexpr const char* kBindingOrder740[] = {"setStdDeviation", "in1", "dx", "dy", "stdDeviationX", "stdDeviationY", "x", "y", "width", "height", "result"};
inline constexpr const char* kBindingOrder741[] = {"x", "y", "width", "height", "result"};
inline constexpr const char* kBindingOrder742[] = {"setStdDeviation", "in1", "stdDeviationX", "stdDeviationY", "x", "y", "width", "height", "result"};
inline constexpr const char* kBindingOrder743[] = {"preserveAspectRatio", "crossOrigin", "fetchPriority", "x", "y", "width", "height", "result", "href"};
inline constexpr const char* kBindingOrder744[] = {"x", "y", "width", "height", "result"};
inline constexpr const char* kBindingOrder745[] = {"in1"};
inline constexpr const char* kBindingOrder746[] = {"in1", "operator", "radiusX", "radiusY", "x", "y", "width", "height", "result", "SVG_MORPHOLOGY_OPERATOR_UNKNOWN", "SVG_MORPHOLOGY_OPERATOR_ERODE", "SVG_MORPHOLOGY_OPERATOR_DILATE"};
inline constexpr const char* kBindingOrder747[] = {"in1", "dx", "dy", "x", "y", "width", "height", "result"};
inline constexpr const char* kBindingOrder748[] = {"x", "y", "z"};
inline constexpr const char* kBindingOrder749[] = {"in1", "surfaceScale", "specularConstant", "specularExponent", "kernelUnitLengthX", "kernelUnitLengthY", "x", "y", "width", "height", "result"};
inline constexpr const char* kBindingOrder750[] = {"x", "y", "z", "pointsAtX", "pointsAtY", "pointsAtZ", "specularExponent", "limitingConeAngle"};
inline constexpr const char* kBindingOrder751[] = {"in1", "x", "y", "width", "height", "result"};
inline constexpr const char* kBindingOrder752[] = {"baseFrequencyX", "baseFrequencyY", "numOctaves", "seed", "stitchTiles", "type", "x", "y", "width", "height", "result", "SVG_TURBULENCE_TYPE_UNKNOWN", "SVG_TURBULENCE_TYPE_FRACTALNOISE", "SVG_TURBULENCE_TYPE_TURBULENCE", "SVG_STITCHTYPE_UNKNOWN", "SVG_STITCHTYPE_STITCH", "SVG_STITCHTYPE_NOSTITCH"};
inline constexpr const char* kBindingOrder753[] = {"filterUnits", "primitiveUnits", "x", "y", "width", "height", "href"};
inline constexpr const char* kBindingOrder754[] = {"x", "y", "width", "height"};
inline constexpr const char* kBindingOrder755[] = {"isPointInFill", "isPointInStroke", "getTotalLength", "getPointAtLength", "pathLength"};
inline constexpr const char* kBindingOrder756[] = {"gradientUnits", "gradientTransform", "spreadMethod", "href", "SVG_SPREADMETHOD_UNKNOWN", "SVG_SPREADMETHOD_PAD", "SVG_SPREADMETHOD_REFLECT", "SVG_SPREADMETHOD_REPEAT"};
inline constexpr const char* kBindingOrder757[] = {"getBBox", "getCTM", "getScreenCTM", "transform", "requiredExtensions", "systemLanguage"};
inline constexpr const char* kBindingOrder758[] = {"decode", "x", "y", "width", "height", "preserveAspectRatio", "crossOrigin", "fetchPriority", "decoding", "href"};
inline constexpr const char* kBindingOrder759[] = {"newValueSpecifiedUnits", "convertToSpecifiedUnits", "unitType", "value", "valueInSpecifiedUnits", "valueAsString", "SVG_LENGTHTYPE_UNKNOWN", "SVG_LENGTHTYPE_NUMBER", "SVG_LENGTHTYPE_PERCENTAGE", "SVG_LENGTHTYPE_EMS", "SVG_LENGTHTYPE_EXS", "SVG_LENGTHTYPE_PX", "SVG_LENGTHTYPE_CM", "SVG_LENGTHTYPE_MM", "SVG_LENGTHTYPE_IN", "SVG_LENGTHTYPE_PT", "SVG_LENGTHTYPE_PC"};
inline constexpr const char* kBindingOrder760[] = {"clear", "initialize", "getItem", "insertItemBefore", "replaceItem", "removeItem", "appendItem", "@@iterator", "length", "numberOfItems"};
inline constexpr const char* kBindingOrder761[] = {"x1", "y1", "x2", "y2"};
inline constexpr const char* kBindingOrder762[] = {"x1", "y1", "x2", "y2"};
inline constexpr const char* kBindingOrder763[] = {"href"};
inline constexpr const char* kBindingOrder764[] = {"setOrientToAuto", "setOrientToAngle", "refX", "refY", "markerUnits", "markerWidth", "markerHeight", "orientType", "orientAngle", "viewBox", "preserveAspectRatio", "SVG_MARKERUNITS_UNKNOWN", "SVG_MARKERUNITS_USERSPACEONUSE", "SVG_MARKERUNITS_STROKEWIDTH", "SVG_MARKER_ORIENT_UNKNOWN", "SVG_MARKER_ORIENT_AUTO", "SVG_MARKER_ORIENT_ANGLE", "SVG_MARKER_ORIENT_AUTO_START_REVERSE"};
inline constexpr const char* kBindingOrder765[] = {"maskUnits", "maskContentUnits", "x", "y", "width", "height", "SVG_MASKTYPE_LUMINANCE", "SVG_MASKTYPE_ALPHA"};
inline constexpr const char* kBindingOrder766[] = {"multiply", "inverse", "translate", "scale", "scaleNonUniform", "rotate", "rotateFromVector", "flipX", "flipY", "skewX", "skewY", "a", "b", "c", "d", "e", "f"};
inline constexpr const char* kBindingOrder767[] = {"value"};
inline constexpr const char* kBindingOrder768[] = {"clear", "initialize", "getItem", "insertItemBefore", "replaceItem", "removeItem", "appendItem", "@@iterator", "length", "numberOfItems"};
inline constexpr const char* kBindingOrder769[] = {"getPathSegmentAtLength", "getPathData", "setPathData"};
inline constexpr const char* kBindingOrder770[] = {"type", "values"};
inline constexpr const char* kBindingOrder771[] = {"patternUnits", "patternContentUnits", "patternTransform", "x", "y", "width", "height", "viewBox", "preserveAspectRatio", "href"};
inline constexpr const char* kBindingOrder772[] = {"matrixTransform", "x", "y"};
inline constexpr const char* kBindingOrder773[] = {"clear", "initialize", "getItem", "insertItemBefore", "replaceItem", "removeItem", "appendItem", "@@iterator", "length", "numberOfItems"};
inline constexpr const char* kBindingOrder774[] = {"points", "animatedPoints"};
inline constexpr const char* kBindingOrder775[] = {"points", "animatedPoints"};
inline constexpr const char* kBindingOrder776[] = {"align", "meetOrSlice", "SVG_PRESERVEASPECTRATIO_UNKNOWN", "SVG_PRESERVEASPECTRATIO_NONE", "SVG_PRESERVEASPECTRATIO_XMINYMIN", "SVG_PRESERVEASPECTRATIO_XMIDYMIN", "SVG_PRESERVEASPECTRATIO_XMAXYMIN", "SVG_PRESERVEASPECTRATIO_XMINYMID", "SVG_PRESERVEASPECTRATIO_XMIDYMID", "SVG_PRESERVEASPECTRATIO_XMAXYMID", "SVG_PRESERVEASPECTRATIO_XMINYMAX", "SVG_PRESERVEASPECTRATIO_XMIDYMAX", "SVG_PRESERVEASPECTRATIO_XMAXYMAX", "SVG_MEETORSLICE_UNKNOWN", "SVG_MEETORSLICE_MEET", "SVG_MEETORSLICE_SLICE"};
inline constexpr const char* kBindingOrder777[] = {"cx", "cy", "r", "fx", "fy", "fr"};
inline constexpr const char* kBindingOrder778[] = {"x", "y", "width", "height"};
inline constexpr const char* kBindingOrder779[] = {"x", "y", "width", "height", "rx", "ry"};
inline constexpr const char* kBindingOrder780[] = {"suspendRedraw", "unsuspendRedraw", "unsuspendRedrawAll", "forceRedraw", "pauseAnimations", "unpauseAnimations", "animationsPaused", "getCurrentTime", "setCurrentTime", "deselectAll", "createSVGNumber", "createSVGLength", "createSVGAngle", "createSVGPoint", "createSVGMatrix", "createSVGRect", "createSVGTransform", "createSVGTransformFromMatrix", "getElementById", "x", "y", "width", "height", "currentScale", "currentTranslate", "viewBox", "preserveAspectRatio", "zoomAndPan", "SVG_ZOOMANDPAN_UNKNOWN", "SVG_ZOOMANDPAN_DISABLE", "SVG_ZOOMANDPAN_MAGNIFY"};
inline constexpr const char* kBindingOrder781[] = {"type", "async", "defer", "crossOrigin", "fetchPriority", "href"};
inline constexpr const char* kBindingOrder782[] = {"offset"};
inline constexpr const char* kBindingOrder783[] = {"clear", "initialize", "getItem", "insertItemBefore", "replaceItem", "removeItem", "appendItem", "@@iterator", "length", "numberOfItems"};
inline constexpr const char* kBindingOrder784[] = {"disabled", "type", "media", "title", "sheet"};
inline constexpr const char* kBindingOrder785[] = {"viewBox", "preserveAspectRatio", "requiredExtensions", "systemLanguage"};
inline constexpr const char* kBindingOrder786[] = {"getNumberOfChars", "getComputedTextLength", "getSubStringLength", "getStartPositionOfChar", "getEndPositionOfChar", "getExtentOfChar", "getRotationOfChar", "getCharNumAtPosition", "selectSubString", "textLength", "lengthAdjust", "LENGTHADJUST_UNKNOWN", "LENGTHADJUST_SPACING", "LENGTHADJUST_SPACINGANDGLYPHS"};
inline constexpr const char* kBindingOrder787[] = {"startOffset", "method", "spacing", "side", "href", "TEXTPATH_METHODTYPE_UNKNOWN", "TEXTPATH_METHODTYPE_ALIGN", "TEXTPATH_METHODTYPE_STRETCH", "TEXTPATH_SPACINGTYPE_UNKNOWN", "TEXTPATH_SPACINGTYPE_AUTO", "TEXTPATH_SPACINGTYPE_EXACT", "TEXTPATH_SIDETYPE_UNKNOWN", "TEXTPATH_SIDETYPE_LEFT", "TEXTPATH_SIDETYPE_RIGHT"};
inline constexpr const char* kBindingOrder788[] = {"x", "y", "dx", "dy", "rotate"};
inline constexpr const char* kBindingOrder789[] = {"setMatrix", "setTranslate", "setScale", "setRotate", "setSkewX", "setSkewY", "type", "matrix", "angle", "SVG_TRANSFORM_UNKNOWN", "SVG_TRANSFORM_MATRIX", "SVG_TRANSFORM_TRANSLATE", "SVG_TRANSFORM_SCALE", "SVG_TRANSFORM_ROTATE", "SVG_TRANSFORM_SKEWX", "SVG_TRANSFORM_SKEWY"};
inline constexpr const char* kBindingOrder790[] = {"clear", "initialize", "getItem", "insertItemBefore", "replaceItem", "removeItem", "appendItem", "createSVGTransformFromMatrix", "consolidate", "@@iterator", "length", "numberOfItems"};
inline constexpr const char* kBindingOrder791[] = {"SVG_UNIT_TYPE_UNKNOWN", "SVG_UNIT_TYPE_USERSPACEONUSE", "SVG_UNIT_TYPE_OBJECTBOUNDINGBOX"};
inline constexpr const char* kBindingOrder792[] = {"x", "y", "width", "height", "href"};
inline constexpr const char* kBindingOrder793[] = {"viewBox", "preserveAspectRatio", "zoomAndPan", "SVG_ZOOMANDPAN_UNKNOWN", "SVG_ZOOMANDPAN_DISABLE", "SVG_ZOOMANDPAN_MAGNIFY"};
inline constexpr const char* kBindingOrder794[] = {"get", "allowElement", "removeElement", "replaceElementWithChildren", "allowProcessingInstruction", "removeProcessingInstruction", "allowAttribute", "removeAttribute", "setComments", "setDataAttributes", "setJavascriptURLs", "removeUnsafe"};
inline constexpr const char* kBindingOrder795[] = {"postTask", "yield"};
inline constexpr const char* kBindingOrder796[] = {"mozLockOrientation", "mozUnlockOrientation", "availWidth", "availHeight", "width", "height", "colorDepth", "pixelDepth", "top", "left", "availTop", "availLeft", "mozOrientation", "onmozorientationchange", "orientation", "colorGamut", "luminance", "onchange"};
inline constexpr const char* kBindingOrder797[] = {"min", "max", "maxAverage"};
inline constexpr const char* kBindingOrder798[] = {"lock", "unlock", "type", "angle", "onchange"};
inline constexpr const char* kBindingOrder799[] = {"onaudioprocess", "bufferSize"};
inline constexpr const char* kBindingOrder800[] = {"initScrollAreaEvent", "x", "y", "width", "height"};
inline constexpr const char* kBindingOrder801[] = {"source", "axis"};
inline constexpr const char* kBindingOrder802[] = {"documentURI", "referrer", "blockedURI", "violatedDirective", "effectiveDirective", "originalPolicy", "sourceFile", "sample", "disposition", "statusCode", "lineNumber", "columnNumber"};
inline constexpr const char* kBindingOrder803[] = {"getRangeAt", "addRange", "removeRange", "removeAllRanges", "empty", "getComposedRanges", "collapse", "setPosition", "collapseToStart", "collapseToEnd", "extend", "setBaseAndExtent", "selectAllChildren", "modify", "deleteFromDocument", "containsNode", "toString", "anchorNode", "anchorOffset", "focusNode", "focusOffset", "isCollapsed", "rangeCount", "type", "direction", "caretBidiLevel"};
inline constexpr const char* kBindingOrder804[] = {"getPorts", "requestPort", "simulateDeviceConnection", "simulateDeviceDisconnection", "removeAllMockDevices", "resetToDefaultMockDevices", "onconnect", "ondisconnect", "autoselectPorts"};
inline constexpr const char* kBindingOrder805[] = {"getInfo", "open", "setSignals", "getSignals", "close", "forget", "onconnect", "ondisconnect", "connected", "readable", "writable"};
inline constexpr const char* kBindingOrder806[] = {"postMessage", "scriptURL", "state", "onstatechange", "onerror"};
inline constexpr const char* kBindingOrder807[] = {"register", "getRegistration", "getRegistrations", "startMessages", "controller", "ready", "oncontrollerchange", "onmessage", "onmessageerror"};
inline constexpr const char* kBindingOrder808[] = {"skipWaiting", "clients", "registration", "oninstall", "onactivate", "onfetch", "onmessage", "onmessageerror", "onpush", "onpushsubscriptionchange", "onnotificationclick", "onnotificationclose", "cookieStore", "oncookiechange", "browser", "chrome"};
inline constexpr const char* kBindingOrder809[] = {"update", "unregister", "showNotification", "getNotifications", "installing", "waiting", "active", "navigationPreload", "scope", "updateViaCache", "onupdatefound", "pushManager", "cookies"};
inline constexpr const char* kBindingOrder810[] = {"toJSON", "url", "id", "xpath", "innerHTML", "children"};
inline constexpr const char* kBindingOrder811[] = {"toJSON", "scroll", "children"};
inline constexpr const char* kBindingOrder812[] = {"getElementById", "importNodeAndAppendChildAt", "createElementAndAppendChildAt", "setHTML", "setHTMLUnsafe", "getHTML", "elementFromPoint", "elementsFromPoint", "getAnimations", "mode", "delegatesFocus", "slotAssignment", "clonable", "serializable", "host", "onslotchange", "referenceTarget", "innerHTML", "activeElement", "styleSheets", "pointerLockElement", "fullscreenElement", "mozFullScreenElement", "customElementRegistry", "pictureInPictureElement", "adoptedStyleSheets"};
inline constexpr const char* kBindingOrder813[] = {"port", "onerror"};
inline constexpr const char* kBindingOrder814[] = {"close", "name", "onconnect"};
inline constexpr const char* kBindingOrder815[] = {"initSimpleGestureEvent", "allowedDirections", "direction", "delta", "clickCount", "DIRECTION_UP", "DIRECTION_DOWN", "DIRECTION_LEFT", "DIRECTION_RIGHT", "ROTATION_COUNTERCLOCKWISE", "ROTATION_CLOCKWISE"};
inline constexpr const char* kBindingOrder816[] = {"appendBuffer", "abort", "remove", "appendBufferAsync", "removeAsync", "changeType", "mode", "updating", "buffered", "timestampOffset", "appendWindowStart", "appendWindowEnd", "onupdatestart", "onupdate", "onupdateend", "onerror", "onabort"};
inline constexpr const char* kBindingOrder817[] = {"@@iterator", "length", "onaddsourcebuffer", "onremovesourcebuffer"};
inline constexpr const char* kBindingOrder818[] = {"src", "weight"};
inline constexpr const char* kBindingOrder819[] = {"item", "addFromUri", "addFromString", "@@iterator", "length"};
inline constexpr const char* kBindingOrder820[] = {"start", "stop", "abort", "grammars", "lang", "continuous", "interimResults", "unspokenPunctuation", "maxAlternatives", "processLocally", "phrases", "onaudiostart", "onsoundstart", "onspeechstart", "onspeechend", "onsoundend", "onaudioend", "onresult", "onnomatch", "onerror", "onstart", "onend"};
inline constexpr const char* kBindingOrder821[] = {"transcript", "confidence"};
inline constexpr const char* kBindingOrder822[] = {"error", "message"};
inline constexpr const char* kBindingOrder823[] = {"resultIndex", "results", "interpretation", "emma"};
inline constexpr const char* kBindingOrder824[] = {"phrase", "boost"};
inline constexpr const char* kBindingOrder825[] = {"item", "@@iterator", "length", "isFinal"};
inline constexpr const char* kBindingOrder826[] = {"item", "@@iterator", "length"};
inline constexpr const char* kBindingOrder827[] = {"speak", "cancel", "pause", "resume", "getVoices", "pending", "speaking", "paused", "onvoiceschanged"};
inline constexpr const char* kBindingOrder828[] = {"error"};
inline constexpr const char* kBindingOrder829[] = {"utterance", "charIndex", "charLength", "elapsedTime", "name"};
inline constexpr const char* kBindingOrder830[] = {"text", "lang", "voice", "volume", "rate", "pitch", "onstart", "onend", "onerror", "onpause", "onresume", "onmark", "onboundary"};
inline constexpr const char* kBindingOrder831[] = {"voiceURI", "name", "lang", "localService", "default"};
inline constexpr const char* kBindingOrder832[] = {"pan"};
inline constexpr const char* kBindingOrder833[] = {"key", "getItem", "setItem", "removeItem", "clear", "open", "close", "beginExplicitSnapshot", "checkpointExplicitSnapshot", "endExplicitSnapshot", "length", "hasSnapshot", "snapshotUsage"};
inline constexpr const char* kBindingOrder834[] = {"initStorageEvent", "key", "oldValue", "newValue", "url", "storageArea"};
inline constexpr const char* kBindingOrder835[] = {"persisted", "persist", "estimate", "getDirectory"};
inline constexpr const char* kBindingOrder836[] = {"suspend", "resume", "close", "disconnect", "write", "status", "error", "ondata", "onstart", "onstop", "onerror"};
inline constexpr const char* kBindingOrder837[] = {"data"};
inline constexpr const char* kBindingOrder838[] = {"deserialize", "dataSize"};
inline constexpr const char* kBindingOrder839[] = {"serializable", "deserializable"};
inline constexpr const char* kBindingOrder840[] = {"set", "append", "delete", "clear"};
inline constexpr const char* kBindingOrder841[] = {"get", "getAll", "has", "entries", "keys", "values", "forEach", "size"};
inline constexpr const char* kBindingOrder842[] = {"next"};
inline constexpr const char* kBindingOrder843[] = {"type", "href", "ownerNode", "parentStyleSheet", "title", "media", "disabled"};
inline constexpr const char* kBindingOrder844[] = {"stylesheet", "applicable"};
inline constexpr const char* kBindingOrder845[] = {"item", "@@iterator", "length"};
inline constexpr const char* kBindingOrder846[] = {"stylesheet"};
inline constexpr const char* kBindingOrder847[] = {"submitter"};
inline constexpr const char* kBindingOrder848[] = {"encrypt", "decrypt", "sign", "verify", "digest", "generateKey", "deriveKey", "deriveBits", "importKey", "exportKey", "wrapKey", "unwrapKey", "encapsulateKey", "encapsulateBits", "decapsulateKey", "decapsulateBits"};
inline constexpr const char* kBindingOrder849[] = {"sendSyncMessage"};
inline constexpr const char* kBindingOrder850[] = {"readBytesInto", "close", "size"};
inline constexpr const char* kBindingOrder851[] = {"close", "localPort", "onconnect", "onerror"};
inline constexpr const char* kBindingOrder852[] = {"socket"};
inline constexpr const char* kBindingOrder853[] = {"upgradeToSecure", "suspend", "resume", "close", "send", "transport", "host", "port", "ssl", "bufferedAmount", "readyState", "binaryType", "onopen", "ondrain", "ondata", "onerror", "onclose"};
inline constexpr const char* kBindingOrder854[] = {"name", "message", "errorCode"};
inline constexpr const char* kBindingOrder855[] = {"data"};
inline constexpr const char* kBindingOrder856[] = {"setPriority"};
inline constexpr const char* kBindingOrder857[] = {"previousPriority"};
inline constexpr const char* kBindingOrder858[] = {"priority", "onprioritychange"};
inline constexpr const char* kBindingOrder859[] = {"getData", "dispose", "dims", "type", "data", "location"};
inline constexpr const char* kBindingOrder860[] = {"toJSON", "message"};
inline constexpr const char* kBindingOrder861[] = {"deprecatedMethod", "deprecatedAttribute"};
inline constexpr const char* kBindingOrder862[] = {"splitText", "getBoxQuads", "convertQuadFromNode", "convertRectFromNode", "convertPointFromNode", "wholeText", "assignedSlot"};
inline constexpr const char* kBindingOrder863[] = {"startOffset", "endOffset", "isCaret", "isTargetClause"};
inline constexpr const char* kBindingOrder864[] = {"decode", "encoding", "fatal", "ignoreBOM"};
inline constexpr const char* kBindingOrder865[] = {"readable", "writable", "encoding", "fatal", "ignoreBOM"};
inline constexpr const char* kBindingOrder866[] = {"encode", "encodeInto", "encoding"};
inline constexpr const char* kBindingOrder867[] = {"readable", "writable", "encoding"};
inline constexpr const char* kBindingOrder868[] = {"initTextEvent", "data"};
inline constexpr const char* kBindingOrder869[] = {"rangeStart", "rangeEnd", "underlineStyle", "underlineThickness"};
inline constexpr const char* kBindingOrder870[] = {"getTextFormats"};
inline constexpr const char* kBindingOrder871[] = {"generate", "clear", "cancel", "terminate"};
inline constexpr const char* kBindingOrder872[] = {"width", "actualBoundingBoxLeft", "actualBoundingBoxRight", "fontBoundingBoxAscent", "fontBoundingBoxDescent", "actualBoundingBoxAscent", "actualBoundingBoxDescent", "emHeightAscent", "emHeightDescent", "hangingBaseline", "alphabeticBaseline", "ideographicBaseline"};
inline constexpr const char* kBindingOrder873[] = {"addCue", "removeCue", "kind", "label", "language", "id", "inBandMetadataTrackDispatchType", "mode", "cues", "activeCues", "oncuechange"};
inline constexpr const char* kBindingOrder874[] = {"track", "id", "startTime", "endTime", "pauseOnExit", "onenter", "onexit"};
inline constexpr const char* kBindingOrder875[] = {"getCueById", "@@iterator", "length"};
inline constexpr const char* kBindingOrder876[] = {"getTrackById", "@@iterator", "length", "onchange", "onaddtrack", "onremovetrack"};
inline constexpr const char* kBindingOrder877[] = {"updateRangeStart", "updateRangeEnd", "text", "selectionStart", "selectionEnd"};
inline constexpr const char* kBindingOrder878[] = {"initTimeEvent", "detail", "view"};
inline constexpr const char* kBindingOrder879[] = {"start", "end", "length"};
inline constexpr const char* kBindingOrder880[] = {"oldState", "newState", "source"};
inline constexpr const char* kBindingOrder881[] = {"identifier", "target", "screenX", "screenY", "clientX", "clientY", "pageX", "pageY", "radiusX", "radiusY", "rotationAngle", "force"};
inline constexpr const char* kBindingOrder882[] = {"initTouchEvent", "touches", "targetTouches", "changedTouches", "altKey", "metaKey", "ctrlKey", "shiftKey"};
inline constexpr const char* kBindingOrder883[] = {"item", "@@iterator", "length"};
inline constexpr const char* kBindingOrder884[] = {"track"};
inline constexpr const char* kBindingOrder885[] = {"readable", "writable"};
inline constexpr const char* kBindingOrder886[] = {"enqueue", "error", "terminate", "desiredSize"};
inline constexpr const char* kBindingOrder887[] = {"propertyName", "elapsedTime", "pseudoElement", "animation"};
inline constexpr const char* kBindingOrder888[] = {"getNext", "getPrevious", "invalidate", "element", "columns", "x", "width", "id", "index", "primary", "cycler", "editable", "type", "previousColumn", "TYPE_TEXT", "TYPE_CHECKBOX"};
inline constexpr const char* kBindingOrder889[] = {"getFirstColumn", "getLastColumn", "getPrimaryColumn", "getSortedColumn", "getKeyColumn", "getColumnFor", "getNamedColumn", "getColumnAt", "invalidateColumns", "@@iterator", "tree", "count", "length"};
inline constexpr const char* kBindingOrder890[] = {"getItemAtIndex", "getIndexOfItem", "getRowProperties", "getCellProperties", "getColumnProperties", "isContainer", "isContainerOpen", "isContainerEmpty", "isSeparator", "isSorted", "canDrop", "drop", "getParentIndex", "hasNextSibling", "getLevel", "getImageSrc", "getCellValue", "getCellText", "setTree", "toggleOpenState", "cycleHeader", "selectionChanged", "cycleCell", "isEditable", "setCellValue", "setCellText", "rowCount", "selection", "DROP_BEFORE", "DROP_ON", "DROP_AFTER"};
inline constexpr const char* kBindingOrder891[] = {"parentNode", "firstChild", "lastChild", "previousSibling", "nextSibling", "previousNode", "nextNode", "root", "whatToShow", "filter", "currentNode"};
inline constexpr const char* kBindingOrder892[] = {"toJSON", "toString"};
inline constexpr const char* kBindingOrder893[] = {"toJSON", "toString"};
inline constexpr const char* kBindingOrder894[] = {"toJSON", "toString"};
inline constexpr const char* kBindingOrder895[] = {"createHTML", "createScript", "createScriptURL", "name"};
inline constexpr const char* kBindingOrder896[] = {"createPolicy", "isHTML", "isScript", "isScriptURL", "getAttributeType", "getPropertyType", "emptyHTML", "emptyScript", "defaultPolicy"};
inline constexpr const char* kBindingOrder897[] = {"remoteAddress", "remotePort", "data"};
inline constexpr const char* kBindingOrder898[] = {"close", "joinMulticastGroup", "leaveMulticastGroup", "send", "localAddress", "localPort", "remoteAddress", "remotePort", "addressReuse", "loopback", "readyState", "opened", "closed", "onmessage"};
inline constexpr const char* kBindingOrder899[] = {"initUIEvent", "view", "detail", "layerX", "layerY", "which", "rangeParent", "rangeOffset", "SCROLL_PAGE_UP", "SCROLL_PAGE_DOWN"};
inline constexpr const char* kBindingOrder900[] = {"toJSON", "toString", "href", "origin", "protocol", "username", "password", "host", "hostname", "port", "pathname", "search", "searchParams", "hash"};
inline constexpr const char* kBindingOrder901[] = {"test", "exec", "protocol", "username", "password", "hostname", "port", "pathname", "search", "hash", "hasRegExpGroups"};
inline constexpr const char* kBindingOrder902[] = {"append", "delete", "get", "getAll", "has", "set", "sort", "entries", "keys", "values", "forEach", "toString", "size"};
inline constexpr const char* kBindingOrder903[] = {"next"};
inline constexpr const char* kBindingOrder904[] = {"hasBeenActive", "isActive"};
inline constexpr const char* kBindingOrder905[] = {"near"};
inline constexpr const char* kBindingOrder906[] = {"getEyeParameters", "getFrameData", "getPose", "resetPose", "requestAnimationFrame", "cancelAnimationFrame", "requestPresent", "exitPresent", "getLayers", "submitFrame", "isConnected", "isPresenting", "capabilities", "stageParameters", "displayId", "displayName", "depthNear", "depthFar"};
inline constexpr const char* kBindingOrder907[] = {"hasPosition", "hasOrientation", "hasExternalDisplay", "canPresent", "maxLayers"};
inline constexpr const char* kBindingOrder908[] = {"display", "reason"};
inline constexpr const char* kBindingOrder909[] = {"offset", "fieldOfView", "renderWidth", "renderHeight"};
inline constexpr const char* kBindingOrder910[] = {"upDegrees", "rightDegrees", "downDegrees", "leftDegrees"};
inline constexpr const char* kBindingOrder911[] = {"timestamp", "leftProjectionMatrix", "leftViewMatrix", "rightProjectionMatrix", "rightViewMatrix", "pose"};
inline constexpr const char* kBindingOrder912[] = {"create", "clear", "setPose", "setButtonPressed", "setButtonTouched", "setButtonTrigger", "setAxisValue", "hand", "capPosition", "capOrientation", "capAngularAcceleration", "capLinearAcceleration", "axisCount", "buttonCount", "hapticCount"};
inline constexpr const char* kBindingOrder913[] = {"create", "setEyeFOV", "setEyeOffset", "setEyeResolution", "setConnected", "setMounted", "setStageSize", "setSittingToStandingTransform", "setPose", "capPosition", "capOrientation", "capPresent", "capExternal", "capAngularAcceleration", "capLinearAcceleration", "capStageParameters", "capMountDetection", "capPositionEmulated"};
inline constexpr const char* kBindingOrder914[] = {"position", "linearVelocity", "linearAcceleration", "orientation", "angularVelocity", "angularAcceleration"};
inline constexpr const char* kBindingOrder915[] = {"getVRDisplay", "getVRController", "run", "reset", "commit", "end", "clearAll", "timeout", "wait", "waitSubmit", "waitPresentationStart", "waitPresentationEnd", "waitHapticIntensity", "captureFrame", "acknowledgeFrame", "rejectFrame", "startTimer", "stopTimer"};
inline constexpr const char* kBindingOrder916[] = {"sittingToStandingTransform", "sizeX", "sizeZ"};
inline constexpr const char* kBindingOrder917[] = {"getCueAsHTML", "region", "vertical", "snapToLines", "line", "lineAlign", "position", "positionAlign", "size", "align", "text"};
inline constexpr const char* kBindingOrder918[] = {"id", "width", "lines", "regionAnchorX", "regionAnchorY", "viewportAnchorX", "viewportAnchorY", "scroll"};
inline constexpr const char* kBindingOrder919[] = {"valueMissing", "typeMismatch", "patternMismatch", "tooLong", "tooShort", "rangeUnderflow", "rangeOverflow", "stepMismatch", "badInput", "customError", "valid"};
inline constexpr const char* kBindingOrder920[] = {"toJSON", "primaries", "transfer", "matrix", "fullRange"};
inline constexpr const char* kBindingOrder921[] = {"configure", "decode", "flush", "reset", "close", "state", "decodeQueueSize", "ondequeue"};
inline constexpr const char* kBindingOrder922[] = {"configure", "encode", "flush", "reset", "close", "mozRequestDebugInfo", "state", "encodeQueueSize", "ondequeue"};
inline constexpr const char* kBindingOrder923[] = {"allocationSize", "copyTo", "clone", "close", "format", "codedWidth", "codedHeight", "codedRect", "visibleRect", "displayWidth", "displayHeight", "duration", "timestamp", "colorSpace"};
inline constexpr const char* kBindingOrder924[] = {"creationTime", "totalVideoFrames", "droppedVideoFrames"};
inline constexpr const char* kBindingOrder925[] = {"id", "kind", "label", "language", "selected"};
inline constexpr const char* kBindingOrder926[] = {"getTrackById", "@@iterator", "length", "selectedIndex", "onchange", "onaddtrack", "onremovetrack"};
inline constexpr const char* kBindingOrder927[] = {"subject", "startOffset", "endOffset"};
inline constexpr const char* kBindingOrder928[] = {"skipTransition", "updateCallbackDone", "ready", "finished", "types"};
inline constexpr const char* kBindingOrder929[] = {"add", "clear", "delete", "entries", "keys", "values", "forEach", "has", "size"};
inline constexpr const char* kBindingOrder930[] = {"offsetLeft", "offsetTop", "pageLeft", "pageTop", "width", "height", "scale", "onresize", "onscroll", "onscrollend"};
inline constexpr const char* kBindingOrder931[] = {"RGBA32F_EXT", "RGB32F_EXT", "FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE_EXT", "UNSIGNED_NORMALIZED_EXT"};
inline constexpr const char* kBindingOrder932[] = {"getSupportedProfiles", "COMPRESSED_RGBA_ASTC_4x4_KHR", "COMPRESSED_RGBA_ASTC_5x4_KHR", "COMPRESSED_RGBA_ASTC_5x5_KHR", "COMPRESSED_RGBA_ASTC_6x5_KHR", "COMPRESSED_RGBA_ASTC_6x6_KHR", "COMPRESSED_RGBA_ASTC_8x5_KHR", "COMPRESSED_RGBA_ASTC_8x6_KHR", "COMPRESSED_RGBA_ASTC_8x8_KHR", "COMPRESSED_RGBA_ASTC_10x5_KHR", "COMPRESSED_RGBA_ASTC_10x6_KHR", "COMPRESSED_RGBA_ASTC_10x8_KHR", "COMPRESSED_RGBA_ASTC_10x10_KHR", "COMPRESSED_RGBA_ASTC_12x10_KHR", "COMPRESSED_RGBA_ASTC_12x12_KHR", "COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR", "COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR", "COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR", "COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR", "COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR", "COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR", "COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR", "COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR", "COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR", "COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR", "COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR", "COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR", "COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR", "COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR"};
inline constexpr const char* kBindingOrder933[] = {"COMPRESSED_R11_EAC", "COMPRESSED_SIGNED_R11_EAC", "COMPRESSED_RG11_EAC", "COMPRESSED_SIGNED_RG11_EAC", "COMPRESSED_RGB8_ETC2", "COMPRESSED_SRGB8_ETC2", "COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2", "COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2", "COMPRESSED_RGBA8_ETC2_EAC", "COMPRESSED_SRGB8_ALPHA8_ETC2_EAC"};
inline constexpr const char* kBindingOrder934[] = {"COMPRESSED_RGB_ETC1_WEBGL"};
inline constexpr const char* kBindingOrder935[] = {"COMPRESSED_RGB_PVRTC_4BPPV1_IMG", "COMPRESSED_RGB_PVRTC_2BPPV1_IMG", "COMPRESSED_RGBA_PVRTC_4BPPV1_IMG", "COMPRESSED_RGBA_PVRTC_2BPPV1_IMG"};
inline constexpr const char* kBindingOrder936[] = {"COMPRESSED_RGB_S3TC_DXT1_EXT", "COMPRESSED_RGBA_S3TC_DXT1_EXT", "COMPRESSED_RGBA_S3TC_DXT3_EXT", "COMPRESSED_RGBA_S3TC_DXT5_EXT"};
inline constexpr const char* kBindingOrder937[] = {"COMPRESSED_SRGB_S3TC_DXT1_EXT", "COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT", "COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT", "COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT"};
inline constexpr const char* kBindingOrder938[] = {"UNMASKED_VENDOR_WEBGL", "UNMASKED_RENDERER_WEBGL"};
inline constexpr const char* kBindingOrder939[] = {"getTranslatedShaderSource"};
inline constexpr const char* kBindingOrder940[] = {"UNSIGNED_INT_24_8_WEBGL"};
inline constexpr const char* kBindingOrder941[] = {"drawBuffersWEBGL", "COLOR_ATTACHMENT0_WEBGL", "COLOR_ATTACHMENT1_WEBGL", "COLOR_ATTACHMENT2_WEBGL", "COLOR_ATTACHMENT3_WEBGL", "COLOR_ATTACHMENT4_WEBGL", "COLOR_ATTACHMENT5_WEBGL", "COLOR_ATTACHMENT6_WEBGL", "COLOR_ATTACHMENT7_WEBGL", "COLOR_ATTACHMENT8_WEBGL", "COLOR_ATTACHMENT9_WEBGL", "COLOR_ATTACHMENT10_WEBGL", "COLOR_ATTACHMENT11_WEBGL", "COLOR_ATTACHMENT12_WEBGL", "COLOR_ATTACHMENT13_WEBGL", "COLOR_ATTACHMENT14_WEBGL", "COLOR_ATTACHMENT15_WEBGL", "DRAW_BUFFER0_WEBGL", "DRAW_BUFFER1_WEBGL", "DRAW_BUFFER2_WEBGL", "DRAW_BUFFER3_WEBGL", "DRAW_BUFFER4_WEBGL", "DRAW_BUFFER5_WEBGL", "DRAW_BUFFER6_WEBGL", "DRAW_BUFFER7_WEBGL", "DRAW_BUFFER8_WEBGL", "DRAW_BUFFER9_WEBGL", "DRAW_BUFFER10_WEBGL", "DRAW_BUFFER11_WEBGL", "DRAW_BUFFER12_WEBGL", "DRAW_BUFFER13_WEBGL", "DRAW_BUFFER14_WEBGL", "DRAW_BUFFER15_WEBGL", "MAX_COLOR_ATTACHMENTS_WEBGL", "MAX_DRAW_BUFFERS_WEBGL"};
inline constexpr const char* kBindingOrder942[] = {"present"};
inline constexpr const char* kBindingOrder943[] = {"loseContext", "restoreContext"};
inline constexpr const char* kBindingOrder944[] = {"provokingVertexWEBGL", "FIRST_VERTEX_CONVENTION_WEBGL", "LAST_VERTEX_CONVENTION_WEBGL", "PROVOKING_VERTEX_WEBGL"};
inline constexpr const char* kBindingOrder945[] = {"entries", "keys", "values", "forEach", "has", "size"};
inline constexpr const char* kBindingOrder946[] = {"request"};
inline constexpr const char* kBindingOrder947[] = {"release", "released", "type", "onrelease"};
inline constexpr const char* kBindingOrder948[] = {"curve", "oversample"};
inline constexpr const char* kBindingOrder949[] = {"runAt", "world", "worldId", "cssOrigin", "cssPaths", "jsPaths"};
inline constexpr const char* kBindingOrder950[] = {"checkGuarded", "canAccessWindow", "canAccessURI", "hasPermission", "quarantinedFromURI", "isWebAccessiblePath", "sourceMayAccessPath", "localize", "getURL", "registerContentScript", "unregisterContentScript", "injectContentScripts", "isManifestBackgroundWorker", "id", "mozExtensionHostname", "baseURL", "name", "version", "type", "isPrivileged", "temporarilyInstalled", "manifestVersion", "baseCSP", "extensionPageCSP", "sandboxPageCSP", "permissions", "allowedOrigins", "fileSchemeAllowed", "guardSets", "contentScripts", "active", "ignoreQuarantine", "hasRecommendedState", "privateBrowsingAllowed", "readyPromise", "browsingContextGroupId"};
inline constexpr const char* kBindingOrder951[] = {"bufferData", "bufferSubData", "copyBufferSubData", "getBufferSubData", "blitFramebuffer", "framebufferTextureLayer", "invalidateFramebuffer", "invalidateSubFramebuffer", "readBuffer", "getInternalformatParameter", "renderbufferStorageMultisample", "texStorage2D", "texStorage3D", "texImage2D", "texSubImage2D", "texImage3D", "texSubImage3D", "copyTexSubImage3D", "compressedTexImage2D", "compressedTexImage3D", "compressedTexSubImage2D", "compressedTexSubImage3D", "getFragDataLocation", "uniform1ui", "uniform2ui", "uniform3ui", "uniform4ui", "uniform1fv", "uniform2fv", "uniform3fv", "uniform4fv", "uniform1iv", "uniform2iv", "uniform3iv", "uniform4iv", "uniform1uiv", "uniform2uiv", "uniform3uiv", "uniform4uiv", "uniformMatrix2fv", "uniformMatrix3x2fv", "uniformMatrix4x2fv", "uniformMatrix2x3fv", "uniformMatrix3fv", "uniformMatrix4x3fv", "uniformMatrix2x4fv", "uniformMatrix3x4fv", "uniformMatrix4fv", "vertexAttribI4i", "vertexAttribI4iv", "vertexAttribI4ui", "vertexAttribI4uiv", "vertexAttribIPointer", "vertexAttribDivisor", "drawArraysInstanced", "drawElementsInstanced", "drawRangeElements", "readPixels", "drawBuffers", "clearBufferfv", "clearBufferiv", "clearBufferuiv", "clearBufferfi", "createQuery", "deleteQuery", "isQuery", "beginQuery", "endQuery", "getQuery", "getQueryParameter", "createSampler", "deleteSampler", "isSampler", "bindSampler", "samplerParameteri", "samplerParameterf", "getSamplerParameter", "fenceSync", "isSync", "deleteSync", "clientWaitSync", "waitSync", "getSyncParameter", "createTransformFeedback", "deleteTransformFeedback", "isTransformFeedback", "bindTransformFeedback", "beginTransformFeedback", "endTransformFeedback", "transformFeedbackVaryings", "getTransformFeedbackVarying", "pauseTransformFeedback", "resumeTransformFeedback", "bindBufferBase", "bindBufferRange", "getIndexedParameter", "getUniformIndices", "getActiveUniforms", "getUniformBlockIndex", "getActiveUniformBlockParameter", "getActiveUniformBlockName", "uniformBlockBinding", "createVertexArray", "deleteVertexArray", "isVertexArray", "bindVertexArray", "getContextAttributes", "isContextLost", "getSupportedExtensions", "getExtension", "activeTexture", "attachShader", "bindAttribLocation", "bindBuffer", "bindFramebuffer", "bindRenderbuffer", "bindTexture", "blendColor", "blendEquation", "blendEquationSeparate", "blendFunc", "blendFuncSeparate", "checkFramebufferStatus", "clear", "clearColor", "clearDepth", "clearStencil", "colorMask", "compileShader", "copyTexImage2D", "copyTexSubImage2D", "createBuffer", "createFramebuffer", "createProgram", "createRenderbuffer", "createShader", "createTexture", "cullFace", "deleteBuffer", "deleteFramebuffer", "deleteProgram", "deleteRenderbuffer", "deleteShader", "deleteTexture", "depthFunc", "depthMask", "depthRange", "detachShader", "disable", "disableVertexAttribArray", "drawArrays", "drawElements", "enable", "enableVertexAttribArray", "finish", "flush", "framebufferRenderbuffer", "framebufferTexture2D", "frontFace", "generateMipmap", "getActiveAttrib", "getActiveUniform", "getAttachedShaders", "getAttribLocation", "getBufferParameter", "getParameter", "getError", "getFramebufferAttachmentParameter", "getProgramParameter", "getProgramInfoLog", "getRenderbufferParameter", "getShaderParameter", "getShaderPrecisionFormat", "getShaderInfoLog", "getShaderSource", "getTexParameter", "getUniform", "getUniformLocation", "getVertexAttrib", "getVertexAttribOffset", "hint", "isBuffer", "isEnabled", "isFramebuffer", "isProgram", "isRenderbuffer", "isShader", "isTexture", "lineWidth", "linkProgram", "pixelStorei", "polygonOffset", "renderbufferStorage", "sampleCoverage", "scissor", "shaderSource", "stencilFunc", "stencilFuncSeparate", "stencilMask", "stencilMaskSeparate", "stencilOp", "stencilOpSeparate", "texParameterf", "texParameteri", "uniform1f", "uniform2f", "uniform3f", "uniform4f", "uniform1i", "uniform2i", "uniform3i", "uniform4i", "useProgram", "validateProgram", "vertexAttrib1f", "vertexAttrib1fv", "vertexAttrib2f", "vertexAttrib2fv", "vertexAttrib3f", "vertexAttrib3fv", "vertexAttrib4f", "vertexAttrib4fv", "vertexAttribPointer", "viewport", "makeXRCompatible", "canvas", "drawingBufferWidth", "drawingBufferHeight", "drawingBufferColorSpace", "unpackColorSpace", "READ_BUFFER", "UNPACK_ROW_LENGTH", "UNPACK_SKIP_ROWS", "UNPACK_SKIP_PIXELS", "PACK_ROW_LENGTH", "PACK_SKIP_ROWS", "PACK_SKIP_PIXELS", "COLOR", "DEPTH", "STENCIL", "RED", "RGB8", "RGBA8", "RGB10_A2", "TEXTURE_BINDING_3D", "UNPACK_SKIP_IMAGES", "UNPACK_IMAGE_HEIGHT", "TEXTURE_3D", "TEXTURE_WRAP_R", "MAX_3D_TEXTURE_SIZE", "UNSIGNED_INT_2_10_10_10_REV", "MAX_ELEMENTS_VERTICES", "MAX_ELEMENTS_INDICES", "TEXTURE_MIN_LOD", "TEXTURE_MAX_LOD", "TEXTURE_BASE_LEVEL", "TEXTURE_MAX_LEVEL", "MIN", "MAX", "DEPTH_COMPONENT24", "MAX_TEXTURE_LOD_BIAS", "TEXTURE_COMPARE_MODE", "TEXTURE_COMPARE_FUNC", "CURRENT_QUERY", "QUERY_RESULT", "QUERY_RESULT_AVAILABLE", "STREAM_READ", "STREAM_COPY", "STATIC_READ", "STATIC_COPY", "DYNAMIC_READ", "DYNAMIC_COPY", "MAX_DRAW_BUFFERS", "DRAW_BUFFER0", "DRAW_BUFFER1", "DRAW_BUFFER2", "DRAW_BUFFER3", "DRAW_BUFFER4", "DRAW_BUFFER5", "DRAW_BUFFER6", "DRAW_BUFFER7", "DRAW_BUFFER8", "DRAW_BUFFER9", "DRAW_BUFFER10", "DRAW_BUFFER11", "DRAW_BUFFER12", "DRAW_BUFFER13", "DRAW_BUFFER14", "DRAW_BUFFER15", "MAX_FRAGMENT_UNIFORM_COMPONENTS", "MAX_VERTEX_UNIFORM_COMPONENTS", "SAMPLER_3D", "SAMPLER_2D_SHADOW", "FRAGMENT_SHADER_DERIVATIVE_HINT", "PIXEL_PACK_BUFFER", "PIXEL_UNPACK_BUFFER", "PIXEL_PACK_BUFFER_BINDING", "PIXEL_UNPACK_BUFFER_BINDING", "FLOAT_MAT2x3", "FLOAT_MAT2x4", "FLOAT_MAT3x2", "FLOAT_MAT3x4", "FLOAT_MAT4x2", "FLOAT_MAT4x3", "SRGB", "SRGB8", "SRGB8_ALPHA8", "COMPARE_REF_TO_TEXTURE", "RGBA32F", "RGB32F", "RGBA16F", "RGB16F", "VERTEX_ATTRIB_ARRAY_INTEGER", "MAX_ARRAY_TEXTURE_LAYERS", "MIN_PROGRAM_TEXEL_OFFSET", "MAX_PROGRAM_TEXEL_OFFSET", "MAX_VARYING_COMPONENTS", "TEXTURE_2D_ARRAY", "TEXTURE_BINDING_2D_ARRAY", "R11F_G11F_B10F", "UNSIGNED_INT_10F_11F_11F_REV", "RGB9_E5", "UNSIGNED_INT_5_9_9_9_REV", "TRANSFORM_FEEDBACK_BUFFER_MODE", "MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS", "TRANSFORM_FEEDBACK_VARYINGS", "TRANSFORM_FEEDBACK_BUFFER_START", "TRANSFORM_FEEDBACK_BUFFER_SIZE", "TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN", "RASTERIZER_DISCARD", "MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS", "MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS", "INTERLEAVED_ATTRIBS", "SEPARATE_ATTRIBS", "TRANSFORM_FEEDBACK_BUFFER", "TRANSFORM_FEEDBACK_BUFFER_BINDING", "RGBA32UI", "RGB32UI", "RGBA16UI", "RGB16UI", "RGBA8UI", "RGB8UI", "RGBA32I", "RGB32I", "RGBA16I", "RGB16I", "RGBA8I", "RGB8I", "RED_INTEGER", "RGB_INTEGER", "RGBA_INTEGER", "SAMPLER_2D_ARRAY", "SAMPLER_2D_ARRAY_SHADOW", "SAMPLER_CUBE_SHADOW", "UNSIGNED_INT_VEC2", "UNSIGNED_INT_VEC3", "UNSIGNED_INT_VEC4", "INT_SAMPLER_2D", "INT_SAMPLER_3D", "INT_SAMPLER_CUBE", "INT_SAMPLER_2D_ARRAY", "UNSIGNED_INT_SAMPLER_2D", "UNSIGNED_INT_SAMPLER_3D", "UNSIGNED_INT_SAMPLER_CUBE", "UNSIGNED_INT_SAMPLER_2D_ARRAY", "DEPTH_COMPONENT32F", "DEPTH32F_STENCIL8", "FLOAT_32_UNSIGNED_INT_24_8_REV", "FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING", "FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE", "FRAMEBUFFER_ATTACHMENT_RED_SIZE", "FRAMEBUFFER_ATTACHMENT_GREEN_SIZE", "FRAMEBUFFER_ATTACHMENT_BLUE_SIZE", "FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE", "FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE", "FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE", "FRAMEBUFFER_DEFAULT", "UNSIGNED_INT_24_8", "DEPTH24_STENCIL8", "UNSIGNED_NORMALIZED", "DRAW_FRAMEBUFFER_BINDING", "READ_FRAMEBUFFER", "DRAW_FRAMEBUFFER", "READ_FRAMEBUFFER_BINDING", "RENDERBUFFER_SAMPLES", "FRAMEBUFFER_ATTACHMENT_TEXTURE_LAYER", "MAX_COLOR_ATTACHMENTS", "COLOR_ATTACHMENT1", "COLOR_ATTACHMENT2", "COLOR_ATTACHMENT3", "COLOR_ATTACHMENT4", "COLOR_ATTACHMENT5", "COLOR_ATTACHMENT6", "COLOR_ATTACHMENT7", "COLOR_ATTACHMENT8", "COLOR_ATTACHMENT9", "COLOR_ATTACHMENT10", "COLOR_ATTACHMENT11", "COLOR_ATTACHMENT12", "COLOR_ATTACHMENT13", "COLOR_ATTACHMENT14", "COLOR_ATTACHMENT15", "FRAMEBUFFER_INCOMPLETE_MULTISAMPLE", "MAX_SAMPLES", "HALF_FLOAT", "RG", "RG_INTEGER", "R8", "RG8", "R16F", "R32F", "RG16F", "RG32F", "R8I", "R8UI", "R16I", "R16UI", "R32I", "R32UI", "RG8I", "RG8UI", "RG16I", "RG16UI", "RG32I", "RG32UI", "VERTEX_ARRAY_BINDING", "R8_SNORM", "RG8_SNORM", "RGB8_SNORM", "RGBA8_SNORM", "SIGNED_NORMALIZED", "COPY_READ_BUFFER", "COPY_WRITE_BUFFER", "COPY_READ_BUFFER_BINDING", "COPY_WRITE_BUFFER_BINDING", "UNIFORM_BUFFER", "UNIFORM_BUFFER_BINDING", "UNIFORM_BUFFER_START", "UNIFORM_BUFFER_SIZE", "MAX_VERTEX_UNIFORM_BLOCKS", "MAX_FRAGMENT_UNIFORM_BLOCKS", "MAX_COMBINED_UNIFORM_BLOCKS", "MAX_UNIFORM_BUFFER_BINDINGS", "MAX_UNIFORM_BLOCK_SIZE", "MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS", "MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS", "UNIFORM_BUFFER_OFFSET_ALIGNMENT", "ACTIVE_UNIFORM_BLOCKS", "UNIFORM_TYPE", "UNIFORM_SIZE", "UNIFORM_BLOCK_INDEX", "UNIFORM_OFFSET", "UNIFORM_ARRAY_STRIDE", "UNIFORM_MATRIX_STRIDE", "UNIFORM_IS_ROW_MAJOR", "UNIFORM_BLOCK_BINDING", "UNIFORM_BLOCK_DATA_SIZE", "UNIFORM_BLOCK_ACTIVE_UNIFORMS", "UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES", "UNIFORM_BLOCK_REFERENCED_BY_VERTEX_SHADER", "UNIFORM_BLOCK_REFERENCED_BY_FRAGMENT_SHADER", "INVALID_INDEX", "MAX_VERTEX_OUTPUT_COMPONENTS", "MAX_FRAGMENT_INPUT_COMPONENTS", "MAX_SERVER_WAIT_TIMEOUT", "OBJECT_TYPE", "SYNC_CONDITION", "SYNC_STATUS", "SYNC_FLAGS", "SYNC_FENCE", "SYNC_GPU_COMMANDS_COMPLETE", "UNSIGNALED", "SIGNALED", "ALREADY_SIGNALED", "TIMEOUT_EXPIRED", "CONDITION_SATISFIED", "WAIT_FAILED", "SYNC_FLUSH_COMMANDS_BIT", "VERTEX_ATTRIB_ARRAY_DIVISOR", "ANY_SAMPLES_PASSED", "ANY_SAMPLES_PASSED_CONSERVATIVE", "SAMPLER_BINDING", "RGB10_A2UI", "INT_2_10_10_10_REV", "TRANSFORM_FEEDBACK", "TRANSFORM_FEEDBACK_PAUSED", "TRANSFORM_FEEDBACK_ACTIVE", "TRANSFORM_FEEDBACK_BINDING", "TEXTURE_IMMUTABLE_FORMAT", "MAX_ELEMENT_INDEX", "TEXTURE_IMMUTABLE_LEVELS", "TIMEOUT_IGNORED", "MAX_CLIENT_WAIT_TIMEOUT_WEBGL", "DEPTH_BUFFER_BIT", "STENCIL_BUFFER_BIT", "COLOR_BUFFER_BIT", "POINTS", "LINES", "LINE_LOOP", "LINE_STRIP", "TRIANGLES", "TRIANGLE_STRIP", "TRIANGLE_FAN", "ZERO", "ONE", "SRC_COLOR", "ONE_MINUS_SRC_COLOR", "SRC_ALPHA", "ONE_MINUS_SRC_ALPHA", "DST_ALPHA", "ONE_MINUS_DST_ALPHA", "DST_COLOR", "ONE_MINUS_DST_COLOR", "SRC_ALPHA_SATURATE", "FUNC_ADD", "BLEND_EQUATION", "BLEND_EQUATION_RGB", "BLEND_EQUATION_ALPHA", "FUNC_SUBTRACT", "FUNC_REVERSE_SUBTRACT", "BLEND_DST_RGB", "BLEND_SRC_RGB", "BLEND_DST_ALPHA", "BLEND_SRC_ALPHA", "CONSTANT_COLOR", "ONE_MINUS_CONSTANT_COLOR", "CONSTANT_ALPHA", "ONE_MINUS_CONSTANT_ALPHA", "BLEND_COLOR", "ARRAY_BUFFER", "ELEMENT_ARRAY_BUFFER", "ARRAY_BUFFER_BINDING", "ELEMENT_ARRAY_BUFFER_BINDING", "STREAM_DRAW", "STATIC_DRAW", "DYNAMIC_DRAW", "BUFFER_SIZE", "BUFFER_USAGE", "CURRENT_VERTEX_ATTRIB", "FRONT", "BACK", "FRONT_AND_BACK", "CULL_FACE", "BLEND", "DITHER", "STENCIL_TEST", "DEPTH_TEST", "SCISSOR_TEST", "POLYGON_OFFSET_FILL", "SAMPLE_ALPHA_TO_COVERAGE", "SAMPLE_COVERAGE", "NO_ERROR", "INVALID_ENUM", "INVALID_VALUE", "INVALID_OPERATION", "OUT_OF_MEMORY", "CW", "CCW", "LINE_WIDTH", "ALIASED_POINT_SIZE_RANGE", "ALIASED_LINE_WIDTH_RANGE", "CULL_FACE_MODE", "FRONT_FACE", "DEPTH_RANGE", "DEPTH_WRITEMASK", "DEPTH_CLEAR_VALUE", "DEPTH_FUNC", "STENCIL_CLEAR_VALUE", "STENCIL_FUNC", "STENCIL_FAIL", "STENCIL_PASS_DEPTH_FAIL", "STENCIL_PASS_DEPTH_PASS", "STENCIL_REF", "STENCIL_VALUE_MASK", "STENCIL_WRITEMASK", "STENCIL_BACK_FUNC", "STENCIL_BACK_FAIL", "STENCIL_BACK_PASS_DEPTH_FAIL", "STENCIL_BACK_PASS_DEPTH_PASS", "STENCIL_BACK_REF", "STENCIL_BACK_VALUE_MASK", "STENCIL_BACK_WRITEMASK", "VIEWPORT", "SCISSOR_BOX", "COLOR_CLEAR_VALUE", "COLOR_WRITEMASK", "UNPACK_ALIGNMENT", "PACK_ALIGNMENT", "MAX_TEXTURE_SIZE", "MAX_VIEWPORT_DIMS", "SUBPIXEL_BITS", "RED_BITS", "GREEN_BITS", "BLUE_BITS", "ALPHA_BITS", "DEPTH_BITS", "STENCIL_BITS", "POLYGON_OFFSET_UNITS", "POLYGON_OFFSET_FACTOR", "TEXTURE_BINDING_2D", "SAMPLE_BUFFERS", "SAMPLES", "SAMPLE_COVERAGE_VALUE", "SAMPLE_COVERAGE_INVERT", "COMPRESSED_TEXTURE_FORMATS", "DONT_CARE", "FASTEST", "NICEST", "GENERATE_MIPMAP_HINT", "BYTE", "UNSIGNED_BYTE", "SHORT", "UNSIGNED_SHORT", "INT", "UNSIGNED_INT", "FLOAT", "DEPTH_COMPONENT", "ALPHA", "RGB", "RGBA", "LUMINANCE", "LUMINANCE_ALPHA", "UNSIGNED_SHORT_4_4_4_4", "UNSIGNED_SHORT_5_5_5_1", "UNSIGNED_SHORT_5_6_5", "FRAGMENT_SHADER", "VERTEX_SHADER", "MAX_VERTEX_ATTRIBS", "MAX_VERTEX_UNIFORM_VECTORS", "MAX_VARYING_VECTORS", "MAX_COMBINED_TEXTURE_IMAGE_UNITS", "MAX_VERTEX_TEXTURE_IMAGE_UNITS", "MAX_TEXTURE_IMAGE_UNITS", "MAX_FRAGMENT_UNIFORM_VECTORS", "SHADER_TYPE", "DELETE_STATUS", "LINK_STATUS", "VALIDATE_STATUS", "ATTACHED_SHADERS", "ACTIVE_UNIFORMS", "ACTIVE_ATTRIBUTES", "SHADING_LANGUAGE_VERSION", "CURRENT_PROGRAM", "NEVER", "LESS", "EQUAL", "LEQUAL", "GREATER", "NOTEQUAL", "GEQUAL", "ALWAYS", "KEEP", "REPLACE", "INCR", "DECR", "INVERT", "INCR_WRAP", "DECR_WRAP", "VENDOR", "RENDERER", "VERSION", "NEAREST", "LINEAR", "NEAREST_MIPMAP_NEAREST", "LINEAR_MIPMAP_NEAREST", "NEAREST_MIPMAP_LINEAR", "LINEAR_MIPMAP_LINEAR", "TEXTURE_MAG_FILTER", "TEXTURE_MIN_FILTER", "TEXTURE_WRAP_S", "TEXTURE_WRAP_T", "TEXTURE_2D", "TEXTURE", "TEXTURE_CUBE_MAP", "TEXTURE_BINDING_CUBE_MAP", "TEXTURE_CUBE_MAP_POSITIVE_X", "TEXTURE_CUBE_MAP_NEGATIVE_X", "TEXTURE_CUBE_MAP_POSITIVE_Y", "TEXTURE_CUBE_MAP_NEGATIVE_Y", "TEXTURE_CUBE_MAP_POSITIVE_Z", "TEXTURE_CUBE_MAP_NEGATIVE_Z", "MAX_CUBE_MAP_TEXTURE_SIZE", "TEXTURE0", "TEXTURE1", "TEXTURE2", "TEXTURE3", "TEXTURE4", "TEXTURE5", "TEXTURE6", "TEXTURE7", "TEXTURE8", "TEXTURE9", "TEXTURE10", "TEXTURE11", "TEXTURE12", "TEXTURE13", "TEXTURE14", "TEXTURE15", "TEXTURE16", "TEXTURE17", "TEXTURE18", "TEXTURE19", "TEXTURE20", "TEXTURE21", "TEXTURE22", "TEXTURE23", "TEXTURE24", "TEXTURE25", "TEXTURE26", "TEXTURE27", "TEXTURE28", "TEXTURE29", "TEXTURE30", "TEXTURE31", "ACTIVE_TEXTURE", "REPEAT", "CLAMP_TO_EDGE", "MIRRORED_REPEAT", "FLOAT_VEC2", "FLOAT_VEC3", "FLOAT_VEC4", "INT_VEC2", "INT_VEC3", "INT_VEC4", "BOOL", "BOOL_VEC2", "BOOL_VEC3", "BOOL_VEC4", "FLOAT_MAT2", "FLOAT_MAT3", "FLOAT_MAT4", "SAMPLER_2D", "SAMPLER_CUBE", "VERTEX_ATTRIB_ARRAY_ENABLED", "VERTEX_ATTRIB_ARRAY_SIZE", "VERTEX_ATTRIB_ARRAY_STRIDE", "VERTEX_ATTRIB_ARRAY_TYPE", "VERTEX_ATTRIB_ARRAY_NORMALIZED", "VERTEX_ATTRIB_ARRAY_POINTER", "VERTEX_ATTRIB_ARRAY_BUFFER_BINDING", "IMPLEMENTATION_COLOR_READ_TYPE", "IMPLEMENTATION_COLOR_READ_FORMAT", "COMPILE_STATUS", "LOW_FLOAT", "MEDIUM_FLOAT", "HIGH_FLOAT", "LOW_INT", "MEDIUM_INT", "HIGH_INT", "FRAMEBUFFER", "RENDERBUFFER", "RGBA4", "RGB5_A1", "RGB565", "DEPTH_COMPONENT16", "STENCIL_INDEX8", "DEPTH_STENCIL", "RENDERBUFFER_WIDTH", "RENDERBUFFER_HEIGHT", "RENDERBUFFER_INTERNAL_FORMAT", "RENDERBUFFER_RED_SIZE", "RENDERBUFFER_GREEN_SIZE", "RENDERBUFFER_BLUE_SIZE", "RENDERBUFFER_ALPHA_SIZE", "RENDERBUFFER_DEPTH_SIZE", "RENDERBUFFER_STENCIL_SIZE", "FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE", "FRAMEBUFFER_ATTACHMENT_OBJECT_NAME", "FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL", "FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE", "COLOR_ATTACHMENT0", "DEPTH_ATTACHMENT", "STENCIL_ATTACHMENT", "DEPTH_STENCIL_ATTACHMENT", "NONE", "FRAMEBUFFER_COMPLETE", "FRAMEBUFFER_INCOMPLETE_ATTACHMENT", "FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT", "FRAMEBUFFER_INCOMPLETE_DIMENSIONS", "FRAMEBUFFER_UNSUPPORTED", "FRAMEBUFFER_BINDING", "RENDERBUFFER_BINDING", "MAX_RENDERBUFFER_SIZE", "INVALID_FRAMEBUFFER_OPERATION", "UNPACK_FLIP_Y_WEBGL", "UNPACK_PREMULTIPLY_ALPHA_WEBGL", "CONTEXT_LOST_WEBGL", "UNPACK_COLORSPACE_CONVERSION_WEBGL", "BROWSER_DEFAULT_WEBGL"};
inline constexpr const char* kBindingOrder952[] = {"size", "type", "name"};
inline constexpr const char* kBindingOrder953[] = {"statusMessage"};
inline constexpr const char* kBindingOrder954[] = {"bufferData", "bufferSubData", "compressedTexImage2D", "compressedTexSubImage2D", "readPixels", "texImage2D", "texSubImage2D", "uniform1fv", "uniform2fv", "uniform3fv", "uniform4fv", "uniform1iv", "uniform2iv", "uniform3iv", "uniform4iv", "uniformMatrix2fv", "uniformMatrix3fv", "uniformMatrix4fv", "getContextAttributes", "isContextLost", "getSupportedExtensions", "getExtension", "activeTexture", "attachShader", "bindAttribLocation", "bindBuffer", "bindFramebuffer", "bindRenderbuffer", "bindTexture", "blendColor", "blendEquation", "blendEquationSeparate", "blendFunc", "blendFuncSeparate", "checkFramebufferStatus", "clear", "clearColor", "clearDepth", "clearStencil", "colorMask", "compileShader", "copyTexImage2D", "copyTexSubImage2D", "createBuffer", "createFramebuffer", "createProgram", "createRenderbuffer", "createShader", "createTexture", "cullFace", "deleteBuffer", "deleteFramebuffer", "deleteProgram", "deleteRenderbuffer", "deleteShader", "deleteTexture", "depthFunc", "depthMask", "depthRange", "detachShader", "disable", "disableVertexAttribArray", "drawArrays", "drawElements", "enable", "enableVertexAttribArray", "finish", "flush", "framebufferRenderbuffer", "framebufferTexture2D", "frontFace", "generateMipmap", "getActiveAttrib", "getActiveUniform", "getAttachedShaders", "getAttribLocation", "getBufferParameter", "getParameter", "getError", "getFramebufferAttachmentParameter", "getProgramParameter", "getProgramInfoLog", "getRenderbufferParameter", "getShaderParameter", "getShaderPrecisionFormat", "getShaderInfoLog", "getShaderSource", "getTexParameter", "getUniform", "getUniformLocation", "getVertexAttrib", "getVertexAttribOffset", "hint", "isBuffer", "isEnabled", "isFramebuffer", "isProgram", "isRenderbuffer", "isShader", "isTexture", "lineWidth", "linkProgram", "pixelStorei", "polygonOffset", "renderbufferStorage", "sampleCoverage", "scissor", "shaderSource", "stencilFunc", "stencilFuncSeparate", "stencilMask", "stencilMaskSeparate", "stencilOp", "stencilOpSeparate", "texParameterf", "texParameteri", "uniform1f", "uniform2f", "uniform3f", "uniform4f", "uniform1i", "uniform2i", "uniform3i", "uniform4i", "useProgram", "validateProgram", "vertexAttrib1f", "vertexAttrib1fv", "vertexAttrib2f", "vertexAttrib2fv", "vertexAttrib3f", "vertexAttrib3fv", "vertexAttrib4f", "vertexAttrib4fv", "vertexAttribPointer", "viewport", "makeXRCompatible", "canvas", "drawingBufferWidth", "drawingBufferHeight", "drawingBufferColorSpace", "unpackColorSpace", "DEPTH_BUFFER_BIT", "STENCIL_BUFFER_BIT", "COLOR_BUFFER_BIT", "POINTS", "LINES", "LINE_LOOP", "LINE_STRIP", "TRIANGLES", "TRIANGLE_STRIP", "TRIANGLE_FAN", "ZERO", "ONE", "SRC_COLOR", "ONE_MINUS_SRC_COLOR", "SRC_ALPHA", "ONE_MINUS_SRC_ALPHA", "DST_ALPHA", "ONE_MINUS_DST_ALPHA", "DST_COLOR", "ONE_MINUS_DST_COLOR", "SRC_ALPHA_SATURATE", "FUNC_ADD", "BLEND_EQUATION", "BLEND_EQUATION_RGB", "BLEND_EQUATION_ALPHA", "FUNC_SUBTRACT", "FUNC_REVERSE_SUBTRACT", "BLEND_DST_RGB", "BLEND_SRC_RGB", "BLEND_DST_ALPHA", "BLEND_SRC_ALPHA", "CONSTANT_COLOR", "ONE_MINUS_CONSTANT_COLOR", "CONSTANT_ALPHA", "ONE_MINUS_CONSTANT_ALPHA", "BLEND_COLOR", "ARRAY_BUFFER", "ELEMENT_ARRAY_BUFFER", "ARRAY_BUFFER_BINDING", "ELEMENT_ARRAY_BUFFER_BINDING", "STREAM_DRAW", "STATIC_DRAW", "DYNAMIC_DRAW", "BUFFER_SIZE", "BUFFER_USAGE", "CURRENT_VERTEX_ATTRIB", "FRONT", "BACK", "FRONT_AND_BACK", "CULL_FACE", "BLEND", "DITHER", "STENCIL_TEST", "DEPTH_TEST", "SCISSOR_TEST", "POLYGON_OFFSET_FILL", "SAMPLE_ALPHA_TO_COVERAGE", "SAMPLE_COVERAGE", "NO_ERROR", "INVALID_ENUM", "INVALID_VALUE", "INVALID_OPERATION", "OUT_OF_MEMORY", "CW", "CCW", "LINE_WIDTH", "ALIASED_POINT_SIZE_RANGE", "ALIASED_LINE_WIDTH_RANGE", "CULL_FACE_MODE", "FRONT_FACE", "DEPTH_RANGE", "DEPTH_WRITEMASK", "DEPTH_CLEAR_VALUE", "DEPTH_FUNC", "STENCIL_CLEAR_VALUE", "STENCIL_FUNC", "STENCIL_FAIL", "STENCIL_PASS_DEPTH_FAIL", "STENCIL_PASS_DEPTH_PASS", "STENCIL_REF", "STENCIL_VALUE_MASK", "STENCIL_WRITEMASK", "STENCIL_BACK_FUNC", "STENCIL_BACK_FAIL", "STENCIL_BACK_PASS_DEPTH_FAIL", "STENCIL_BACK_PASS_DEPTH_PASS", "STENCIL_BACK_REF", "STENCIL_BACK_VALUE_MASK", "STENCIL_BACK_WRITEMASK", "VIEWPORT", "SCISSOR_BOX", "COLOR_CLEAR_VALUE", "COLOR_WRITEMASK", "UNPACK_ALIGNMENT", "PACK_ALIGNMENT", "MAX_TEXTURE_SIZE", "MAX_VIEWPORT_DIMS", "SUBPIXEL_BITS", "RED_BITS", "GREEN_BITS", "BLUE_BITS", "ALPHA_BITS", "DEPTH_BITS", "STENCIL_BITS", "POLYGON_OFFSET_UNITS", "POLYGON_OFFSET_FACTOR", "TEXTURE_BINDING_2D", "SAMPLE_BUFFERS", "SAMPLES", "SAMPLE_COVERAGE_VALUE", "SAMPLE_COVERAGE_INVERT", "COMPRESSED_TEXTURE_FORMATS", "DONT_CARE", "FASTEST", "NICEST", "GENERATE_MIPMAP_HINT", "BYTE", "UNSIGNED_BYTE", "SHORT", "UNSIGNED_SHORT", "INT", "UNSIGNED_INT", "FLOAT", "DEPTH_COMPONENT", "ALPHA", "RGB", "RGBA", "LUMINANCE", "LUMINANCE_ALPHA", "UNSIGNED_SHORT_4_4_4_4", "UNSIGNED_SHORT_5_5_5_1", "UNSIGNED_SHORT_5_6_5", "FRAGMENT_SHADER", "VERTEX_SHADER", "MAX_VERTEX_ATTRIBS", "MAX_VERTEX_UNIFORM_VECTORS", "MAX_VARYING_VECTORS", "MAX_COMBINED_TEXTURE_IMAGE_UNITS", "MAX_VERTEX_TEXTURE_IMAGE_UNITS", "MAX_TEXTURE_IMAGE_UNITS", "MAX_FRAGMENT_UNIFORM_VECTORS", "SHADER_TYPE", "DELETE_STATUS", "LINK_STATUS", "VALIDATE_STATUS", "ATTACHED_SHADERS", "ACTIVE_UNIFORMS", "ACTIVE_ATTRIBUTES", "SHADING_LANGUAGE_VERSION", "CURRENT_PROGRAM", "NEVER", "LESS", "EQUAL", "LEQUAL", "GREATER", "NOTEQUAL", "GEQUAL", "ALWAYS", "KEEP", "REPLACE", "INCR", "DECR", "INVERT", "INCR_WRAP", "DECR_WRAP", "VENDOR", "RENDERER", "VERSION", "NEAREST", "LINEAR", "NEAREST_MIPMAP_NEAREST", "LINEAR_MIPMAP_NEAREST", "NEAREST_MIPMAP_LINEAR", "LINEAR_MIPMAP_LINEAR", "TEXTURE_MAG_FILTER", "TEXTURE_MIN_FILTER", "TEXTURE_WRAP_S", "TEXTURE_WRAP_T", "TEXTURE_2D", "TEXTURE", "TEXTURE_CUBE_MAP", "TEXTURE_BINDING_CUBE_MAP", "TEXTURE_CUBE_MAP_POSITIVE_X", "TEXTURE_CUBE_MAP_NEGATIVE_X", "TEXTURE_CUBE_MAP_POSITIVE_Y", "TEXTURE_CUBE_MAP_NEGATIVE_Y", "TEXTURE_CUBE_MAP_POSITIVE_Z", "TEXTURE_CUBE_MAP_NEGATIVE_Z", "MAX_CUBE_MAP_TEXTURE_SIZE", "TEXTURE0", "TEXTURE1", "TEXTURE2", "TEXTURE3", "TEXTURE4", "TEXTURE5", "TEXTURE6", "TEXTURE7", "TEXTURE8", "TEXTURE9", "TEXTURE10", "TEXTURE11", "TEXTURE12", "TEXTURE13", "TEXTURE14", "TEXTURE15", "TEXTURE16", "TEXTURE17", "TEXTURE18", "TEXTURE19", "TEXTURE20", "TEXTURE21", "TEXTURE22", "TEXTURE23", "TEXTURE24", "TEXTURE25", "TEXTURE26", "TEXTURE27", "TEXTURE28", "TEXTURE29", "TEXTURE30", "TEXTURE31", "ACTIVE_TEXTURE", "REPEAT", "CLAMP_TO_EDGE", "MIRRORED_REPEAT", "FLOAT_VEC2", "FLOAT_VEC3", "FLOAT_VEC4", "INT_VEC2", "INT_VEC3", "INT_VEC4", "BOOL", "BOOL_VEC2", "BOOL_VEC3", "BOOL_VEC4", "FLOAT_MAT2", "FLOAT_MAT3", "FLOAT_MAT4", "SAMPLER_2D", "SAMPLER_CUBE", "VERTEX_ATTRIB_ARRAY_ENABLED", "VERTEX_ATTRIB_ARRAY_SIZE", "VERTEX_ATTRIB_ARRAY_STRIDE", "VERTEX_ATTRIB_ARRAY_TYPE", "VERTEX_ATTRIB_ARRAY_NORMALIZED", "VERTEX_ATTRIB_ARRAY_POINTER", "VERTEX_ATTRIB_ARRAY_BUFFER_BINDING", "IMPLEMENTATION_COLOR_READ_TYPE", "IMPLEMENTATION_COLOR_READ_FORMAT", "COMPILE_STATUS", "LOW_FLOAT", "MEDIUM_FLOAT", "HIGH_FLOAT", "LOW_INT", "MEDIUM_INT", "HIGH_INT", "FRAMEBUFFER", "RENDERBUFFER", "RGBA4", "RGB5_A1", "RGB565", "DEPTH_COMPONENT16", "STENCIL_INDEX8", "DEPTH_STENCIL", "RENDERBUFFER_WIDTH", "RENDERBUFFER_HEIGHT", "RENDERBUFFER_INTERNAL_FORMAT", "RENDERBUFFER_RED_SIZE", "RENDERBUFFER_GREEN_SIZE", "RENDERBUFFER_BLUE_SIZE", "RENDERBUFFER_ALPHA_SIZE", "RENDERBUFFER_DEPTH_SIZE", "RENDERBUFFER_STENCIL_SIZE", "FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE", "FRAMEBUFFER_ATTACHMENT_OBJECT_NAME", "FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL", "FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE", "COLOR_ATTACHMENT0", "DEPTH_ATTACHMENT", "STENCIL_ATTACHMENT", "DEPTH_STENCIL_ATTACHMENT", "NONE", "FRAMEBUFFER_COMPLETE", "FRAMEBUFFER_INCOMPLETE_ATTACHMENT", "FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT", "FRAMEBUFFER_INCOMPLETE_DIMENSIONS", "FRAMEBUFFER_UNSUPPORTED", "FRAMEBUFFER_BINDING", "RENDERBUFFER_BINDING", "MAX_RENDERBUFFER_SIZE", "INVALID_FRAMEBUFFER_OPERATION", "UNPACK_FLIP_Y_WEBGL", "UNPACK_PREMULTIPLY_ALPHA_WEBGL", "CONTEXT_LOST_WEBGL", "UNPACK_COLORSPACE_CONVERSION_WEBGL", "BROWSER_DEFAULT_WEBGL"};
inline constexpr const char* kBindingOrder955[] = {"rangeMin", "rangeMax", "precision"};
inline constexpr const char* kBindingOrder956[] = {"close", "send", "url", "readyState", "bufferedAmount", "onopen", "onerror", "onclose", "extensions", "protocol", "onmessage", "binaryType", "CONNECTING", "OPEN", "CLOSING", "CLOSED"};
inline constexpr const char* kBindingOrder957[] = {"getStats", "exportKeyingMaterial", "close", "createBidirectionalStream", "createUnidirectionalStream", "createSendGroup", "ready", "reliability", "congestionControl", "responseHeaders", "protocol", "closed", "draining", "datagrams", "incomingBidirectionalStreams", "incomingUnidirectionalStreams"};
inline constexpr const char* kBindingOrder958[] = {"readable", "writable"};
inline constexpr const char* kBindingOrder959[] = {"createWritable", "readable", "writable", "maxDatagramSize", "incomingMaxAge", "outgoingMaxAge", "incomingMaxBufferedDatagrams", "outgoingMaxBufferedDatagrams"};
inline constexpr const char* kBindingOrder960[] = {"sendGroup", "sendOrder"};
inline constexpr const char* kBindingOrder961[] = {"source", "streamErrorCode"};
inline constexpr const char* kBindingOrder962[] = {"getStats"};
inline constexpr const char* kBindingOrder963[] = {"getStats"};
inline constexpr const char* kBindingOrder964[] = {"getStats", "sendGroup", "sendOrder"};
inline constexpr const char* kBindingOrder965[] = {"wheelDeltaX", "wheelDeltaY", "wheelDelta", "deltaX", "deltaY", "deltaZ", "deltaMode", "DOM_DELTA_PIXEL", "DOM_DELTA_LINE", "DOM_DELTA_PAGE"};
inline constexpr const char* kBindingOrder966[] = {"close", "stop", "focus", "blur", "open", "alert", "confirm", "prompt", "print", "printPreview", "postMessage", "captureEvents", "releaseEvents", "getSelection", "getComputedStyle", "matchMedia", "moveTo", "moveBy", "resizeTo", "resizeBy", "scroll", "scrollTo", "scrollBy", "getDefaultComputedStyle", "scrollByLines", "scrollByPages", "updateCommands", "find", "dump", "setResizable", "getAttention", "getAttentionWithCycleCount", "setCursor", "maximize", "minimize", "restore", "getWorkspaceID", "moveToWorkspace", "notifyDefaultButtonLoaded", "getGroupMessageManager", "promiseDocumentFlushed", "requestIdleCallback", "cancelIdleCallback", "getWebExposedLocales", "requestAnimationFrame", "cancelAnimationFrame", "reportError", "btoa", "atob", "setTimeout", "clearTimeout", "setInterval", "clearInterval", "queueMicrotask", "createImageBitmap", "structuredClone", "fetch", "isRFPTargetActive", "self", "name", "history", "navigation", "customElements", "locationbar", "menubar", "personalbar", "scrollbars", "statusbar", "toolbar", "status", "closed", "event", "frames", "length", "opener", "parent", "frameElement", "navigator", "clientInformation", "external", "screen", "innerWidth", "innerHeight", "scrollX", "pageXOffset", "scrollY", "pageYOffset", "screenLeft", "screenTop", "screenX", "screenY", "outerWidth", "outerHeight", "performance", "mozInnerScreenX", "mozInnerScreenY", "devicePixelRatio", "scrollMaxX", "scrollMaxY", "fullScreen", "ondevicemotion", "ondeviceorientation", "ondeviceorientationabsolute", "onuserproximity", "ondevicelight", "InstallTrigger", "windowState", "isFullyOccluded", "isCloaked", "browserDOMWindow", "messageManager", "isChromeWindow", "Glean", "GleanPings", "onvrdisplayconnect", "onvrdisplaydisconnect", "onvrdisplayactivate", "onvrdisplaydeactivate", "onvrdisplaypresentchange", "intlUtils", "visualViewport", "cookieStore", "originAgentCluster", "documentPictureInPicture", "crypto", "onabort", "onblur", "onfocus", "oncancel", "onauxclick", "onbeforeinput", "onbeforematch", "onbeforetoggle", "oncanplay", "oncanplaythrough", "onchange", "onclick", "onclose", "oncontentvisibilityautostatechange", "oncontextlost", "oncontextmenu", "oncommand", "oncontextrestored", "oncopy", "oncuechange", "oncut", "ondblclick", "ondrag", "ondragend", "ondragenter", "ondragexit", "ondragleave", "ondragover", "ondragstart", "ondrop", "ondurationchange", "onemptied", "onended", "onformdata", "oninput", "oninvalid", "onkeydown", "onkeypress", "onkeyup", "onload", "onloadeddata", "onloadedmetadata", "onloadstart", "onmousedown", "onmouseenter", "onmouseleave", "onmousemove", "onmouseout", "onmouseover", "onmouseup", "onwheel", "onpaste", "onpause", "onplay", "onplaying", "onprogress", "onratechange", "onreset", "onresize", "onscroll", "onscrollend", "onsecuritypolicyviolation", "onseeked", "onseeking", "onselect", "onslotchange", "onstalled", "onsubmit", "onsuspend", "ontimeupdate", "onvolumechange", "onwaiting", "onselectstart", "onselectionchange", "ontoggle", "onpointercancel", "onpointerdown", "onpointerup", "onpointermove", "onpointerout", "onpointerover", "onpointerenter", "onpointerleave", "onpointerrawupdate", "ongotpointercapture", "onlostpointercapture", "onmozfullscreenchange", "onmozfullscreenerror", "onanimationcancel", "onanimationend", "onanimationiteration", "onanimationstart", "ontransitioncancel", "ontransitionend", "ontransitionrun", "ontransitionstart", "onwebkitanimationend", "onwebkitanimationiteration", "onwebkitanimationstart", "onwebkittransitionend", "onerror", "pushManager", "speechSynthesis", "ontouchstart", "ontouchend", "ontouchmove", "ontouchcancel", "onafterprint", "onbeforeprint", "onbeforeunload", "onhashchange", "onlanguagechange", "onmessage", "onmessageerror", "onoffline", "ononline", "onpagehide", "onpagereveal", "onpageshow", "onpageswap", "onpopstate", "onrejectionhandled", "onstorage", "onunhandledrejection", "onunload", "ongamepadconnected", "ongamepaddisconnected", "localStorage", "origin", "crossOriginIsolated", "isSecureContext", "indexedDB", "caches", "scheduler", "trustedTypes", "sessionStorage", "STATE_MAXIMIZED", "STATE_MINIMIZED", "STATE_NORMAL", "STATE_FULLSCREEN"};
inline constexpr const char* kBindingOrder967[] = {"focus", "navigate", "visibilityState", "focused"};
inline constexpr const char* kBindingOrder968[] = {"processCloseRequest", "browsingContext", "windowGlobalChild", "innerWindowId", "parentWindowContext", "topWindowContext", "isInProcess", "isInBFCache", "hasBeforeUnload", "isLocalIP", "hasActiveCloseWatcher", "shouldResistFingerprinting", "usingStorageAccess", "overriddenFingerprintingSettings", "allowJavascript"};
inline constexpr const char* kBindingOrder969[] = {"findBrowsingContextWithName", "getActor", "getExistingActor", "isClosed", "isInProcess", "browsingContext", "windowContext", "contentWindow", "isCurrentGlobal", "innerWindowId", "outerWindowId", "contentParentId", "isProcessRoot", "sameOriginWithTop", "parentActor"};
inline constexpr const char* kBindingOrder970[] = {"permitUnload", "requestDocumentLanguageMetadata", "getActor", "getExistingActor", "drawSnapshot", "hasActivePeerConnections", "updateFullscreenKeyboardLockStatus", "isClosed", "isCurrentGlobal", "isActiveInTab", "outerWindowId", "contentParentId", "osPid", "remoteType", "isProcessRoot", "isInitialDocument", "isUncommittedInitialDocument", "rootFrameLoader", "childActor", "documentPrincipal", "documentPartitionedPrincipal", "documentStoragePrincipal", "contentBlockingAllowListPrincipal", "documentURI", "documentTitle", "cookieJarSettings", "documentChannel", "failedChannel", "fullscreen", "contentBlockingEvents", "contentBlockingLog", "domProcess"};
inline constexpr const char* kBindingOrder971[] = {"window"};
inline constexpr const char* kBindingOrder972[] = {"terminate", "postMessage", "onmessage", "onmessageerror", "onerror"};
inline constexpr const char* kBindingOrder973[] = {"createSandbox", "loadSubScript", "enterEventLoop", "leaveEventLoop", "postMessage", "setImmediate", "reportError", "retrieveConsoleEvents", "setConsoleEventHandler", "clearConsoleEvents", "btoa", "atob", "dump", "global", "onmessage", "onmessageerror"};
inline constexpr const char* kBindingOrder974[] = {"importScripts", "dump", "getJSTestingFunctions", "reportError", "btoa", "atob", "setTimeout", "clearTimeout", "setInterval", "clearInterval", "queueMicrotask", "createImageBitmap", "structuredClone", "fetch", "isRFPTargetActive", "self", "location", "navigator", "onerror", "onlanguagechange", "onoffline", "ononline", "onrejectionhandled", "onunhandledrejection", "performance", "fonts", "crypto", "origin", "crossOriginIsolated", "isSecureContext", "indexedDB", "caches", "scheduler", "trustedTypes"};
inline constexpr const char* kBindingOrder975[] = {"toString", "href", "origin", "protocol", "host", "hostname", "port", "pathname", "search", "hash"};
inline constexpr const char* kBindingOrder976[] = {"taintEnabled", "connection", "mediaCapabilities", "permissions", "serviceWorker", "serial", "globalPrivacyControl", "hardwareConcurrency", "gpu", "appCodeName", "appName", "appVersion", "platform", "userAgent", "product", "language", "languages", "locks", "onLine", "storage"};
inline constexpr const char* kBindingOrder977[] = {"addModule"};
inline constexpr const char* kBindingOrder978[] = {"dump"};
inline constexpr const char* kBindingOrder979[] = {"abort", "close", "getWriter", "locked"};
inline constexpr const char* kBindingOrder980[] = {"error", "signal"};
inline constexpr const char* kBindingOrder981[] = {"abort", "close", "releaseLock", "write", "closed", "desiredSize", "ready"};
inline constexpr const char* kBindingOrder982[] = {"open", "setRequestHeader", "send", "abort", "getResponseHeader", "getAllResponseHeaders", "overrideMimeType", "onreadystatechange", "readyState", "timeout", "withCredentials", "upload", "responseURL", "status", "statusText", "responseType", "response", "responseText", "responseXML", "mozAnon", "mozSystem", "UNSENT", "OPENED", "HEADERS_RECEIVED", "LOADING", "DONE"};
inline constexpr const char* kBindingOrder983[] = {"onloadstart", "onprogress", "onabort", "onerror", "onload", "ontimeout", "onloadend"};
inline constexpr const char* kBindingOrder984[] = {"serializeToString"};
inline constexpr const char* kBindingOrder985[] = {"createExpression", "createNSResolver", "evaluate"};
inline constexpr const char* kBindingOrder986[] = {"evaluate"};
inline constexpr const char* kBindingOrder987[] = {"iterateNext", "snapshotItem", "resultType", "numberValue", "stringValue", "booleanValue", "singleNodeValue", "invalidIteratorState", "snapshotLength", "ANY_TYPE", "NUMBER_TYPE", "STRING_TYPE", "BOOLEAN_TYPE", "UNORDERED_NODE_ITERATOR_TYPE", "ORDERED_NODE_ITERATOR_TYPE", "UNORDERED_NODE_SNAPSHOT_TYPE", "ORDERED_NODE_SNAPSHOT_TYPE", "ANY_UNORDERED_NODE_TYPE", "FIRST_ORDERED_NODE_TYPE"};
inline constexpr const char* kBindingOrder988[] = {"boundsGeometry"};
inline constexpr const char* kBindingOrder989[] = {"getViewerPose", "getPose", "session"};
inline constexpr const char* kBindingOrder990[] = {"handedness", "targetRayMode", "targetRaySpace", "gripSpace", "profiles", "gamepad"};
inline constexpr const char* kBindingOrder991[] = {"@@iterator", "keys", "values", "entries", "forEach", "length"};
inline constexpr const char* kBindingOrder992[] = {"frame", "inputSource"};
inline constexpr const char* kBindingOrder993[] = {"session", "added", "removed"};
inline constexpr const char* kBindingOrder994[] = {"transform", "emulatedPosition"};
inline constexpr const char* kBindingOrder995[] = {"getOffsetReferenceSpace", "onreset"};
inline constexpr const char* kBindingOrder996[] = {"referenceSpace", "transform"};
inline constexpr const char* kBindingOrder997[] = {"depthNear", "depthFar", "inlineVerticalFieldOfView", "baseLayer"};
inline constexpr const char* kBindingOrder998[] = {"position", "orientation", "matrix", "inverse"};
inline constexpr const char* kBindingOrder999[] = {"updateRenderState", "requestReferenceSpace", "updateTargetFrameRate", "requestAnimationFrame", "cancelAnimationFrame", "end", "visibilityState", "renderState", "inputSources", "frameRate", "supportedFrameRates", "onend", "oninputsourceschange", "onselect", "onselectstart", "onselectend", "onsqueeze", "onsqueezestart", "onsqueezeend", "onvisibilitychange"};
inline constexpr const char* kBindingOrder1000[] = {"session"};
inline constexpr const char* kBindingOrder1001[] = {"isSessionSupported", "requestSession", "ondevicechange"};
inline constexpr const char* kBindingOrder1002[] = {"eye", "projectionMatrix", "transform"};
inline constexpr const char* kBindingOrder1003[] = {"views"};
inline constexpr const char* kBindingOrder1004[] = {"x", "y", "width", "height"};
inline constexpr const char* kBindingOrder1005[] = {"getViewport", "antialias", "ignoreDepthValues", "framebuffer", "framebufferWidth", "framebufferHeight"};
inline constexpr const char* kBindingOrder1006[] = {"importStylesheet", "transformToFragment", "transformToDocument", "setParameter", "getParameter", "removeParameter", "clearParameters", "reset"};
inline constexpr const char* kBindingOrder1007[] = {"initCommandEvent", "ctrlKey", "shiftKey", "altKey", "metaKey", "button", "inputSource", "sourceEvent"};
inline constexpr const char* kBindingOrder1008[] = {"click", "doCommand", "hasMenu", "openMenu", "focus", "blur", "hidden", "collapsed", "observes", "menu", "contextMenu", "tooltip", "tooltipText", "src", "controllers", "style", "attributeStyleMap", "offsetParent", "offsetTop", "offsetLeft", "offsetWidth", "offsetHeight", "onabort", "onblur", "onfocus", "oncancel", "onauxclick", "onbeforeinput", "onbeforematch", "onbeforetoggle", "oncanplay", "oncanplaythrough", "onchange", "onclick", "onclose", "oncontentvisibilityautostatechange", "oncontextlost", "oncontextmenu", "oncommand", "oncontextrestored", "oncopy", "oncuechange", "oncut", "ondblclick", "ondrag", "ondragend", "ondragenter", "ondragexit", "ondragleave", "ondragover", "ondragstart", "ondrop", "ondurationchange", "onemptied", "onended", "onformdata", "oninput", "oninvalid", "onkeydown", "onkeypress", "onkeyup", "onload", "onloadeddata", "onloadedmetadata", "onloadstart", "onmousedown", "onmouseenter", "onmouseleave", "onmousemove", "onmouseout", "onmouseover", "onmouseup", "onwheel", "onpaste", "onpause", "onplay", "onplaying", "onprogress", "onratechange", "onreset", "onresize", "onscroll", "onscrollend", "onsecuritypolicyviolation", "onseeked", "onseeking", "onselect", "onslotchange", "onstalled", "onsubmit", "onsuspend", "ontimeupdate", "onvolumechange", "onwaiting", "onselectstart", "onselectionchange", "ontoggle", "onpointercancel", "onpointerdown", "onpointerup", "onpointermove", "onpointerout", "onpointerover", "onpointerenter", "onpointerleave", "onpointerrawupdate", "ongotpointercapture", "onlostpointercapture", "onmozfullscreenchange", "onmozfullscreenerror", "onanimationcancel", "onanimationend", "onanimationiteration", "onanimationstart", "ontransitioncancel", "ontransitionend", "ontransitionrun", "ontransitionstart", "onwebkitanimationend", "onwebkitanimationiteration", "onwebkitanimationstart", "onwebkittransitionend", "dataset", "autofocus", "tabIndex", "onerror", "ontouchstart", "ontouchend", "ontouchmove", "ontouchcancel"};
inline constexpr const char* kBindingOrder1009[] = {"docShell", "webNavigation", "contentWindow", "contentDocument", "browserId", "openWindowInfo"};
inline constexpr const char* kBindingOrder1010[] = {"handleKeyPress", "activeChild", "openedWithKey"};
inline constexpr const char* kBindingOrder1011[] = {"openPopup", "openPopupAtScreen", "openPopupAtScreenRect", "hidePopup", "activateItem", "getMenuItems", "getOuterScreenRect", "moveTo", "moveToAnchor", "sizeTo", "setConstraintRect", "label", "position", "state", "triggerNode", "anchorNode", "isWaylandDragSource", "isWaylandPopup", "isNativeMenu"};
inline constexpr const char* kBindingOrder1012[] = {"disabled", "value", "accessKey"};
inline constexpr const char* kBindingOrder1013[] = {"getFirstVisibleRow", "getLastVisibleRow", "getPageLength", "ensureRowIsVisible", "ensureCellIsVisible", "scrollToRow", "scrollByLines", "scrollByPages", "invalidate", "invalidateColumn", "invalidateRow", "invalidateCell", "invalidateRange", "getRowAt", "getCellAt", "getCoordsForCellItem", "isCellCropped", "rowCountChanged", "beginUpdateBatch", "endUpdateBatch", "clearStyleAndImageCaches", "removeImageCacheEntry", "columns", "view", "focused", "treeBody", "rowHeight", "rowWidth", "scrollbarPosition", "scrollbarMaxPosition"};
inline constexpr BindingOrderEntry kBindingOrders[] = {
  {"ANGLE_instanced_arrays", kBindingOrder0, 4, 3},
  {"APZHitResultFlags", kBindingOrder1, 13, 0},
  {"AbortController", kBindingOrder2, 2, 2},
  {"AbortSignal", kBindingOrder3, 4, 4},
  {"AbstractRange", kBindingOrder4, 5, 5},
  {"AccessibleNode", kBindingOrder5, 49, 49},
  {"Addon", kBindingOrder6, 10, 10},
  {"AddonEvent", kBindingOrder7, 1, 1},
  {"AddonInstall", kBindingOrder8, 6, 6},
  {"AddonManager", kBindingOrder9, 3, 3},
  {"AnalyserNode", kBindingOrder10, 9, 9},
  {"Animation", kBindingOrder11, 25, 25},
  {"AnimationEffect", kBindingOrder12, 3, 3},
  {"AnimationEvent", kBindingOrder13, 4, 4},
  {"AnimationPlaybackEvent", kBindingOrder14, 2, 2},
  {"AnimationTimeline", kBindingOrder15, 2, 2},
  {"AnonymousContent", kBindingOrder16, 1, 1},
  {"Attr", kBindingOrder17, 7, 7},
  {"AudioBuffer", kBindingOrder18, 7, 7},
  {"AudioBufferSourceNode", kBindingOrder19, 7, 7},
  {"AudioContext", kBindingOrder20, 9, 9},
  {"AudioData", kBindingOrder21, 10, 10},
  {"AudioDecoder", kBindingOrder22, 8, 8},
  {"AudioDestinationNode", kBindingOrder23, 1, 1},
  {"AudioEncoder", kBindingOrder24, 9, 9},
  {"AudioListener", kBindingOrder25, 2, 2},
  {"AudioNode", kBindingOrder26, 8, 8},
  {"AudioParam", kBindingOrder27, 10, 10},
  {"AudioParamMap", kBindingOrder28, 7, 7},
  {"AudioProcessingEvent", kBindingOrder29, 3, 3},
  {"AudioScheduledSourceNode", kBindingOrder30, 3, 3},
  {"AudioSession", kBindingOrder31, 3, 3},
  {"AudioTrack", kBindingOrder32, 5, 5},
  {"AudioTrackList", kBindingOrder33, 6, 6},
  {"AudioWorklet", kBindingOrder34, 1, 1},
  {"AudioWorkletGlobalScope", kBindingOrder35, 5, 5},
  {"AudioWorkletNode", kBindingOrder36, 3, 3},
  {"AudioWorkletProcessor", kBindingOrder37, 1, 1},
  {"AuthenticatorAssertionResponse", kBindingOrder38, 3, 3},
  {"AuthenticatorAttestationResponse", kBindingOrder39, 5, 5},
  {"AuthenticatorResponse", kBindingOrder40, 1, 1},
  {"BarProp", kBindingOrder41, 1, 1},
  {"BaseAudioContext", kBindingOrder42, 27, 27},
  {"BatteryManager", kBindingOrder43, 8, 8},
  {"BeforeUnloadEvent", kBindingOrder44, 1, 1},
  {"BiquadFilterNode", kBindingOrder45, 6, 6},
  {"Blob", kBindingOrder46, 7, 7},
  {"BlobEvent", kBindingOrder47, 1, 1},
  {"BroadcastChannel", kBindingOrder48, 5, 5},
  {"BrowsingContext", kBindingOrder49, 72, 72},
  {"BrowsingContextGroup", kBindingOrder50, 2, 2},
  {"ByteLengthQueuingStrategy", kBindingOrder51, 2, 2},
  {"CSPViolationReportBody", kBindingOrder52, 12, 12},
  {"CSSAnimation", kBindingOrder53, 1, 1},
  {"CSSColor", kBindingOrder54, 3, 3},
  {"CSSConditionRule", kBindingOrder55, 1, 1},
  {"CSSContainerRule", kBindingOrder56, 3, 3},
  {"CSSCounterStyleRule", kBindingOrder57, 11, 11},
  {"CSSCustomMediaRule", kBindingOrder58, 2, 2},
  {"CSSCustomPropertyRegisteredEvent", kBindingOrder59, 1, 1},
  {"CSSFontFaceDescriptors", kBindingOrder60, 30, 30},
  {"CSSFontFaceRule", kBindingOrder61, 1, 1},
  {"CSSFontFeatureValuesRule", kBindingOrder62, 2, 2},
  {"CSSFontPaletteValuesRule", kBindingOrder63, 4, 4},
  {"CSSGroupingRule", kBindingOrder64, 3, 3},
  {"CSSHSL", kBindingOrder65, 4, 4},
  {"CSSHWB", kBindingOrder66, 4, 4},
  {"CSSImportRule", kBindingOrder67, 5, 5},
  {"CSSKeyframeRule", kBindingOrder68, 2, 2},
  {"CSSKeyframesRule", kBindingOrder69, 7, 7},
  {"CSSKeywordValue", kBindingOrder70, 1, 1},
  {"CSSLCH", kBindingOrder71, 4, 4},
  {"CSSLab", kBindingOrder72, 4, 4},
  {"CSSLayerBlockRule", kBindingOrder73, 1, 1},
  {"CSSLayerStatementRule", kBindingOrder74, 1, 1},
  {"CSSMarginRule", kBindingOrder75, 2, 2},
  {"CSSMathClamp", kBindingOrder76, 3, 3},
  {"CSSMathInvert", kBindingOrder77, 1, 1},
  {"CSSMathMax", kBindingOrder78, 1, 1},
  {"CSSMathMin", kBindingOrder79, 1, 1},
  {"CSSMathNegate", kBindingOrder80, 1, 1},
  {"CSSMathProduct", kBindingOrder81, 1, 1},
  {"CSSMathSum", kBindingOrder82, 1, 1},
  {"CSSMathValue", kBindingOrder83, 1, 1},
  {"CSSMatrixComponent", kBindingOrder84, 1, 1},
  {"CSSMediaRule", kBindingOrder85, 1, 1},
  {"CSSNamespaceRule", kBindingOrder86, 2, 2},
  {"CSSNestedDeclarations", kBindingOrder87, 1, 1},
  {"CSSNumericArray", kBindingOrder88, 6, 6},
  {"CSSNumericValue", kBindingOrder89, 10, 10},
  {"CSSOKLCH", kBindingOrder90, 4, 4},
  {"CSSOKLab", kBindingOrder91, 4, 4},
  {"CSSPageDescriptors", kBindingOrder92, 21, 21},
  {"CSSPageRule", kBindingOrder93, 2, 2},
  {"CSSPerspective", kBindingOrder94, 1, 1},
  {"CSSPositionTryDescriptors", kBindingOrder95, 82, 82},
  {"CSSPositionTryRule", kBindingOrder96, 2, 2},
  {"CSSPropertyRule", kBindingOrder97, 4, 4},
  {"CSSPseudoElement", kBindingOrder98, 2, 2},
  {"CSSRGB", kBindingOrder99, 4, 4},
  {"CSSRotate", kBindingOrder100, 4, 4},
  {"CSSRule", kBindingOrder101, 16, 4},
  {"CSSRuleList", kBindingOrder102, 3, 3},
  {"CSSScale", kBindingOrder103, 3, 3},
  {"CSSScopeRule", kBindingOrder104, 2, 2},
  {"CSSSkew", kBindingOrder105, 2, 2},
  {"CSSSkewX", kBindingOrder106, 1, 1},
  {"CSSSkewY", kBindingOrder107, 1, 1},
  {"CSSStyleDeclaration", kBindingOrder108, 9, 9},
  {"CSSStyleProperties", kBindingOrder109, 1269, 1269},
  {"CSSStyleRule", kBindingOrder110, 3, 3},
  {"CSSStyleSheet", kBindingOrder111, 9, 9},
  {"CSSStyleValue", kBindingOrder112, 1, 1},
  {"CSSTransformComponent", kBindingOrder113, 3, 3},
  {"CSSTransformValue", kBindingOrder114, 8, 8},
  {"CSSTransition", kBindingOrder115, 1, 1},
  {"CSSTranslate", kBindingOrder116, 3, 3},
  {"CSSUnitValue", kBindingOrder117, 2, 2},
  {"CSSUnparsedValue", kBindingOrder118, 6, 6},
  {"CSSVariableReferenceValue", kBindingOrder119, 2, 2},
  {"CSSViewTransitionRule", kBindingOrder120, 2, 2},
  {"Cache", kBindingOrder121, 7, 7},
  {"CacheStorage", kBindingOrder122, 5, 5},
  {"CallbackDebuggerNotification", kBindingOrder123, 1, 1},
  {"CanonicalBrowsingContext", kBindingOrder124, 36, 36},
  {"CanvasCaptureMediaStream", kBindingOrder125, 2, 2},
  {"CanvasGradient", kBindingOrder126, 1, 1},
  {"CanvasPattern", kBindingOrder127, 1, 1},
  {"CanvasRenderingContext2D", kBindingOrder128, 79, 74},
  {"CaretPosition", kBindingOrder129, 3, 3},
  {"CaretStateChangedEvent", kBindingOrder130, 10, 10},
  {"ChannelWrapper", kBindingOrder131, 46, 46},
  {"CharacterBoundsUpdateEvent", kBindingOrder132, 2, 2},
  {"CharacterData", kBindingOrder133, 13, 13},
  {"CheckerboardReportService", kBindingOrder134, 4, 4},
  {"ChildSHistory", kBindingOrder135, 5, 5},
  {"ChromeMessageBroadcaster", kBindingOrder136, 3, 3},
  {"ChromeMessageSender", kBindingOrder137, 3, 3},
  {"ChromeNodeList", kBindingOrder138, 3, 3},
  {"Client", kBindingOrder139, 5, 5},
  {"Clients", kBindingOrder140, 4, 4},
  {"Clipboard", kBindingOrder141, 4, 4},
  {"ClipboardEvent", kBindingOrder142, 1, 1},
  {"ClipboardItem", kBindingOrder143, 3, 3},
  {"CloseEvent", kBindingOrder144, 3, 3},
  {"CloseWatcher", kBindingOrder145, 5, 5},
  {"CollectorLogAnalyzer", kBindingOrder146, 9, 7},
  {"CollectorNodeFlags", kBindingOrder147, 7, 0},
  {"CommandEvent", kBindingOrder148, 2, 2},
  {"CompositionEvent", kBindingOrder149, 2, 2},
  {"CompressionStream", kBindingOrder150, 2, 2},
  {"ConnectionAllowlistViolationReportBody", kBindingOrder151, 5, 5},
  {"ConsoleInstance", kBindingOrder152, 24, 24},
  {"ConstantSourceNode", kBindingOrder153, 1, 1},
  {"ContentFrameMessageManager", kBindingOrder154, 12, 12},
  {"ContentProcessMessageManager", kBindingOrder155, 11, 11},
  {"ContentVisibilityAutoStateChangeEvent", kBindingOrder156, 1, 1},
  {"ConvolverNode", kBindingOrder157, 2, 2},
  {"CookieChangeEvent", kBindingOrder158, 2, 2},
  {"CookieStore", kBindingOrder159, 5, 5},
  {"CookieStoreManager", kBindingOrder160, 3, 3},
  {"CountQueuingStrategy", kBindingOrder161, 2, 2},
  {"CreateOfferRequest", kBindingOrder162, 4, 4},
  {"Credential", kBindingOrder163, 2, 2},
  {"CredentialsContainer", kBindingOrder164, 4, 4},
  {"Crypto", kBindingOrder165, 3, 3},
  {"CryptoKey", kBindingOrder166, 4, 4},
  {"CustomElementRegistry", kBindingOrder167, 6, 6},
  {"CustomEvent", kBindingOrder168, 2, 2},
  {"CustomStateSet", kBindingOrder169, 9, 9},
  {"DOMException", kBindingOrder170, 34, 9},
  {"DOMImplementation", kBindingOrder171, 4, 4},
  {"DOMLocalization", kBindingOrder172, 10, 10},
  {"DOMMatrix", kBindingOrder173, 34, 34},
  {"DOMMatrixReadOnly", kBindingOrder174, 42, 42},
  {"DOMParser", kBindingOrder175, 3, 3},
  {"DOMPoint", kBindingOrder176, 4, 4},
  {"DOMPointReadOnly", kBindingOrder177, 6, 6},
  {"DOMQuad", kBindingOrder178, 6, 6},
  {"DOMRect", kBindingOrder179, 4, 4},
  {"DOMRectList", kBindingOrder180, 3, 3},
  {"DOMRectReadOnly", kBindingOrder181, 9, 9},
  {"DOMStringList", kBindingOrder182, 4, 4},
  {"DOMTokenList", kBindingOrder183, 15, 15},
  {"DataTransfer", kBindingOrder184, 13, 13},
  {"DataTransferItem", kBindingOrder185, 5, 5},
  {"DataTransferItemList", kBindingOrder186, 5, 5},
  {"DebuggerNotification", kBindingOrder187, 2, 2},
  {"DebuggerNotificationObserver", kBindingOrder188, 4, 4},
  {"DecompressionStream", kBindingOrder189, 2, 2},
  {"DedicatedWorkerGlobalScope", kBindingOrder190, 8, 8},
  {"DelayNode", kBindingOrder191, 1, 1},
  {"DeprecationReportBody", kBindingOrder192, 7, 7},
  {"DeviceAcceleration", kBindingOrder193, 3, 3},
  {"DeviceLightEvent", kBindingOrder194, 1, 1},
  {"DeviceMotionEvent", kBindingOrder195, 5, 5},
  {"DeviceOrientationEvent", kBindingOrder196, 5, 5},
  {"DeviceRotationRate", kBindingOrder197, 3, 3},
  {"DigitalCredential", kBindingOrder198, 3, 3},
  {"Directory", kBindingOrder199, 4, 4},
  {"Document", kBindingOrder200, 254, 254},
  {"DocumentFragment", kBindingOrder201, 11, 11},
  {"DocumentL10n", kBindingOrder202, 2, 2},
  {"DocumentPictureInPicture", kBindingOrder203, 3, 3},
  {"DocumentPictureInPictureEvent", kBindingOrder204, 1, 1},
  {"DocumentType", kBindingOrder205, 7, 7},
  {"DominatorTree", kBindingOrder206, 4, 4},
  {"DragEvent", kBindingOrder207, 2, 2},
  {"DynamicsCompressorNode", kBindingOrder208, 6, 6},
  {"EXT_blend_minmax", kBindingOrder209, 2, 0},
  {"EXT_clip_control", kBindingOrder210, 7, 1},
  {"EXT_color_buffer_half_float", kBindingOrder211, 4, 0},
  {"EXT_depth_clamp", kBindingOrder212, 1, 0},
  {"EXT_disjoint_timer_query", kBindingOrder213, 15, 8},
  {"EXT_polygon_offset_clamp", kBindingOrder214, 2, 1},
  {"EXT_sRGB", kBindingOrder215, 4, 0},
  {"EXT_texture_compression_bptc", kBindingOrder216, 4, 0},
  {"EXT_texture_compression_rgtc", kBindingOrder217, 4, 0},
  {"EXT_texture_filter_anisotropic", kBindingOrder218, 2, 0},
  {"EXT_texture_mirror_clamp_to_edge", kBindingOrder219, 1, 0},
  {"EXT_texture_norm16", kBindingOrder220, 8, 0},
  {"EditContext", kBindingOrder221, 16, 16},
  {"Element", kBindingOrder222, 153, 153},
  {"ElementInternals", kBindingOrder223, 63, 63},
  {"EncodedAudioChunk", kBindingOrder224, 5, 5},
  {"EncodedVideoChunk", kBindingOrder225, 5, 5},
  {"ErrorEvent", kBindingOrder226, 5, 5},
  {"Event", kBindingOrder227, 27, 19},
  {"EventCallbackDebuggerNotification", kBindingOrder228, 2, 2},
  {"EventCounts", kBindingOrder229, 7, 7},
  {"EventSource", kBindingOrder230, 10, 7},
  {"EventTarget", kBindingOrder231, 3, 3},
  {"Exception", kBindingOrder232, 9, 9},
  {"ExtendableCookieChangeEvent", kBindingOrder233, 2, 2},
  {"ExtendableEvent", kBindingOrder234, 1, 1},
  {"ExtendableMessageEvent", kBindingOrder235, 5, 5},
  {"ExtensionAlarms", kBindingOrder236, 6, 6},
  {"ExtensionBrowser", kBindingOrder237, 9, 9},
  {"ExtensionBrowserSettings", kBindingOrder238, 20, 20},
  {"ExtensionBrowserSettingsColorManagement", kBindingOrder239, 3, 3},
  {"ExtensionDns", kBindingOrder240, 1, 1},
  {"ExtensionEventManager", kBindingOrder241, 4, 4},
  {"ExtensionGuardSet", kBindingOrder242, 4, 4},
  {"ExtensionMockAPI", kBindingOrder243, 8, 8},
  {"ExtensionPort", kBindingOrder244, 7, 7},
  {"ExtensionProxy", kBindingOrder245, 3, 3},
  {"ExtensionPublicSuffix", kBindingOrder246, 3, 3},
  {"ExtensionRuntime", kBindingOrder247, 20, 20},
  {"ExtensionScripting", kBindingOrder248, 7, 7},
  {"ExtensionSetting", kBindingOrder249, 4, 4},
  {"ExtensionTest", kBindingOrder250, 14, 14},
  {"External", kBindingOrder251, 2, 2},
  {"FetchEvent", kBindingOrder252, 6, 6},
  {"FetchObserver", kBindingOrder253, 4, 4},
  {"File", kBindingOrder254, 3, 3},
  {"FileList", kBindingOrder255, 3, 3},
  {"FileReader", kBindingOrder256, 17, 14},
  {"FileReaderSync", kBindingOrder257, 4, 4},
  {"FileSystem", kBindingOrder258, 2, 2},
  {"FileSystemDirectoryEntry", kBindingOrder259, 3, 3},
  {"FileSystemDirectoryHandle", kBindingOrder260, 7, 7},
  {"FileSystemDirectoryHandleAsyncIterator", kBindingOrder261, 1, 1},
  {"FileSystemDirectoryIterator", kBindingOrder262, 1, 1},
  {"FileSystemDirectoryReader", kBindingOrder263, 1, 1},
  {"FileSystemEntry", kBindingOrder264, 6, 6},
  {"FileSystemFileEntry", kBindingOrder265, 1, 1},
  {"FileSystemFileHandle", kBindingOrder266, 3, 3},
  {"FileSystemHandle", kBindingOrder267, 4, 4},
  {"FileSystemSyncAccessHandle", kBindingOrder268, 6, 6},
  {"FileSystemWritableFileStream", kBindingOrder269, 3, 3},
  {"Flex", kBindingOrder270, 3, 3},
  {"FlexItemValues", kBindingOrder271, 9, 9},
  {"FlexLineValues", kBindingOrder272, 6, 6},
  {"FluentBundle", kBindingOrder273, 5, 5},
  {"FluentBundleAsyncIterator", kBindingOrder274, 2, 2},
  {"FluentBundleIterator", kBindingOrder275, 2, 2},
  {"FluentResource", kBindingOrder276, 1, 1},
  {"FocusEvent", kBindingOrder277, 1, 1},
  {"FontFace", kBindingOrder278, 17, 17},
  {"FontFaceSet", kBindingOrder279, 15, 15},
  {"FontFaceSetIterator", kBindingOrder280, 1, 1},
  {"FontFaceSetLoadEvent", kBindingOrder281, 1, 1},
  {"FormData", kBindingOrder282, 10, 10},
  {"FormDataEvent", kBindingOrder283, 1, 1},
  {"FormDataIterator", kBindingOrder284, 1, 1},
  {"FrameCrashedEvent", kBindingOrder285, 3, 3},
  {"FrameLoader", kBindingOrder286, 19, 19},
  {"GPU", kBindingOrder287, 3, 3},
  {"GPUAdapter", kBindingOrder288, 5, 5},
  {"GPUAdapterInfo", kBindingOrder289, 7, 7},
  {"GPUBindGroup", kBindingOrder290, 1, 1},
  {"GPUBindGroupLayout", kBindingOrder291, 1, 1},
  {"GPUBuffer", kBindingOrder292, 8, 8},
  {"GPUBufferUsage", kBindingOrder293, 10, 0},
  {"GPUCanvasContext", kBindingOrder294, 5, 5},
  {"GPUColorWrite", kBindingOrder295, 5, 0},
  {"GPUCommandBuffer", kBindingOrder296, 1, 1},
  {"GPUCommandEncoder", kBindingOrder297, 13, 13},
  {"GPUCompilationInfo", kBindingOrder298, 1, 1},
  {"GPUCompilationMessage", kBindingOrder299, 6, 6},
  {"GPUComputePassEncoder", kBindingOrder300, 9, 9},
  {"GPUComputePipeline", kBindingOrder301, 2, 2},
  {"GPUDevice", kBindingOrder302, 25, 25},
  {"GPUDeviceLostInfo", kBindingOrder303, 2, 2},
  {"GPUError", kBindingOrder304, 1, 1},
  {"GPUExternalTexture", kBindingOrder305, 1, 1},
  {"GPUMapMode", kBindingOrder306, 2, 0},
  {"GPUPipelineError", kBindingOrder307, 1, 1},
  {"GPUPipelineLayout", kBindingOrder308, 1, 1},
  {"GPUQuerySet", kBindingOrder309, 4, 4},
  {"GPUQueue", kBindingOrder310, 6, 6},
  {"GPURenderBundle", kBindingOrder311, 1, 1},
  {"GPURenderBundleEncoder", kBindingOrder312, 13, 13},
  {"GPURenderPassEncoder", kBindingOrder313, 20, 20},
  {"GPURenderPipeline", kBindingOrder314, 2, 2},
  {"GPUSampler", kBindingOrder315, 1, 1},
  {"GPUShaderModule", kBindingOrder316, 2, 2},
  {"GPUShaderStage", kBindingOrder317, 3, 0},
  {"GPUSupportedFeatures", kBindingOrder318, 6, 6},
  {"GPUSupportedLimits", kBindingOrder319, 35, 35},
  {"GPUTexture", kBindingOrder320, 11, 11},
  {"GPUTextureUsage", kBindingOrder321, 6, 0},
  {"GPUTextureView", kBindingOrder322, 1, 1},
  {"GPUUncapturedErrorEvent", kBindingOrder323, 1, 1},
  {"GainNode", kBindingOrder324, 1, 1},
  {"Gamepad", kBindingOrder325, 12, 12},
  {"GamepadAxisMoveEvent", kBindingOrder326, 2, 2},
  {"GamepadButton", kBindingOrder327, 3, 3},
  {"GamepadButtonEvent", kBindingOrder328, 1, 1},
  {"GamepadEvent", kBindingOrder329, 1, 1},
  {"GamepadHapticActuator", kBindingOrder330, 2, 2},
  {"GamepadLightIndicator", kBindingOrder331, 2, 2},
  {"GamepadPose", kBindingOrder332, 8, 8},
  {"GamepadServiceTest", kBindingOrder333, 12, 12},
  {"GamepadTouch", kBindingOrder334, 4, 4},
  {"Geolocation", kBindingOrder335, 3, 3},
  {"GeolocationCoordinates", kBindingOrder336, 8, 8},
  {"GeolocationPosition", kBindingOrder337, 3, 3},
  {"GeolocationPositionError", kBindingOrder338, 5, 2},
  {"GetUserMediaRequest", kBindingOrder339, 11, 11},
  {"GleanBoolean", kBindingOrder340, 1, 1},
  {"GleanCounter", kBindingOrder341, 1, 1},
  {"GleanCustomDistribution", kBindingOrder342, 2, 2},
  {"GleanDatetime", kBindingOrder343, 1, 1},
  {"GleanDenominator", kBindingOrder344, 1, 1},
  {"GleanDualLabeledCounter", kBindingOrder345, 1, 1},
  {"GleanEvent", kBindingOrder346, 1, 1},
  {"GleanMemoryDistribution", kBindingOrder347, 1, 1},
  {"GleanNumerator", kBindingOrder348, 1, 1},
  {"GleanObject", kBindingOrder349, 1, 1},
  {"GleanQuantity", kBindingOrder350, 1, 1},
  {"GleanRate", kBindingOrder351, 2, 2},
  {"GleanString", kBindingOrder352, 1, 1},
  {"GleanStringList", kBindingOrder353, 2, 2},
  {"GleanText", kBindingOrder354, 1, 1},
  {"GleanTimespan", kBindingOrder355, 4, 4},
  {"GleanTimingDistribution", kBindingOrder356, 5, 5},
  {"GleanUrl", kBindingOrder357, 1, 1},
  {"GleanUuid", kBindingOrder358, 2, 2},
  {"Grid", kBindingOrder359, 3, 3},
  {"GridArea", kBindingOrder360, 6, 6},
  {"GridDimension", kBindingOrder361, 2, 2},
  {"GridLine", kBindingOrder362, 6, 6},
  {"GridLines", kBindingOrder363, 3, 3},
  {"GridTrack", kBindingOrder364, 4, 4},
  {"GridTracks", kBindingOrder365, 3, 3},
  {"HTMLAllCollection", kBindingOrder366, 4, 4},
  {"HTMLAnchorElement", kBindingOrder367, 26, 26},
  {"HTMLAreaElement", kBindingOrder368, 24, 24},
  {"HTMLBRElement", kBindingOrder369, 1, 1},
  {"HTMLBaseElement", kBindingOrder370, 2, 2},
  {"HTMLBodyElement", kBindingOrder371, 26, 26},
  {"HTMLButtonElement", kBindingOrder372, 21, 21},
  {"HTMLCanvasElement", kBindingOrder373, 9, 9},
  {"HTMLCollection", kBindingOrder374, 4, 4},
  {"HTMLDListElement", kBindingOrder375, 1, 1},
  {"HTMLDataElement", kBindingOrder376, 1, 1},
  {"HTMLDataListElement", kBindingOrder377, 1, 1},
  {"HTMLDetailsElement", kBindingOrder378, 2, 2},
  {"HTMLDialogElement", kBindingOrder379, 7, 7},
  {"HTMLDirectoryElement", kBindingOrder380, 1, 1},
  {"HTMLDivElement", kBindingOrder381, 1, 1},
  {"HTMLElement", kBindingOrder382, 147, 147},
  {"HTMLEmbedElement", kBindingOrder383, 7, 7},
  {"HTMLFieldSetElement", kBindingOrder384, 11, 11},
  {"HTMLFontElement", kBindingOrder385, 3, 3},
  {"HTMLFormControlsCollection", kBindingOrder386, 2, 2},
  {"HTMLFormElement", kBindingOrder387, 19, 19},
  {"HTMLFrameElement", kBindingOrder388, 10, 10},
  {"HTMLFrameSetElement", kBindingOrder389, 22, 22},
  {"HTMLHRElement", kBindingOrder390, 5, 5},
  {"HTMLHeadingElement", kBindingOrder391, 1, 1},
  {"HTMLHtmlElement", kBindingOrder392, 1, 1},
  {"HTMLIFrameElement", kBindingOrder393, 21, 21},
  {"HTMLImageElement", kBindingOrder394, 27, 27},
  {"HTMLInputElement", kBindingOrder395, 72, 72},
  {"HTMLLIElement", kBindingOrder396, 2, 2},
  {"HTMLLabelElement", kBindingOrder397, 3, 3},
  {"HTMLLegendElement", kBindingOrder398, 2, 2},
  {"HTMLLinkElement", kBindingOrder399, 20, 20},
  {"HTMLMapElement", kBindingOrder400, 2, 2},
  {"HTMLMarqueeElement", kBindingOrder401, 13, 13},
  {"HTMLMediaElement", kBindingOrder402, 66, 57},
  {"HTMLMenuElement", kBindingOrder403, 1, 1},
  {"HTMLMetaElement", kBindingOrder404, 5, 5},
  {"HTMLMeterElement", kBindingOrder405, 7, 7},
  {"HTMLModElement", kBindingOrder406, 2, 2},
  {"HTMLOListElement", kBindingOrder407, 4, 4},
  {"HTMLObjectElement", kBindingOrder408, 26, 26},
  {"HTMLOptGroupElement", kBindingOrder409, 2, 2},
  {"HTMLOptionElement", kBindingOrder410, 8, 8},
  {"HTMLOptionsCollection", kBindingOrder411, 5, 5},
  {"HTMLOutputElement", kBindingOrder412, 13, 13},
  {"HTMLParagraphElement", kBindingOrder413, 1, 1},
  {"HTMLParamElement", kBindingOrder414, 4, 4},
  {"HTMLPreElement", kBindingOrder415, 1, 1},
  {"HTMLProgressElement", kBindingOrder416, 4, 4},
  {"HTMLQuoteElement", kBindingOrder417, 1, 1},
  {"HTMLScriptElement", kBindingOrder418, 16, 16},
  {"HTMLSelectElement", kBindingOrder419, 26, 26},
  {"HTMLSlotElement", kBindingOrder420, 4, 4},
  {"HTMLSourceElement", kBindingOrder421, 7, 7},
  {"HTMLStyleElement", kBindingOrder422, 5, 5},
  {"HTMLTableCaptionElement", kBindingOrder423, 1, 1},
  {"HTMLTableCellElement", kBindingOrder424, 15, 15},
  {"HTMLTableColElement", kBindingOrder425, 6, 6},
  {"HTMLTableElement", kBindingOrder426, 23, 23},
  {"HTMLTableRowElement", kBindingOrder427, 10, 10},
  {"HTMLTableSectionElement", kBindingOrder428, 7, 7},
  {"HTMLTemplateElement", kBindingOrder429, 9, 9},
  {"HTMLTextAreaElement", kBindingOrder430, 31, 31},
  {"HTMLTimeElement", kBindingOrder431, 1, 1},
  {"HTMLTitleElement", kBindingOrder432, 1, 1},
  {"HTMLTrackElement", kBindingOrder433, 11, 7},
  {"HTMLUListElement", kBindingOrder434, 2, 2},
  {"HTMLVideoElement", kBindingOrder435, 21, 21},
  {"HashChangeEvent", kBindingOrder436, 2, 2},
  {"Headers", kBindingOrder437, 10, 10},
  {"HeadersIterator", kBindingOrder438, 1, 1},
  {"HeapSnapshot", kBindingOrder439, 5, 5},
  {"Highlight", kBindingOrder440, 11, 11},
  {"HighlightIterator", kBindingOrder441, 1, 1},
  {"HighlightRegistry", kBindingOrder442, 11, 11},
  {"HighlightRegistryIterator", kBindingOrder443, 1, 1},
  {"History", kBindingOrder444, 8, 8},
  {"IDBCursor", kBindingOrder445, 10, 10},
  {"IDBCursorWithValue", kBindingOrder446, 1, 1},
  {"IDBDatabase", kBindingOrder447, 11, 11},
  {"IDBFactory", kBindingOrder448, 4, 4},
  {"IDBIndex", kBindingOrder449, 17, 17},
  {"IDBKeyRange", kBindingOrder450, 5, 5},
  {"IDBObjectStore", kBindingOrder451, 21, 21},
  {"IDBOpenDBRequest", kBindingOrder452, 2, 2},
  {"IDBRecord", kBindingOrder453, 3, 3},
  {"IDBRequest", kBindingOrder454, 7, 7},
  {"IDBTransaction", kBindingOrder455, 11, 11},
  {"IDBVersionChangeEvent", kBindingOrder456, 2, 2},
  {"IIRFilterNode", kBindingOrder457, 1, 1},
  {"IdentityCredential", kBindingOrder458, 3, 3},
  {"IdleDeadline", kBindingOrder459, 2, 2},
  {"ImageBitmap", kBindingOrder460, 3, 3},
  {"ImageBitmapRenderingContext", kBindingOrder461, 3, 3},
  {"ImageCapture", kBindingOrder462, 4, 4},
  {"ImageCaptureError", kBindingOrder463, 6, 2},
  {"ImageCaptureErrorEvent", kBindingOrder464, 1, 1},
  {"ImageData", kBindingOrder465, 4, 4},
  {"ImageDecoder", kBindingOrder466, 7, 7},
  {"ImageDocument", kBindingOrder467, 4, 4},
  {"ImageTrack", kBindingOrder468, 4, 4},
  {"ImageTrackList", kBindingOrder469, 5, 5},
  {"InferenceSession", kBindingOrder470, 6, 6},
  {"InputEvent", kBindingOrder471, 5, 5},
  {"InspectorCSSParser", kBindingOrder472, 3, 3},
  {"InspectorFontFace", kBindingOrder473, 36, 17},
  {"InspectorUtils", kBindingOrder474, 4, 0},
  {"IntegrityViolationReportBody", kBindingOrder475, 6, 6},
  {"IntersectionObserver", kBindingOrder476, 8, 8},
  {"IntersectionObserverEntry", kBindingOrder477, 7, 7},
  {"IntlUtils", kBindingOrder478, 2, 2},
  {"JSProcessActorChild", kBindingOrder479, 4, 4},
  {"JSProcessActorParent", kBindingOrder480, 4, 4},
  {"JSWindowActorChild", kBindingOrder481, 9, 9},
  {"JSWindowActorParent", kBindingOrder482, 6, 6},
  {"KeyEvent", kBindingOrder483, 186, 0},
  {"KeyboardEvent", kBindingOrder484, 203, 13},
  {"KeyframeEffect", kBindingOrder485, 6, 6},
  {"L10nFileSource", kBindingOrder486, 8, 8},
  {"L10nOverlays", kBindingOrder487, 6, 0},
  {"L10nRegistry", kBindingOrder488, 9, 9},
  {"LargestContentfulPaint", kBindingOrder489, 9, 9},
  {"LegacyMozTCPSocket", kBindingOrder490, 2, 2},
  {"LlamaRunner", kBindingOrder491, 3, 3},
  {"Localization", kBindingOrder492, 9, 9},
  {"Lock", kBindingOrder493, 2, 2},
  {"LockManager", kBindingOrder494, 2, 2},
  {"MIDIAccess", kBindingOrder495, 4, 4},
  {"MIDIConnectionEvent", kBindingOrder496, 1, 1},
  {"MIDIInput", kBindingOrder497, 1, 1},
  {"MIDIInputMap", kBindingOrder498, 7, 7},
  {"MIDIMessageEvent", kBindingOrder499, 1, 1},
  {"MIDIOutput", kBindingOrder500, 2, 2},
  {"MIDIOutputMap", kBindingOrder501, 7, 7},
  {"MIDIPort", kBindingOrder502, 10, 10},
  {"MLS", kBindingOrder503, 9, 9},
  {"MLSGroupView", kBindingOrder504, 17, 17},
  {"MOZ_debug", kBindingOrder505, 6, 1},
  {"MatchGlob", kBindingOrder506, 2, 2},
  {"MatchPattern", kBindingOrder507, 7, 7},
  {"MatchPatternSet", kBindingOrder508, 8, 8},
  {"MathMLAnchorElement", kBindingOrder509, 19, 19},
  {"MathMLElement", kBindingOrder510, 113, 113},
  {"MediaCapabilities", kBindingOrder511, 2, 2},
  {"MediaController", kBindingOrder512, 31, 31},
  {"MediaDeviceInfo", kBindingOrder513, 5, 5},
  {"MediaDevices", kBindingOrder514, 6, 6},
  {"MediaElementAudioSourceNode", kBindingOrder515, 1, 1},
  {"MediaEncryptedEvent", kBindingOrder516, 2, 2},
  {"MediaError", kBindingOrder517, 6, 2},
  {"MediaKeyError", kBindingOrder518, 1, 1},
  {"MediaKeyMessageEvent", kBindingOrder519, 2, 2},
  {"MediaKeySession", kBindingOrder520, 12, 12},
  {"MediaKeyStatusMap", kBindingOrder521, 7, 7},
  {"MediaKeyStatusMapIterator", kBindingOrder522, 1, 1},
  {"MediaKeySystemAccess", kBindingOrder523, 3, 3},
  {"MediaKeys", kBindingOrder524, 4, 4},
  {"MediaList", kBindingOrder525, 7, 7},
  {"MediaMetadata", kBindingOrder526, 4, 4},
  {"MediaQueryList", kBindingOrder527, 5, 5},
  {"MediaQueryListEvent", kBindingOrder528, 2, 2},
  {"MediaRecorder", kBindingOrder529, 16, 16},
  {"MediaRecorderErrorEvent", kBindingOrder530, 1, 1},
  {"MediaSession", kBindingOrder531, 4, 4},
  {"MediaSource", kBindingOrder532, 12, 12},
  {"MediaStream", kBindingOrder533, 11, 11},
  {"MediaStreamAudioDestinationNode", kBindingOrder534, 1, 1},
  {"MediaStreamAudioSourceNode", kBindingOrder535, 1, 1},
  {"MediaStreamError", kBindingOrder536, 3, 3},
  {"MediaStreamEvent", kBindingOrder537, 1, 1},
  {"MediaStreamTrack", kBindingOrder538, 15, 15},
  {"MediaStreamTrackEvent", kBindingOrder539, 1, 1},
  {"MerchantValidationEvent", kBindingOrder540, 3, 3},
  {"MessageBroadcaster", kBindingOrder541, 4, 4},
  {"MessageChannel", kBindingOrder542, 2, 2},
  {"MessageEvent", kBindingOrder543, 6, 6},
  {"MessageListenerManager", kBindingOrder544, 2, 2},
  {"MessagePort", kBindingOrder545, 5, 5},
  {"MessageSender", kBindingOrder546, 3, 3},
  {"MimeType", kBindingOrder547, 4, 4},
  {"MimeTypeArray", kBindingOrder548, 4, 4},
  {"ModelContext", kBindingOrder549, 4, 4},
  {"ModelContextClient", kBindingOrder550, 1, 1},
  {"MouseEvent", kBindingOrder551, 31, 24},
  {"MouseScrollEvent", kBindingOrder552, 4, 2},
  {"MozCanvasPrintState", kBindingOrder553, 2, 2},
  {"MozDocumentMatcher", kBindingOrder554, 12, 12},
  {"MozDocumentObserver", kBindingOrder555, 2, 2},
  {"MozSharedMap", kBindingOrder556, 6, 6},
  {"MozSharedMapChangeEvent", kBindingOrder557, 1, 1},
  {"MozSharedMapIterator", kBindingOrder558, 1, 1},
  {"MozStorageAsyncStatementParams", kBindingOrder559, 2, 2},
  {"MozStorageStatementParams", kBindingOrder560, 2, 2},
  {"MozWritableSharedMap", kBindingOrder561, 3, 3},
  {"MutationObserver", kBindingOrder562, 3, 3},
  {"MutationRecord", kBindingOrder563, 9, 9},
  {"NamedNodeMap", kBindingOrder564, 9, 9},
  {"NavigateEvent", kBindingOrder565, 13, 13},
  {"Navigation", kBindingOrder566, 16, 16},
  {"NavigationActivation", kBindingOrder567, 3, 3},
  {"NavigationCurrentEntryChangeEvent", kBindingOrder568, 2, 2},
  {"NavigationDestination", kBindingOrder569, 6, 6},
  {"NavigationHistoryEntry", kBindingOrder570, 7, 7},
  {"NavigationPrecommitController", kBindingOrder571, 2, 2},
  {"NavigationPreloadManager", kBindingOrder572, 4, 4},
  {"NavigationTransition", kBindingOrder573, 4, 4},
  {"Navigator", kBindingOrder574, 61, 61},
  {"NavigatorLogin", kBindingOrder575, 1, 1},
  {"NetworkInformation", kBindingOrder576, 2, 2},
  {"Node", kBindingOrder577, 49, 31},
  {"NodeFilter", kBindingOrder578, 16, 0},
  {"NodeIterator", kBindingOrder579, 8, 8},
  {"NodeList", kBindingOrder580, 7, 7},
  {"Notification", kBindingOrder581, 17, 17},
  {"NotificationEvent", kBindingOrder582, 2, 2},
  {"OES_draw_buffers_indexed", kBindingOrder583, 7, 7},
  {"OES_standard_derivatives", kBindingOrder584, 1, 0},
  {"OES_texture_half_float", kBindingOrder585, 1, 0},
  {"OES_vertex_array_object", kBindingOrder586, 5, 4},
  {"OVR_multiview2", kBindingOrder587, 5, 1},
  {"OfflineAudioCompletionEvent", kBindingOrder588, 1, 1},
  {"OfflineAudioContext", kBindingOrder589, 3, 3},
  {"OffscreenCanvas", kBindingOrder590, 8, 8},
  {"OffscreenCanvasRenderingContext2D", kBindingOrder591, 71, 71},
  {"OscillatorNode", kBindingOrder592, 4, 4},
  {"PageRevealEvent", kBindingOrder593, 1, 1},
  {"PageSwapEvent", kBindingOrder594, 2, 2},
  {"PageTransitionEvent", kBindingOrder595, 1, 1},
  {"PaintRequest", kBindingOrder596, 2, 2},
  {"PaintRequestList", kBindingOrder597, 3, 3},
  {"PaintWorkletGlobalScope", kBindingOrder598, 1, 1},
  {"PannerNode", kBindingOrder599, 16, 16},
  {"ParentProcessMessageManager", kBindingOrder600, 5, 5},
  {"Path2D", kBindingOrder601, 11, 11},
  {"PaymentAddress", kBindingOrder602, 12, 12},
  {"PaymentMethodChangeEvent", kBindingOrder603, 2, 2},
  {"PaymentRequest", kBindingOrder604, 11, 11},
  {"PaymentRequestUpdateEvent", kBindingOrder605, 1, 1},
  {"PaymentResponse", kBindingOrder606, 12, 12},
  {"PeerConnectionImpl", kBindingOrder607, 41, 41},
  {"PeerConnectionObserver", kBindingOrder608, 16, 16},
  {"Performance", kBindingOrder609, 18, 18},
  {"PerformanceContainerTiming", kBindingOrder610, 9, 9},
  {"PerformanceEntry", kBindingOrder611, 5, 5},
  {"PerformanceEntryEvent", kBindingOrder612, 6, 6},
  {"PerformanceEventTiming", kBindingOrder613, 6, 6},
  {"PerformanceMark", kBindingOrder614, 1, 1},
  {"PerformanceMeasure", kBindingOrder615, 1, 1},
  {"PerformanceNavigation", kBindingOrder616, 7, 3},
  {"PerformanceNavigationTiming", kBindingOrder617, 11, 11},
  {"PerformanceObserver", kBindingOrder618, 3, 3},
  {"PerformanceObserverEntryList", kBindingOrder619, 3, 3},
  {"PerformancePaintTiming", kBindingOrder620, 3, 3},
  {"PerformanceResourceTiming", kBindingOrder621, 25, 25},
  {"PerformanceServerTiming", kBindingOrder622, 4, 4},
  {"PerformanceTiming", kBindingOrder623, 25, 25},
  {"PermissionStatus", kBindingOrder624, 3, 3},
  {"Permissions", kBindingOrder625, 1, 1},
  {"PermissionsPolicy", kBindingOrder626, 4, 4},
  {"PermissionsPolicyViolationReportBody", kBindingOrder627, 5, 5},
  {"PictureInPictureEvent", kBindingOrder628, 1, 1},
  {"PictureInPictureWindow", kBindingOrder629, 3, 3},
  {"PlacesBookmark", kBindingOrder630, 8, 8},
  {"PlacesBookmarkAddition", kBindingOrder631, 11, 11},
  {"PlacesBookmarkChanged", kBindingOrder632, 1, 1},
  {"PlacesBookmarkKeyword", kBindingOrder633, 1, 1},
  {"PlacesBookmarkMoved", kBindingOrder634, 10, 10},
  {"PlacesBookmarkRemoved", kBindingOrder635, 3, 3},
  {"PlacesBookmarkTags", kBindingOrder636, 1, 1},
  {"PlacesBookmarkTime", kBindingOrder637, 1, 1},
  {"PlacesBookmarkTitle", kBindingOrder638, 1, 1},
  {"PlacesEvent", kBindingOrder639, 1, 1},
  {"PlacesEventCounts", kBindingOrder640, 7, 7},
  {"PlacesFavicon", kBindingOrder641, 3, 3},
  {"PlacesVisit", kBindingOrder642, 12, 12},
  {"PlacesVisitRemoved", kBindingOrder643, 8, 6},
  {"PlacesVisitTitle", kBindingOrder644, 3, 3},
  {"Plugin", kBindingOrder645, 7, 7},
  {"PluginArray", kBindingOrder646, 5, 5},
  {"PluginCrashedEvent", kBindingOrder647, 6, 6},
  {"PointerEvent", kBindingOrder648, 15, 15},
  {"PopStateEvent", kBindingOrder649, 2, 2},
  {"PopupBlockedEvent", kBindingOrder650, 4, 4},
  {"PopupPositionedEvent", kBindingOrder651, 4, 4},
  {"PositionStateEvent", kBindingOrder652, 3, 3},
  {"PrecompiledScript", kBindingOrder653, 3, 3},
  {"ProcessMessageManager", kBindingOrder654, 6, 6},
  {"ProcessingInstruction", kBindingOrder655, 2, 2},
  {"ProfilerCounter", kBindingOrder656, 1, 1},
  {"ProgressEvent", kBindingOrder657, 3, 3},
  {"PromiseRejectionEvent", kBindingOrder658, 2, 2},
  {"PublicKeyCredential", kBindingOrder659, 5, 5},
  {"PushEvent", kBindingOrder660, 1, 1},
  {"PushManager", kBindingOrder661, 3, 3},
  {"PushManagerImpl", kBindingOrder662, 3, 3},
  {"PushMessageData", kBindingOrder663, 5, 5},
  {"PushSubscription", kBindingOrder664, 6, 6},
  {"PushSubscriptionChangeEvent", kBindingOrder665, 2, 2},
  {"PushSubscriptionOptions", kBindingOrder666, 1, 1},
  {"QuotaExceededError", kBindingOrder667, 2, 2},
  {"RTCCertificate", kBindingOrder668, 2, 2},
  {"RTCDTMFSender", kBindingOrder669, 4, 4},
  {"RTCDTMFToneChangeEvent", kBindingOrder670, 1, 1},
  {"RTCDataChannel", kBindingOrder671, 19, 19},
  {"RTCDataChannelEvent", kBindingOrder672, 1, 1},
  {"RTCDtlsTransport", kBindingOrder673, 5, 5},
  {"RTCEncodedAudioFrame", kBindingOrder674, 3, 3},
  {"RTCEncodedVideoFrame", kBindingOrder675, 4, 4},
  {"RTCError", kBindingOrder676, 5, 5},
  {"RTCErrorEvent", kBindingOrder677, 1, 1},
  {"RTCIceCandidate", kBindingOrder678, 15, 15},
  {"RTCIceCandidatePair", kBindingOrder679, 2, 2},
  {"RTCIceTransport", kBindingOrder680, 7, 7},
  {"RTCIdentityProviderRegistrar", kBindingOrder681, 1, 1},
  {"RTCPeerConnection", kBindingOrder682, 47, 47},
  {"RTCPeerConnectionIceErrorEvent", kBindingOrder683, 5, 5},
  {"RTCPeerConnectionIceEvent", kBindingOrder684, 1, 1},
  {"RTCRtpReceiver", kBindingOrder685, 8, 8},
  {"RTCRtpScriptTransformer", kBindingOrder686, 5, 5},
  {"RTCRtpSender", kBindingOrder687, 9, 9},
  {"RTCRtpTransceiver", kBindingOrder688, 8, 8},
  {"RTCSctpTransport", kBindingOrder689, 5, 5},
  {"RTCSessionDescription", kBindingOrder690, 3, 3},
  {"RTCStatsReport", kBindingOrder691, 7, 7},
  {"RTCTrackEvent", kBindingOrder692, 4, 4},
  {"RTCTransformEvent", kBindingOrder693, 1, 1},
  {"RadioNodeList", kBindingOrder694, 2, 2},
  {"Range", kBindingOrder695, 29, 25},
  {"ReadableByteStreamController", kBindingOrder696, 5, 5},
  {"ReadableStream", kBindingOrder697, 7, 7},
  {"ReadableStreamAsyncIterator", kBindingOrder698, 2, 2},
  {"ReadableStreamBYOBReader", kBindingOrder699, 4, 4},
  {"ReadableStreamBYOBRequest", kBindingOrder700, 3, 3},
  {"ReadableStreamDefaultController", kBindingOrder701, 4, 4},
  {"ReadableStreamDefaultReader", kBindingOrder702, 4, 4},
  {"RedirectBlockedEvent", kBindingOrder703, 2, 2},
  {"Report", kBindingOrder704, 4, 4},
  {"ReportBody", kBindingOrder705, 1, 1},
  {"ReportingObserver", kBindingOrder706, 3, 3},
  {"Request", kBindingOrder707, 23, 23},
  {"ResizeObserver", kBindingOrder708, 3, 3},
  {"ResizeObserverEntry", kBindingOrder709, 5, 5},
  {"ResizeObserverSize", kBindingOrder710, 2, 2},
  {"Response", kBindingOrder711, 16, 16},
  {"SVGAElement", kBindingOrder712, 19, 19},
  {"SVGAngle", kBindingOrder713, 11, 6},
  {"SVGAnimatedAngle", kBindingOrder714, 2, 2},
  {"SVGAnimatedBoolean", kBindingOrder715, 2, 2},
  {"SVGAnimatedEnumeration", kBindingOrder716, 2, 2},
  {"SVGAnimatedInteger", kBindingOrder717, 2, 2},
  {"SVGAnimatedLength", kBindingOrder718, 2, 2},
  {"SVGAnimatedLengthList", kBindingOrder719, 2, 2},
  {"SVGAnimatedNumber", kBindingOrder720, 2, 2},
  {"SVGAnimatedNumberList", kBindingOrder721, 2, 2},
  {"SVGAnimatedPreserveAspectRatio", kBindingOrder722, 2, 2},
  {"SVGAnimatedRect", kBindingOrder723, 2, 2},
  {"SVGAnimatedString", kBindingOrder724, 2, 2},
  {"SVGAnimatedTransformList", kBindingOrder725, 2, 2},
  {"SVGAnimationElement", kBindingOrder726, 13, 13},
  {"SVGCircleElement", kBindingOrder727, 3, 3},
  {"SVGClipPathElement", kBindingOrder728, 2, 2},
  {"SVGComponentTransferFunctionElement", kBindingOrder729, 13, 7},
  {"SVGElement", kBindingOrder730, 117, 117},
  {"SVGEllipseElement", kBindingOrder731, 4, 4},
  {"SVGFEBlendElement", kBindingOrder732, 25, 8},
  {"SVGFEColorMatrixElement", kBindingOrder733, 13, 8},
  {"SVGFEComponentTransferElement", kBindingOrder734, 6, 6},
  {"SVGFECompositeElement", kBindingOrder735, 20, 12},
  {"SVGFEConvolveMatrixElement", kBindingOrder736, 21, 17},
  {"SVGFEDiffuseLightingElement", kBindingOrder737, 10, 10},
  {"SVGFEDisplacementMapElement", kBindingOrder738, 15, 10},
  {"SVGFEDistantLightElement", kBindingOrder739, 2, 2},
  {"SVGFEDropShadowElement", kBindingOrder740, 11, 11},
  {"SVGFEFloodElement", kBindingOrder741, 5, 5},
  {"SVGFEGaussianBlurElement", kBindingOrder742, 9, 9},
  {"SVGFEImageElement", kBindingOrder743, 9, 9},
  {"SVGFEMergeElement", kBindingOrder744, 5, 5},
  {"SVGFEMergeNodeElement", kBindingOrder745, 1, 1},
  {"SVGFEMorphologyElement", kBindingOrder746, 12, 9},
  {"SVGFEOffsetElement", kBindingOrder747, 8, 8},
  {"SVGFEPointLightElement", kBindingOrder748, 3, 3},
  {"SVGFESpecularLightingElement", kBindingOrder749, 11, 11},
  {"SVGFESpotLightElement", kBindingOrder750, 8, 8},
  {"SVGFETileElement", kBindingOrder751, 6, 6},
  {"SVGFETurbulenceElement", kBindingOrder752, 17, 11},
  {"SVGFilterElement", kBindingOrder753, 7, 7},
  {"SVGForeignObjectElement", kBindingOrder754, 4, 4},
  {"SVGGeometryElement", kBindingOrder755, 5, 5},
  {"SVGGradientElement", kBindingOrder756, 8, 4},
  {"SVGGraphicsElement", kBindingOrder757, 6, 6},
  {"SVGImageElement", kBindingOrder758, 10, 10},
  {"SVGLength", kBindingOrder759, 17, 6},
  {"SVGLengthList", kBindingOrder760, 10, 10},
  {"SVGLineElement", kBindingOrder761, 4, 4},
  {"SVGLinearGradientElement", kBindingOrder762, 4, 4},
  {"SVGMPathElement", kBindingOrder763, 1, 1},
  {"SVGMarkerElement", kBindingOrder764, 18, 11},
  {"SVGMaskElement", kBindingOrder765, 8, 6},
  {"SVGMatrix", kBindingOrder766, 17, 17},
  {"SVGNumber", kBindingOrder767, 1, 1},
  {"SVGNumberList", kBindingOrder768, 10, 10},
  {"SVGPathElement", kBindingOrder769, 3, 3},
  {"SVGPathSegment", kBindingOrder770, 2, 2},
  {"SVGPatternElement", kBindingOrder771, 10, 10},
  {"SVGPoint", kBindingOrder772, 3, 3},
  {"SVGPointList", kBindingOrder773, 10, 10},
  {"SVGPolygonElement", kBindingOrder774, 2, 2},
  {"SVGPolylineElement", kBindingOrder775, 2, 2},
  {"SVGPreserveAspectRatio", kBindingOrder776, 16, 2},
  {"SVGRadialGradientElement", kBindingOrder777, 6, 6},
  {"SVGRect", kBindingOrder778, 4, 4},
  {"SVGRectElement", kBindingOrder779, 6, 6},
  {"SVGSVGElement", kBindingOrder780, 31, 28},
  {"SVGScriptElement", kBindingOrder781, 6, 6},
  {"SVGStopElement", kBindingOrder782, 1, 1},
  {"SVGStringList", kBindingOrder783, 10, 10},
  {"SVGStyleElement", kBindingOrder784, 5, 5},
  {"SVGSymbolElement", kBindingOrder785, 4, 4},
  {"SVGTextContentElement", kBindingOrder786, 14, 11},
  {"SVGTextPathElement", kBindingOrder787, 14, 5},
  {"SVGTextPositioningElement", kBindingOrder788, 5, 5},
  {"SVGTransform", kBindingOrder789, 16, 9},
  {"SVGTransformList", kBindingOrder790, 12, 12},
  {"SVGUnitTypes", kBindingOrder791, 3, 0},
  {"SVGUseElement", kBindingOrder792, 5, 5},
  {"SVGViewElement", kBindingOrder793, 6, 3},
  {"Sanitizer", kBindingOrder794, 12, 12},
  {"Scheduler", kBindingOrder795, 2, 2},
  {"Screen", kBindingOrder796, 18, 18},
  {"ScreenLuminance", kBindingOrder797, 3, 3},
  {"ScreenOrientation", kBindingOrder798, 5, 5},
  {"ScriptProcessorNode", kBindingOrder799, 2, 2},
  {"ScrollAreaEvent", kBindingOrder800, 5, 5},
  {"ScrollTimeline", kBindingOrder801, 2, 2},
  {"SecurityPolicyViolationEvent", kBindingOrder802, 12, 12},
  {"Selection", kBindingOrder803, 26, 26},
  {"Serial", kBindingOrder804, 9, 9},
  {"SerialPort", kBindingOrder805, 11, 11},
  {"ServiceWorker", kBindingOrder806, 5, 5},
  {"ServiceWorkerContainer", kBindingOrder807, 9, 9},
  {"ServiceWorkerGlobalScope", kBindingOrder808, 16, 16},
  {"ServiceWorkerRegistration", kBindingOrder809, 13, 13},
  {"SessionStoreFormData", kBindingOrder810, 6, 6},
  {"SessionStoreScrollData", kBindingOrder811, 3, 3},
  {"ShadowRoot", kBindingOrder812, 26, 26},
  {"SharedWorker", kBindingOrder813, 2, 2},
  {"SharedWorkerGlobalScope", kBindingOrder814, 3, 3},
  {"SimpleGestureEvent", kBindingOrder815, 11, 5},
  {"SourceBuffer", kBindingOrder816, 17, 17},
  {"SourceBufferList", kBindingOrder817, 4, 4},
  {"SpeechGrammar", kBindingOrder818, 2, 2},
  {"SpeechGrammarList", kBindingOrder819, 5, 5},
  {"SpeechRecognition", kBindingOrder820, 22, 22},
  {"SpeechRecognitionAlternative", kBindingOrder821, 2, 2},
  {"SpeechRecognitionErrorEvent", kBindingOrder822, 2, 2},
  {"SpeechRecognitionEvent", kBindingOrder823, 4, 4},
  {"SpeechRecognitionPhrase", kBindingOrder824, 2, 2},
  {"SpeechRecognitionResult", kBindingOrder825, 4, 4},
  {"SpeechRecognitionResultList", kBindingOrder826, 3, 3},
  {"SpeechSynthesis", kBindingOrder827, 9, 9},
  {"SpeechSynthesisErrorEvent", kBindingOrder828, 1, 1},
  {"SpeechSynthesisEvent", kBindingOrder829, 5, 5},
  {"SpeechSynthesisUtterance", kBindingOrder830, 13, 13},
  {"SpeechSynthesisVoice", kBindingOrder831, 5, 5},
  {"StereoPannerNode", kBindingOrder832, 1, 1},
  {"Storage", kBindingOrder833, 13, 13},
  {"StorageEvent", kBindingOrder834, 6, 6},
  {"StorageManager", kBindingOrder835, 4, 4},
  {"StreamFilter", kBindingOrder836, 11, 11},
  {"StreamFilterDataEvent", kBindingOrder837, 1, 1},
  {"StructuredCloneHolder", kBindingOrder838, 2, 2},
  {"StructuredCloneTester", kBindingOrder839, 2, 2},
  {"StylePropertyMap", kBindingOrder840, 4, 4},
  {"StylePropertyMapReadOnly", kBindingOrder841, 8, 8},
  {"StylePropertyMapReadOnlyIterator", kBindingOrder842, 1, 1},
  {"StyleSheet", kBindingOrder843, 7, 7},
  {"StyleSheetApplicableStateChangeEvent", kBindingOrder844, 2, 2},
  {"StyleSheetList", kBindingOrder845, 3, 3},
  {"StyleSheetRemovedEvent", kBindingOrder846, 1, 1},
  {"SubmitEvent", kBindingOrder847, 1, 1},
  {"SubtleCrypto", kBindingOrder848, 16, 16},
  {"SyncMessageSender", kBindingOrder849, 1, 1},
  {"SyncReadFile", kBindingOrder850, 3, 3},
  {"TCPServerSocket", kBindingOrder851, 4, 4},
  {"TCPServerSocketEvent", kBindingOrder852, 1, 1},
  {"TCPSocket", kBindingOrder853, 17, 17},
  {"TCPSocketErrorEvent", kBindingOrder854, 3, 3},
  {"TCPSocketEvent", kBindingOrder855, 1, 1},
  {"TaskController", kBindingOrder856, 1, 1},
  {"TaskPriorityChangeEvent", kBindingOrder857, 1, 1},
  {"TaskSignal", kBindingOrder858, 2, 2},
  {"Tensor", kBindingOrder859, 6, 6},
  {"TestReportBody", kBindingOrder860, 2, 2},
  {"TestingDeprecatedInterface", kBindingOrder861, 2, 2},
  {"Text", kBindingOrder862, 7, 7},
  {"TextClause", kBindingOrder863, 4, 4},
  {"TextDecoder", kBindingOrder864, 4, 4},
  {"TextDecoderStream", kBindingOrder865, 5, 5},
  {"TextEncoder", kBindingOrder866, 3, 3},
  {"TextEncoderStream", kBindingOrder867, 3, 3},
  {"TextEvent", kBindingOrder868, 2, 2},
  {"TextFormat", kBindingOrder869, 4, 4},
  {"TextFormatUpdateEvent", kBindingOrder870, 1, 1},
  {"TextGenerator", kBindingOrder871, 4, 4},
  {"TextMetrics", kBindingOrder872, 12, 12},
  {"TextTrack", kBindingOrder873, 11, 11},
  {"TextTrackCue", kBindingOrder874, 7, 7},
  {"TextTrackCueList", kBindingOrder875, 3, 3},
  {"TextTrackList", kBindingOrder876, 6, 6},
  {"TextUpdateEvent", kBindingOrder877, 5, 5},
  {"TimeEvent", kBindingOrder878, 3, 3},
  {"TimeRanges", kBindingOrder879, 3, 3},
  {"ToggleEvent", kBindingOrder880, 3, 3},
  {"Touch", kBindingOrder881, 12, 12},
  {"TouchEvent", kBindingOrder882, 8, 8},
  {"TouchList", kBindingOrder883, 3, 3},
  {"TrackEvent", kBindingOrder884, 1, 1},
  {"TransformStream", kBindingOrder885, 2, 2},
  {"TransformStreamDefaultController", kBindingOrder886, 4, 4},
  {"TransitionEvent", kBindingOrder887, 4, 4},
  {"TreeColumn", kBindingOrder888, 16, 14},
  {"TreeColumns", kBindingOrder889, 13, 13},
  {"TreeContentView", kBindingOrder890, 31, 28},
  {"TreeWalker", kBindingOrder891, 11, 11},
  {"TrustedHTML", kBindingOrder892, 2, 2},
  {"TrustedScript", kBindingOrder893, 2, 2},
  {"TrustedScriptURL", kBindingOrder894, 2, 2},
  {"TrustedTypePolicy", kBindingOrder895, 4, 4},
  {"TrustedTypePolicyFactory", kBindingOrder896, 9, 9},
  {"UDPMessageEvent", kBindingOrder897, 3, 3},
  {"UDPSocket", kBindingOrder898, 14, 14},
  {"UIEvent", kBindingOrder899, 10, 8},
  {"URL", kBindingOrder900, 14, 14},
  {"URLPattern", kBindingOrder901, 11, 11},
  {"URLSearchParams", kBindingOrder902, 13, 13},
  {"URLSearchParamsIterator", kBindingOrder903, 1, 1},
  {"UserActivation", kBindingOrder904, 2, 2},
  {"UserProximityEvent", kBindingOrder905, 1, 1},
  {"VRDisplay", kBindingOrder906, 18, 18},
  {"VRDisplayCapabilities", kBindingOrder907, 5, 5},
  {"VRDisplayEvent", kBindingOrder908, 2, 2},
  {"VREyeParameters", kBindingOrder909, 4, 4},
  {"VRFieldOfView", kBindingOrder910, 4, 4},
  {"VRFrameData", kBindingOrder911, 6, 6},
  {"VRMockController", kBindingOrder912, 15, 15},
  {"VRMockDisplay", kBindingOrder913, 18, 18},
  {"VRPose", kBindingOrder914, 6, 6},
  {"VRServiceTest", kBindingOrder915, 18, 18},
  {"VRStageParameters", kBindingOrder916, 3, 3},
  {"VTTCue", kBindingOrder917, 11, 11},
  {"VTTRegion", kBindingOrder918, 8, 8},
  {"ValidityState", kBindingOrder919, 11, 11},
  {"VideoColorSpace", kBindingOrder920, 5, 5},
  {"VideoDecoder", kBindingOrder921, 8, 8},
  {"VideoEncoder", kBindingOrder922, 9, 9},
  {"VideoFrame", kBindingOrder923, 14, 14},
  {"VideoPlaybackQuality", kBindingOrder924, 3, 3},
  {"VideoTrack", kBindingOrder925, 5, 5},
  {"VideoTrackList", kBindingOrder926, 7, 7},
  {"ViewTimeline", kBindingOrder927, 3, 3},
  {"ViewTransition", kBindingOrder928, 5, 5},
  {"ViewTransitionTypeSet", kBindingOrder929, 9, 9},
  {"VisualViewport", kBindingOrder930, 10, 10},
  {"WEBGL_color_buffer_float", kBindingOrder931, 4, 0},
  {"WEBGL_compressed_texture_astc", kBindingOrder932, 29, 1},
  {"WEBGL_compressed_texture_etc", kBindingOrder933, 10, 0},
  {"WEBGL_compressed_texture_etc1", kBindingOrder934, 1, 0},
  {"WEBGL_compressed_texture_pvrtc", kBindingOrder935, 4, 0},
  {"WEBGL_compressed_texture_s3tc", kBindingOrder936, 4, 0},
  {"WEBGL_compressed_texture_s3tc_srgb", kBindingOrder937, 4, 0},
  {"WEBGL_debug_renderer_info", kBindingOrder938, 2, 0},
  {"WEBGL_debug_shaders", kBindingOrder939, 1, 1},
  {"WEBGL_depth_texture", kBindingOrder940, 1, 0},
  {"WEBGL_draw_buffers", kBindingOrder941, 35, 1},
  {"WEBGL_explicit_present", kBindingOrder942, 1, 1},
  {"WEBGL_lose_context", kBindingOrder943, 2, 2},
  {"WEBGL_provoking_vertex", kBindingOrder944, 4, 1},
  {"WGSLLanguageFeatures", kBindingOrder945, 6, 6},
  {"WakeLock", kBindingOrder946, 1, 1},
  {"WakeLockSentinel", kBindingOrder947, 4, 4},
  {"WaveShaperNode", kBindingOrder948, 2, 2},
  {"WebExtensionContentScript", kBindingOrder949, 6, 6},
  {"WebExtensionPolicy", kBindingOrder950, 36, 36},
  {"WebGL2RenderingContext", kBindingOrder951, 789, 230},
  {"WebGLActiveInfo", kBindingOrder952, 3, 3},
  {"WebGLContextEvent", kBindingOrder953, 1, 1},
  {"WebGLRenderingContext", kBindingOrder954, 438, 142},
  {"WebGLShaderPrecisionFormat", kBindingOrder955, 3, 3},
  {"WebSocket", kBindingOrder956, 16, 12},
  {"WebTransport", kBindingOrder957, 16, 16},
  {"WebTransportBidirectionalStream", kBindingOrder958, 2, 2},
  {"WebTransportDatagramDuplexStream", kBindingOrder959, 8, 8},
  {"WebTransportDatagramsWritable", kBindingOrder960, 2, 2},
  {"WebTransportError", kBindingOrder961, 2, 2},
  {"WebTransportReceiveStream", kBindingOrder962, 1, 1},
  {"WebTransportSendGroup", kBindingOrder963, 1, 1},
  {"WebTransportSendStream", kBindingOrder964, 3, 3},
  {"WheelEvent", kBindingOrder965, 10, 7},
  {"Window", kBindingOrder966, 265, 261},
  {"WindowClient", kBindingOrder967, 4, 4},
  {"WindowContext", kBindingOrder968, 15, 15},
  {"WindowGlobalChild", kBindingOrder969, 15, 15},
  {"WindowGlobalParent", kBindingOrder970, 32, 32},
  {"WindowRoot", kBindingOrder971, 1, 1},
  {"Worker", kBindingOrder972, 5, 5},
  {"WorkerDebuggerGlobalScope", kBindingOrder973, 16, 16},
  {"WorkerGlobalScope", kBindingOrder974, 34, 34},
  {"WorkerLocation", kBindingOrder975, 10, 10},
  {"WorkerNavigator", kBindingOrder976, 20, 20},
  {"Worklet", kBindingOrder977, 1, 1},
  {"WorkletGlobalScope", kBindingOrder978, 1, 1},
  {"WritableStream", kBindingOrder979, 4, 4},
  {"WritableStreamDefaultController", kBindingOrder980, 2, 2},
  {"WritableStreamDefaultWriter", kBindingOrder981, 7, 7},
  {"XMLHttpRequest", kBindingOrder982, 26, 21},
  {"XMLHttpRequestEventTarget", kBindingOrder983, 7, 7},
  {"XMLSerializer", kBindingOrder984, 1, 1},
  {"XPathEvaluator", kBindingOrder985, 3, 3},
  {"XPathExpression", kBindingOrder986, 1, 1},
  {"XPathResult", kBindingOrder987, 19, 9},
  {"XRBoundedReferenceSpace", kBindingOrder988, 1, 1},
  {"XRFrame", kBindingOrder989, 3, 3},
  {"XRInputSource", kBindingOrder990, 6, 6},
  {"XRInputSourceArray", kBindingOrder991, 6, 6},
  {"XRInputSourceEvent", kBindingOrder992, 2, 2},
  {"XRInputSourcesChangeEvent", kBindingOrder993, 3, 3},
  {"XRPose", kBindingOrder994, 2, 2},
  {"XRReferenceSpace", kBindingOrder995, 2, 2},
  {"XRReferenceSpaceEvent", kBindingOrder996, 2, 2},
  {"XRRenderState", kBindingOrder997, 4, 4},
  {"XRRigidTransform", kBindingOrder998, 4, 4},
  {"XRSession", kBindingOrder999, 20, 20},
  {"XRSessionEvent", kBindingOrder1000, 1, 1},
  {"XRSystem", kBindingOrder1001, 3, 3},
  {"XRView", kBindingOrder1002, 3, 3},
  {"XRViewerPose", kBindingOrder1003, 1, 1},
  {"XRViewport", kBindingOrder1004, 4, 4},
  {"XRWebGLLayer", kBindingOrder1005, 6, 6},
  {"XSLTProcessor", kBindingOrder1006, 8, 8},
  {"XULCommandEvent", kBindingOrder1007, 8, 8},
  {"XULElement", kBindingOrder1008, 130, 130},
  {"XULFrameElement", kBindingOrder1009, 6, 6},
  {"XULMenuElement", kBindingOrder1010, 3, 3},
  {"XULPopupElement", kBindingOrder1011, 19, 19},
  {"XULTextElement", kBindingOrder1012, 3, 3},
  {"XULTreeElement", kBindingOrder1013, 30, 30},
};
}
