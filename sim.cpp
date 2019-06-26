#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <numeric>
#include <queue>
#include <random>
#include <thread>
#include <vector>

using namespace std;

int gNum{0};
std::mutex gNumMutex;
std::mutex gPrintMutex;

mt19937 createRandomEngine() {
  random_device rd;
  array<int, mt19937::state_size> seed_data;
  generate_n(seed_data.data(), seed_data.size(), ref(rd));
  seed_seq seq(begin(seed_data), end(seed_data));
  return mt19937(seq);
}

namespace Timing {


class TimerManager {
private:
  using TimePoint = chrono::high_resolution_clock::time_point;
  struct Timer {
    TimePoint endTime;
    int num;
    Timer() = default;
    Timer(TimePoint et, int n) : endTime(et), num(n) {}
    bool operator<(const Timer &other) const { return endTime < other.endTime; }
    bool operator>(const Timer &other) const { return endTime > other.endTime; }
  };
  std::priority_queue<Timer, vector<Timer>, greater<Timer>> timerData;
  std::condition_variable cv;
  std::mutex timerDataMutex;
  std::thread thr;
  void waitForData();
  void internalRun();
  void pruneTimers();
  void timerFinished(const Timer &timer);
public:
  void createTimer(chrono::milliseconds timerDuration, int timerId);
  void run();
  ~TimerManager();
};

} // namespace Timing

namespace Timing {

void TimerManager::waitForData() {
  // {
  //   unique_lock<mutex> printLock(gPrintMutex);
  //   cout << '[' << chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count() << "] TimerManager is waiting for data\n";
  // }
  unique_lock<mutex> timerDataLock(timerDataMutex);
  cv.wait(timerDataLock, [this](){ return !timerData.empty(); });
}

void TimerManager::internalRun() {
  while (true) {
    if (timerData.empty()) {
      waitForData();
    }

    while (!timerData.empty()) {
      // We have data
      // Print it
      // priority_queue<Timer, vector<Timer>, greater<Timer>> duplicateData;
      // {
      //   {
      //     unique_lock<mutex> timerDataLock(timerDataMutex);
      //     duplicateData = timerData;
      //   }
      //   unique_lock<mutex> printLock(gPrintMutex);
      //   cout << '[' << chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count() << "] TimerManager has data: ";
      //   while (!duplicateData.empty()) {
      //     cout << chrono::duration_cast<chrono::milliseconds>(duplicateData.top().endTime-chrono::high_resolution_clock::now()).count() << ", ";
      //     duplicateData.pop();
      //   }
      //   cout << '\n';
      // }

      // Print info about shortest timer
      // if (!timerData.empty()) {
      //   unique_lock<mutex> printLock(gPrintMutex);
      //   auto diff = timerData.top().endTime-chrono::high_resolution_clock::now();
      //   cout << '[' << chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count() << "] TimerManager waiting for " << chrono::duration_cast<chrono::milliseconds>(diff).count() << "ms\n";
      // }

      // Wait on shortest timer
      {
        unique_lock<mutex> timerDataLock(timerDataMutex);
        // Wait until we've reached our target time or someone has inserted a timer that will expire sooner
        cv.wait_until(timerDataLock, timerData.top().endTime, [this](){ return chrono::high_resolution_clock::now() >= timerData.top().endTime; });
      }
      if (chrono::high_resolution_clock::now() < timerData.top().endTime) {
        // Woken up for a timer that will expire sooner
        // unique_lock<mutex> printLock(gPrintMutex);
        // cout << '[' << chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count() << "] TimerManager woken for newer timer\n";
      } else {
        // Woken up because our timer finished
        {
          // unique_lock<mutex> printLock(gPrintMutex);
          // cout << '[' << chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count() << "] TimerManager woken because timer expired\n";
        }
        pruneTimers();
      }
    }
  }
}

void TimerManager::pruneTimers() {
  while (!timerData.empty() && timerData.top().endTime <= chrono::high_resolution_clock::now()) {
    Timer t;
    {
      unique_lock<mutex> timerDataLock(timerDataMutex);
      if (!timerData.empty()) {
        t = timerData.top();
        timerData.pop();
      }
    }
    timerFinished(t);
  }
}

void TimerManager::timerFinished(const Timer &timer) {
  unique_lock<mutex> printLock(gPrintMutex);
  cout << '[' << chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count() << "] done " << timer.num << '\n';
}

void TimerManager::createTimer(chrono::milliseconds timerDuration, int timerId) {
  {
    unique_lock<mutex> printLock(gPrintMutex);
    cout << '[' << chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count() << "] create " << timerId << ' ' << timerDuration.count() << '\n';
  }
  auto timerEndTimePoint = std::chrono::high_resolution_clock::now() + timerDuration;
  TimePoint prevTime;
  bool shouldNotify=false;
  {
    unique_lock<mutex> timerDataLock(timerDataMutex);
    prevTime = (timerData.empty() ? TimePoint::max() : timerData.top().endTime);
    timerData.emplace(timerEndTimePoint, timerId);
    if (timerData.top().endTime < prevTime) {
      // New timer ends sooner than the previous soonest
      // Wake up the thread to handle this
      shouldNotify = true;
    }
  }
  if (shouldNotify) {
    cv.notify_one();
  }
}

void TimerManager::run() {
  thr = thread(&TimerManager::internalRun, this);
}

TimerManager::~TimerManager() {
  thr.join();
}

} // namespace Timing

void timerCreation(Timing::TimerManager &timerManager) {
  auto eng = createRandomEngine();
  uniform_int_distribution<int> sleepLengthDist(10, 100);
  uniform_int_distribution<int> timerLengthDist(10, 100);
  while (true) {
    // Sleep for a bit
    const int sleepLength = sleepLengthDist(eng);
    this_thread::sleep_for(chrono::milliseconds(sleepLength));

    // Schedule a timer
    const int timerLength = timerLengthDist(eng);
    int timerId;
    {
      lock_guard<mutex> numLockGuard(gNumMutex);
      timerId = gNum;
      ++gNum;
    }
    timerManager.createTimer(chrono::milliseconds(timerLength), timerId);
  }
}

int main() {
  constexpr int kThreadCount = 2000;
  Timing::TimerManager timerManager;
  timerManager.run();
  vector<thread> threads;
  for (int i=0; i<kThreadCount; ++i) {
    threads.emplace_back(timerCreation, std::ref(timerManager));
  }
  for (auto &thr : threads) {
    thr.join();
  }
  return 0;
}