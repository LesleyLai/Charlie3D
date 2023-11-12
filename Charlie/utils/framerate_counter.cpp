#include "framerate_counter.hpp"

#include <beyond/utils/narrowing.hpp>
#include <numeric>

namespace charlie {

void FramerateCounter::update(Duration delta_time)
{
  accumulated_time_ += delta_time;
  const float ms_per_frame = std::chrono::duration<float, std::milli>(delta_time).count();
  mspf_samples_.push_back(ms_per_frame);

  if (accumulated_time_ > time_per_update_) {
    accumulated_time_ -= time_per_update_;
    average_ms_per_frame = std::accumulate(mspf_samples_.begin(), mspf_samples_.end(), 0.f) /
                           beyond::narrow<float>(mspf_samples_.size());
    mspf_samples_.clear();
  }
}

FramerateCounter::FramerateCounter(Duration time_per_update) : time_per_update_{time_per_update}
{
  // Reserve space for samples up to 4ms per frame
  mspf_samples_.reserve(time_per_update.count() / 4'000'000 + 1);
}

} // namespace charlie