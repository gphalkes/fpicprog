#ifndef INTERVAL_SET_H_
#define INTERVAL_SET_H_

#include <algorithm>
#include <vector>

template <class T>
class Interval {
 public:
  Interval(T min, T max) : min_(min), max_(max) {
    if (min_ > max_) {
      std::swap(min_, max_);
    }
  }

  bool Contains(const Interval &other) const { return min_ <= other.min_ && max_ >= other.max_; }

  bool Overlaps(const Interval &other) const { return !(max_ <= other.min_ || min_ >= other.max_); }

  bool Connects(const Interval &other) const { return max_ == other.min_ || min_ == other.max_; }

  void Merge(const Interval &other) {
    min_ = std::min(min_, other.min_);
    max_ = std::max(max_, other.max_);
  }

  bool operator<(const Interval &other) const {
    return min_ < other.min_ || (min_ == other.min_ && max_ < other.max_);
  }

  const T &min() const { return min_; }
  const T &max() const { return max_; }

 private:
  T min_;
  T max_;
};

template <class T>
class IntervalSet {
 public:
  void Add(const Interval<T> &interval) {
    if (intervals_.empty()) {
      intervals_.emplace_back(interval);
      return;
    }
    auto iter = std::upper_bound(intervals_.begin(), intervals_.end(), interval);
    if (iter != intervals_.end() && (interval.Overlaps(*iter) || interval.Connects(*iter))) {
      iter->Merge(interval);
      if (iter != intervals_.begin()) {
        auto prev = iter;
        --prev;
        if (prev->Overlaps(*iter) || prev->Connects(*iter)) {
          prev->Merge(*iter);
          intervals_.erase(iter);
        }
      }
    }
    if (iter != intervals_.begin()) {
      --iter;
      if (interval.Overlaps(*iter) || interval.Connects(*iter)) {
        iter->Merge(interval);
      }
    }
  }

  bool Contains(const Interval<T> &interval) const {
    auto iter = std::upper_bound(intervals_.begin(), intervals_.end(), interval);
    if (iter != intervals_.end()) {
      if (iter->Contains(interval)) {
        return true;
      }
    }
    if (iter != intervals_.begin()) {
      --iter;
      return iter->Contains(interval);
    }
    return false;
  }

  bool Overlaps(const Interval<T> &interval) const {
    auto iter = std::upper_bound(intervals_.begin(), intervals_.end(), interval);
    if (iter != intervals_.end()) {
      if (interval.Overlaps(*iter)) {
        return true;
      }
    }
    if (iter != intervals_.begin()) {
      --iter;
      return interval.Overlaps(*iter);
    }
    return false;
  }

  const std::vector<Interval<T>> intervals() const { return intervals_; }

 private:
  std::vector<Interval<T>> intervals_;
};

#endif
