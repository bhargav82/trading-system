#pragma once
#include <stdio.h>
#include <chrono>
#include <atomic>
#include <thread>
#include <pthread.h>
#include <mach/mach.h>
#include <iostream>

template<typename Func, typename... Args>
auto launch_thread(int group_id, int qos, Func&& f, Args&& ...args) noexcept;

bool set_thread_affinity(int group_id, int qos_class);


enum class QoS {
    QOS_CLASS_DEFAULT = 0,// normal priority
    QOS_CLASS_UTILITY = 1, // long running tasks
    QOS_CLASS_BACKGROUND = 2, // low priority background tasks
    QOS_CLASS_USER_INITIATED = 3, // user triggered work

};



/// @brief A general wrapper function to create and launch a thread to execute any function with any number of parameters. Can also be pinned to a core 
/// @tparam Func
/// @tparam ...Args 
/// @param f function to executre
/// @param ...args arguments for given function
/// @param group_id mach thread groups, group together threads that should share a core/L2 cache
/// @param qos task priority, when should OS schedule this thread
/// @return pointer to the thread object (nullptr when failed)
template <typename Func, typename... Args>
auto launch_thread(int group_id, int qos, Func&& f, Args&& ...args) noexcept {
    std::atomic<bool> failed(false);
    std::atomic<bool> running(false);

    auto thread_work = [&]() {
        if (qos < 0 || qos > 3 || !set_thread_affinity(group_id, qos)) {
            std::cerr << "Failed to set thread affinity for " << pthread_self() << std::endl;
            failed = true;
            return;
        }

        std::cout << pthread_self() << " has been 'pinned' to group " << group_id << " with QoS " << qos << std::endl;
        running = true;

        // Execute forwarded function with forwarded arguments
        std::forward<Func>(f)((std::forward<Args>(args))...);
    };
    
    std::thread* thread = new std::thread(thread_work);

    // Thread has been scheduled but hasn't started doing any of its work yet
    while (!failed.load() && !running.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (failed) {
        thread->join();
        delete thread;
        thread = nullptr;
    }

    return thread;
}




/// @brief "Pin" the current thread to a core, Apple doesn't allow this directly but can hint to the OS to schedule threads on given core or shared cache
/// @param group_id group threads that should be on same core/share cache, but don't contend for same data
/// @param qos_class enumerated priority level, choose the priority level based on what function thread is executing
/// @return true if pinning the core was successful, false otherwise
bool set_thread_affinity(int group_id, int qos_class) {
    // Use thread_policy_set with THREAD_AFFINITY_POLICY to suggest threads with same affinity tag to be scheduled on same core or share L2 cache

    // 1. Convert pthread_t to a thread_port_t
    mach_port_t mach_thread = pthread_mach_thread_np(pthread_self());

    // 2. Set group id and qos class
    thread_affinity_policy policy;
    policy.affinity_tag = group_id;
    QoS q = static_cast<QoS>(qos_class);
    if (pthread_set_qos_class_self_np(static_cast<qos_class_t>(q), 0) < 0) {
        return false;
    }
    
    // 3. Set thread policy
    kern_return_t k_ret = thread_policy_set(mach_thread, THREAD_STANDARD_POLICY, thread_policy_t(&policy), THREAD_STANDARD_POLICY_COUNT);
    if (k_ret != KERN_SUCCESS) {
        return false;
    }

    return true;
}