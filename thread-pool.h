/**
 * File: thread-pool.h
 * -------------------
 * Exports a ThreadPool abstraction, which manages a finite pool
 * of worker threads that collaboratively work through a sequence of tasks.
 * As each task is scheduled, the ThreadPool waits for at least
 * one worker thread to be free and then assigns that task to that worker.  
 * Threads are scheduled and served in a FIFO manner, and tasks need to
 * take the form of thunks, which are zero-argument thread routines.
 */

#ifndef _thread_pool_
#define _thread_pool_

#include <cstdlib>
#include <functional>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <iostream>
#include <condition_variable>
#include <atomic>
#include "semaphore.h"
// place additional #include statements here

namespace develop {

class ThreadPool {
 public:

/**
 * Constructs a ThreadPool configured to spawn up to the specified
 * number of threads.
 */
  ThreadPool(size_t numThreads);

/**
 * Destroys the ThreadPool class
 */
  ~ThreadPool();

/**
 * Schedules the provided thunk (which is something that can
 * be invoked as a zero-argument function without a return value)
 * to be executed by one of the ThreadPool's threads as soon as
 * all previously scheduled thunks have been handled.
 */
  void schedule(const std::function<void(void)>& thunk);
  
/**
 * Blocks and waits until all previously scheduled thunks
 * have been executed in full.
 */
  void wait();

 private:
  
  typedef std::function<void(void)> thunk_t;
  typedef struct worker_t {
      size_t id;
      int available;
      semaphore job_waiting;
      thunk_t job;
  } worker_t;

/**
 * Dispatcher thread runs in a loop taking thunks from the queue and
 * scheduling them with workers.
 */
  void dispatcher();

/**
 *A worker thread waits for a job then executes that job.
 */
  void worker(worker_t& worker);
  
  std::thread dt;
  std::vector<std::thread> wts;
  std::vector<worker_t> workers;

  semaphore available_workers;
  semaphore queue_not_empty;

  std::atomic<size_t> num_workers;
  std::atomic<size_t> num_available_workers;

  std::mutex cv_lock;
  std::condition_variable_any all_available;

  bool done;

  std::mutex q_lock;
  std::queue<thunk_t> scheduled;
  
  ThreadPool(const ThreadPool& original) = delete;
  ThreadPool& operator=(const ThreadPool& rhs) = delete;
};

#endif

}
