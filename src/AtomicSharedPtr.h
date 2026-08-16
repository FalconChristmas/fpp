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
#include <mutex>
#include <pthread.h>
#include <unistd.h>
#include <utility>

// A mutex that lends its holder the priority of whoever is waiting.
//
// Blocking (rather than spinning) is what stops the real-time reader from
// livelocking, but on its own it still allows unbounded priority inversion: the
// normal-priority lock holder can be preempted by any of the other
// normal-priority threads while a real-time thread waits behind it, and on a
// single core that costs a scheduling round.  PTHREAD_PRIO_INHERIT bounds the
// wait to the length of the critical section instead, by boosting the holder --
// Linux will promote a SCHED_OTHER owner to the waiter's real-time priority for
// as long as it holds the lock.
class PriorityInheritingMutex {
public:
    PriorityInheritingMutex() {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
#ifdef _POSIX_THREAD_PRIO_INHERIT
        // Deliberately unchecked: where inheritance is unavailable (macOS
        // accepts the call without implementing it) the mutex is still a
        // correct mutex, just without the boost.  Nothing here depends on the
        // boost for correctness.
        pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
#endif
        pthread_mutex_init(&m_mutex, &attr);
        pthread_mutexattr_destroy(&attr);
    }
    ~PriorityInheritingMutex() { pthread_mutex_destroy(&m_mutex); }

    PriorityInheritingMutex(const PriorityInheritingMutex&) = delete;
    PriorityInheritingMutex& operator=(const PriorityInheritingMutex&) = delete;

    void lock() { pthread_mutex_lock(&m_mutex); }
    void unlock() { pthread_mutex_unlock(&m_mutex); }
    bool try_lock() { return pthread_mutex_trylock(&m_mutex) == 0; }

private:
    pthread_mutex_t m_mutex;
};

// A portable atomic<shared_ptr<T>>, deliberately implemented with a mutex.
//
// This used to prefer the C++20 std::atomic<std::shared_ptr<T>> specialization
// (P0718) where the standard library provided it, falling back to the mutex
// only for Apple's libc++, which does not.  That preference is gone: the
// native specialization must not be used here.
//
// libstdc++ implements atomic<shared_ptr<T>> with an internal spinlock whose
// backoff calls sched_yield().  Sequence::m_seqFile is read several times per
// frame from the channel output thread, which runs at real-time priority, and
// is written/read by the frame reader thread at normal priority.  On a
// single-core board that combination livelocks: when the normal-priority
// thread holds the spinlock and is preempted, the real-time thread spins on
// sched_yield(), which by definition only yields to threads of equal or higher
// priority -- so the lock holder can never be scheduled to release it.  It only
// breaks when kernel RT throttling (sched_rt_runtime_us, ~95%) forcibly
// deschedules the spinner.
//
// Measured on a single-core AM335x board: roughly a third of one-minute
// playbacks hit a ~1 s freeze of every non-RT thread on the system, with
// 115,000+ sched_yield() calls from the output thread and almost no other
// syscalls, costing a second of frozen output and leaving the sequence
// permanently ~600 ms out of step with the audio.  Forcing this mutex
// implementation took that from 5 freezes in 16 runs to 0 in 10.  Multi-core
// boards do not show it -- the lock holder simply runs on another CPU -- which
// is why it looked platform-specific rather than like the scheduling bug it is.
//
// A mutex blocks on a futex instead of spinning, so the real-time thread sleeps
// and the lock holder runs.  The cost is one uncontended lock per load(), a
// handful of times per frame, which does not register against a 20 ms frame.
// PriorityInheritingMutex above additionally bounds how long that wait can be.
template<typename T>
class AtomicSharedPtr {
public:
    AtomicSharedPtr() = default;
    AtomicSharedPtr(std::shared_ptr<T> p) :
        m_ptr(std::move(p)) {}

    AtomicSharedPtr(const AtomicSharedPtr&) = delete;
    AtomicSharedPtr& operator=(const AtomicSharedPtr&) = delete;

    std::shared_ptr<T> load(std::memory_order = std::memory_order_seq_cst) const {
        std::lock_guard<PriorityInheritingMutex> lk(m_lock);
        return m_ptr;
    }
    void store(std::shared_ptr<T> p, std::memory_order = std::memory_order_seq_cst) {
        std::lock_guard<PriorityInheritingMutex> lk(m_lock);
        m_ptr = std::move(p);
    }
    AtomicSharedPtr& operator=(std::shared_ptr<T> p) {
        store(std::move(p));
        return *this;
    }

private:
    mutable PriorityInheritingMutex m_lock;
    std::shared_ptr<T> m_ptr;
};
