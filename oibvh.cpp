#include <iostream>
#include <immintrin.h>

uint32_t CeilLog2(const uint32_t x)
{
	return (32 - _lzcnt_u32(x - 1));
}

uint32_t FloorLog2(const uint32_t x)
{
	return (31 - _lzcnt_u32(x));
}

int main()
{
	// Oi-BVH example (Chitalu et al. / Binary Ostensibly-Implicit Trees)

	// see Figure 3
	const uint32_t invalid = std::numeric_limits<uint32_t>::max();
	const uint32_t t = 11; // # triangles
	const uint32_t l_ = CeilLog2(t);
	const uint32_t Lc = 1 << l_; // total # of leaves
	const uint32_t Nc = 2 * Lc - 1; // total # of nodes
	const uint32_t Lv = Lc - t; // # of virtual leaves
	const uint32_t Nv = 2 * Lv - _mm_popcnt_u32(Lv); // # of virtual nodes
	const uint32_t Nr = Nc - Nv; // # of real nodes

	std::cout << "T :\t" << t << std::endl;
	std::cout << "l_:\t" << l_ << std::endl;
	std::cout << "Lc:\t" << Lc << std::endl;
	std::cout << "Lv:\t" << Lv << std::endl;
	std::cout << "Nc:\t" << Nc << std::endl;
	std::cout << "Nv:\t" << Nv << std::endl;
	std::cout << "Nr:\t" << Nr << std::endl;

	// input : index i of complete binary tree
	// output: the actual momory location of i
	auto ID = [&](const uint32_t i)
	{
		const uint32_t li = FloorLog2(i + 1);
		const uint32_t Lvl2 = Lv >> (l_ - li);
		const uint32_t Lvl = Lvl2 >> 1;
		const uint32_t Nvl = Lvl * 2 - _mm_popcnt_u32(Lvl);
		const uint32_t Lc_l = 1 << (li + 1); // 2 x total leaves at level l
		if (Lc_l <= i + 1 + Lvl2)
		{
			return invalid;
		}
		return i - Nvl;
	};

	// see Figure 3
	for (auto i = 0; i < 31; ++i)
	{
		std::cout << i << " " << ID(i) << std::endl;
	}

	return 0;
}
