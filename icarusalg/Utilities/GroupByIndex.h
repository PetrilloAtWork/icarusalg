/**
 * @file   icarusalg/Utilities/GroupByIndex.h
 * @brief  Algorithm to cluster objects according to their index.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date   July 7, 2023
 *
 * This is a header-only library.
 */

#ifndef ICARUSALG_UTILITIES_GROUPBYINDEX_H
#define ICARUSALG_UTILITIES_GROUPBYINDEX_H


// C/C++ standard libraries
#include <vector>
#include <utility> // std::forward()
#include <cstddef> // std::size_t

//------------------------------------------------------------------------------
namespace icarus::ns::util {
  template <typename T> class GroupByIndex;
  
  // deduction guide
  template <typename Coll, typename KeyFunc>
  GroupByIndex(Coll const&, KeyFunc&&)
    -> GroupByIndex<typename Coll::value_type>;
  
} // namespace icarus::ns::util

/**
 * @brief Creates a map of objects grouped by an index
 * @tparam T type of the objects to group
 * 
 * This class keeps a "map" of objects grouped by an "index".
 * The index is supposed to be an integer whose value ranges from `0` to some
 * number; a list of objects is allocated for each of the `N` indices.
 * 
 * Each group is implemented as a vector (`std::vector`) of pointers to the
 * original data. _These groups are valid only as long as the original data is
 * accessible._
 * 
 * Access is valid for any index, even `N` and above; in the latter cases, a
 * prepacked empty list is returned.
 * 
 * The map is defined on construction and can't be modified afterwards.
 * Also the pointed original objects can't be modified via the pointers from
 * this map.
 * While it is required to specify the method to extract a grouping index out of
 * an object of type `T`, this extraction function is only used during the
 * construction and it's not kept with the map.
 * 
 * Example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 * auto const& waveforms
 *   = event.getProduct<std::vector<raw::OpDetWaveform>>("daqPMT");
 * 
 * icarus::ns::util::GroupByIndex byChannel
 *  { waveforms, std::mem_fn(&raw::OpDetWaveform::ChannelNumber) };
 * 
 * for (std::vector<raw::OpDetWaveform const*> const& chWf: byChannel) {
 *   if (chWf.empty()) continue;
 *   std::cout << "Channel " << chWf.front()->ChannelNumber() << ": "
 *     << chWf.size() << " waveforms" << std::endl;
 * }
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * 
 */
template <typename T>
class icarus::ns::util::GroupByIndex {
  
    public:
  
  using Object_t = T; ///< Type of the object being grouped.
  
  /// Collection of objects (as non-mutable pointers to the original position).
  using ObjectPtrColl_t = std::vector<Object_t const*>;
  using size_type = typename std::vector<ObjectPtrColl_t>::size_type;
  using const_iterator = typename std::vector<ObjectPtrColl_t>::const_iterator;
  
  /**
   * @brief Constructor: groups the elements of the collection.
   * @tparam Coll type of collection of objects of type `T`
   * @tparam KeyFunc type of functor to extract an index from object of type `T`
   * @param coll collection of objects of type `T` to be grouped
   * @param extractKey functor extracting an index from an object of type `T`
   */
  template <typename Coll, typename KeyFunc>
  GroupByIndex(Coll const& coll, KeyFunc&& extractKey)
    : fMap{ buildMap(coll, std::forward<KeyFunc>(extractKey)) } {}
  
  /// Returns the list of objects in the specified group `index`.
  ObjectPtrColl_t const& operator[] (std::size_t index) const
    { return (index < size())? fMap[index]: EmptyGroup; }
  
  /// Returns whether there is at least one entry in the map.
  bool empty() const noexcept { return fMap.empty(); }
  
  /// Returns the number of groups in the map (including empty ones).
  size_type size() const noexcept { return fMap.size(); }
  
  /**
   * @name Iterators
   * 
   * All functions return constant iterators whose value type is
   * `ObjectPtrColl_t`.
   */
  /// @{
  const_iterator begin() const { return cbegin(); }
  const_iterator cbegin() const { return fMap.cbegin(); }
  const_iterator end() const { return cend(); }
  const_iterator cend() const { return fMap.cend(); }
  /// @}
  
    private:
  
  static ObjectPtrColl_t const EmptyGroup;
  
  std::vector<ObjectPtrColl_t> fMap; ///< Internal representation of the map.
  
  /// Groups the data and returns the underlying map.
  template <typename Coll, typename KeyFunc>
  static std::vector<ObjectPtrColl_t> buildMap
    (Coll const& coll, KeyFunc&& extractKey);
  
}; // icarus::ns::util::GroupByIndex


// -----------------------------------------------------------------------------
// --- Template implementation
// -----------------------------------------------------------------------------
template <typename T>
typename icarus::ns::util::GroupByIndex<T>::ObjectPtrColl_t const
icarus::ns::util::GroupByIndex<T>::EmptyGroup;


// -----------------------------------------------------------------------------
template <typename T>
template <typename Coll, typename KeyFunc>
auto icarus::ns::util::GroupByIndex<T>::buildMap
  (Coll const& coll, KeyFunc&& extractKey) -> std::vector<ObjectPtrColl_t>
{
  std::vector<ObjectPtrColl_t> map;
  
  auto const accessMap = [&map](std::size_t index) -> ObjectPtrColl_t&
    { if (index >= map.size()) map.resize(index + 1); return map[index]; };
  
  for (Object_t const& obj: coll) accessMap(extractKey(obj)).push_back(&obj);
  
  return map;
} // icarus::ns::util::GroupByIndex<>::buildMap()


// -----------------------------------------------------------------------------

#endif // ICARUSALG_UTILITIES_GROUPBYINDEX_H
