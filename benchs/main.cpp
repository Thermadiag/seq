/**
 * MIT License
 *
 * Copyright (c) 2022 Victor Moncada <vtr.moncada@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "cvector.hpp"
 
#include "bench_hash.hpp"
#include "bench_map.hpp"
#include "bench_text_stream.hpp"
#include "bench_tiny_string.hpp"
#include "bench_tiered_vector.hpp"
#include "bench_sequence.hpp"
#include "bench_mem_pool.hpp"




using namespace seq;

#include <fstream>

template<class T>
bool test_decode(T* src, size_t blocks, char* compressed, size_t csize)
{
	
	T* dst = (T*)malloc(sizeof(T) * blocks * 256);
	memset(dst, 0, sizeof(T) * blocks * 256);
	tick();
	int loop = 10;
	unsigned dd;
	for(int i=0; i < loop; ++i)
		dd= block_decode_256(compressed, csize, sizeof(T), blocks, dst);
	size_t el = tock_ms();
	std::cout << "decode: " << 1e-6 * loop * (blocks*256 * sizeof(T)) / ((double)el * 0.001) << " MB/s" << std::endl;
	if (dd >= SEQ_LAST_ERROR_CODE)
		std::cout << "decoding error" << std::endl;

	size_t count = 256 * blocks;
	bool eq = true;
	for (size_t i = 0; i < count; ++i) {
		if (src[i] != dst[i]) {
			//std::cout << "'" << src[i] <<"'"<< std::endl;
			//std::cout << "'" << dst[i] << "'" << std::endl;
			std::cout << "failed at " << i << std::endl;
			eq = false;
			break;
		}
	}

	/*std::ofstream outd("outd.txt");
	for (int i = 0; i < 256; ++i)
		outd << dst[i] << std::endl;
	out.close();*/

	free(dst);
	return eq;
}

struct Lessd
{
	template<class A,class B>
	bool operator()(const A& a, const B& b)
	{
		bool ret = a < b;
		if (ret) {
			SEQ_ASSERT_DEBUG(!(b<a), "invalid comparator");
		}
		return ret;
	}
};

//#include "fastpfor.h"

static int test_count = 0;
struct Test
{
	int i;
	Test(int _i = 0) : i(_i) {
		test_count++;
	}
	Test(const Test& other) {
		i = other.i;
		test_count++;
	}
	~Test() {
		test_count--;
	}
	bool operator<(const Test& other) const {
		return i < other.i;
	}
};
namespace seq
{
	template<>
	struct is_relocatable<Test> : std::true_type {};
}

struct LessPtr
{
	template<class T>
	bool operator()(const std::shared_ptr<T>& a, const std::shared_ptr<T>& b) const
	{
		return a.get() < b.get();
	}
	template<class T>
	bool operator()(const std::unique_ptr<T>& a, const std::unique_ptr<T>& b)const
	{
		return a.get() < b.get();
	}
};

namespace std
{
	
}

template<class it1, class it2>
void my_move(it1 first, it1 last, it2 dst)
{
	for (; first != last; ++first, ++dst)
		*dst = std::move(*first);
}
template<class It>
It getit(It i) { return i; }
template<class it1, class it2>
void m_move(it1 first, it1 last, it2 dst)
{
	return my_move(getit(first), getit(last), getit(dst));
}

#include "c-blosc-main/blosc/blosc.h"

/*template<class T>
void encode_256(T* src, char* dst, unsigned out_size, unsigned blocks, unsigned acceleration)
{
	tick();
	char* saved = dst;
	unsigned tot = 0;
	for (int j = 0; j < 1000; ++j) {
		tot = 0;
		dst = saved;
		for (int i = 0; i < blocks; ++i)
		{
			const char* in = (const char*)(src + 256 * i);
			//unsigned r = block_encode_256(in, sizeof(T), 1, dst, out_size - (dst - saved), acceleration);
			unsigned r = block_compute_1_256(in, sizeof(T),255*sizeof(T), acceleration);
			if (r == SEQ_ERROR_DST_OVERFLOW) {
				memcpy(dst, src, sizeof(T) * 256);
				r = sizeof(T) * 256;
			}
			else {
				unsigned tmp = block_encode_1_256(src, sizeof(T), dst, out_size - (dst - saved));
				if (tmp >= SEQ_LAST_ERROR_CODE)
					std::cout << "encode error" << std::endl;
				r = tmp;
			}
			
			if (r >= SEQ_LAST_ERROR_CODE)
				std::cout << "encode error" << std::endl;
			dst += r;
			tot += r;
		}
	}

	size_t el = tock_ms();
	float ratio = float(tot) / (blocks * 256 * sizeof(T));
	std::cout <<"acc: "<<acceleration<<" "<< ratio << " " << ((blocks * 256 * 1000 * sizeof(T)) / 1000000.) / (el * 0.001) << std::endl;
}*/


