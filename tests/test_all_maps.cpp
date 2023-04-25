#include <seq/ordered_map.hpp>
#include <seq/flat_map.hpp>
#include <seq/radix_map.hpp>
#include <seq/radix_hash_map.hpp>


template<class Map>
void test_map()
{
	using value_type = typename Map::value_type;
	//using key_type = typename Map::key_type;
	//using mapped_type = typename Map::mapped_type;
	Map m;

	// Make sure all of these compile

	value_type v("tata", "ok");
	// copy insert
	m.insert(v);
	//move insert
	m.insert(value_type("toto", "ok"));
	//insert( P&&)
	m.insert(std::make_pair("titi", "ok"));

	m.emplace(v);
	m.emplace(value_type("toto", "ok"));
	m.emplace(std::make_pair("titi", "ok"));


	m.emplace("toto", "ok");
	m.try_emplace("toto", "ok");
}

int test_all_maps(int, char** const)
{
	test_map<seq::ordered_map<std::string, std::string> >();
	test_map<seq::flat_map<std::string, std::string> >();
	test_map<seq::flat_multimap<std::string, std::string> >();
	test_map<seq::radix_map<std::string, std::string> >();
	test_map<seq::radix_hash_map<std::string, std::string> >();
	return 0;
}