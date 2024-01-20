/**
 * @file   sortLike_test.cc
 * @brief  Unit test for utilities from `sortLike.h`.
 * @date   August 28, 2022
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @see    `icarusalg/Utilities/sortLike.h`
 */

// Boost libraries
#define BOOST_TEST_MODULE sortLike
#include <boost/test/unit_test.hpp>
#include <boost/iterator/transform_iterator.hpp>

// ICARUS libraries
#include "icarusalg/Utilities/sortLike.h"

// C/C++ standard library
#include <ostream>
#include <algorithm> // std::sort()
#include <functional> // std::greater<>
#include <memory> // std::unique_ptr()
#include <string>
#include <array>
#include <vector>


//------------------------------------------------------------------------------
class NastyUncopiableData {
  
  std::unique_ptr<int> fValue;
  
    public:
  
  NastyUncopiableData(int value): fValue{ std::make_unique<int>(value) } {}
  
  bool hasValue() const noexcept { return bool(fValue); }
  int value() const noexcept { return *fValue; }
  
  operator int() const noexcept { return value(); }
  
  bool operator< (NastyUncopiableData const& other) const noexcept
    {
      if (hasValue()) {
        return other.hasValue()? (value() < other.value()): false;
      }
      else return !other.hasValue();
    }
  
}; // NastyUncopiableData

std::ostream& operator<< (std::ostream& out, NastyUncopiableData const& data) {
  if (data.hasValue()) out << "<" << data.value() << ">";
  else out << "<n/a>";
  return out;
}


//------------------------------------------------------------------------------
void sortLike_test1() {
  
  std::vector const values{
     8,  6,  4,  2,  7,  5,  3,
    28, 26, 24, 22, 27, 25, 23,
    18, 16, 14, 12, 17, 15, 13,
    };
  
  std::vector<NastyUncopiableData> data;
  for (int v: values) data.emplace_back(v);
  
  auto const dbegin = data.begin(), dend = data.end();
  
  auto extractKeys = [](auto const& values)
    {
      std::vector<float> keys;
      for (int const v: values) keys.push_back(-float(v));
      return keys;
    };
  std::vector<float> const keys = extractKeys(values);

  std::vector<int> expectedData{ values };
  std::sort(expectedData.begin(), expectedData.end(), std::greater<>{});
  
  util::sortLike(data.begin(), data.end(), keys.begin(), keys.end());
  
  BOOST_CHECK_EQUAL_COLLECTIONS(
    boost::transform_iterator
      (data.begin(), std::mem_fn(&NastyUncopiableData::value)),
    boost::transform_iterator
      (data.end(), std::mem_fn(&NastyUncopiableData::value)),
    expectedData.cbegin(), expectedData.cend()
    );
  
  // iterators should not be invalidated
  BOOST_TEST((data.begin() == dbegin));
  BOOST_TEST((data.end() == dend));
  
} // sortLike_test1()


//------------------------------------------------------------------------------
void sortLike_doc1_test() {
  /*
   * The promise:
   */
  /*
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
   * std::string name = "ACIRSU";
   * constexpr std::array order{ 3, 2, 1, 4, 6, 5 };
   * 
   * util::sortLike(name.begin(), name.end(), order.begin(), order.end());
   * std::cout << name << std::endl;
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   * should print `ICARUS`.
   */
  
  std::string name = "ACIRSU";
  constexpr std::array order{ 3, 2, 1, 4, 6, 5 };
  
  util::sortLike(name.begin(), name.end(), order.begin(), order.end());
  
  BOOST_TEST(name == "ICARUS");
  
} // sortLike_doc1_test()


//------------------------------------------------------------------------------
void sortCollLike_test1() {
  
  std::vector const values{
     8,  6,  4,  2,  7,  5,  3,
    28, 26, 24, 22, 27, 25, 23,
    18, 16, 14, 12, 17, 15, 13,
    };
  
  std::vector<NastyUncopiableData> data;
  for (int v: values) data.emplace_back(v);
  
  auto const dbegin = data.begin(), dend = data.end();
  
  auto extractKeys = [](auto const& values)
    {
      std::vector<float> keys;
      for (int const v: values) keys.push_back(-float(v));
      return keys;
    };
  std::vector<float> const keys = extractKeys(values);

  std::vector<int> expectedData{ values };
  std::sort(expectedData.begin(), expectedData.end(), std::greater<>{});
  
  util::sortCollLike(data, keys);
  
  BOOST_CHECK_EQUAL_COLLECTIONS(
    boost::transform_iterator
      (data.begin(), std::mem_fn(&NastyUncopiableData::value)),
    boost::transform_iterator
      (data.end(), std::mem_fn(&NastyUncopiableData::value)),
    expectedData.cbegin(), expectedData.cend()
    );
  
  // iterators should not be invalidated
  BOOST_TEST((data.begin() == dbegin));
  BOOST_TEST((data.end() == dend));
  
} // sortCollLike_test1()


//------------------------------------------------------------------------------
void sortCollLike_doc1_test() {
  /*
   * The promise:
   */
  /*
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
   * std::string name = "ACIRSU";
   * constexpr std::array order{ 3, 2, 1, 4, 6, 5 };
   * 
   * util::sortLike(name.begin(), name.end(), order.begin(), order.end());
   * std::cout << name << std::endl;
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   * should print `ICARUS`.
   */
  
  std::string name = "ACIRSU";
  constexpr std::array order{ 3, 2, 1, 4, 6, 5 };
  
  util::sortCollLike(name, order);
  
  BOOST_TEST(name == "ICARUS");
  
} // sortCollLike_doc1_test()


//------------------------------------------------------------------------------
//---  The tests
//---
BOOST_AUTO_TEST_CASE( sortLike_testcase ) {
  
  sortLike_test1();
  
  sortLike_doc1_test();
  
} // BOOST_AUTO_TEST_CASE( sortLike_testcase )

BOOST_AUTO_TEST_CASE( sortCollLike_testcase ) {
  
  sortCollLike_test1();
  
  sortCollLike_doc1_test();
  
} // BOOST_AUTO_TEST_CASE( sortCollLike_testcase )