// Home made parallel_for_each
template<class T, class Al, unsigned Acc, class Functor>
auto parallel_for_each(cvector<T,Al,Acc> &vec, size_t start, size_t end, Functor fun) -> Functor
{
	SEQ_ASSERT_DEBUG(start <= end && end <= vec.size(), "parallel_for_each: invalid range");
	int bindex = (int)(start / 256u);
	int pos = (int)(start & 255u);
	int last_bindex = (int)(end / 256u);
	int last_pos = (int)(end & 255u);

#pragma omp parallel for
	for (int b = bindex; b <= last_bindex; ++b)
	{
		int first = b == bindex ? pos : 0;
		int last = b == last_bindex ? last_pos : 256;
		if (first == last)
			continue;
		// lock block since we multithread the loop
		auto guard = vec.lock_block(b);

		//retrieve the block
		auto bl = vec.block(b);

		for (; first != last; ++first)
			fun(bl.first[first]);

		vec.mark_dirty_block(b);
	}

	return fun;
}

#include "internal/lz4small.h"
#include "bench_tiny_string.hpp"

template<class T>
struct position
{
	T value;
};
template<class T>
struct flat_vec
{
	using index_type = unsigned;
	using size_type = position<index_type>;
	using vec_type = std::vector<T>;

	struct Less
	{
		using is_transparent = std::true_type;
		const vec_type* vec;
		Less(const vec_type* v) : vec(v) {}

		bool operator()(size_type a, size_type b) const
		{
			return (*vec)[a.value] < (*vec)[b.value];
		}
		bool operator()(const T & a, size_type b) const
		{
			return a < (*vec)[b.value];
		}
		bool operator()(size_type a, const T & b) const
		{
			return (*vec)[a.value] < b;
		}
	};
	seq::flat_set<size_type, Less> fset;
	vec_type vset;


	void eytzinger(size_t & i, size_t k , size_t n, vec_type & out_vec) {
		if (k <= n) {
			eytzinger(i,2 * k,n, out_vec);
			out_vec[k] = vset[fset.pos(i).value];
			const_cast<index_type&>(fset.pos(i).value) = k-1;
			i++;
			eytzinger(i,2 * k + 1,n, out_vec);
		}
	}
	void resize(size_t new_size)
	{
		
		/*vec_type _new(new_size + 1);
		size_t i = 0;
		eytzinger(i,1,fset.size(), _new);
		memmove(_new.data(), _new.data() + 1, new_size * sizeof(T));
		_new.resize(new_size);
		vset = std::move(_new);*/

		
		vec_type _new(new_size);
		index_type i = 0;
		for (auto it = fset.begin(); it != fset.end(); ++it, ++i)
		{
			_new[i] = vset[it->value];
			const_cast<size_type&>(*it).value = i;
		}
		vset = std::move(_new);
	}

	flat_vec()
		:fset(Less(&vset)) {}

	SEQ_ALWAYS_INLINE std::pair<size_t,bool> insert_pos(const T& v)
	{
		if (vset.size() && ((vset.size() -1) & vset.size()) == 0 )
			resize(vset.size() * 2);
		vset.push_back(v);
		auto r = fset.insert_pos(size_type{ (index_type)(vset.size() - 1) });
		if (r.second == false)
			vset.pop_back();
		return r;
	}

	SEQ_ALWAYS_INLINE size_t find_pos(const T& v)
	{
		return fset.find_pos(v);
	}

	size_t size() const { return fset.size(); }
};
template<unsigned size>
struct test
{
	size_t vals[size];
	test(size_t v = 0) {
		vals[0] = v;
	}
	operator size_t() const { return vals[0]; }
	bool operator<(const test& b) const {
		return vals[0] < b.vals[0];
	}
};

