/* Measuring performance of concurrent hashmaps under several
 * workload configurations.
 *
 * Copyright 2023 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#include "seq/bits.hpp"
#ifdef SEQ_HAS_CPP_20

#include <algorithm>
#include <array>
#include <chrono>
#include <numeric>

std::chrono::high_resolution_clock::time_point measure_start,measure_pause;

template<typename F>
double measure(F f)
{
  using namespace std::chrono;

  static const int              num_trials=10;
  static const milliseconds     min_time_per_trial(10);
  std::array<double,num_trials> trials;

  for(int i=0;i<num_trials;++i){
    int                               runs=0;
    high_resolution_clock::time_point t2;
    volatile decltype(f())            res; /* to avoid optimizing f() away */

    measure_start=high_resolution_clock::now();
    do{
      res=f();
      ++runs;
      t2=high_resolution_clock::now();
    }while(t2-measure_start<min_time_per_trial);
    trials[i]=duration_cast<duration<double>>(t2-measure_start).count()/runs;
  }

  std::sort(trials.begin(),trials.end());
  return std::accumulate(
    trials.begin()+2,trials.end()-2,0.0)/(trials.size()-4);
}

void pause_timing()
{
  measure_pause=std::chrono::high_resolution_clock::now();
}

void resume_timing()
{
  measure_start+=std::chrono::high_resolution_clock::now()-measure_pause;
}

#include <boost/bind/bind.hpp>
#include <boost/core/detail/splitmix64.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <iostream>
#include <latch>
#include <random>
#include <thread>
#include <vector>
#include "gtl/phmap.hpp"
#include "seq/concurrent_map.hpp"
#include "./zipfian_int_distribution.h"

using boost_map=boost::concurrent_flat_map<int,int, seq::hasher<int>>;

using tbb_map=seq::concurrent_map<int,int, seq::hasher<int>>; 

using gtl_map=gtl::parallel_flat_hash_map<
  int,int, seq::hasher<int>,gtl::priv::hash_default_eq<int>,
  std::allocator<std::pair<const int,int>>,
  8,std::mutex>;

struct bulk_map:boost::concurrent_flat_map<int,int, seq::hasher<int>>{};

template<typename... Args>
inline void map_update(boost_map& m,Args&&... args)
{
  m.emplace_or_visit(std::forward<Args>(args)...,[](auto& x){++x.second;});
}

template<typename Key>
inline bool map_find(const boost_map& m,const Key& x)
{
  return m.contains(x);
}

template<typename... Args>
inline void map_update(tbb_map& m,Args&&... args)
{
    m.emplace_or_visit( std::forward<Args>(args)..., [](auto& x) {++x.second; });
}

template<typename Key>
inline bool map_find(const tbb_map& m,const Key& x)
{
  return m.count(x);
}

template<typename Key,typename... Args>
inline void map_update(gtl_map& m,Key&& k,Args&&... args)
{
  m.lazy_emplace_l(
    k,
    [](auto& x){++x.second;},
    [&](const auto& ctor){
      ctor(std::forward<Key>(k),std::forward<Args>(args)...);});
}

template<typename Key>
inline bool map_find(const gtl_map& m,const Key& x)
{
  return m.contains(x);
}

template<typename Distribution>
class updater
{
public:
  updater(const Distribution& dist_):dist{dist_}{}

  template<typename Map,typename URNG>
  void operator()(Map& m,URNG& gen)
  {
    map_update(m,dist(gen),0);
  }

private:
  Distribution dist;
};

template<typename Distribution>
class finder
{
public:
  finder(const Distribution& dist_):dist{dist_}{}

  template<typename Map,typename URNG>
  void operator()(const Map& m,URNG& gen)
  {
    if(map_find(m,dist(gen)))++res;
  }

  int res=0;

private:
  Distribution dist;
};

template<typename Distribution>
class bulk_finder
{
public:
  bulk_finder(const Distribution& dist_):dist{dist_}{}

  template<typename URNG>
  void operator()(const bulk_map& m,URNG& gen)
  {
    keys[i++]=dist(gen);
    if(i==N){
      flush(m);
      i=0;
    }
  }

  void flush(const bulk_map& m)
  {
    //res+=(int)m.visit(keys.begin(),keys.begin()+i,[](const auto&){});
      for (int v : keys)
          res += m.visit(v, [](const auto&) {});
  }

  int res=0;

private:
    static constexpr std::size_t N = 16;//bulk_map::bulk_visit_size;

  Distribution      dist;
  int               i=0;
  std::array<int,N> keys;
};

/* contributed by Martin Leinter-Ankerl */

template<size_t N>
class simple_discrete_distribution {
public:
  /* N-1 because we don't store the last probability*/
  std::array<uint64_t, N> m_cummulative{};

public:
  simple_discrete_distribution(std::initializer_list<double> l)
  {
    std::array<double, N> sums{};
    double                sum=0.0;

    auto i=0;
    for(auto x:l){
      sum+=x;
      sums[i]=sum;
      ++i;
    }

    /* normalize to 2^64 */
    for(int i=0;i<N;++i){
      m_cummulative[i]=static_cast<uint64_t>(
        sums[i]/sum*(double)(std::numeric_limits<uint64_t>::max)());
    }
    m_cummulative.back()=(std::numeric_limits<uint64_t>::max)();
  }

