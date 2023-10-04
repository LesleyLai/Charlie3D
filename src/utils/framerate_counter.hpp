#ifndef CHARLIE3D_FRAMERATE_COUNTER_HPP
#define CHARLIE3D_FRAMERATE_COUNTER_HPP

#include <chrono>
#include <vector>

namespace charlie {

class FramerateCounter {
  using Duration = std::chrono::steady_clock::duration;

  Duration time_per_update_;
  Duration accumulated_time_{};
  std::vector<float> mspf_samples_;

public:
  float average_ms_per_frame = 0;

  explicit FramerateCounter(Duration time_per_update = std::chrono::milliseconds{100});

  void update(Duration delta_time);
};
} // namespace charlie

#endif // CHARLIE3D_FRAMERATECOUNTER_HPP
