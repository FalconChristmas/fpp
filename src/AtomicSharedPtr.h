#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include <atomic>
#include <memory>

// A portable atomic<shared_ptr<T>>.
//
// The C++20 std::atomic<std::shared_ptr<T>> specialization (P0718) is only
// implemented in newer standard libraries (libstdc++ in GCC 12+, libc++ 19+).
// Apple's libc++ shipped with current Xcode does not provide it, so using it
// directly fails to compile on macOS.  This wrapper uses the native
// specialization where available and otherwise falls back to a tiny
// mutex-guarded shared_ptr exposing the same load()/store() subset.

#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L

template<typename T>
using AtomicSharedPtr = std::atomic<std::shared_ptr<T>>;

#else

#include <mutex>
#include <utility>

template<typename T>
class AtomicSharedPtr {
public:
    AtomicSharedPtr() = default;
    AtomicSharedPtr(std::shared_ptr<T> p) :
        m_ptr(std::move(p)) {}

    AtomicSharedPtr(const AtomicSharedPtr&) = delete;
    AtomicSharedPtr& operator=(const AtomicSharedPtr&) = delete;

    std::shared_ptr<T> load(std::memory_order = std::memory_order_seq_cst) const {
        std::lock_guard<std::mutex> lk(m_lock);
        return m_ptr;
    }
    void store(std::shared_ptr<T> p, std::memory_order = std::memory_order_seq_cst) {
        std::lock_guard<std::mutex> lk(m_lock);
        m_ptr = std::move(p);
    }
    AtomicSharedPtr& operator=(std::shared_ptr<T> p) {
        store(std::move(p));
        return *this;
    }

private:
    mutable std::mutex m_lock;
    std::shared_ptr<T> m_ptr;
};

#endif
