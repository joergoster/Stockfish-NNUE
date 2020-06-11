#ifndef _MULTI_THINK_
#define _MULTI_THINK_

#if defined(EVAL_LEARN)

#include <atomic>
#include <functional>

#include "../misc.h"
#include "../learn/learn.h"
#include "../thread_win32_osx.h"

// 棋譜からの学習や、自ら思考させて定跡を生成するときなど、
// 複数スレッドが個別にSearch::think()を呼び出したいときに用いるヘルパクラス。
// このクラスを派生させて用いる。
// When learning from a game record, or generating a fixed path by letting it think for itself, etc.
// Helpers to use when multiple threads want to call Search::think() separately.
// The class from which this class is derived.
struct MultiThink {

  MultiThink() : prng(21120903) { loop_count = 0; }

  // マスタースレッドからこの関数を呼び出すと、スレッドがそれぞれ思考して、
  // 思考終了条件を満たしたところで制御を返す。
  // 他にやってくれること。
  // ・各スレッドがLearner::search(),qsearch()を呼び出しても安全なように
  // 　置換表をスレッドごとに分離してくれる。(終了後、元に戻してくれる。)
  // ・bookはon the flyモードだとthread safeではないので、このモードを一時的に
  // 　オフにしてくれる。
  // [要件]
  // 1) thread_worker()のオーバーライド
  // 2) set_loop_max()でループ回数の設定
  // 3) 定期的にcallbackされる関数を設定する(必要なら)
  //   callback_funcとcallback_interval
  // When you call this function from the master thread, each thread thinks and
  // return control when the thought termination condition is met.
  // What else it does.
  // When each thread calls Learner::search(),qsearch() Just to be safe.
  // It separates the replacement table into separate threads. (It will return to the original one when it is finished.)
  // The book is not thread safe in on the fly mode.
  // The book is not thread safe when it is in on the fly mode, so you can temporarily change this mode to turn it off.
  // [Requirement]
  // 1) Override thread_worker()
  // 2) Set the number of loops in set_loop_max()
  // 3) Set a function to be called back periodically (if necessary)
  // callback_func and callback_interval
  void go_think();

  // 派生クラス側で初期化したいものがあればこれをoverrideしておけば、
  // go_think()で初期化が終わったタイミングで呼び出される。
  // 定跡の読み込みなどはそのタイミングで行うと良い。
  // If there is anything you want to initialize on the derived class side,
  // you can override it, and then you can use
  // Called when the initialization is done in go_think().
  // It is recommended that you do things such as
  // reading of the constant at that time.
  virtual void init() {}

  // go_think()したときにスレッドを生成して呼び出されるthread worker
  // これをoverrideして用いる。
  // The thread worker to be invoked by creating a thread when go_think() is done.
  // This is used as an override.
  virtual void thread_worker(size_t thread_id) = 0;

  // go_think()したときにcallback_seconds[秒]ごとにcallbackされる。
  // The callback will be done every callback_seconds when go_think() is done
  std::function<void()> callback_func;
  uint64_t callback_seconds = 600;

  // workerが処理する(Search::think()を呼び出す)回数を設定する。
  // Set the number of times the worker will process (call Search::think())
  void set_loop_max(uint64_t loop_max_) { loop_max = loop_max_; }
	
  // set_loop_max()で設定した値を取得する。
  // Get the value set in set_loop_max().
  uint64_t get_loop_max() const { return loop_max; }

  // [ASYNC] ループカウンターの値を取り出して、取り出し後にループカウンターを加算する。
  // もしループカウンターがloop_maxに達していたらUINT64_MAXを返す。
  // 局面を生成する場合などは、局面を生成するタイミングでこの関数を呼び出すようにしないと、
  // 生成した局面数と、カウンターの値が一致しなくなってしまうので注意すること。
  // [ASYNC] Retrieves the value of the loop counter, and increments
  // the loop counter after the retrieval.
  // If the loop counter reaches loop_max, it returns UINT64_MAX.
  // If this function is not called at the time of creating a phase,
  // such as when generating a game, it must be called at the time of creating the game.
  // Note that the number of phases generated does not match the value of the counter.
  uint64_t get_next_loop_count() {
    std::unique_lock<std::mutex> lk(loop_mutex);

    if (loop_count >= loop_max)
      return UINT64_MAX;

    return loop_count++;
	}

