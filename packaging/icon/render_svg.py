#!/usr/bin/env python3
"""Renders an SVG file to a square PNG with the system's librsvg and cairo.

Uses only ctypes (no Python package), for systems with the librsvg and
cairo shared libraries but without the `rsvg-convert` program (ImageMagick's
own SVG renderer gets gradients and opacity wrong). Copied from
SilentHeritage's android/icon/render_svg.py.

Usage: render_svg.py [--css <stylesheet>] <in.svg> <out.png> <size in pixels>

`--css` applies a user stylesheet (librsvg 2.48+); its declarations
override the SVG's presentation attributes, e.g. `#background{display:none}`.
"""

import argparse
import ctypes
import ctypes.util


class RsvgRectangle(ctypes.Structure):
    """The `RsvgRectangle` of librsvg: a viewport in user units."""

    _fields_ = [
        ("x", ctypes.c_double),
        ("y", ctypes.c_double),
        ("width", ctypes.c_double),
        ("height", ctypes.c_double),
    ]


def _load(name: str, fallback: str) -> ctypes.CDLL:
    """Returns the shared library `name`, found by ctypes or by its soname."""
    return ctypes.CDLL(ctypes.util.find_library(name) or fallback)


def render(svg_path: str, out_path: str, size: int, css: str | None = None) -> None:
    """Renders `svg_path` scaled into a `size` x `size` PNG at `out_path`;
    `css`, when given, is applied as a user stylesheet.

    Raises `RuntimeError` when librsvg cannot read the SVG or the stylesheet,
    or cairo cannot write the PNG.
    """
    rsvg = _load("rsvg-2", "librsvg-2.so.2")
    cairo = _load("cairo", "libcairo.so.2")
    rsvg.rsvg_handle_new_from_file.restype = ctypes.c_void_p
    rsvg.rsvg_handle_new_from_file.argtypes = [ctypes.c_char_p, ctypes.c_void_p]
    rsvg.rsvg_handle_render_document.restype = ctypes.c_int
    rsvg.rsvg_handle_render_document.argtypes = [
        ctypes.c_void_p,
        ctypes.c_void_p,
        ctypes.POINTER(RsvgRectangle),
        ctypes.c_void_p,
    ]
    cairo.cairo_image_surface_create.restype = ctypes.c_void_p
    cairo.cairo_image_surface_create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
    cairo.cairo_create.restype = ctypes.c_void_p
    cairo.cairo_create.argtypes = [ctypes.c_void_p]
    cairo.cairo_surface_write_to_png.restype = ctypes.c_int
    cairo.cairo_surface_write_to_png.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    cairo.cairo_destroy.argtypes = [ctypes.c_void_p]
    cairo.cairo_surface_destroy.argtypes = [ctypes.c_void_p]
    rsvg.rsvg_handle_set_stylesheet.restype = ctypes.c_int
    rsvg.rsvg_handle_set_stylesheet.argtypes = [
        ctypes.c_void_p,
        ctypes.c_char_p,
        ctypes.c_size_t,
        ctypes.c_void_p,
    ]
    gobject = _load("gobject-2.0", "libgobject-2.0.so.0")
    gobject.g_object_unref.argtypes = [ctypes.c_void_p]

    handle = rsvg.rsvg_handle_new_from_file(svg_path.encode(), None)
    if not handle:
        raise RuntimeError(f"librsvg cannot read {svg_path}")
    if css is not None:
        sheet = css.encode()
        if not rsvg.rsvg_handle_set_stylesheet(handle, sheet, len(sheet), None):
            gobject.g_object_unref(handle)
            raise RuntimeError(f"librsvg cannot parse the stylesheet {css!r}")
    cairo_format_argb32 = 0
    surface = cairo.cairo_image_surface_create(cairo_format_argb32, size, size)
    context = cairo.cairo_create(surface)
    try:
        viewport = RsvgRectangle(0, 0, size, size)
        if not rsvg.rsvg_handle_render_document(handle, context, ctypes.byref(viewport), None):
            raise RuntimeError(f"librsvg cannot render {svg_path}")
        if cairo.cairo_surface_write_to_png(surface, out_path.encode()) != 0:
            raise RuntimeError(f"cairo cannot write {out_path}")
    finally:
        cairo.cairo_destroy(context)
        cairo.cairo_surface_destroy(surface)
        gobject.g_object_unref(handle)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Renders an SVG file to a square PNG.")
    parser.add_argument("--css", help="a user stylesheet applied over the SVG's styles")
    parser.add_argument("svg", help="the SVG file")
    parser.add_argument("out", help="the output PNG")
    parser.add_argument("size", type=int, help="the picture's width and height in pixels")
    args = parser.parse_args()
    render(args.svg, args.out, args.size, args.css)
