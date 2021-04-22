#include <queue>
#include <thread>
#include <mutex>
#include <iostream>
#include <condition_variable>
#include <unistd.h>

using namespace std;

struct someData {
    int data;
};

int calculate(someData data) {
    usleep(100000);
    return data.data;
}

void getData(queue<someData>& dataQueue, int amount) {
    for (int i = 0; i < amount; i++) {
	struct someData d;
	d.data = i;
	dataQueue.push(d);
    }
}

const int kNumThreads = 8;
const int kDataAmount = 1234;

void parallelCalculate(queue<someData>& dataQueue, mutex& q_lock, condition_variable_any& print_cv,
		       int& num_popped, int& num_printed, mutex& print_lock) {
    while (true) {
	q_lock.lock();
	if (dataQueue.size() == 0) {
	    q_lock.unlock();
	    return;
	}
	struct someData d = dataQueue.front();
	dataQueue.pop();
	int current = num_popped;
	num_popped++;
	q_lock.unlock();
	int value = calculate(d);
	print_lock.lock();
	print_cv.wait(print_lock, [current, &num_printed] { return num_printed == current; });
	cout << value << endl;
	num_printed++;
	print_cv.notify_all();
	print_lock.unlock();
    }
}

int main() {
    queue<someData> dataQueue;
    getData(dataQueue, kDataAmount);
    vector<thread> threads;
    mutex q_lock;
    mutex print_lock;
    condition_variable_any print_cv;
    int num_popped = 0;
    int num_printed = 0;

    for (int i = 0; i < kNumThreads; i++) {
	threads.push_back(thread(parallelCalculate, ref(dataQueue), ref(q_lock), ref(print_cv),
				 ref(num_popped), ref(num_printed), ref(print_lock)));
    }

    for (int i = 0; i < kNumThreads; i++) {
	threads[i].join();
    }

    return 0;
}