  // [ASYNC] 処理した個数を返す用。呼び出されるごとにインクリメントされたカウンターが返る。
  // [ASYNC] To return the number of items processed. Each time it is called,
  // it returns an incremented counter.
  uint64_t get_done_count() {
    std::unique_lock<std::mutex> lk(loop_mutex);
    return ++done_count;
  }

  // worker threadがI/Oにアクセスするときのmutex
  // Mutex for worker thread to access I/O
  std::mutex io_mutex;

protected:
  // 乱数発生器本体
  // Random number generator
  AsyncPRNG prng;

private:
  // workerが処理する(Search::think()を呼び出す)回数
  // Number of times worker will process (call Search::think())
  std::atomic<uint64_t> loop_max;

  // workerが処理した(Search::think()を呼び出した)回数
  // Number of times worker processed (called Search::think())
  std::atomic<uint64_t> loop_count;

  // 処理した回数を返す用。
  // To return the number of times processed
  std::atomic<uint64_t> done_count;

  // ↑の変数を変更するときのmutex
  // Mutex for changing above variables
  std::mutex loop_mutex;

  // スレッドの終了フラグ。
  // vector<bool>にすると複数スレッドから書き換えようとしたときに正しく反映されないことがある…はず。
  // End of thread flag.
  // vector<bool> may not be properly reflected when trying to rewrite from multiple threads...should be.
  typedef uint8_t Flag;
  std::vector<Flag> thread_finished;
};

// idle時間にtaskを処理する仕組み。
// masterは好きなときにpush_task_async()でtaskを渡す。
// slaveは暇なときにon_idle()を実行すると、taskを一つ取り出してqueueがなくなるまで実行を続ける。
// MultiThinkのthread workerをmaster-slave方式で書きたいときに用いると便利。
// A mechanism to process tasks at idle time.
// master passes tasks to the master whenever he wants to with push_task_async().
// When slave runs on_idle() in its spare time, it takes out one task and keeps running until the queue runs out.
// You can use this if you want to write MultiThink thread workers in the master-slave way.
struct TaskDispatcher {

  typedef std::function<void(size_t /* thread_id */)> Task;

  // slaveはidle中にこの関数を呼び出す。
  // slave will call this function during idle
  void on_idle(size_t thread_id)
  {
    Task task;
    while ((task = get_task_async()) != nullptr)
      task(thread_id);

    sleep(1);
  }

  // [ASYNC] taskを一つ積む。
  // Build a single [ASYNC] task
  void push_task_async(Task task)
  {
    std::unique_lock<std::mutex> lk(task_mutex);
    tasks.push_back(task);
  }

  // task用の配列の要素をsize分だけ事前に確保する。
  // Allocate the elements of the array for the task in advance by size
  void task_reserve(size_t size)
  {
    tasks.reserve(size);
  }

protected:
  // taskの集合
  // Set of tasks
  std::vector<Task> tasks;

  // [ASYNC] taskを一つ取り出す。on_idle()から呼び出される。
  // [ASYNC] Extract a task, which is called by on_idle()
  Task get_task_async()
  {
    std::unique_lock<std::mutex> lk(task_mutex);

    if (tasks.size() == 0)
      return nullptr;

    Task task = *tasks.rbegin();
    tasks.pop_back();

    return task;
  }

  // tasksにアクセスするとき用のmutex
  // Mutex for accessing tasks
  std::mutex task_mutex;
};

#endif // defined(EVAL_LEARN) && defined(YANEURAOU_2018_OTAFUKU_ENGINE)

#endif
