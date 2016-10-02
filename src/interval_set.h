/* Copyright (C) 2016 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
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
  bool IsEmpty() const { return max_ <= min_; }

 private:
  T min_;
  T max_;
};

template <class T>
class IntervalSet {
 public:
  void Add(Interval<T> interval) {
    if (interval.IsEmpty()) {
      return;
    }
    if (intervals_.empty()) {
      intervals_.emplace_back(interval);
      return;
    }
    // FIXME: this can be sped up by finding the beginning using a binary search algorithm like
    // std::lower_bound
    for (auto iter = intervals_.begin(); iter != intervals_.end();) {
      if (interval.max() < iter->min()) {
        intervals_.emplace(iter, interval);
        return;
      } else if (interval.max() == iter->min()) {
        iter->Merge(interval);
        return;
      } else if (interval.min() <= iter->max()) {
        interval.Merge(*iter);
        iter = intervals_.erase(iter);
      } else {
        ++iter;
      }
    }
    intervals_.emplace_back(interval);
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
