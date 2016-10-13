#include <chrono>
#include <thread>

static void SleepTicks(int ticks) {
  std::chrono::high_resolution_clock::time_point end;
  std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < ticks; ++i) {
    do {
      end = std::chrono::high_resolution_clock::now();
    } while (end == start);
    start = end;
  }
}

static void PrintWarning() {
  fprintf(stderr, "WARNING: timing accuracy is at worse than 1ms resolution. This means programming will be slow, and could potentially fail.\n");
}

void TestAccuracy() {
  constexpr std::chrono::high_resolution_clock::duration tick(1);
  if (tick > std::chrono::milliseconds(1)) {
    PrintWarning();
    return;
  }

  std::chrono::high_resolution_clock::duration duration;
  std::chrono::high_resolution_clock::time_point start;

  // Get a first estimate of the clock resolution. Using the value of tick may be way off.
  start = std::chrono::high_resolution_clock::now();
  SleepTicks(10);
  duration = (std::chrono::high_resolution_clock::now() - start) / 10;

  // Get a more accurate estimate of the clock resolution.
  int iterations = std::chrono::milliseconds(10) / duration;
  start = std::chrono::high_resolution_clock::now();
  SleepTicks(iterations);
  duration = (std::chrono::high_resolution_clock::now() - start) / iterations;

  // FIXME: use print_msg
  fprintf(stderr, "High resolution clock ticks at: %ldns\n", (long) std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
  if (duration > std::chrono::milliseconds(1)) {
    PrintWarning();
    return;
  }

  // Sleep a first time to make sure we start at tick boundary.
  std::this_thread::sleep_for(std::chrono::nanoseconds(1000000));
    // Use nanoseconds to specify the duration, to make the timing potentially more accurate.
  constexpr std::chrono::nanoseconds test_sleep(std::chrono::milliseconds(1));
  start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 10 ; ++i) {
    std::this_thread::sleep_for(test_sleep);
  }
  duration = (std::chrono::high_resolution_clock::now() - start) / 10;
  // FIXME: use print_msg
  fprintf(stderr, "Measured sleep of %dms (avg): %dns\n",
    (int) std::chrono::duration_cast<std::chrono::milliseconds>(test_sleep).count(),
    (int) std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
  // FIXME: we can do busy waiting in this case, perhaps using the sleep timer if it ticks < several ms.
  if (duration - test_sleep > std::chrono::milliseconds(1)) {
    PrintWarning();
    return;
  }
}

int main(int, char**) {
  TestAccuracy();
}