  std::size_t operator()(uint64_t r01)const noexcept
  {
    for(size_t i=0;i<m_cummulative.size();++i)
    {
      if(r01<=m_cummulative[i])return i;
    }
    return m_cummulative.size()-1;
  }

  template<typename URNG>
  std::size_t operator()(URNG& rng)const noexcept
  {
    static_assert((URNG::min)()==0,"URNG::min must be 0");
    static_assert(
      (URNG::max)()==(std::numeric_limits<uint64_t>::max)(),
      "URNG::max must be max of uint64_t");
    return operator()(rng());
  }
};

struct splitmix64_urng:boost::detail::splitmix64
{
  using boost::detail::splitmix64::splitmix64;
  using result_type=boost::uint64_t;

  static constexpr result_type (min)(){return 0u;}
  static constexpr result_type(max)()
  {return (std::numeric_limits<result_type>::max)();}
};

template<typename Map>
struct parallel_load
{
  using result_type=std::size_t;

  BOOST_NOINLINE result_type operator()(int N,double theta,int num_threads)const
  {
    int res=0;
    pause_timing(); 
    {
      Map                           m;
      std::vector<std::thread>      threads;
      std::vector<int>              results(num_threads);
      zipfian_int_distribution<int> zipf1{1,N,theta},
                                    zipf2{N+1,2*N,theta};
      std::latch                    ready(num_threads),
                                    start(1),
                                    completed(num_threads),
                                    finish(1);

      for(int i=0;i<num_threads;++i)threads.emplace_back([&,i,zipf1,zipf2]{
        static constexpr auto is_bulk=std::is_same<Map,bulk_map>::value;
        using finder_type=typename std::conditional<
          is_bulk,
          bulk_finder<zipfian_int_distribution<int>>,
          finder<zipfian_int_distribution<int>>
        >::type;

        simple_discrete_distribution<3> dist({10,45,45});
        splitmix64_urng                 gen(std::size_t(282472+i*213731));

        updater     update{zipf1};
        finder_type successful_find{zipf1},
                    unsuccessful_find{zipf2};

        int n=i==0?(N+num_threads-1)/num_threads:N/num_threads;
        n*=10; /* so that sum(#updates(i)) = N */

        ready.count_down();
        start.wait();

        for(int j=0;j<n;++j){
          switch(dist(gen)){
            case 0:  update(m,gen); break;
            case 1:  successful_find(m,gen); break;
            case 2:
            default: unsuccessful_find(m,gen); break;
          }
        }
        if constexpr(is_bulk){
          successful_find.flush(m);
          unsuccessful_find.flush(m);
        }
        results[i]=successful_find.res+unsuccessful_find.res;
        completed.count_down();
        finish.wait();
      });

      ready.wait();
      resume_timing();
      start.count_down();
      completed.wait();
      pause_timing();
      finish.count_down();
      for(int i=0;i<num_threads;++i){
        threads[i].join();
        res+=results[i];
      }
    }
    resume_timing();
    return res;
  }
};

template<
  template<typename> class Tester,
  typename Container1,typename Container2,
  typename Container3,typename Container4
>
BOOST_NOINLINE void test(
  const char* title,int N,double theta,
  const char* name1,const char* name2,const char* name3,const char* name4)
{
#ifdef NUM_THREADS
  const int num_threads=NUM_THREADS;
#else
  const int num_threads=16;
#endif

  std::cout<<title<<" (N="<<N<<", theta="<<theta<<"):"<<std::endl;
  std::cout<<"#threads;"<<name1<<";"<<name2<<";"<<name3<<";"<<name4<<std::endl;

  for(int n=1;n<=num_threads;++n){
    std::cout<<n<<";";
    auto t=measure(boost::bind(Tester<Container1>(),N,theta,n));
    std::cout<<10*N/t/1E6<<";";
    t=measure(boost::bind(Tester<Container2>(),N,theta,n));
    std::cout<<10*N/t/1E6<<";";
    t=measure(boost::bind(Tester<Container3>(),N,theta,n));
    std::cout<<10*N/t/1E6<<";";
    t=measure(boost::bind(Tester<Container4>(),N,theta,n));
    std::cout<<10*N/t/1E6<<std::endl;
  }
}

#endif

int bench_boost_unordered_benchmarks(int, char** const)
{
#ifdef SEQ_HAS_CPP_20
  std::cout<<"#logical cores: "<<std::thread::hardware_concurrency()<<std::endl;

  for(auto N:{500'000,5'000'000}){
    for(auto theta:{0.01,0.5,0.99}){
      test<
        parallel_load,
        tbb_map,
        gtl_map,
        boost_map,
        bulk_map>
      (
        "Parallel load",N,theta,
        "seq::concurrent_map",
        "gtl::parallel_flat_hash_map",
        "boost::concurrent_flat_map",
        "boost::concurrent_flat_map bulk"
      );
    }
  }
#endif
  return 0;
}