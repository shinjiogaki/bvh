#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <algorithm>
#include <numeric>
#include <execution>

// small parallel BVH (object splitting) construction simulator (c) shinji ogaki

struct Pivot
{
	int Start;
	int End;

	void Set(int s, int e)
	{
		Start = s;
		End = e;
		std::cout << "[" << std::this_thread::get_id() << "]\t" << Start << "\t" << End << std::endl;
	}

	int Partition() const
	{
		return (Start + End) / 2;
	}

	int Length() const
	{
		return End - Start;
	}
};

struct BVH
{
	static const int LeafCount = 4;

	// depth x 2 x num of threads is enough for binary tree
	Pivot Pivots[1024];

	std::mutex Mutex;
	int ActiveThreads;
	int NumberOfTasks;

	BVH()
	{
		ActiveThreads = 0;
		NumberOfTasks = 0;
	}

	void InsertPivots(const Pivot &p)
	{
		std::lock_guard<std::mutex> lock(Mutex);

		// enough size?
		if (LeafCount < p.Length())
		{
			// create inner node
			const auto partition = p.Partition();
			Pivots[NumberOfTasks + 0].Set(p.Start, partition);
			Pivots[NumberOfTasks + 1].Set(partition, p.End);

			// two tasks added
			NumberOfTasks += 2;
		}

		// task is done
		--ActiveThreads;
	}

	bool GetPivot(Pivot &p)
	{
		std::lock_guard<std::mutex> lock(Mutex);

		// NumberOfTasks=0 doesn't necessarily mean that build is done
		if (0 < NumberOfTasks)
		{
			--NumberOfTasks; // single task is taken
			++ActiveThreads; // new threads starts working on it
			p = Pivots[NumberOfTasks]; // task
			return true;
		}
		return false;
	}

	bool UnderConstruction()
	{
		return (0 < ActiveThreads) || (0 < NumberOfTasks);
	}

	void ProcessPivot(const Pivot &p)
	{
		// do something costly
		std::this_thread::sleep_for(std::chrono::milliseconds(10 + p.Length()));

		// insert new node & pivots
		InsertPivots(p);
	}

	void Build()
	{
		const auto num_threads = std::thread::hardware_concurrency();

		NumberOfTasks = 1;
		Pivots[0].Set(0, 64);

		// alloc
		std::vector<std::thread> threads(num_threads);

		// create threads
		for (auto i = 0u; i < num_threads; ++i)
			threads[i] = std::thread([&]
		{
			Pivot p;
			while (UnderConstruction())
				if (GetPivot(p))
					ProcessPivot(p);
		});

		// wait!
		for (auto i = 0u; i < num_threads; ++i)
			threads[i].join();
	}
};

int main()
{
	static const auto num_repeats = 1; // repeat many times to make sure it doesn't crash

	for (auto l = 0; l < num_repeats; ++l)
	{
		// start
		const auto start = std::chrono::system_clock::now();

		// create bvh and add a pivot
		BVH bvh;
		bvh.Build();

		// end
		const auto end = std::chrono::system_clock::now();

		// log
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		std::cout << elapsed << " [msec]" << std::endl;
	}
	return 0;
}
