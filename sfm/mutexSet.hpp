
// Copyright (c) 2014 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#define USE_CXX11
#ifdef USE_CXX11
#include <mutex>
using namespace std;
typedef std::mutex mutexT;
typedef std::lock_guard<mutexT> lock_guardT;
#else
#include "third_party/tinythread/fast_mutex.h"
#include "third_party/tinythread/tinythread.h"
using namespace tthread;
typedef tthread::fast_mutex mutexT;
typedef tthread::lock_guard<mutexT> lock_guardT;
#endif

namespace openMVG {

/// ThreadSafe Set thanks to a mutex
template <typename T>
class MutexSet {

public:
    void discard(const T & value) {
      lock_guardT guard(m_Mutex);
      m_Set.insert(value);
    }

    bool isDiscarded(const T & value) const {
      lock_guardT guard(m_Mutex);
      return m_Set.find(value) != m_Set.end();
    }

    size_t size() const {
      lock_guardT guard(m_Mutex);
      return m_Set.size();
    }

private:
    std::set<T> m_Set;
    mutable mutexT m_Mutex;
};

}; // namespace openMVG
