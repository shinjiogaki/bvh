#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <mutex>

// Parallel BVH construction simulator - (c) shinji ogaki

struct Pivot
{
   int Start;
   int End;

   int Split() const
   {
      return (Start + End) / 2;
   }

   void Set(int s, int e)
   {
      Start = s;
      End = e;
      std::cout << std::this_thread::get_id() << "\t\t" << Start << "\t" << End << std::endl;
   }

   int Length() const
   {
      return End - Start;
   }
};

void Build(const Pivot &p)
{
   // Heavy
   std::this_thread::sleep_for(std::chrono::milliseconds(1 + p.Length()));
}

struct Pool
{
   static const int Depth = 128;
   static const int LeafCount = 3;

   // mutex
   std::mutex Mutex;

   // Depth x 2 is enough
   Pivot Pivots[Depth * 2];

   // number of active threads
   std::atomic<int> ActiveThreads;

   // number of untaken tasks
   int NumberOfTasks;

   Pool()
   {
      ActiveThreads = 0;
   }

   void Push(const Pivot &p)
   {
      // leaf node
      if (LeafCount > p.Length())
      {
         return;
      }

      const auto split = p.Split();

      // create two child nodes
      std::lock_guard<std::mutex> lock(Mutex);
      const auto id = NumberOfTasks;
      Pivots[id + 0].Set(p.Start, split + 0);
      Pivots[id + 1].Set(split + 1, p.End);
      NumberOfTasks += 2;
   }

   bool Pop(Pivot &p)
   {
      std::lock_guard<std::mutex> lock(Mutex);
      if (0 < NumberOfTasks)
      {
         --NumberOfTasks;
         p = Pivots[NumberOfTasks];
         return true;
      }
      return false;
   }

   bool Undone()
   {
      return (0 < ActiveThreads) || (0 < NumberOfTasks);
   }
};

int main()
{
   auto start = std::chrono::system_clock::now();

   Pool pool;

   // create root node
   pool.NumberOfTasks = 1;
   pool.Pivots[0].Set(0, 17);

   // number of threads
   static const auto N = std::thread::hardware_concurrency();

   // create threads
   std::vector<std::thread> threads(N);
   for (auto i = 0; i < N; ++i)
   {
      threads[i] = std::thread([&]
      {
         Pivot p;
         while (pool.Undone())
         {
            if (pool.Pop(p))
            {
               ++pool.ActiveThreads;
               Build(p);
               pool.Push(p);
               --pool.ActiveThreads;
            }
         }
      });
   }

   // wait til build gets done
   for (auto i = 0; i < N; ++i)
      threads[i].join();

   auto end = std::chrono::system_clock::now();
   auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

   // log
   std::cout << std::to_string(elapsed) << " msecs with " << N <<" threads " << std::endl;
   return 0;
}
