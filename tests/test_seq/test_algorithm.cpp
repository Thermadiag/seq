#include <seq/algorithm.hpp>
#include <seq/tiny_string.hpp>
#include <seq/testing.hpp>

#include <memory>
#include <vector>

void test_stability()
{
	// Test stability
	using ptr_type = std::shared_ptr<size_t>;

	std::vector<ptr_type> vec(1000000);
	for (size_t i = 0; i < vec.size(); ++i)
		vec[i] = std::make_shared<size_t>(i % 100);

	auto le = [](const auto& l, const auto& r) { return *l < *r; };

	auto v = vec;
	std::stable_sort(v.begin(), v.end(), le);
	SEQ_TEST(std::is_sorted(v.begin(), v.end(), le));

	auto v2 = vec;
	seq::net_sort(v2.begin(), v2.end(), le);
	SEQ_TEST(std::is_sorted(v2.begin(), v2.end(), le));

	bool eq = std::equal(v.begin(), v.end(), v2.begin(), v2.end());
	SEQ_TEST(eq);
}

void test_reverse_sort_stability()
{
	// Test stability on vector sorted in descending order.
	// This tests the validity of seq::reverse_sort
	using ptr_type = std::shared_ptr<size_t>;

	std::vector<ptr_type> vec(1000000);
	for (size_t i = 0; i < vec.size(); ++i)
		vec[i] = std::make_shared<size_t>(i % 100);

	auto le = [](const auto& l, const auto& r) { return *l < *r; };
	auto ge = [](const auto& l, const auto& r) { return *l > *r; };

	// sort in reverse order
	std::sort(vec.begin(), vec.end(), ge);

	auto v = vec;
	std::stable_sort(v.begin(), v.end(), le);
	SEQ_TEST(std::is_sorted(v.begin(), v.end(), le));

	auto v2 = vec;
	seq::net_sort(v2.begin(), v2.end(), le);
	SEQ_TEST(std::is_sorted(v2.begin(), v2.end(), le));

	bool eq = std::equal(v.begin(), v.end(), v2.begin(), v2.end());
	SEQ_TEST(eq);
}


void test_move_only()
{
	// Test net_sort on move only type
	using ptr_type = std::unique_ptr<size_t>;

	std::vector<ptr_type> vec(1000000);
	for (size_t i = 0; i < vec.size(); ++i)
		vec[i] = std::make_unique<size_t>(i % 100);

	auto le = [](const auto& l, const auto& r) { return *l < *r; };

	seq::net_sort(vec.begin(), vec.end(), le);
	SEQ_TEST(std::is_sorted(vec.begin(), vec.end(), le));
}

void test_unique()
{
	//Test seq::unique validity and stability

	using ptr_type = std::shared_ptr<size_t>;
	std::vector<ptr_type> vec(1000000);
	for (size_t i=0; i < vec.size(); ++i)
		vec[i] = std::make_shared<size_t>(i % 100);

	auto v = vec;
	auto end = seq::unique(v.begin(), v.end(), [](const auto& v) { return seq::hasher<size_t>{}(*v); }, [](const auto& l, const auto& r) { return *l == *r; });
	v.erase(end, v.end());
	SEQ_TEST(v.size() == 100);
	SEQ_TEST(std::equal(v.begin(), v.end(), vec.begin(), vec.begin() + 100));

}


void test_unique_move_only()
{
	// Test seq::unique on move only type

	using ptr_type = std::unique_ptr<size_t>;
	std::vector<ptr_type> vec(1000000);
	for (size_t i = 0; i < vec.size(); ++i)
		vec[i] = std::make_unique<size_t>(i % 100);

	auto end = seq::unique(vec.begin(), vec.end(), [](const auto& v) { return seq::hasher<size_t>{}(*v); }, [](const auto& l, const auto& r) { return *l == *r; });
	vec.erase(end, vec.end());
	SEQ_TEST(vec.size() == 100);
}

SEQ_PROTOTYPE(int test_algorithm(int, char** const))
{
	test_reverse_sort_stability();
	test_unique();
	test_unique_move_only();
	test_stability();
	test_move_only();
	
	return 0;
}