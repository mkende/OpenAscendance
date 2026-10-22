#include "audio/sound_system.h"

#include <algorithm>
#include <format>
#include <memory>

#include "assets/pcm.h"
#include "core/error.h"

namespace oa {

SoundSystem::SoundSystem(const GameFiles& files, Mixer* mixer)
    : files_(files), mixer_(mixer), sfx_table_(parse_soundfx_table(files.load("soundfx.txt"))) {
  for (std::string& name : parse_music_table(files.load("music.txt"))) {
    tunes_.push_back({.file = std::move(name)});
  }
}

bool SoundSystem::play_tune(int tune, bool resume) {
  if (music_muted_ || mixer_ == nullptr) {
    return false;
  }
  if (tune < 0 || tune >= tune_count()) {
    throw DataError(std::format("no tune {} in music.txt", tune));
  }
  stop_tune();
  Tune& t = tunes_[static_cast<std::size_t>(tune)];
  auto data = std::make_shared<const std::vector<std::uint8_t>>(files_.load(t.file));
  const std::size_t position = resume ? std::min(t.saved_position, data->size()) : 0;
  mixer_->set_music_volume(music_volume_);
  mixer_->play_music(std::move(data), position);
  tune_playing_ = true;
  current_tune_ = tune;
  return true;
}

void SoundSystem::stop_tune() {
  if (!tune_playing_) {
    return;
  }
  tune_playing_ = false;
  const bool playing = mixer_->music_playing();
  const std::size_t position = mixer_->stop_music();
  // A tune that ran out is rewound (stream_music).
  tunes_[static_cast<std::size_t>(current_tune_)].saved_position = playing ? position : 0;
}

void SoundSystem::stream_music() {
  if (music_muted_ || mixer_ == nullptr || !tune_playing_) {
    return;
  }
  if (!mixer_->music_playing()) {
    tunes_[static_cast<std::size_t>(current_tune_)].saved_position = 0;
    tune_playing_ = false;
    mixer_->stop_music();
  }
}

void SoundSystem::set_music_volume(int volume) {
  if (volume >= 0 && volume <= Mixer::kMaxVolume) {
    music_volume_ = volume;
  }
  if (mixer_ != nullptr) {
    mixer_->set_music_volume(music_volume_);
  }
}

void SoundSystem::set_music_level(int percent) {
  music_level_ = std::clamp(percent, 0, Mixer::kMaxLevel);
  if (mixer_ != nullptr) {
    mixer_->set_music_level(music_level_);
  }
}

void SoundSystem::set_sfx_level(int percent) {
  sfx_level_ = std::clamp(percent, 0, Mixer::kMaxLevel);
  if (mixer_ != nullptr) {
    mixer_->set_sfx_level(sfx_level_);
  }
}

void SoundSystem::play_sfx(int id) {
  if (sfx_muted_ || mixer_ == nullptr) {
    return;
  }
  if (id < 0 || static_cast<std::size_t>(id) >= sfx_table_.names.size()) {
    throw DataError(std::format("no sound {} in soundfx.txt", id));
  }
  SampleData& sample = sfx_cache_[id];
  if (!sample) {
    const std::string file = std::format("data\\{}.voc", sfx_table_.names[static_cast<std::size_t>(id)]);
    sample = std::make_shared<const std::vector<std::uint8_t>>(sfx_samples(files_.load(file), file));
  }
  for (int voice = 0; voice < Mixer::kSfxVoices; ++voice) {
    if (!mixer_->sfx_playing(voice)) {
      mixer_->play_sfx(voice, sample, Mixer::kMaxVolume);
      return;
    }
  }
}

}  // namespace oa
