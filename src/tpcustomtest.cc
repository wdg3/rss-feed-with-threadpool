/**
 * File: tpcustomtest.cc
 * ---------------------
 * Unit tests *you* write to exercise the ThreadPool in a variety
 * of ways.
 */

#include <iostream>
#include <sstream>
#include <map>
#include <string>
#include <functional>
#include <cstring>

#include <sys/types.h> // used to count the number of threads
#include <unistd.h>    // used to count the number of threads
#include <dirent.h>    // for opendir, readdir, closedir

#include "thread-pool.h"
#include "thread-pool-release.h"
#include "thread-utils.h"
#include "ostreamlock.h"
using namespace std;

namespace tp = develop;
using tp::ThreadPool;

static void singleThreadNoWaitTest() {
  ThreadPool pool(4);
  pool.schedule([] {
    cout << "This is a test." << endl;
  });
  sleep_for(1000); // emulate wait without actually calling wait (that's a different test)
}

static void singleThreadSingleWaitTest() {
  ThreadPool pool(4);
  pool.schedule([] {
    cout << "This is a test." << endl;
    sleep_for(1000);
  });
}

static void noThreadsDoubleWaitTest() {
  ThreadPool pool(4);
  pool.wait();
  pool.wait();
}

static void reuseThreadPoolTest() {
  ThreadPool pool(4);
  for (size_t i = 0; i < 16; i++) {
    pool.schedule([] {
      cout << oslock << "This is a thread." << endl << osunlock;
      sleep_for(50);
    });
  }
  pool.wait();
  pool.schedule([] {
    cout << "This is the end." << endl;
    sleep_for(1000);
  });
  pool.wait();
}

static void preWaitTest() {
  ThreadPool pool(4);
  sleep_for(2000);
  for (size_t i = 0; i < 4; i++) {
    pool.schedule([] {
      cout << oslock << "This is a thread." << endl << osunlock;
      sleep_for(50);
    });
  }
}

static void stressPoolTest() {
 ThreadPool pool(1000);
 for (size_t j = 0; j < 2; j++) {
   for (size_t i = 0; i < 2048; i++) {
     pool.schedule([i] {
       cout << oslock << "Thread " << i << " starting." << endl << osunlock;
       sleep_for(50);
       cout << oslock << "Thread " << i << " ending." << endl << osunlock;
     });
   }
   pool.wait();
 }
}

struct testEntry {
  string flag;
  function<void(void)> testfn;
};

static void buildMap(map<string, function<void(void)>>& testFunctionMap) {
  testEntry entries[] = {
    {"--single-thread-no-wait", singleThreadNoWaitTest},
    {"--single-thread-single-wait", singleThreadSingleWaitTest},
    {"--no-threads-double-wait", noThreadsDoubleWaitTest},
    {"--reuse-thread-pool", reuseThreadPoolTest},
    {"--stress-pool", stressPoolTest},
    {"--pre-wait", preWaitTest},
  };

  for (const testEntry& entry: entries) {
    testFunctionMap[entry.flag] = entry.testfn;
  }
}

static void executeAll(const map<string, function<void(void)>>& testFunctionMap) {
  for (const auto& entry: testFunctionMap) {
    cout << entry.first << ":" << endl;
    entry.second();
  }
}

int main(int argc, char **argv) {
  if (argc != 2) {
    cout << "Ouch! I need exactly two arguments." << endl;
    return 0;
  }
  
  map<string, function<void(void)>> testFunctionMap;
  buildMap(testFunctionMap);
  string flag = argv[1];
  if (flag == "--all") {
    executeAll(testFunctionMap);
    return 0;
  }
  auto found = testFunctionMap.find(argv[1]);
  if (found == testFunctionMap.end()) {
    cout << "Oops... we don't recognize the flag \"" << argv[1] << "\"." << endl;
    return 0;
  }
  
  found->second();
  return 0;
}
