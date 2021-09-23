//
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2020 Project PAI Foundation
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//


#include "sync.h"

#include "util.h"
#include "utilstrencodings.h"

#include <stdio.h>

#include <boost/thread.hpp>

#ifdef DEBUG_LOCKCONTENTION
void PrintLockContention(const char* pszName, const char* pszFile, int nLine)
{
    LogPrintf("LOCKCONTENTION: %s\n", pszName);
    LogPrintf("Locker: %s:%d\n", pszFile, nLine);
}
#endif /* DEBUG_LOCKCONTENTION */

#ifdef DEBUG_LOCKTHREADID
#include <thread>
std::string getThreadId() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}
#endif

#ifdef DEBUG_LOCKORDER
//
// Early deadlock detection.
// Problem being solved:
//    Thread 1 locks  A, then B, then C
//    Thread 2 locks  D, then C, then A
//     --> may result in deadlock between the two threads, depending on when they run.
// Solution implemented here:
// Keep track of pairs of locks: (A before B), (A before C), etc.
// Complain if any thread tries to lock in a different order.
//

struct CLockLocation {
    CLockLocation(const char* pszName, const char* pszFile, int nLine, const std::string& threadIdIn, bool fRecursiveIn, bool fTryIn)
    {
        mutexName = pszName;
        sourceFile = pszFile;
        sourceLine = nLine;
        threadId = threadIdIn;
        fRecursive = fRecursiveIn;
        fTry = fTryIn;
#ifdef DEBUG_LOCKTHREADID
        threadId = getThreadId();
#endif
    }

    std::string ToString() const
    {
        return mutexName + "  " + sourceFile + ":" + itostr(sourceLine) + " (" + (threadId.size() > 0 ? threadId : "<unknown>") + ")" + (fTry ? " (TRY)" : "") + (fRecursive ? " (RECURSIVE)" : " (NON-RECURSIVE)");
    }

    bool fTry;
    bool fRecursive;
    std::string threadId;
private:
    std::string mutexName;
    std::string sourceFile;
    int sourceLine;
#ifdef DEBUG_LOCKTHREADID
    std::string threadId;
#endif
};

typedef std::vector<std::pair<void*, CLockLocation> > LockStack;
typedef std::map<std::pair<void*, void*>, LockStack> LockOrders;
typedef std::set<std::pair<void*, void*> > InvLockOrders;

struct LockData {
    // Very ugly hack: as the global constructs and destructors run single
    // threaded, we use this boolean to know whether LockData still exists,
    // as DeleteLock can get called by global CCriticalSection destructors
    // after LockData disappears.
    bool available;
    LockData() : available(true) {}
    ~LockData() { available = false; }

    LockOrders lockorders;
    InvLockOrders invlockorders;
    boost::mutex dd_mutex;
} static lockdata;

boost::thread_specific_ptr<LockStack> lockstack;

static void potential_deadlock_detected(const std::pair<void*, void*>& mismatch, const LockStack& s1, const LockStack& s2)
{
    LogPrintf("POTENTIAL DEADLOCK DETECTED\n");
    LogPrintf("Previous lock order was:\n");
    for (const std::pair<void*, CLockLocation> & i : s2) {
        if (i.first == mismatch.first) {
            LogPrintf(" (1)");
        }
        if (i.first == mismatch.second) {
            LogPrintf(" (2)");
        }
        LogPrintf(" %s\n", i.second.ToString());
    }
    LogPrintf("Current lock order is:\n");
    for (const std::pair<void*, CLockLocation> & i : s1) {
        if (i.first == mismatch.first) {
            LogPrintf(" (1)");
        }
        if (i.first == mismatch.second) {
            LogPrintf(" (2)");
        }
        LogPrintf(" %s\n", i.second.ToString());
    }
    assert(false);
}

static void double_lock_detected(const void* mutex, const LockStack& lock_stack)
{
    LogPrintf("DOUBLE LOCK DETECTED\n");
    LogPrintf("Lock order:\n");
    for (const std::pair<void*, CLockLocation> & i : lock_stack) {
        if (i.first == mutex) {
            LogPrintf(" (*)"); /* Continued */
        }
        LogPrintf(" %s\n", i.second.ToString());
    }
    assert(false);
}