int  main  (int , char** )
{ 
	/* {
		using type = test<1>;

		std::vector<type> vals;
		for (int i = 0; i < 100000; ++i)
			vals.push_back(rand() * UINT64_C(0xc4ceb9fe1a85ec53) * i);

		tick();
		flat_vec<type> vv;
		for (int i = 0; i < vals.size(); ++i)
			vv.insert_pos(vals[i]);
		size_t el = tock_ms();
		std::cout << "flat_vec insert: " << el <<" "<<vv.size()<< std::endl;

		tick();
		flat_set<type> f;
		for (int i = 0; i < vals.size(); ++i)
			f.insert_pos(vals[i]);
		el = tock_ms();
		std::cout << "flat_set insert: " << el << " " << f.size() << std::endl;

		tick();
		phmap::btree_set<type> s;
		for (int i = 0; i < vals.size(); ++i)
			s.insert(vals[i]);
		el = tock_ms();
		std::cout << "phmap::btree_set insert: " << el << " "<<s.size()<<std::endl;


		size_t sum = 0;
		tick();
		for (int i = 0; i < vals.size(); ++i)
		{
			sum += vv.find_pos(vals[i]) != vv.size();
		}
		el = tock_ms();
		std::cout << "flat_vec find: " << el <<" " << sum << std::endl;

		sum = 0;
		tick();
		for (int i = 0; i < vals.size(); ++i)
		{
			sum += f.find_pos(vals[i]) != f.size();
		}
		el = tock_ms();
		std::cout << "flat_set find: " << el << " " << sum << std::endl;

		sum = 0;
		tick();
		for (int i = 0; i < vals.size(); ++i)
		{
			sum += s.find(vals[i]) != s.end();
		}
		el = tock_ms();
		std::cout << "phmap::btree_set find: " << el << " " << sum << std::endl;
		return 0;

	}*/
	{
		/*std::vector<short> vals(256 * 100000);
		std::vector<short> vals2(256 * 100000);
		std::vector<short> vals3(256 * 100000);
		
		for (int i = 0; i < vals.size(); ++i)
			vals[i] = rand() * UINT64_C(0xc4ceb9fe1a85ec53);


		tick();
		for (size_t i = 0; i < vals.size() ; i+=256)
		{
			transpose_generic((char*)(vals.data() + i), (char*)(vals2.data() + i),256, 2);
		}
		size_t el = tock_ms();
		std::cout << "transposeg: " << el << " ms" << std::endl;

		tick();
		for (size_t i = 0; i < vals.size(); i+=256)
		{
			transpose_256_rows((char*)(vals.data() + i), (char*)(vals2.data() + i ), 2);
		}
		 el = tock_ms();
		std::cout << "transpose: " << el << " ms" << std::endl;
		
		tick();
		for (size_t i = 0; i < vals.size() ; i+=256)
		{
			transpose_inv_256_rows((char*)(vals2.data() + i ), (char*)(vals3.data() + i ), 2);
		}
		 el = tock_ms();
		std::cout << "transpose inv: " << el << " ms" << std::endl;

		bool eq = std::equal(vals.begin(), vals.end(), vals3.begin(), vals3.end());
		std::cout << eq << std::endl;
		return 0;
		*/

		test_tiered_vector<size_t>();
	}
	
	{
			using ptr_type = std::unique_ptr<size_t>;
	 
			// Create a cvector of unique_ptr with random integers
			cvector<ptr_type, std::allocator<ptr_type>,0 > vec;
			std::srand(0);
			for(size_t i = 0; i < 1000000; ++i)
				vec.emplace_back(new size_t(std::rand()));
	 
			// print the compression ratio
			std::cout<< vec.current_compression_ratio() <<std::endl;
	 
			// sort the cvector using the defined comparison operator between 2 std::unique_ptr objects (sort by pointer address)
			std::sort(vec.begin(),vec.end());
	 
			// print again the compression ratio
			std::cout<< vec.current_compression_ratio() <<std::endl;
	 
			// Now we want to sort by pointed value. 
			// We need a custom comparison function that will be passed to seq::make_comparator
			
			std::sort(vec.begin(),vec.end(), make_comparator([](const auto & a, const auto & b){return *a < *b; }));
	 
			// print again the compression ratio
			std::cout<< vec.current_compression_ratio() <<std::endl;

			return 0;
	}

	//test_tiered_vector_algorithms<size_t>();
	//test_tiered_vector<size_t>();
	//return 0;

		
	
	
	/*test_tiered_vector_algorithms<size_t>();
	test_tiered_vector<size_t>();
	return 0;
	{
		for (int count = 1000; count <= 100000000; count *= 10)
		{
			std::cout << std::endl << "count: " << count << std::endl;
			cvector<unsigned short, std::allocator<unsigned short >, 0> vv;
			tick();
			for (int i = 0; i < count; ++i)
				vv.push_back(rand());
			size_t el = tock_ms();
			std::cout << "push_back: " << el << " ms" << std::endl;
			std::cout << vv.current_compression_ratio() << std::endl;

			tick();
			std::sort(vv.begin(), vv.end());
			el = tock_ms();
			std::cout << "sort: " << el << " ms" << std::endl;
			std::cout << vv.current_compression_ratio() << std::endl;
			vv.shrink_to_fit();
			std::cout << vv.current_compression_ratio() << std::endl;

			tick();
			seq::random_shuffle(vv.begin(), vv.end());
			el = tock_ms();
			std::cout << "random_shuffle: " << el << " ms" << std::endl;
			std::cout << vv.current_compression_ratio() << std::endl;

			std::vector<unsigned short> tmp(vv.begin(), vv.end());
			std::ostringstream oss;
			vv.serialize(oss);

			cvector<unsigned short, std::allocator<unsigned short >, 0> vv2;
			std::istringstream iss(oss.str());
			vv2.deserialize(iss);

			bool eq = std::equal(vv2.begin(), vv2.end(), tmp.begin(), tmp.end());
			std::cout << "equal: " << eq << std::endl;

		}
		return 0;
	}*/
	
	//test_tiered_vector<size_t>();
	//test_tiered_vector_algorithms<size_t>();
	//return 0;
	
	

	/* {
		std::cout << " alignas(16) static const char SEQ_shuffle_table[] = {" << std::endl;
		for (int i = 0; i < 256; ++i) {
			hse_vector tmp;
			_mm_store_si128((__m128i*) & tmp, ((const __m128i*)get_shuffle_table())[i]);
			for (int j = 0; j < 16; ++j)
				std::cout << "(char)" << (int)tmp.u8[j] << ", ";
			std::cout << std::endl;
		}
		std::cout <<"}"<< std::endl;
		__m128i sh = ((const __m128i*)get_shuffle_table())[(unsigned short)~(4 | (4 << 8))];
		bool stop = true;
	}*/
	  {
		char input[256];
		for (int i = 0; i < 256; ++i)
			input[i] = 127-i;
		for (int i = 0; i < 16; ++i)
			input[i * 16]++;

		memset(input, 0, 8);
		memset(input + 8, 120, 8);
		input[0] = 1;

		//for (int i = 0; i < 256; ++i)
		//	input[i] = rand();
		//for (int i = 0; i < 16; ++i)
		//	input[i] = (i & 1) ? 2 : 3;
		

		char output[300];
		unsigned r = block_encode_256(input, 1, 1, output, sizeof(output),  7);
		std::cout << "encode" << std::endl;
		char input2[256];
		block_decode_256(output, r, 1, 1, input2);

		for (int i = 0; i < 256; ++i)
			if (input[i] != input2[i])
				bool stop = true;

		bool stop = true;
		std::cout << "continue" << std::endl;
	} 
	
	{
		std::ifstream in("C:/Users/VM213788/Desktop/test.html");
		std::vector<tstring> strs;
		while (true)
		{
			tstring s;
			in >> s;
			if (in)
				strs.push_back(s);
			else
				break;
		}
		size_t blocks = strs.size() / 256;
		std::vector<char> out(256 * sizeof(tstring));
		tick();
		int tot = 0;
		for (int j = 0; j < 1000; ++j) {
			tot = 0;
			for (int i = 0; i < blocks; ++i)
			{
				tstring* in = strs.data() + i * 256;
				int r = block_encode_256((char*)in,  1, sizeof(tstring), out.data(), out.size(), 0);
				tot += r;
			}
		}
		size_t el = tock_ms();
		float ratio = float(tot) / (blocks * 256 * sizeof(tstring));
		std::cout << ratio <<" "<< ((blocks * 256 * 1000* sizeof(tstring))/1000000.)/(el*0.001)<< std::endl;
		return 0;
	}
	
	{
		cvector<size_t> vv;
		for (int i = 0; i < 2560000; ++i)
			vv.push_back(rand());

		cvector<size_t> vv2;

		std::ostringstream oss;
		vv.serialize(oss);
		if (!oss)
			return 0;

		size_t ssize = oss.str().size();
		std::istringstream iss(oss.str());
		vv2.deserialize(iss);

		bool eq = std::equal(vv.begin(), vv.end(), vv2.begin(), vv2.end());


		tick();
		for (int i = 0; i < vv.size(); ++i)
		{
			vv[i] = std::cos((double)vv[i]);
		}
		size_t el = tock_ms();
		std::cout << "non parallel: " << el << std::endl;

		tick();
#pragma omp parallel for
		for (int i = 0; i < vv.size(); ++i)
		{
			auto guard = vv.lock(i);
			vv[i] = std::cos((double)vv[i]);
		}
		el = tock_ms();
		std::cout << "parallel: " << el << std::endl;
	}
	test_tiered_vector_algorithms<size_t>();
	{
		cvector<std::unique_ptr<int> > vv;
		vv.emplace_back(new int(2));
		std::list<std::unique_ptr<int> > vv2(1);
		auto it1 = vv2.begin();
		auto it2 = vv.begin();
		*it1 = std::move(*it2);
		auto tmp = vv[0];
		*it1 = std::move(tmp);
		std::unique_ptr<int> uu; uu = std::move(tmp);
		uu = std::move(*vv.begin());
		std::move(vv.begin(), vv.end(), vv2.begin());
		m_move(vv.begin(), vv.end(), vv2.begin());
	}
	cvector<tstring> vv;
	vv.emplace_back("ulidfsgmohghhgmlidfshi");
	const tstring& ss = (vv[0]);
	tstring ss2 = (vv[0]);
	tstring ss3 = std::move(vv[0]);
	tstring ss4 = (vv[0]);
	std::vector<tstring> tv;
	
	std::cout << "'" << ss << "'" << std::endl;
	std::cout << "'" << ss2 << "'" << std::endl;
	std::cout << "'" << ss3 << "'" << std::endl;
	std::cout << "'" << ss4 << "'" << std::endl;
	//return 0;

	//test_tiered_vector_algorithms<size_t>(5000000);
	//test_sort_strings(2000000);
	//return 0;
	{
		cvector<std::unique_ptr<Test>> vec;
		for (int i = 0; i < 100000; ++i)
			vec.emplace_back(new Test(i));

		//std::vector<std::unique_ptr<Test>> tmp(vec.size());
		//std::move(vec.begin(), vec.end(), tmp.begin());

		//std::unique_ptr<Test> t; t = std::move(vec[0]);

		std::cout << vec.current_compression_ratio() << std::endl;
		seq::random_shuffle(vec.begin(), vec.end());
		bool sorted1 = std::is_sorted(vec.begin(), vec.end(), comp_wrapper<LessPtr>{});
		std::sort(vec.begin(), vec.end(), comp_wrapper<LessPtr>{});
		bool sorted = std::is_sorted(vec.begin(), vec.end(), comp_wrapper<LessPtr>{});
		std::vector< std::unique_ptr<Test> > tmp(vec.size());
		
		std::cout << " sorted: " << sorted1 << " " << sorted <<" "<<vec[0].get()->i<<" "<< vec[1].get()->i<< std::endl;
		//std::move(vec.begin(), vec.end(), tmp.begin());
		vec.shrink_to_fit();
		std::cout << vec.current_compression_ratio() << std::endl;
		vec.resize(2000000);
		vec.resize(5000);
		vec.shrink_to_fit();
		vec.clear();
		
		bool stop = true;
	}
	
	test_tiered_vector<size_t>();
	test_tiered_vector_algorithms<size_t>(5000000);
	return 0;
	/* {
		using type = size_t;

		std::vector<type> vec(25600000);
		for (size_t i = 0; i < vec.size(); ++i)
			vec[i] =  i *UINT64_C(0xc4ceb9fe1a85ec53);
		//std::sort(vec.begin(), vec.end());
		//std::random_shuffle(vec.begin(), vec.end());

		std::vector<std::uint32_t> dst(((vec.size() * sizeof(type)) / sizeof(std::uint32_t))*2);
		std::vector<size_t> dst2((vec.size() * sizeof(type)) / sizeof(size_t));

		//FastPForLib::FastPFor<> pfor;
		tick();
		unsigned r = block_encode_256((char*)vec.data(), sizeof(type), vec.size() / 256, (char*)dst.data(), dst.size()*sizeof(std::uint32_t), 1);
		if (r >= SEQ_LAST_ERROR_CODE)
			std::cout << "dst too small" << std::endl;
		//size_t nvals = dst.size();
		//pfor.encodeArray((std::uint64_t*)vec.data(), vec.size(), dst.data(), nvals);
		//unsigned r = nvals * sizeof(std::uint32_t);
		size_t el = tock_ms();

		std::cout << "level 0: " << (double)r / (vec.size() * sizeof(type)) << ", " << 1e-6 * 1 * (vec.size() * sizeof(type)) / ((double)el * 0.001) << " MB/s" << std::endl;

		tick();
		block_decode_256((char*)dst.data(), r, sizeof(type), vec.size() / 256, dst2.data());
		//size_t nvals2 = dst2.size();
		//pfor.decodeArray(dst.data(), nvals, (std::uint64_t*)dst2.data(), nvals2);
		el = tock_ms();

		bool ok = std::equal((char*)vec.data(), (char*)vec.data() + vec.size()*sizeof(type), (char*)dst2.data(), (char*)dst2.data()+ vec.size() * sizeof(type));
		
		std::cout << "decode 1: " << 1e-6 * 1 * (vec.size() * sizeof(type)) / ((double)el * 0.001) << " MB/s" << std::endl;

		std::cout << ok << std::endl;
		return 0;
	}*/
	
	{
		/* {
			cvector<int> w;
			for (int i = 0; i < 5000; ++i)
				w.push_back(i);
			cvector<int> w2 = w;
			bool eq = std::equal(w.begin(), w.end(), w2.begin(), w2.end());
			float s1 = w2.current_compression_ratio();
			w2.shrink_to_fit();
			float s2 = w2.current_compression_ratio();
			eq = std::equal(w.begin(), w.end(), w2.begin(), w2.end());
			s1 = w2.current_compression_ratio();
			w.erase(w.begin() + 2999, w.begin() + 3001);
			std::cout << (int)w[2999] << " " << (int)w[3000] << " " << (int)w[3001] <<" "<< w.size() << std::endl;
			float s = w.compression_ratio();
			bool stop = true;
		}*/		
		cvector<int> w;
		w.set_max_contexts(context_ratio(10,Ratio));

		/*w.push_back(2);
		w.resize(6000000);
		std::cout << w.d_comp.current_compression_ratio() << std::endl;
		while (w.size())
			w.d_comp.pop_back();
		bool sstop = true;
		std::cout<<"after pop_back(): "<< w.d_comp.current_compression_ratio() << std::endl;
		for (int i = 0; i < 1000; ++i)
			w.push_back(i);
		size_t ss = w.size();
		int a0 = w[0];
		int a10 = w[10];
		
		w.clear();*/

		getchar();

		for (size_t i = 0; i < 10000000; ++i) {
			w.push_back(i);
		}
		

		std::cout <<"push_back: " <<w.current_compression_ratio() << std::endl;

		getchar();

		tick();
		std::random_shuffle(w.begin(), w.end());
		size_t el = tock_ms();
		std::cout << "random_shuffle: " << w.current_compression_ratio() << " in " << el << " ms" << std::endl;

		getchar();

		w.shrink_to_fit();
		std::cout << "shrink_to_fit "<< w.current_compression_ratio() << std::endl;
		

		getchar();
		
		std::deque<int> dd(w.begin(), w.end());
		tick();
		std::sort(dd.begin(), dd.end());
		el = tock_ms();
		std::cout << "std::sort deque: " << el << " ms" << std::endl;
		
		tick();
		std::sort(w.begin(), w.end());
		el = tock_ms();
		std::cout <<"std::sort: "<< w.current_compression_ratio() << " in " <<el << " ms"<< std::endl;

		

		getchar();

		w.shrink_to_fit();
		std::cout << "shrink_to_fit " << w.current_compression_ratio() << std::endl;

		getchar();

		std::cout << "sorted " << std::is_sorted(w.begin(), w.end()) << std::endl;

		getchar();
		return 0;
	}

	/* {
		std::vector<tstring> vec(2560000);
		for (size_t i = 0; i < vec.size(); ++i)
			vec[i] = generate_random_string<tstring>(14, true);
		std::sort(vec.begin(), vec.end());

		std::vector<char> dst(256 * sizeof(tstring)*1000);
		unsigned s0 = block_encode_256((char*)vec.data(), sizeof(tstring), 1000, dst.data(), 256 * sizeof(tstring) * 1000, 0);

		tstring* dec0 = (tstring*)malloc(256 * sizeof(tstring)*1000);
		block_decode_256(dst.data(), s0, sizeof(tstring),1000, dec0);
		bool q = std::equal(vec.data(),vec.data()+256*1000,dec0,dec0+256*1000);
		bool stop = true;
	}*/

	/*using type = tstring;
	srand(0);
	std::vector<tstring> vec(2560000);
	for (size_t i = 0; i < vec.size(); ++i)
		vec[i] = generate_random_string<tstring>(32,false);//i;// *UINT64_C(0xc4ceb9fe1a85ec53);
	*/
														   //seq::random_shuffle(vec.begin(), vec.end());
	//std::nth_element(vec.begin(), vec.begin() + vec.size() / 2, vec.end());
	//std::sort(vec.begin(), vec.end());
	
	using type = size_t;
	std::vector<type> vec(2560000);
	for (size_t i = 0; i < vec.size(); ++i)
		vec[i] = type(i *UINT64_C(0xc4ceb9fe1a85ec53));
	//std::random_shuffle(vec.begin(), vec.end());

	std::vector<char> dst(vec.size() * sizeof(type)*2);

	tick();
	unsigned sr = block_encode_256((char*)vec.data(), 1, (vec.size()*sizeof(type))/256, dst.data(), dst.size(),  0);
	size_t elr = tock_ms();
	std::cout << "raw 2: " << (double)sr / (vec.size() * sizeof(type)) << ", " << 1e-6 * (vec.size() * sizeof(type)) / ((double)elr * 0.001) << " MB/s" << std::endl;
	

#ifdef __BMI2__
	std::cout << "__BMI2__" << std::endl;
#endif

	int loop = 100;

	tick();
	unsigned s0;
	for(int i=0; i < loop; ++i)
		s0 = block_encode_256((char*)vec.data(), sizeof(type), vec.size() / 256, dst.data(), dst.size(), 0);
	size_t el0 = tock_ms();
	

	std::cout <<s0<<" "<<block_bound(sizeof(type))* vec.size() / 256<<" "<< test_decode(vec.data(), vec.size() / 256, dst.data(), s0) << std::endl;

	tick();
	unsigned s2;
	for (int i = 0; i < loop; ++i)
		s2 = block_encode_256((char*)vec.data(), sizeof(type), vec.size() / 256, dst.data(), dst.size(), 0);
	size_t el2 = tock_ms();

	std::cout << s2 << " " << test_decode(vec.data(), vec.size() / 256, dst.data(), s2) << std::endl;

	std::cout << "level 0: " << (double)s0 / (vec.size() * sizeof(type)) << ", " << 1e-6 * loop * (vec.size() * sizeof(type)) / ((double)el0 * 0.001) <<" MB/s"<< std::endl;
	std::cout << "level 2: " << (double)s2 / (vec.size() * sizeof(type)) << ", " << 1e-6 * loop * (vec.size() * sizeof(type)) / ((double)el2 * 0.001) << " MB/s" << std::endl;
	
	return 0;
	test_object_pool(1000000);

	test_tstring_members(20000000);
	test_sort_strings(2000000);
	test_tstring_operators<25>(5000000, 14);

	
	
	test_sequence_vs_colony<size_t>(5000000);
	
	test_tiered_vector_algorithms<size_t>(5000000);
	test_tiered_vector<size_t>();


	test_hash<std::string, std::hash<tstring> >(5000000, [](size_t i) { return generate_random_string<std::string>(14, true); });
	test_hash<tstring, std::hash<tstring> >(5000000, [](size_t i) { return generate_random_string<tstring>(14, true); });
	test_hash<double, std::hash<double> >(10000000, [](size_t i) { return (i * UINT64_C(0xc4ceb9fe1a85ec53)); });

	test_map<double>(1000000, [](size_t i) { return (i * UINT64_C(0xc4ceb9fe1a85ec53)); });
	
	test_write_numeric<std::int64_t>(1000000);
	test_write_numeric<float,seq::general>(1000000,12);
	test_write_numeric<double, seq::general>(1000000, 12);
	test_write_numeric<long double, seq::general>(1000000, 12);
	test_read_numeric<std::int64_t>(1000000);
	test_read_numeric<float>(1000000);
	test_read_numeric<double>(1000000);

	// last benchmark, as strtold is buggy on gcc
	test_read_numeric<long double>(1000000);

	return 0;

	
}
