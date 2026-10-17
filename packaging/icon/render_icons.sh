#!/usr/bin/env bash
# Renders the launcher icons from packaging/icon/open_ascendance.svg (the
# placeholder icon) into the committed pictures, so that no build needs an
# SVG renderer:
#   packaging/icon/icon-<size>.png  the desktop icons, 16 to 1024 pixels
#                                   (Linux, and macOS's .icns made by
#                                   packaging/build_desktop.sh)
#   packaging/icon/icon.ico         the Windows executable's icon, 16 to 256
#   android/res/mipmap-<density>/   Android's launcher icons: the legacy
#                                   square and round 48 dp icons, and the
#                                   adaptive icon's 108 dp foreground (the
#                                   galaxy within the central 66 dp the
#                                   launchers keep) and monochrome layer
# Run it after changing the SVG, and commit its output.
#
# Needs python3 with the librsvg and cairo libraries (render_svg.py) and
# ImageMagick 7 (`magick`).
#
# Usage: packaging/icon/render_icons.sh
set -euo pipefail

readonly root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
readonly icon_dir="${root}/packaging/icon"
readonly svg="${icon_dir}/open_ascendance.svg"
readonly res="${root}/android/res"
work="$(mktemp -d)"
readonly work
trap 'rm -rf "${work}"' EXIT

# The masters, at 1024 pixels: the whole icon and the galaxy alone.
python3 "${icon_dir}/render_svg.py" "${svg}" "${work}/full.png" 1024
python3 "${icon_dir}/render_svg.py" --css '#background{display:none}' "${svg}" "${work}/foreground.png" 1024

# PNGs without time stamps or other varying chunks, for stable files.
readonly png_opts=(-strip -define png:exclude-chunks=date,time)

for size in 16 24 32 48 64 128 256 512 1024; do
  magick "${work}/full.png" -filter Lanczos -resize "${size}x${size}" "${png_opts[@]}" "${icon_dir}/icon-${size}.png"
done
magick "${icon_dir}"/icon-{16,24,32,48,64,128,256}.png "${icon_dir}/icon.ico"

# Android densities: mdpi is 1 pixel per dp.
for density in mdpi:1 hdpi:1.5 xhdpi:2 xxhdpi:3 xxxhdpi:4; do
  name="${density%%:*}"
  scale="${density#*:}"
  dir="${res}/mipmap-${name}"
  mkdir -p "${dir}"
  legacy="$(awk -v s="${scale}" 'BEGIN { printf "%d", 48 * s }')"
  full="$(awk -v s="${scale}" 'BEGIN { printf "%d", 108 * s }')"
  safe="$(awk -v s="${scale}" 'BEGIN { printf "%d", 66 * s }')"
  magick "${work}/full.png" -filter Lanczos -resize "${legacy}x${legacy}" "${png_opts[@]}" "${dir}/ic_launcher.png"
  magick "${work}/full.png" -filter Lanczos -resize "${legacy}x${legacy}" \
    \( -size "${legacy}x${legacy}" xc:none -fill white -draw "circle $((legacy / 2)),$((legacy / 2)) $((legacy / 2)),0" \) \
    -compose DstIn -composite "${png_opts[@]}" "${dir}/ic_launcher_round.png"
  magick "${work}/foreground.png" -filter Lanczos -resize "${safe}x${safe}" -background none -gravity center \
    -extent "${full}x${full}" "${png_opts[@]}" "${dir}/ic_launcher_foreground.png"
  magick "${dir}/ic_launcher_foreground.png" -fill white -colorize 100 "${png_opts[@]}" "${dir}/ic_launcher_monochrome.png"
done
