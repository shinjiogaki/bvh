#include "bvh_binary.h"

#include <chrono>
#include <execution> // execution::par
#include <iostream>  // std::cout
#include <iomanip>   // std::setprecision

thread_local glm::vec3  MiniRay::Inverse;

// delta function in sec3 of the paper
// "Fast and Simple Agglomerative LBVH Construction"
__forceinline uint32_t delta(const std::vector<glm::uvec2> &leaves, const uint32_t id)
{
	return leaves[id + 1].y ^ leaves[id].y;
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
	std::vector<glm::uvec2> leaves(T);

	// morton codes
#pragma omp parallel for
	for (auto i = 0; i < T; ++i)
	{
		const auto id0 = i * 3 + 0;
		const auto id1 = i * 3 + 1;
		const auto id2 = i * 3 + 2;

		const auto centroid = (Ps[PIDs[id0]] + Ps[PIDs[id1]] + Ps[PIDs[id2]]) / 3.0f;
		const auto unitcube = Bound.Nomalize(centroid); // coord in unit cube
		leaves[i].x = i; // reference to primitive
		leaves[i].y = morton3D(unitcube.x, unitcube.y, unitcube.z); // morton code
	}

	// sort leaves by morton code in ascending order
	std::sort(std::execution::par, std::begin(leaves), std::end(leaves), [&](const glm::uvec2 &l, const glm::uvec2 &r)
	{
		return (l.y < r.y);
	});

	// number of nodes
	const auto N = T - 1;

	// allocate inner nodes
	Nodes.resize(N);

	// otherBounds in algorithm 1 of the paper
	// "Massively Parallel Construction of Radix Tree Forests for the Efficient Sampling of Discrete Probability Distributions"	// https://arxiv.org/pdf/1901.05423.pdf
	std::vector<std::atomic<uint32_t>> other_bounds(N);
	for (auto &p : other_bounds)
		p.store(invalid);

	// Scnene minimum
	Minimum = glm::vec3(maximum);
	for (auto&p : Ps)
		Minimum = glm::min(Minimum, p);
	for (auto&p : Ps)
		p += Minimum;

	std::cout
		<< "scene minimum "
		<< Minimum.x << " "
		<< Minimum.x << " "
		<< Minimum.x << std::endl;

	// for all leaf
#pragma omp parallel for
	for (auto i = 0; i < T; ++i)
	{
		// current leaf/node id
		auto current = i;

		// range
		auto L = i;
		auto R = i;

		// leaf aabb
		const auto id = leaves[i].x * 3;
		AABB aabb;
		aabb.Expand(Ps[PIDs[id + 0]]);
		aabb.Expand(Ps[PIDs[id + 1]]);
		aabb.Expand(Ps[PIDs[id + 2]]);
		
		// current is leaf or node?
		auto leaf = true;
		while (1)
		{
			// the whole range is covered
			if (0 == L && R == N)
			{
				Root = current;
				break;
			}

			// leaf/node index
			const auto index = leaf ? leaves[current].x * 2 + 1 : current * 2;

			// choose parent
			auto terminate = false;
			uint32_t parent;
			if (0 == L || (R != N && delta(leaves, R) < delta(leaves, L - 1)))
			{
				// parent is right and "L" doesn't change
				parent    = R;
				terminate = (other_bounds[parent] == invalid);
				if(terminate)
					other_bounds[parent] = L;
				else
					R = other_bounds[parent];
				// set left child
				Nodes[parent].L = index;
			}
			else
			{
				// parent is left and "R" doesn't change
				parent    = L - 1;
				terminate = (other_bounds[parent] == invalid);
				if(terminate)
					other_bounds[parent] = R;
				else
					L = other_bounds[parent];
				// set right child
				Nodes[parent].R = index;
			}

			//assert(L < R);

			// expand aabb
			Nodes[parent].AABB.Expand(aabb);

			// terminate this thread
			if (terminate)
				break;

			// ascend
			current = parent;
			aabb    = Nodes[current].AABB;
			leaf    = false;
		}
	}
	
	const auto end = std::chrono::steady_clock::now();

	std::cout << "bvh: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;

	// debug
	double sah = 0;
	for (const auto &n : Nodes)
	{
		sah += n.AABB.HalvedSurface();
	}
	std::cout << "sah: " <<std::setprecision(16) << sah << std::endl;
}

// for debug
void AABB::Print()
{
	std::cout << "min: "
		<< to_float(AtomicMin[0]) << " "
		<< to_float(AtomicMin[1]) << " "
		<< to_float(AtomicMin[2]) << std::endl;
	std::cout << "min: "
		<< Min.x << " "
		<< Min.y << " "
		<< Min.z << std::endl;

	std::cout << "max: "
		<< to_float(AtomicMax[0]) << " "
		<< to_float(AtomicMax[1]) << " "
		<< to_float(AtomicMax[2]) << std::endl;
	std::cout << "max: "
		<< Max.x << " "
		<< Max.y << " "
		<< Max.z << std::endl;
}
