#include "../types.h"

#if defined(EVAL_LEARN)

#include <thread>

#include "multi_think.h"
#include "../tt.h"
#include "../uci.h"


void MultiThink::go_think() {

	// あとでOptionsの設定を復元するためにコピーで保持しておく。
	// Keep a copy to restore the Options settings later
	auto oldOptions = Options;

	// 定跡を用いる場合、on the flyで行なうとすごく時間がかかる＆ファイルアクセスを行なう部分が
	// thread safeではないので、メモリに丸読みされている状態であることをここで保証する。
	// If you are using a constant, it takes a lot of time to do it on the fly & the part of the file access is
	// thread safe, so we guarantee here that it is read in memory.
	Options["BookOnTheFly"] = std::string("false");

	// 評価関数の読み込み等
	// learnコマンドの場合、評価関数読み込み後に評価関数の値を補正している可能性があるので、
	// メモリの破損チェックは省略する。
	// Reading the evaluation function, etc.
	// In the case of the learn command, the value of the evaluation function
    // may have been corrected after loading the evaluation function, so the
	// Omit the memory corruption check.
	is_ready(true);

	// 派生クラスのinit()を呼び出す。
	// Call init() of the derived class
	init();

	// ループ上限はset_loop_max()で設定されているものとする。
	// The loop limit is assumed to be set by set_loop_max()
	loop_count = 0;
	done_count = 0;

	// threadをOptions["Threads"]の数だけ生成して思考開始。
	// Create as many threads as Options["Threads"] and start thinking
	std::vector<std::thread> threads;
	auto thread_num = (size_t)Options["Threads"];

	// worker threadの終了フラグの確保
	// Allocate the exit flag of the worker thread
	thread_finished.resize(thread_num);
	
	// worker threadの起動
    // Launching the worker thread
	for (size_t i = 0; i < thread_num; ++i)
	{
		thread_finished[i] = 0;

		threads.push_back(std::thread([i, this]
		{ 
			// プロセッサの全スレッドを使い切る。
			// Use up all the threads of the processor
			WinProcGroup::bindThisThread(i);

			// オーバーライドされている処理を実行
			// Execute the overridden process
			this->thread_worker(i);

			// スレッドが終了したので終了フラグを立てる
			// The thread has ended, so we flag it as done.
			this->thread_finished[i] = 1;
		}));
	}

	// すべてのthreadの終了待ちを
	// for (auto& th : threads)
	//  th.join();
	// のように書くとスレッドがまだ仕事をしている状態でここに突入するので、
	// その間、callback_func()が呼び出せず、セーブできなくなる。
	// そこで終了フラグを自前でチェックする必要がある。
	// Waiting for all threads to end.
	// for (auto& th : threads)
	// th.join();
	// so we'll go in here with the thread still working, so we'll write
	// In the meantime, we can't call call callback_func() and can't save the file.
	// So we need to check the exit flag on our own.

	// すべてのスレッドが終了したかを判定する関数
	// Determine if all threads are finished
	auto threads_done = [&]()
	{
		// ひとつでも終了していなければfalseを返す
		for (auto& f : thread_finished)
			if (!f)
				return false;

		return true;
	};

	// コールバック関数が設定されているならコールバックする。
	// If the callback function has been set, call back
	auto do_a_callback = [&]()
	{
		if (callback_func)
			callback_func();
	};


	for (uint64_t i = 0 ; ; )
	{
		// 全スレッドが終了していたら、ループを抜ける。
		// When all threads are finished, exit the loop
		if (threads_done())
			break;

		sleep(1000);

		// callback_secondsごとにcallback_func()が呼び出される。
		// Callback_func() will be called every callback_seconds
		if (++i == callback_seconds)
		{
			do_a_callback();
			// ↑から戻ってきてからカウンターをリセットしているので、
			// do_a_callback()のなかでsave()などにどれだけ時間がかかろうと
			// 次に呼び出すのは、そこから一定時間の経過を要する。
			// I've been resetting the counter since I came back from ↑.
			// No matter how long it takes to do things like save() in do_a_callback()
			// The next call requires a certain amount of time to pass from there.
			i = 0;
		}
	}

	// 最後の保存。
	// Save the last
	std::cout << std::endl << "finalize..";

	// do_a_callback();
	// →　呼び出し元で保存するはずで、ここでは要らない気がする。

	// 終了したフラグは立っているがスレッドの終了コードの実行中であるということはありうるので
	// join()でその終了を待つ必要がある。
	// It is possible that the exit flag has been raised, but the thread's exit code
    // is still running. You need to wait for it to end with join().
	for (auto& th : threads)
		th.join();

	// 全スレッドが終了しただけでfileの書き出しスレッドなどはまだ動いていて
	// 作業自体は完了していない可能性があるのでスレッドがすべて終了したことだけ出力する。
	// All the threads are done, but the file export thread and so on are still running, and the
	// the work itself may not be complete, so only output that the thread is all done.
	std::cout << "all threads are joined." << std::endl;

	// Optionsを書き換えたので復元。
	// 値を代入しないとハンドラが起動しないのでこうやって復元する。
	// Restoration of the rewritten Options.
	// The handler will not be started unless the value is assigned,
    // so we restore it like this.
	for (auto& s : oldOptions)
		Options[s.first] = std::string(s.second);
}

#endif // defined(EVAL_LEARN)
