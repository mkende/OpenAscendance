#include "audio/mixer.h"

#include <algorithm>

namespace oa {

void Mixer::play_music(SampleData music, std::size_t position) {
  const std::scoped_lock lock(mutex_);
  music_.data = std::move(music);
  music_.position = position;
  music_.playing = music_.data != nullptr && position < music_.data->size();
}

std::size_t Mixer::stop_music() {
  const std::scoped_lock lock(mutex_);
  const std::size_t pos = music_.playing ? music_.position : 0;
  music_.playing = false;
  music_.data.reset();
  return pos;
}

bool Mixer::music_playing() const {
  const std::scoped_lock lock(mutex_);
  return music_.playing;
}

void Mixer::set_music_volume(int volume) {
  const std::scoped_lock lock(mutex_);
  music_.volume = std::clamp(volume, 0, kMaxVolume);
}

int Mixer::music_volume() const {
  const std::scoped_lock lock(mutex_);
  return music_.volume;
}

void Mixer::set_music_level(int percent) {
  const std::scoped_lock lock(mutex_);
  music_level_ = std::clamp(percent, 0, kMaxLevel);
}

int Mixer::music_level() const {
  const std::scoped_lock lock(mutex_);
  return music_level_;
}

void Mixer::set_sfx_level(int percent) {
  const std::scoped_lock lock(mutex_);
  sfx_level_ = std::clamp(percent, 0, kMaxLevel);
}

int Mixer::sfx_level() const {
  const std::scoped_lock lock(mutex_);
  return sfx_level_;
}

void Mixer::play_sfx(int voice, SampleData sample, int volume) {
  const std::scoped_lock lock(mutex_);
  Voice& v = sfx_.at(static_cast<std::size_t>(voice));
  v.data = std::move(sample);
  v.position = 0;
  v.volume = std::clamp(volume, 0, kMaxVolume);
  v.playing = v.data != nullptr && !v.data->empty();
}

bool Mixer::sfx_playing(int voice) const {
  const std::scoped_lock lock(mutex_);
  return sfx_.at(static_cast<std::size_t>(voice)).playing;
}

void Mixer::stop_all_sfx() {
  const std::scoped_lock lock(mutex_);
  for (Voice& v : sfx_) {
    v.playing = false;
    v.data.reset();
  }
}

void Mixer::render(std::span<std::int16_t> out) {
  std::vector<std::int32_t> acc(out.size(), 0);
  {
    const std::scoped_lock lock(mutex_);
    const auto mix = [&](Voice& v, int level) {
      if (!v.playing) {
        return;
      }
      const std::vector<std::uint8_t>& d = *v.data;
      const std::size_t n = std::min(out.size(), d.size() - v.position);
      for (std::size_t i = 0; i < n; ++i) {
        // 8-bit unsigned to 16-bit signed, scaled by the AIL volume and the
        // level (at most 127 * 256 * 127 * 100, within 32 bits).
        acc[i] +=
            (static_cast<std::int32_t>(d[v.position + i]) - 128) * 256 * v.volume * level / (kMaxVolume * kMaxLevel);
      }
      v.position += n;
      if (v.position >= d.size()) {
        v.playing = false;
      }
    };
    mix(music_, music_level_);
    for (Voice& v : sfx_) {
      mix(v, sfx_level_);
    }
  }
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = static_cast<std::int16_t>(std::clamp(acc[i], -32768, 32767));
  }
}

}  // namespace oa
