/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include <stdio.h>
#include <unistd.h>
#include "thread-pool.h"
#include "ostreamlock.h"

using namespace std;
using develop::ThreadPool;

ThreadPool::ThreadPool(size_t numThreads):
    wts(numThreads), workers(numThreads), num_workers(0), num_available_workers(0), done(false)
{
    // Spawn dispatcher thread
    dt = thread([this]() { dispatcher(); });
    for (size_t i = 0; i < numThreads; i++) {
	// Spawn appropriate number of worker threads, initialize struct
        workers[i].id = i;
        workers[i].available = true;
        workers[i].job = nullptr;
        wts[i] = thread([this, i]() { worker(workers[i]); });
    }
}
void ThreadPool::schedule(const thunk_t& thunk) {
    // Lock around the queue when we modify it
    q_lock.lock();
    scheduled.push(thunk);
    q_lock.unlock();
    queue_not_empty.signal(); // Signal that we have something in the queue
}

void ThreadPool::dispatcher() {
    while (true) {
	// Wait for there to be a job and available worker
        queue_not_empty.wait();
        available_workers.wait();
	// If we actually got here because of a destructor signal, exit
	if (done) {
	    break;
	}
        size_t worker_id;
        for (worker_t& w : workers) {
	    // Find an available worker, mark it unvailable
            if (w.available) {
                worker_id = w.id;
                w.available = false;
		num_available_workers--; // <atomic>!
                break;
            }
        }
        worker_t& worker = workers[worker_id];
        
	// Lock around the queue before retrieving the first thunk
        q_lock.lock();
        const thunk_t thunk = scheduled.front();
        scheduled.pop();
        q_lock.unlock();
        
	// Put the thunk in the worker's struct and tell it it has a job waiting
	worker.job = thunk;
        worker.job_waiting.signal();
    }
}

void ThreadPool::worker(worker_t& worker) {
    num_workers++;
    num_available_workers++; // <atomic>
    while (true) {
	// Marks itself available and waits for a job
	worker.available = true;
	available_workers.signal();
        worker.job_waiting.wait();
	if (done) {
	    break; // If we got here because of a destructor signal, exit
	}
        worker.job(); // Run the job, signal the wait() cv if it's the last to finish something
	num_available_workers++;
	if (num_available_workers == num_workers) {
	    cv_lock.lock();
	    all_available.notify_all();
	    cv_lock.unlock();
	}
    }
}

void ThreadPool::wait() {
    // Wait for all workers to be available with nothing in the queue
    lock_guard<mutex> lg(cv_lock);
    all_available.wait(cv_lock, [this] { return (num_available_workers == num_workers) && (scheduled.size() == 0); }); 
}

ThreadPool::~ThreadPool() {
    wait(); // Wait for the pool to clear out
    done = true; // Loop-breaking condition for dispatcher and workers
    for (size_t i = 0; i < wts.size(); i++) {
	// Break each worker out of the job waiting block so it hits the
	// if (done) segment, then join it
	workers[i].job_waiting.signal();
	wts[i].join();
    }
    // Do the same for the dispatcher thread
    queue_not_empty.signal();
    available_workers.signal();
    dt.join();
}

