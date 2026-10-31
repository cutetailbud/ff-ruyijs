"""Prepare native Linux inputs from an unmodified SpiderMonkey build."""
import argparse
from pathlib import Path

from donor import Donor
from generate import generate
import product_sources
import sanitizer_data
from provenance import write_if_changed

PLATFORM_BACKENDS = {
    "web_crypto/cng_backend.cpp", "web_crypto/digest_backend.cpp",
    "web_geometry/font_aliases.cpp", "web_geometry/font_collection.cpp",
    "web_geometry/font_missing.cpp", "web_geometry/font_preferences.cpp",
    "web_geometry/font_shaping.cpp", "web_geometry/system_font.cpp",
    "web_geometry/font_face_mac.cpp", "web_geometry/canvas_text.cpp",
    "web_geometry/canvas_text_path.cpp", "web_geometry/glyph_color.cpp",
    "web_geometry/glyph_mask.cpp", "web_geometry/glyph_palette.cpp",
    "web_geometry/glyph_runs.cpp", "web_geometry/glyph_style.cpp",
    "web_geometry/svg_length_font.cpp", "web_geometry/svg_path.cpp",
    "web_media/image_codec.cpp", "web_media/image_gif.cpp",
    "web_media/image_composition.cpp", "web_media/image_color.cpp",
    "web_canvas/skia_subset_stubs.cpp",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--donor-obj", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    donor = Donor(args.donor_obj)
    if donor.os_arch != "Linux":
        parser.error("a Linux SpiderMonkey donor build is required")
    directory = args.output.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    shell, _ = generate(donor, root, directory)
    product_sources.profile(root, directory)
    sanitizer_data.prepare(donor.src, directory)
    for header in (root / "tools/generated").glob("*.h"):
        write_if_changed(directory / header.name, header.read_text(encoding="utf-8"), encoding="utf-8")
    sources = [shell]
    sources += [path for path, _ in product_sources.sources(root)
                if path.relative_to(root / "src").as_posix() not in PLATFORM_BACKENDS]
    objects = [path for path in donor.objects() if path.name != "Unified_cpp_js_src_shell0.o"]
    def cmake_list(name, values):
        return "set(" + name + "\n" + "".join('  "' + str(value).replace('\\', '/') + '"\n' for value in values) + ")\n"
    output = cmake_list("RUYIJS_SOURCES", sources)
    output += cmake_list("DONOR_OBJECTS", objects)
    output += cmake_list("DONOR_LIBRARIES", donor.libraries())
    output += cmake_list("DONOR_LINK_FLAGS", donor.flags("COMPUTED_LDFLAGS") + donor.flags("OS_LIBS"))
    output += cmake_list("DONOR_SOURCE", [donor.src])
    write_if_changed(directory / "sources.cmake", output, encoding="utf-8")
    print(f"Prepared {len(sources)} Linux translation units")


if __name__ == "__main__":
    main()
