/**
 * @file   icarusalg/Utilities/sortLike.h
 * @brief  Provides `sortLike()` class of utilities.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date   August 28, 2023
 * 
 * This is a header-only library.
 */

#ifndef ICARUSALG_UTILITIES_SORTLIKE_H
#define ICARUSALG_UTILITIES_SORTLIKE_H


// C/C++ standard libraries
#include <algorithm> // std::partition()
#include <functional> // std::less<>
#include <vector>
#include <iterator> // std::back_inserter(), std::iterator_traits, ...
#include <utility> // std::pair<>
#include <cassert>


// -----------------------------------------------------------------------------
namespace util {
  
  // ---------------------------------------------------------------------------
  /**
   * @brief Sorts elements on a range according to keys from another range.
   * @tparam BIter type of begin iterator to objects to be sorted
   * @tparam EIter type of end iterator to objects to be sorted
   * @tparam BKIter type of begin (constant) iterator to sorting keys
   * @tparam EKIter type of end (constant) iterator to sorting keys
   * @tparam Comp (default: `std::less`) type of functor comparing two keys
   * @param begin iterator to the first element of the collection to be sorted
   * @param end iterator after the last element of the collection to be sorted
   * @param key_begin iterator to the sorting key of the first object
   * @param key_end iterator to the sorting key after the last object
   * @param comp (default: `std::less{}`) functor comparing two keys
   * @see `sortCollLike()`
   * 
   * This function sorts the elements between `begin` and `end` using the
   * respective entries between `key_begin` and `key_end` as sorting keys.
   * Sorting happens in place (or it should look like it did).
   * 
   * The structure of the vector is not changed, so effectively the iterators
   * are not invalidated.
   * 
   * Example:
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
   * std::string name = "ACIRSU";
   * constexpr std::array order{ 3, 2, 1, 4, 6, 5 };
   * 
   * util::sortLike(name.begin(), name.end(), order.begin(), order.end());
   * std::cout << name << std::endl;
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   * should print `ICARUS`.
   * 
   * Requirements
   * =============
   * 
   * The requirements are pretty much the same as for `std::sort()`:
   *  * the objects contained in `Data` must be swappable.
   * 
   * In fact, chances are that the requirements are looser, since the algorithm
   * should work also with any forward iterator.
   */
  template <
    typename BIter, typename EIter, typename BKIter, typename EKIter,
    typename Comp = std::less<void>
    >
  void sortLike
    (BIter begin, EIter end, BKIter key_begin, EKIter key_end, Comp comp = {});

  
  // ---------------------------------------------------------------------------
  /**
   * @brief Sorts `data` elements according to keys from another range.
   * @tparam DataColl type of collection of objects to be sorted
   * @tparam KeyColl type of collection with sorting keys
   * @tparam Comp (default: `std::less`) type of functor comparing two keys
   * @param data collection of objects to be sorted
   * @param keys collection of the sorting keys for `data`
   * @param comp (default: `std::less{}`) functor comparing two keys
   * @see `sortLike()`
   * 
   * Sorting happens in place (or it should look like it did).
   * 
   * Example:
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
   * std::string name = "ACIRSU";
   * constexpr std::array order{ 3, 2, 1, 4, 6, 5 };
   * 
   * util::sortCollLike(name, order);
   * std::cout << name << std::endl;
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   * should print `ICARUS`.
   * 
   */
  template <typename DataColl, typename KeyColl, typename Comp = std::less<>>
  void sortCollLike(DataColl& data, KeyColl const& keys, Comp comp = {});
  
  // ---------------------------------------------------------------------------
  
} // namespace util


// -----------------------------------------------------------------------------
// ---  template implementation
// -----------------------------------------------------------------------------
namespace util::details {
  
  /// Special type to allow `swap()` customization.
  template <typename FirstIter, typename SecondIter>
  struct SwappableIteratorPair {
    using This_t = SwappableIteratorPair<FirstIter, SecondIter>;
    
    FirstIter first;
    SecondIter second;
    