static void push_lock(void* c, const CLockLocation& locklocation)
{
    if (lockstack.get() == nullptr)
        lockstack.reset(new LockStack);

    boost::unique_lock<boost::mutex> lock(lockdata.dd_mutex);

    (*lockstack).push_back(std::make_pair(c, locklocation));

    for (size_t j = 0; j < (*lockstack).size() - 1; ++j) {
        const std::pair<void*, CLockLocation>& i = (*lockstack)[j];
        if (i.first == c) {
            if (locklocation.fRecursive)
                break;

            double_lock_detected(c, (*lockstack));
        }

        std::pair<void*, void*> p1 = std::make_pair(i.first, c);
        if (lockdata.lockorders.count(p1))
            continue;

        lockdata.lockorders[p1] = (*lockstack);

        std::pair<void*, void*> p2 = std::make_pair(c, i.first);
        lockdata.invlockorders.insert(p2);

        auto deadlock_candidate_stack = lockdata.lockorders.find(p2);
        if (deadlock_candidate_stack != lockdata.lockorders.end()) {
            // this should not happen, however it's better to double check if it did
            if (deadlock_candidate_stack->second.size() == 0)
                potential_deadlock_detected(p1, deadlock_candidate_stack->second, lockdata.lockorders[p1]);

            // there is a deadlock only if having different threads or not recursive mutex on the same thread
            if ((deadlock_candidate_stack->second[0].second.threadId != locklocation.threadId) || (!locklocation.fRecursive))
                potential_deadlock_detected(p1, deadlock_candidate_stack->second, lockdata.lockorders[p1]);
        }
    }
}

static void pop_lock()
{
    (*lockstack).pop_back();
}

#define DEBUG_SHOW_LOCK_THREAD_ID
void EnterCritical(const char* pszName, const char* pszFile, int nLine, void* cs, bool fRecursive, bool fTry)
{
#ifdef DEBUG_SHOW_LOCK_THREAD_ID
    std::stringstream ss;
    ss << boost::this_thread::get_id();
    push_lock(cs, CLockLocation(pszName, pszFile, nLine, ss.str(), fRecursive, fTry));
#else
    push_lock(cs, CLockLocation(pszName, pszFile, nLine, "", fRecursive, fTry));
#endif
}

void LeaveCritical()
{
    pop_lock();
}

std::string LocksHeld()
{
    std::string result;
    for (const std::pair<void*, CLockLocation> & i : *lockstack)
        result += i.second.ToString() + std::string("\n");
    return result;
}

void AssertLockHeldInternal(const char* pszName, const char* pszFile, int nLine, void* cs)
{
    for (const std::pair<void*, CLockLocation> & i : *lockstack)
        if (i.first == cs)
            return;
    fprintf(stderr, "Assertion failed: lock %s not held in %s:%i; locks held:\n%s", pszName, pszFile, nLine, LocksHeld().c_str());
    abort();
}

void DeleteLock(void* cs)
{
    if (!lockdata.available) {
        // We're already shutting down.
        return;
    }
    boost::unique_lock<boost::mutex> lock(lockdata.dd_mutex);
    std::pair<void*, void*> item = std::make_pair(cs, nullptr);
    LockOrders::iterator it = lockdata.lockorders.lower_bound(item);
    while (it != lockdata.lockorders.end() && it->first.first == cs) {
        std::pair<void*, void*> invitem = std::make_pair(it->first.second, it->first.first);
        lockdata.invlockorders.erase(invitem);
        lockdata.lockorders.erase(it++);
    }
    InvLockOrders::iterator invit = lockdata.invlockorders.lower_bound(item);
    while (invit != lockdata.invlockorders.end() && invit->first == cs) {
        std::pair<void*, void*> invinvitem = std::make_pair(invit->second, invit->first);
        lockdata.lockorders.erase(invinvitem);
        lockdata.invlockorders.erase(invit++);
    }
}

#endif /* DEBUG_LOCKORDER */
