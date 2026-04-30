#include <pthread.h>
#include <mach/mach.h>
#include <stdio.h>
#include <stdexcept>

enum class QoS {
    QOS_CLASS_DEFAULT = 0,// normal priority
    QOS_CLASS_UTILITY = 1, // long running tasks
    QOS_CLASS_BACKGROUND = 2, // low priority background tasks
    QOS_CLASS_USER_INITIATED = 3, // user triggered work

};

/// @brief "Pin" the current thread to a core, Apple doesn't allow this directly but can hint to the OS to schedule threads on given core or shared cache
/// @param group_id group threads that should be on same core/share cache, but don't contend for same data
/// @param qos_class enumerated priority level, choose the priority level based on what function thread is executing
/// @return true if pinning the core was successful, false otherwise
void set_thread_affinity(int group_id, int qos_class) {
    // Use thread_policy_set with THREAD_AFFINITY_POLICY to suggest threads with same affinity tag to be scheduled on same core or share L2 cache

    // 1. Convert pthread_t to a thread_port_t
    mach_port_t mach_thread = pthread_mach_thread_np(pthread_self());

    // 2. Set group id and qos class
    thread_affinity_policy policy;
    policy.affinity_tag = group_id;
    if (qos_class < 0 || qos_class > 3) {
        qos_class = 0;
    }
    QoS q = static_cast<QoS>(qos_class);
    if (pthread_set_qos_class_self_np(static_cast<qos_class_t>(q), 0) < 0) {
        throw std::runtime_error("thread.h: Could not set QoS class.");
    }
    
    // 3. Set thread policy
    kern_return_t k_ret = thread_policy_set(mach_thread, THREAD_STANDARD_POLICY, thread_policy_t(&policy), THREAD_STANDARD_POLICY_COUNT);
    if (k_ret != KERN_SUCCESS) {
        throw std::runtime_error("thread.h: Could not set thread policy.");
    }
}