    bool operator< (This_t const& other) const noexcept
      { return *first < *(other.first); }
    friend void swap(This_t& a, This_t& b) noexcept
      {
        using std::swap;
        swap(a.first, b.first);
        std::iter_swap(a.second, b.second);
      }
  }; // struct SwappableIteratorPair
  
  
  // Shamelessly copied on 2023-08-13 from example in
  // https://en.cppreference.com/w/cpp/algorithm/partition;
  // because, seriously: who wants to write quick sort from scratch again??
  // this sorting algorithm us not stable and requires the value to be copyable
  template<typename FwdIter, typename Comp = std::less<>>
  void unoptimisedQuickSort(FwdIter begin, FwdIter end, Comp const& comp = {}) {
    if (begin == end) return;
    
    using PtrDiff_t = typename std::iterator_traits<FwdIter>::difference_type;
    using Value_t = typename std::iterator_traits<FwdIter>::value_type;
    
    // we could branch out with an optimization for small collections here
    PtrDiff_t const size = std::distance(begin, end);
    
    // std::partition() will move the pivot value around, so we can't just refer
    // to it, we really need a copy (or to track its moves)
    Value_t const pivot = *std::next(begin, size / 2); // copy happens here
    FwdIter const middle1 = std::partition(
      begin, end, [&pivot,&comp](Value_t const& v){ return comp(v, pivot); }
      );
    // [ middle1; middle2 [ all have the same value
    FwdIter const middle2 = std::partition(
      middle1, end, [&pivot,&comp](Value_t const& v){ return !comp(pivot, v); }
      );

    unoptimisedQuickSort(begin, middle1, comp);
    unoptimisedQuickSort(middle2, end, comp);
  } // unoptimisedQuickSort()

} // namespace util::details


// -----------------------------------------------------------------------------
template <
  typename BIter, typename EIter, typename BKIter, typename EKIter,
  typename Comp /* = std::less<void> */
  >
void util::sortLike(
  BIter begin, EIter end, BKIter key_begin, EKIter key_end, Comp comp /* = {} */
) {
#if 1
  
  /*
   * make the container of references (to be sorted);
   * the type SwappableIteratorPair is special in that when swapped
   * the iterators to the keys get swapped (unusual, to allow them constant)
   * while the iterators to data are left as they are, but data is swapped
   *   instead (as usual, to avoid memory allocation).
   * A side effect of actually sorting a `std::vector` is that its iterators
   * are random iterators and provide the random access functionality to the
   * underlying iterators (which probably can just be forward iterators).
   * 
   * Unfortunately `std::sort()` in GCC has an optimization where for small
   * collections it will use a different algorithm which moves instead of
   * swapping, and it turns out that making SwappableIteratorPair efficiently
   * moveable is very hard. So I am backing up to a custom algorithm that always
   * uses swaps.
   */
  using Ref_t = details::SwappableIteratorPair<BKIter, BIter>;
  std::vector<Ref_t> dataKeyRef;
  while (key_begin != key_end) dataKeyRef.push_back({ key_begin++, begin++ });
  assert(begin == end);
  
  // sort it
  struct RefComparer {
    Comp comp;
    bool operator() (Ref_t const& a, Ref_t const& b) const
      { return comp(*(a.first), *(b.first)); }
  };
  details::unoptimisedQuickSort
    (dataKeyRef.begin(), dataKeyRef.end(), RefComparer{ std::move(comp) });
  
#else // this is a more standard (and slower) implementation:
  
  // move the content of `data` and copy a `keys` iterator into a new vector
  // of pairs
  using Ref_t
    = std::pair<BKIter, typename std::iterator_traits<BIter>::value_type>;
  std::vector<Ref_t> dataKeyRef;
  auto itData = begin, itKey = key_begin;
  while (itKey != key_end)
    dataKeyRef.emplace_back(itKey++, std::move(*itData++));
  assert(itData == end);
  
  // sort it
  struct RefComparer {
    Comp comp;
    bool operator() (Ref_t const& a, Ref_t const& b)
      { return comp(*(a.first), *(b.first)); }
  };
  std::sort
    (dataKeyRef.begin(), dataKeyRef.end(), RefComparer{ std::move(comp) });
  
  // move back into `data`
  itData = begin;
  for (Ref_t& keyAndData: dataKeyRef) *itData++ = std::move(keyAndData.second);
  
#endif // 0
  
} // util::sortLike()


// -----------------------------------------------------------------------------
template
  <typename DataColl, typename KeyColl, typename Comp /* = std::less<> */>
void util::sortCollLike
  (DataColl& data, KeyColl const& keys, Comp comp /* = {} */)
{
  using std::begin, std::end;
  sortLike(begin(data), end(data), begin(keys), end(keys), std::move(comp));
} // util::sortCollLike()


// -----------------------------------------------------------------------------


#endif // ICARUSALG_UTILITIES_SORTLIKE_H
