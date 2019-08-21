#include "bvh_binary.h"

#include <chrono>
#include <execution> // execution::par
#include <iostream>  // std::cout
#include <iomanip>   // std::setprecision

thread_local glm::vec3  MiniRay::Inverse;

// delta function in sec3 of the paper
// "Fast and Simple Agglomerative LBVH Construction"
__forceinline uint32_t Delta(const std::vector<glm::uvec3> &leaves, const uint32_t id)
{
	return leaves[id + 1].z ^ leaves[id].z;
}

void LBVH::Build()
{
	const auto start = std::chrono::steady_clock::now();

	// object bounding box
	for (const auto &p : Ps)
		Bound.Expand(p);

	// number of triangles
	const auto T = PIDs.size() / 3;

	// allocate pair <reference, morton code>
	std::vector<glm::uvec3> leaves(T);

	// set order
	for (auto i = 0; i < T; ++i)
		leaves[i].x = i;

	// set morton code
	std::for_each(std::execution::par, leaves.begin(), leaves.end(), [&](glm::uvec3 &leaf)
	{
		const auto id0 = leaf.x * 3 + 0;
		const auto id1 = leaf.x * 3 + 1;
		const auto id2 = leaf.x * 3 + 2;
		const auto centroid = (Ps[PIDs[id0]] + Ps[PIDs[id1]] + Ps[PIDs[id2]]) / 3.0f;
		const auto unitcube = Bound.Nomalize(centroid);        // coord in unit cube
		leaf.z = morton3D(unitcube.x, unitcube.y, unitcube.z); // morton code
	});

	// sort leaves by morton code in ascending order
	std::sort(std::execution::par, std::begin(leaves), std::end(leaves), [&](const glm::uvec3 &l, const glm::uvec3 &r)
	{
		return (l.z < r.z);
	});

	// set order (because getting thread id in for_each is somewhat cumbersome...)
	for (auto i = 0; i < T; ++i)
		leaves[i].y = i;

	// number of nodes
	const auto N = T - 1;

	// allocate inner nodes
	Nodes.resize(N);

	// otherBounds in algorithm 1 of the paper
	// "Massively Parallel Construction of Radix Tree Forests for the Efficient Sampling of Discrete Probability Distributions"
	// https://arxiv.org/pdf/1901.05423.pdf
	std::vector<std::atomic<uint32_t>> other_bounds(N);
	std::for_each(std::execution::par, other_bounds.begin(), other_bounds.end(), [&](std::atomic<uint32_t> &b)
	{
		b.store(invalid);
	});

	// subtract scene minimum. don't forget transforming rays.
	std::for_each(std::execution::par, Ps.begin(), Ps.end(), [&](glm::vec3 &p)
	{
		p -= Bound.Min;
	});

	// for each leaf
	std::for_each(std::execution::par, leaves.begin(), leaves.end(), [&](glm::uvec3 &leaf)
	{
		// current leaf/node id
		auto current = leaf.y;

		// range
		uint32_t L = current;
		uint32_t R = current;

		// leaf aabb
		const auto id = leaf.x * 3;
		AABB aabb;
		aabb.Expand(Ps[PIDs[id + 0]]);
		aabb.Expand(Ps[PIDs[id + 1]]);
		aabb.Expand(Ps[PIDs[id + 2]]);

		// current is leaf or node?
		auto is_leaf = true;
		while (1)
		{
			// the whole range is covered
			if (0 == L && R == N)
			{
				Root = current;
				break;
			}

			// leaf/node index
			const auto index = is_leaf ? leaves[current].x * 2 + 1 : current * 2;

			// choose parent
			uint32_t previous, parent;
			if (0 == L || (R != N && Delta(leaves, R) < Delta(leaves, L - 1)))
			{
				// parent is right and "L" doesn't change
				parent   = R;
				previous = other_bounds[parent].exchange(L);
				if(invalid != previous)
					R = previous;
				Nodes[parent].L = index;
			}
			else
			{
				// parent is left and "R" doesn't change
				parent   = L - 1;
				previous = other_bounds[parent].exchange(R);
				if (invalid != previous)
					L = previous;
				Nodes[parent].R = index;
			}

			assert(L < R);

			// expand aabb
			Nodes[parent].AABB.Expand(aabb);

			// terminate this thread
			if (invalid == previous)
				break;

			// ascend
			current = parent;
			aabb    = Nodes[current].AABB;
			is_leaf = false;
		}
	});

	const auto end = std::chrono::steady_clock::now();

	std::cout << "bvh: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;

	// debug
	double sah = 0;
	for (const auto &n : Nodes)
	{
		sah += n.AABB.HalvedSurface();
	}
	std::cout << "sah: " << std::setprecision(16) << sah << std::endl;
}

