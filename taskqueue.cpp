#include <iostream>
#include <thread>
#include <vector>
#include <mutex>

// small parallel BVH (object splitting) construction simulator (c) shinji ogaki

struct Range
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
		// object median
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
	Range Ranges[1024];

	std::mutex Mutex;
	int Progress;
	int MaxElems;
	int NumTasks;

	BVH()
	{
		Progress = 0;
		NumTasks = 0;
	}

	void InsertRange(const Range &r)
	{
		std::lock_guard<std::mutex> lock(Mutex);

		// Node
		if (LeafCount < r.Length())
		{
			// create inner node
			const auto partition = r.Partition();
			Ranges[NumTasks + 0].Set(r.Start, partition);
			Ranges[NumTasks + 1].Set(partition, r.End);

			// two tasks added
			NumTasks += 2;
		}
		// Leaf
		else
		{
			Progress += r.Length();
		}
	}

	bool GetRange(Range &r)
	{
		std::lock_guard<std::mutex> lock(Mutex);

		// NumTasks=0 doesn't necessarily mean that build is done
		if (0 < NumTasks)
		{
			--NumTasks; // single task is taken
			r = Ranges[NumTasks]; // task
			return true;
		}
		return false;
	}

	bool UnderConstruction()
	{
		// Yuichi Sayama's idea
		return (Progress < MaxElems);
	}

	void ProcessRange(const Range &r)
	{
		// do something expensive
		std::this_thread::sleep_for(std::chrono::milliseconds(10 + r.Length()));

		// insert new node & ranges
		InsertRange(r);
	}

	void Build()
	{
		const auto num_threads = std::thread::hardware_concurrency();

		// alloc
		std::vector<std::thread> threads(num_threads);

		// create threads
		for (auto i = 0u; i < num_threads; ++i)
			threads[i] = std::thread([&]
		{
			Range p;
			while (UnderConstruction())
				if (GetRange(p))
					ProcessRange(p);
		});

		// wait!
		for (auto& thread : threads)
			thread.join();
	}
};

int main()
{
	// start
	const auto start = std::chrono::system_clock::now();

	// build bvh
	BVH bvh;
	bvh.NumTasks = 1;
	bvh.MaxElems = 64;
	bvh.Ranges[0].Set(0, 64);
	bvh.Build();

	// end
	const auto end = std::chrono::system_clock::now();

	// log
	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	std::cout << elapsed << " [msec]" << std::endl;

	return 0;
}
