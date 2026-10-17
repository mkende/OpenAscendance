#include "platform/clock.h"

#include <SDL3/SDL_timer.h>

namespace oa {

namespace {

constexpr std::uint64_t kVblankUs = 1'000'000 / Clock::kVblankHz;

}  // namespace

Clock::Clock(bool virtual_time) : virtual_(virtual_time), start_ns_(virtual_time ? 0 : SDL_GetTicksNS()) {}

std::uint64_t Clock::system_ns() const {
  return (suspended_at_ns_ != 0 ? suspended_at_ns_ : SDL_GetTicksNS()) - start_ns_;
}

std::uint64_t Clock::micros() const { return virtual_ ? virtual_us_ : system_ns() / 1000; }

void Clock::suspend() {
  if (!virtual_ && suspended_at_ns_ == 0) {
    suspended_at_ns_ = SDL_GetTicksNS();
  }
}

void Clock::resume() {
  if (suspended_at_ns_ != 0) {
    // The start moves by the time spent suspended.
    start_ns_ += SDL_GetTicksNS() - suspended_at_ns_;
    suspended_at_ns_ = 0;
  }
}

std::uint32_t Clock::ticks() const { return static_cast<std::uint32_t>(micros() / (1'000'000 / kTickHz)); }

void Clock::wait_vblank() {
  const std::uint64_t now = micros();
  const std::uint64_t next = (now / kVblankUs + 1) * kVblankUs;
  if (virtual_) {
    virtual_us_ = next;
  } else {
    SDL_DelayNS((next - now) * 1000);
  }
}

void Clock::idle() {
  if (virtual_) {
    virtual_us_ += 1000;
  } else {
    SDL_DelayNS(1'000'000);
  }
}

void Clock::wait_until_tick(std::uint32_t tick) {
  while (static_cast<std::int32_t>(tick - ticks()) > 0) {
    idle();
  }
}

}  // namespace oa
