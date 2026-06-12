
#include <seq/flat_map.hpp>
#include <seq/radix_map.hpp>
#include <seq/legacy/format.hpp>
#include <seq/any.hpp>
#include <seq/timer.hpp>

#include "gtl/btree.hpp"

#ifdef BOOST_FOUND
#include "boost/container/flat_set.hpp"
#endif

#include <iostream>
#include <set>
#include <algorithm>
#include <random>



#include "testing.hpp"

using namespace seq;

template<class C>
struct is_boost_set: std::false_type
{
};
#ifdef BOOST_FOUND
template<class K, class V>
struct is_boost_set<boost::container::flat_set<K, V>> : std::true_type
{
};
#endif

using measure_type = std::vector<std::pair<size_t,size_t>>;

template<class Set, class T>
measure_type bench_insert(Set & s, const std::vector<T> & vec)
{
    timer t;

    std::vector<std::pair<size_t,size_t>> elapsed;
    elapsed.reserve(vec.size() / 1024 + 1);

    t.tick();

    for(size_t i = 0; i < vec.size(); ++i){
        s.insert(vec[i]);
        if((i & (1023u)) == 0 && i){
            auto el = t.tock();
            elapsed.push_back ({i, el/1024} );
            t.tick();
        } 
    } 
    return elapsed;
}

template<class Set, class T>
measure_type bench_erase(Set & s, const std::vector<T> & vec)
{
    timer t;

    std::vector<std::pair<size_t,size_t>> elapsed;
    elapsed.reserve(vec.size() / 1024 + 1);

    std::vector<size_t> idx(vec.size());
    for(size_t i = 0; i < vec.size(); ++i){
        s.insert(vec[i] );
        idx.push_back(i);
    } 
    std::random_shuffle(idx.begin(), idx.end());
    

    t.tick();

    for(size_t i = 0; i < vec.size(); ++i){
        s.erase(vec[idx[i]]);
        if((i & (1023u)) == 0 && i){
            auto el = t.tock();
            elapsed.push_back ({vec.size() -i, el/1024} );
            t.tick();
        } 
    } 
    std::sort(elapsed.begin(),elapsed.end());
    return elapsed;
}



template<class Set, class T>
measure_type bench_find_success(Set & s, const std::vector<T> & vec)
{
    std::vector<std::pair<size_t,size_t>> elapsed;
    elapsed.reserve(vec.size() / 1024 + 1);
    std::vector<size_t> idx;
    idx.reserve(vec.size());

    for(size_t i = 0; i < vec.size(); ++i){
        s.insert(vec[i]);
        idx.push_back(i);
        if((i & (1023u)) == 0 && i){

            auto tmp = idx;
            std::random_shuffle(tmp.begin(),tmp.end());
            timer t;
            t.tick();

            // perform look up of all inserted values
            for(auto id : tmp)
                SEQ_TEST(s.count(vec[id] )==1);

            auto el = t.tock();
            elapsed.push_back ({i, el/tmp.size()} );
        } 
    } 
    return elapsed;
} 

template<class Set, class T>
measure_type bench_find_failed(Set & s, const std::vector<T> & vec, const std::vector<T> & failed)
{
    std::vector<std::pair<size_t,size_t>> elapsed;
    elapsed.reserve(vec.size() / 1024 + 1);
    
    for(size_t i = 0; i < vec.size(); ++i){
        s.insert(vec[i]);
        if((i & (1023u)) == 0 && i){

            timer t;
            t.tick();

            // perform look up of all inserted values
            for(const auto &v : failed)
                SEQ_TEST(s.count(v)==0);

            auto el = t.tock();
            elapsed.push_back ({i, el/failed.size()} );
        } 
    } 
    return elapsed;
} 

static void print_measure(const measure_type & m){
    for(size_t i = 0; i < m.size(); ++i)
        std::cout << m[i].first  <<" " << m[i].second << std::endl; 
}

struct SetResults
{

};



template<class Set, class T>
void bench_set(const std::vector<T> & keys, const std::vector<T> & failed)
{
   {
    Set s;
    auto m = bench_insert(s,keys);
    std::cout << "insert" <<std::endl;
    print_measure(m);
   } 
   //return;
   {
    Set s;
    auto m = bench_erase(s,keys);
    std::cout << "erase" <<std::endl;
    print_measure(m);
   } 
   {
    Set s;
    auto m = bench_find_success(s,keys);
    std::cout << "find" <<std::endl;
    print_measure(m);
   } 
    {
    Set s;
    auto m = bench_find_failed(s,keys,failed);
    std::cout << "find failed" <<std::endl;
    print_measure(m);
   } 
}




int bench_map_csv(int, char** const)
{

   { 
    std::vector<size_t> keys(500000);
    for(size_t i = 0; i< keys.size(); ++i)
        keys[i] = i;
    
    std::random_shuffle(keys.begin(), keys.end());
    std::vector<size_t> failed(keys.begin() + keys.size() - 5000, keys.end());
    keys.erase(keys.begin() + keys.size() - failed.size(), keys.end());
    
    bench_set<flat_set<size_t>>(keys,failed);

   } 



   return 0;
}
