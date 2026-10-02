#ifndef KS_MUL_TIMER_H
#define KS_MUL_TIMER_H

#include <iostream>
#include <string>
#include <unordered_map>

#include <scl/util/measurement.h>
#include <scl/util/time.h>

class ProofTimer final {
 public:
  void start(const std::string& name) {
    m_start_times[name] = scl::util::Time::now();
  }

  void stop(const std::string& name) {
    m_measurements[name].addSample(scl::util::Time::now() -
                                   m_start_times.at(name));
  }

  void print(const std::string& name,
             const std::optional<std::string>& title = {}) {
    if (title.has_value()) {
      std::cout << title.value() << "\n";
    } else {
      std::cout << name << "\n";
    }

    const auto m = m_measurements.at(name);

    std::cout << "  median: " << scl::util::timeToMillis(m.median()) << " ms\n";
    std::cout << "  stddev: " << scl::util::timeToMillis(m.stddev()) << " ms\n";
  }

  scl::util::TimeMeasurement get(const std::string& name) {
    return m_measurements.at(name);
  }

 private:
  std::unordered_map<std::string, scl::util::Time::TimePoint> m_start_times;
  std::unordered_map<std::string, scl::util::TimeMeasurement> m_measurements;
};

#define PT_START(pt_, n_) \
  do {                    \
    if ((pt_)) {          \
      (pt_)->start((n_)); \
    }                     \
  } while (0)

#define PT_END(pt_, n_)  \
  do {                   \
    if ((pt_)) {         \
      (pt_)->stop((n_)); \
    }                    \
  } while (0)

#endif  // KS_MUL_TIMER_H
