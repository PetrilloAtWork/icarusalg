/**
 * @file icarusalg/Utilities/AssnsCrosser.h
 * @brief Unit test for `icarus::ns::util::AssnsCrosser` class and utilities.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date June 9, 2023
 */

#ifndef ICARUSALG_UTILITIES_ASSNSCROSSER_H
#define ICARUSALG_UTILITIES_ASSNSCROSSER_H

// LArSoft libraries
#include "larcorealg/CoreUtils/DebugUtils.h" // lar::debug::demangle()
#include "larcorealg/CoreUtils/enumerate.h"

// framework libraries
#include "canvas/Persistency/Common/Assns.h"
#include "canvas/Persistency/Common/Ptr.h"
#if defined CANVAS_DEC_VERSION && (CANVAS_DEC_VERSION >= 31100)
# include "canvas/Persistency/Common/ProductPtr.h"
#endif
#include "canvas/Persistency/Provenance/BranchDescription.h"
#include "canvas/Persistency/Provenance/ProductID.h"
#include "canvas/Utilities/InputTag.h"
#include "canvas/Utilities/Exception.h"

// C/C++ standard libraries
#include <algorithm> // std::set_difference(), std::any_of(), ...
#include <functional> // std::mem_fn()
#include <utility> // std::move(), std::forward()
#include <ostream>
#include <vector>
#include <variant>
#include <optional>
#include <initializer_list>
#include <unordered_map>
#include <tuple>
#include <string>
#include <iterator> // std::move_iterator, std::back_inserter
#include <stdexcept> // std::logic_error
#include <type_traits> // std::is_constructible_v, std::enable_if_t...
#include <cstddef>
#include <cassert>


namespace icarus::ns::util {
  
  class InputSpec;
  template <typename T> class StartSpec;
  template <typename T> class StartSpecs;
  template <typename T> class InputSpecs;
  template <typename T> using hopTo = InputSpecs<T>;
  template <typename T> using startFrom = StartSpecs<T>;
  template <typename KeyType, typename... OtherTypes> class AssnsCrosser;
  
  template <typename KeyType, typename... OtherTypes, typename Event>
  AssnsCrosser<KeyType, OtherTypes...> makeAssnsCrosser
    (Event const& event, InputSpecs<OtherTypes>... inputSpecs);
  
  template <typename KeyType, typename... OtherTypes, typename Event>
  AssnsCrosser<KeyType, OtherTypes...> makeAssnsCrosser(
    Event const& event,
    StartSpecs<KeyType>, InputSpecs<OtherTypes>... inputSpecs
    );
  
  std::ostream& operator<< (std::ostream& out, InputSpec const& spec);
  template <typename T>
  std::ostream& operator<< (std::ostream& out, StartSpec<T> const& spec);
  template <typename T>
  std::ostream& operator<< (std::ostream& out, InputSpecs<T> const& specs);
  template <typename T>
  std::ostream& operator<< (std::ostream& out, StartSpecs<T> const& specs);

  namespace details {
    template <typename SupportedVariants> class SpecBase;
    template <typename SpecType> class InputSpecsBase;
    template <typename KeyType, typename TargetType> class AssnsMap;
    template <typename KeyType, typename... OtherTypes> class AssnsCrosserTypes;
    template <typename T> struct PointerSelector;
    using SupportedInputSpecs = std::variant<
        std::monostate
      , art::InputTag
      , art::ProductID
      >;
    template <typename T>
    using SupportedStartSpecs = std::variant<
        std::monostate
      , art::InputTag
      , art::ProductID
      , art::Ptr<T>
#if defined CANVAS_DEC_VERSION && (CANVAS_DEC_VERSION >= 531100)
      , art::ProductPtr<T>
#endif
      , std::vector<art::Ptr<T>>
      >;
    
  } // // namespace icarus::ns::util::details
  
} // namespace icarus::ns::util

// -----------------------------------------------------------------------------
/**
 * @brief Builds multi-hop one-to-many associations from associated pairs.
 * @tparam Key the type of the data to associate to
 * @tparam OtherTypes intermediate types to reach the target type (the last one)
 * 
 * This class facilitates the crossing of multi-level associations.
 * For example, suppose the data contains associations between hits
 * (`recob::Hit`) and tracks (`recob::Track`) and between tracks and particle
 * flow objects (`recob::PFParticle`). Starting from a particle flow object
 * (_PFO_ from now on), we want to know which hits it is associated to.
 * We need therefore to cross and join two associations. This problem is solved
 * by using a
 * `icarus::ns::util::AssnsCrosser<recob::PFParticle, recob::Track, recob::Hit>`
 * object.
 * 
 * This class supports any number of indirections ("hops").
 * 
 * One major issue in establishing the chain is to find the relevant association
 * data products. There are one key type (`Key`) and one or more types to
 * hop through until the target type is reached (`OtherTypes`, the last one of
 * which is the target type). Assuming there are _N_ types listed in
 * `OtherTypes`, and calling `Target` the last one (`OtherTypes[N-1]`),
 * there are as well _N_ hops to follow:
 * `Key` &rarr; `OtherTypes[0]`, `OtherTypes[0]` &rarr; `OtherTypes[1]`, ...
 * up to `OtherTypes[N-2]` &rarr; `Target`.
 * The interface of this object requires some information for each of the _N_
 * hops.
 *
 * Currently the following patterns are supported:
 * 
 *  * the data product input tag of all associations are known in advance,
 *    and there is only one of them. This is the simplest case for
 *    implementation. On the other end it may be hard for the user to know which
 *    are all the involved input tags, and the requirement of having a single
 *    association data product for each hop may be a deal-breaker.
 *  * the data product input tag of all associations are known in advance,
 *    and there may be more of them per hop. This is the case with fewest
 *    assumptions. As in the previous case, it may be hard for the user to know
 *    which are all the involved input tags.
 *  * the data product of the first association is known, but not all the others
 *    are. In that case, one assumption can be that the relevant associations
 *    are created by the same module and with the same label as the data product
 *    at the right side of the association. In the example above, this situation
 *    translates into knowing the tag for the track/PFO association, but not the
 *    hit/track one; and then the hit/track associations would be assumed to
 *    have been created by the same module, and with the same tag, which also
 *    created the track collection (because `recob::Track` is the object on the
 *    right of the known track/PFO association). This case is currently
 *    supported only when that assumption holds; otherwise, the behaviour is
 *    undefined.
 *  * the data product of the last association is known, but not all the others
 *    are. The assumption here may be the mirror of the one in the previous
 *    point. In the example above, this situation translates into knowing the
 *    tag for the hit/track association, but not the the track/PFO one; and then
 *    the track/PFO associations would be assumed to have been created by the
 *    same module, and with the same tag, which also created the PFO collection
 *    (which we know to be not likely). This case is currently supported only
 *    when that assumption holds; otherwise, the behaviour is undefined.
 *  * the data product of the starting data product (strictly speaking not the
 *    first association) is known, but not all the association data product tags
 *    are. This case is similar to the case where the first association was
 *    known, as described above, and it is supported under the same assumptions,
 *    which in this case extend to the first association as well.
 * 
 * The result can be limited to a selected list of key entries by listing the
 * desired elements in the first constructor argument (see `StartSpecs`, alias
 * `startFrom`). This mirrors the feature of `art::FindXxx`, but keep in mind
 * that while there the lookup is by index of the start list, in this object
 * the lookup is by pointer. This also implicitly quenches duplicates in the
 * input list, and there is no guarantee that the associations are presented
 * in the same key order as the start list (in fact, the order of the results
 * is not even defined in this object).
 * 
 * 
 * ### Many-to-one associations
 * 
 * The support for one-to-many associations in the hopping direction is full.
 * In the presence of many-to-one associations, there are some things to be
 * kept in mind.
 * 
 * In the case of many-to-one associations, the same target object may appear
 * associated to several keys.
 * 
 * The list of target objects associated to a key has an unspecified order and
 * it _can_ contain duplicates. For example, in a "diamond" association:
 *     
 *        B1
 *       /  \
 *     A1    C1
 *       \  /
 *        B2
 *     
 * that is an association `A1` &harr; `B1`, `A1` &harr; `B2`,
 * `C1` &harr; `B1` and `C1` &harr; `B2`, `C1` will appear in the list of
 * `C`s associated with `A1` twice, because there are two paths connecting
 * `A1` and `C1`.
 * 
 * 
 * ### Comparison with `art::FindManyP`
 * 
 * Both `art::FindManyP` and `icarus::ns::util::AssnsCrosser`:
 *  * support two directly associated data products
 *    (but then there is little reason to use `AssnsCrosser` over `FindManyP`).
 *  * precompute all the information at construction, so they are better
 *    instantiated once.
 *  * yield for each associated key a vector of _art_ pointers to the associated
 *    target elements.
 *  * support a generic `art::Event`-like interface, including (in principle)
 *    `gallery::Event`.
 * 
 * Differences include:
 *  * `AssnsCrosser` interface only covers the functionality of `art::FindManyP`
 *    and `art::FindOneP`, not `art::FindMany`.
 *  * of course, `AssnsCrosser` supports _indirectly_ associated data products.
 *  * the assumption on whether an association is one-to-one or one-to-many
 *    is reflected in which `art::FindXxx` class is chosen (respectively
 *    `art::FindOneP` and `art::FindManyP`), while here the same class
 *    `AssnsCrosser` is used, and the assumption is reflected on whether
 *    `assPtr()` or `assPtrs()` method is used for the query.
 *  * `AssnsCrosser` indexes by _art_ pointer of the key, while `art::FindManyP`
 *    indexes by the position of the key in the list specified as input (which
 *    is bound to match the pointer `key()` when a whole handle is specified as
 *    input).
 *  * `AssnsCrosser` does not support target metadata (the metadata on the
 *    intermediate hops is not relevant, since _art_ can use an association
 *    with metadata in place of one without, ignoring the metadata itself).
 *    This feature does not fundamentally conflict with the implementation, but
 *    neither the interface (presumably similar to `art::FindManyP`) nor the
 *    implementation were developed.
 * 
 * 
 * Examples
 * ---------
 * 
 * ### Setting up a association crosser object
 * 
 * Let's assume we have three data types, `DataTypeA` associated with
 * `DataTypeB` and the latter associated with `DataTypeC`.
 * The goal is to have the direct association from `DataTypeA` to `DataTypeC`.
 * 
 * If it is known that the associations between `DataTypeA` and `DataTypeB`
 * are all stored in data product tag `"B"` and the associations between
 * `DataTypeB` and `DataTypeC` are all stored in data product tag `"C"`,
 * the following initializations will work:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 * icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const AtoC
 *   { event, art::InputTag{ "B" }, art::InputTag{ "C" } };
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * or
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 * using icarus::ns::util::hopTo;
 * auto const AtoC = icarus::ns::util::makeAssnsCrosser<DataTypeA>
 *   (event, hopTo<DataTypeB>{ "B" }, hopTo<DataTypeC>{ "C" });
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * or
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 * using icarus::ns::util::startFrom, icarus::ns::util::hopTo;
 * icarus::ns::util::AssnsCrosser const AtoC{ event
 *   , startFrom<DataTypeA>{}
 *   , hopTo<DataTypeB>{ "B" }
 *   , hopTo<DataTypeC>{ "C" }
 *   };
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * or
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 * using icarus::ns::util::startFrom, icarus::ns::util::hopTo;
 * auto const AtoC = makeAssnsCrosser(event
 *   , startFrom<DataTypeA>{}
 *   , hopTo<DataTypeB>{ "B" }
 *   , hopTo<DataTypeC>{ "C" }
 *   );
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * The latter describe more clearly the relation between the data types and
 * their input tags.
 * 
 * If there are two sets of associations between `DataTypeA` and `DataTypeB`,
 * `"B:1"` and `"B:2"`, the following initializations will work:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 * icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const AtoC
 *   { event, { "B:1", "B:2" }, { "C" } };
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * or
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 * using icarus::ns::util::hopTo;
 * auto const AtoC = icarus::ns::util::makeAssnsCrosser<DataTypeA>(
 *   event,
 *   hopTo<DataTypeB>{ "B:1", "B:2" }, hopTo<DataTypeC>{ "C" }
 *   );
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * 
 * If the associations are needed only for a certain subset of pointers, it is
 * possible to specify them, in a way similar to `art::FindManyP`:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 * using icarus::ns::util::startFrom, icarus::ns::util::hopTo;
 * auto const AtoC = icarus::ns::util::makeAssnsCrosser<DataTypeA>(
 *   , startFrom{ ptrA1, ptrA2 }
 *   , hopTo<DataTypeB>{ "B" }
 *   , hopTo<DataTypeC>{ "C" }
 *   );
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * or
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 * std::vector const selectedAptr { ptrA1, ptrA2 };
 * using icarus::ns::util::startFrom, icarus::ns::util::hopTo;
 * auto const AtoC = icarus::ns::util::makeAssnsCrosser<DataTypeA>(
 *   , selectedAptr
 *   , hopTo<DataTypeB>{ "B" }
 *   , hopTo<DataTypeC>{ "C" }
 *   );
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Note that only a collection of type `std::vector<art::Ptr<DataTypeA>>` is
 * supported (for example, `art::PtrVector<DataTypeA>` would not be).
 * 
 * 
 * ### Using a association crosser object
 * 
 * As mentioned in the introduction, `AssnsCrosser` objects differ from
 * `art::FindManyP` in that they are queried via _art_ pointers rather than
 * indices.
 * 
 * The interface for the query is `assPtr()` (see its documentation for
 * examples).
 * 
 */
template <typename KeyType, typename... OtherTypes>
class icarus::ns::util::AssnsCrosser
  : public details::AssnsCrosserTypes<KeyType, OtherTypes...>
{
  
  using This_t = AssnsCrosser<KeyType, OtherTypes...>;
  
    public:
  
  using KeyPtr_t = typename This_t::KeyPtr_t;
  using TargetPtr_t = typename This_t::TargetPtr_t;
  using TargetPtrs_t = typename This_t::TargetPtrs_t;
  
  /**
   * @brief Constructor: reads and joins the specified associations.
   * @tparam Event type to read the data from (`art::Event` interface)
   * @param event data source
   * @param otherInputSpecs input specifications for all the hops
   * 
   * The associations are read and joined reading the data from `event`.
   * 
   * There needs to be one input specification for each hop, the first
   * specification being the one from the key to the first intermediate object
   * type.
   */
  template <typename Event>
  AssnsCrosser
    (Event const& event, InputSpecs<OtherTypes>... otherInputSpecs);
  
  /**
   * @brief Constructor: reads and joins the specified associations.
   * @tparam Event type to read the data from (`art::Event` interface)
   * @param event data source
   * @param startSpec specifies which type to start hopping from
   * @param otherInputSpecs input specifications for all the hops
   * 
   * This constructor acts exactly like
   * `AssnsCrosser(Event const&, InputSpecs<OtherTypes>...)`, but the additional
   * argument allows C++ to fully determine the type of the object from the
   * arguments, thus allowing the direct initialization syntax where data types
   * are specified only once and close to their input specification:
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
   * using icarus::ns::util::startFrom, icarus::ns::util::hopTo;
   * icarus::ns::util::AssnsCrosser const AtoC{ event
   *   , startFrom<DataTypeA>{}
   *   , hopTo<DataTypeB>{ "B" }
   *   );
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   */
  template <typename Event>
  AssnsCrosser(
    Event const& event,
    StartSpecs<KeyType> startSpec,
    InputSpecs<OtherTypes>... otherInputSpecs
    );

  /**
   * @brief Returns pointers to all target objects associated to `keyPtr`.
   * @param keyPtr pointer to the key object to find the associated objects of
   * @return a list pointers to all target objects associated to `keyPtr`
   * 
   * This query supports a one-to-many association.
   * If the `keyPtr` is unknown (either because it's not a valid object in this
   * context, or because the pointed object does not have any associated target
   * object) an empty collection is returned.
   * 
   * In this example, associating `DataTypeA` objects to `DataTypeC` ones,
   * all objects of type `DataTypeA` are tried one after the other:
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
   * icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const AtoC
   *   { event, art::InputTag{ "B" }, art::InputTag{ "C" } };
   * 
   * auto const& Ahandle = event.getValidHandle<std::vector<DataTypeA>>("B");
   * 
   * for (std::size_t iA = 0; iA < Ahandle->size(); ++iA) {
   *   
   *   art::Ptr<DataTypeA> const Aptr{ Ahandle, iA };
   *   
   *   std::vector<art::Ptr<DataTypeC>> const& Cptrs = AtoC.assPtrs(Aptr);
   *   
   *   // ...
   * } // for
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   * In the loop, a _art_ pointer to each `DataTypeA` object is created in order
   * to query the associated `DataTypeC`. See also `assPtr()` for further usage
   * patterns common to the two methods.
   */
  TargetPtrs_t const& assPtrs(KeyPtr_t const& keyPtr) const
    { return fAssnsMap.assPtrs(keyPtr); }
  
  /**
   * @brief Returns a pointer to the target object associated to `keyPtr`.
   * @param keyPtr pointer to the key object to find the associated objects of
   * @return a pointer to the target object associated to `keyPtr`
   * @throw art::Exception (code: `art::errors::LogicError`) if there are more
   *        than one target pointer associated to the specified key
   * 
   * This query assumes a one-to-one association.
   * If the `keyPtr` is unknown (either because it's not a valid object in this
   * context, or because the pointed object does not have any associated target
   * object) the pointer is returned null.
   * If there are more than one elements associated with the key,
   * an exception is thrown.
   * 
   * In this example, associating `DataTypeA` objects to `DataTypeC` ones,
   * all objects of type `DataTypeA` are tried one after the other:
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
   * icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const AtoC
   *   { event, art::InputTag{ "B" }, art::InputTag{ "C" } };
   * 
   * auto const& Ahandle = event.getValidHandle<std::vector<DataTypeA>>("B");
   * 
   * for (std::size_t iA = 0; iA < Ahandle->size(); ++iA) {
   *   
   *   art::Ptr<DataTypeA> const Aptr{ Ahandle, iA };
   *   
   *   art::Ptr<DataTypeC> const Cptr = AtoC.assPtr(Aptr);
   *   
   *   // ...
   * } // for
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   * In the loop, a _art_ pointer to each `DataTypeA` object is created in order
   * to query the associated `DataTypeC`. While this may add complications in
   * the simplest case (as in the example), it allows for mixing multiple input
   * collections (e.g. trying to cross an associations of tracks stored with one
   * data product per cryostat to CRT hits stored in a single data product), and
   * to pass a selection of objects (via _art_ pointers):
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
   * icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const AtoC
   *   { event, art::InputTag{ "B" }, art::InputTag{ "C" } };
   * 
   * auto const& Ahandle = event.getValidHandle<std::vector<DataTypeA>>("B");
   * 
   * for (art::Ptr<DataTypeA> const& Aptr: selectDataA(Ahandle)) {
   *   
   *   art::Ptr<DataTypeC> const Cptr = AtoC.assPtr(Aptr);
   *   
   *   // ...
   * } // for
   * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
   * (with
   * `template <typename Handle> std::vector<art::Ptr<DataTypeA>> selectDataA(Handle const& handle)`
   * some selection function, templated to `Handle` to support both
   * `art::Handle` and `art::ValidHandle`).
   * 
   */
  TargetPtr_t const& assPtr(KeyPtr_t const& keyPtr) const;
  
  
    private:
  
  using Key_t = typename This_t::Key_t;
  using Target_t = typename This_t::Target_t;
  
  using AssnsMap_t = details::AssnsMap<Key_t, Target_t>;
  
  /// Which algorithm to use for traversing the associations.
  enum class HoppingAlgo { forward, backward };
  
  AssnsMap_t fAssnsMap; ///< Associated objects per key.
  
  
  static TargetPtr_t const NullTargetPtr; ///< Used as return reference value.
  
  
  /// Returns the full content of the association map.
  template <typename Event>
  AssnsMap_t prepare(
    Event const& event,
    StartSpecs<KeyType> startSpecs, InputSpecs<OtherTypes>... otherInputSpecs
    ) const;
  
  /// Determines which algorithm should be used for association traversal.
  HoppingAlgo chooseTraversalAlgorithm(
    StartSpecs<KeyType> const& startSpecs,
    InputSpecs<OtherTypes> const&... otherInputSpecs
    ) const;
  
  /// Returns a list of relevant pointers from the start specifications.
  template <typename T, typename Event>
  std::optional<details::PointerSelector<T>> keysFromSpecs
    (Event const& event, StartSpecs<T> const& specs) const;

}; // icarus::ns::util::AssnsCrosser


// -----------------------------------------------------------------------------
/// Wrapper to specify a single source of an association.
template <typename SupportedVariants>
class icarus::ns::util::details::SpecBase: public SupportedVariants {
  using SupportedVariants::SupportedVariants;
  
    public:
  using SupportedSpecs_t = SupportedVariants;
      
  /// Returns the specification (as a variant).
  // Newer C++17 revision won't need this.
  SupportedSpecs_t const& spec() const { return *this; }
  
    protected:
  
  struct HasSpecTest {
    
    bool operator() (std::monostate) const { return false; }
    bool operator() (art::ProductID id) const { return id != art::ProductID{}; }
    bool operator() (art::InputTag const& tag) const { return !tag.empty(); }
    
  }; // HasSpecTest

}; // icarus::ns::util::SpecBase


// -----------------------------------------------------------------------------
/// Wrapper to specify a single source of an association.
class icarus::ns::util::InputSpec
  : public details::SpecBase<details::SupportedInputSpecs>
{
  using Base_t = details::SpecBase<details::SupportedInputSpecs>;
  using Base_t::Base_t;
  
    public:
  
  bool hasSpec() const noexcept;
  
}; // icarus::ns::util::InputSpec


// -----------------------------------------------------------------------------
/// Wrapper to specify a single source of an association.
template <typename T>
class icarus::ns::util::StartSpec
  : public details::SpecBase<details::SupportedStartSpecs<T>>
{
  using Base_t = details::SpecBase<details::SupportedStartSpecs<T>>;
  using Base_t::Base_t;
  
    public:
  using Key_t = T;
  
  bool hasSpec() const noexcept;
  
}; // icarus::ns::util::StartSpec


// -----------------------------------------------------------------------------
template <typename SpecType>
class icarus::ns::util::details::InputSpecsBase
  : private std::vector<SpecType> // saved list of specifications
{
  using Specs_t = std::vector<SpecType>;
  
    public:
  using Spec_t = SpecType;
  
  /// Constructor: single input specification (whatever can construct it).
  template <
    typename... Args,
    typename = std::enable_if_t
      <std::is_constructible_v<Spec_t, Args...>>
    >
  InputSpecsBase(Args&&... specArgs)
    : Specs_t{ Spec_t{ std::forward<Args>(specArgs)... } } {}
  
  /// Constructor: a list of input specifications.
  InputSpecsBase(std::initializer_list<Spec_t> specs)
    : Specs_t
      { std::move_iterator{ specs.begin() }, std::move_iterator{ specs.end() } }
    {}
  
  /// Constructor: a list of input specifications.
  InputSpecsBase(std::vector<Spec_t> specs)
    : Specs_t(std::move(specs)) {}
  
  /// Returns whether at least one of the specs specifies an input.
  bool hasSpecs() const noexcept;
  
  /// Returns whether at least one of the specs specifies no input.
  bool hasEmptySpecs() const noexcept;
  
  using Specs_t::empty;
  using Specs_t::size;
  using Specs_t::begin;
  using Specs_t::end;
  using Specs_t::cbegin;
  using Specs_t::cend;
  using Specs_t::at;
  using Specs_t::operator[];
  
}; // icarus::ns::util::details::InputSpecsBase


// -----------------------------------------------------------------------------
/**
 * @brief Wrapper to specify the key type for the association hops.
 * @tparam T type of the association hop these specifications refer to
 * 
 * There are several ways to specify the content of a start list;
 * all of them require the explicit specification of the type `T`
 * (however the full type can be sometimes deduced in `AssnsCrosser` constructor
 * call or `makeAssnsCrosser()` function).
 * If the start list is empty or includes only invalid entries (like null
 * pointers, empty input tags, invalid product IDs) it is assumed that all the
 * pointers in the deduced input set are desired.
 * 
 * Several input types are supported: the same ones as in `InputSpecs`,
 * plus `art::Ptr<T>`, `std::vector<art::Ptr<T>>` and `art::ProductPtr<T>`.
 * 
 * Examples in conjunction with `AssnsCrosser` and `makeAssnsCrosser()`,
 * assuming `event` and `DataType`s being a data source object and data types:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 * using icarus::ns::util::StartSpecs;
 * 
 * std::vector const Aptrs{ ptrA1, ptrA2 };
 * 
 * // all A entries found in the "B" associations
 * icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const AtoC_1
 *   { event, StartSpec<DataTypeA>{}, "B", "C" };
 * 
 * // all A entries found in the "B" associations, same as above
 * icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const AtoC_1
 *   { event, {}, "B", "C" };
 * 
 * // all entries found in the "A" data product
 * icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const AtoC_1
 *   { event, "A", "B", "C" };
 * 
 * // only entries pointed by `ptrA1` and `ptrA2`
 * icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const AtoC_1
 *   { event, { ptrA1, ptrA2 }, "B", "C" };
 * 
 * // only entries pointed by the pointers in `Aptrs`
 * icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const AtoC_1
 *   { event, Aptrs, "B", "C" };
 * 
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Note that `startFrom` is available as an alias of `StartSpecs`, with exactly
 * the same semantics and syntax.
 */
template <typename T>
class icarus::ns::util::StartSpecs
  : public details::InputSpecsBase<StartSpec<T>>
{
  using details::InputSpecsBase<StartSpec<T>>::InputSpecsBase;
};


// -----------------------------------------------------------------------------
/**
 * @brief Wrapper to specify all the sources of an association.
 * @tparam T type of the association hop these specifications refer to
 * 
 * There are several ways to specify the content of the specifications;
 * all of them require the explicit specification of the type `T`.
 * 
 * Examples in conjunction with `AssnsCrosser` and `makeAssnsCrosser()`,
 * assuming `event` and `DataType`s being a data source object and data types:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 * using icarus::ns::util::InputSpecs, icarus::ns::util::InputSpec;
 * 
 * using AtoZ_t = icarus::ns::util::AssnsCrosser<
 *   DataTypeA, DataTypeB, DataTypeC, DataTypeD, DataTypeE, DataTypeF
 *   >;
 * 
 * AtoZ_t const AtoZ{ event
 *   
 *   // implicit conversion to `art::InputTag`:
 *   , InputSpecs<DataTypeB>{ "TagB" }
 *   
 *   // implicit conversion to `art::InputTag` then to `InputSpecs<DataTypeC>`:
 *   , "TagC"
 *   
 *   // explicit vector of input tags (not recommended):
 *   , InputSpecs<DataTypeD>{ std::vector<InputSpec>{ "TagD1", "TagD2" } }
 *   
 *   // list of input tags, converted to `InputSpecs<DataTypeE>`:
 *   , InputSpecs<DataTypeE>{ "TagE1", "TagE2" }
 *   
 *   // implicit list of input tags, converted to `InputSpecs<DataTypeF>`:
 *   , { "TagF1", "TagF2" }
 *   
 *   };
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Note that `hopTo` is available as an alias of `InputSpecs`, with exactly the
 * same semantics and syntax.
 */
template <typename T>
class icarus::ns::util::InputSpecs: public details::InputSpecsBase<InputSpec> {
  using details::InputSpecsBase<InputSpec>::InputSpecsBase;
};


// ----------------------------------------------------------------------------
// ---  Implementation
// ----------------------------------------------------------------------------
namespace icarus::ns::util::details {
  
  /// The first of the template types.
  template <typename... Ts>
  using first_type_t = std::tuple_element_t<0, std::tuple<Ts...>>;
  
  /// The type with index `N` in the type list reversed from `Ts`.
  template <std::size_t N, typename... Ts>
  struct end_type {
    using type
      = std::tuple_element_t<sizeof...(Ts) - 1 - N, std::tuple<Ts...>>;
  };
  
  /// Returns the last of `Ts` types.
  template <typename... Ts>
  struct last_type { using type = typename end_type<0, Ts...>::type; };
  
  template <typename KeyType, typename FirstHopType, typename... OtherHopTypes>
  struct MapJoiner;
  
  template <typename KeyType, typename TargetType>
  std::ostream& operator<<
    (std::ostream& out, AssnsMap<KeyType, TargetType> const& map);
  
  /// Appends to `dest` a copy of the content of `src`.
  template <typename Dest, typename Src>
  Dest& append(Dest& dest, Src const& src);

  /// Steals and appends to `dest` the content of `src`.
  template <typename Dest, typename Src>
  Dest& append(Dest& dest, Src&& src);

  /// Returns a constant reference to the `Index`-th of the `data` arguments.
  template <int Index, typename... Ts>
  auto const& getElement(Ts const&... data);

  /// Returns a vector with the elements of the sorted `minuend` which
  /// are not present in the sorted `subtrahend`.
  template <typename Minuend, typename Subtrahend>
  std::vector<typename Minuend::value_type> set_difference
    (Minuend const& minuend, Subtrahend const& subtrahend);
  
  template <typename SpecType>
  std::ostream& operator<<
    (std::ostream& out, InputSpecsBase<SpecType> const& specs);
  
} // namespace icarus::ns::util::details


// -----------------------------------------------------------------------------
template <typename KeyType, typename... OtherTypes>
class icarus::ns::util::details::AssnsCrosserTypes {
  
    protected:
  
  static constexpr std::size_t NOtherTypes = sizeof...(OtherTypes);
  static_assert(NOtherTypes >= 1, "AssnsCrosser requires at least two types.");
  
    public:
  
  using Key_t = KeyType;
  using Target_t = typename last_type<OtherTypes...>::type;
  
  using KeyPtr_t = art::Ptr<Key_t>;
  using TargetPtr_t = art::Ptr<Target_t>;
  using TargetPtrs_t = std::vector<TargetPtr_t>;
  
  using Assns_t = art::Assns<Key_t, Target_t>;
  
  using AssnsMap_t = std::unordered_map<KeyPtr_t, TargetPtrs_t>;
  
}; // icarus::ns::util::details::AssnsCrosserTypes


// -----------------------------------------------------------------------------
template <typename KeyType, typename TargetType>
class icarus::ns::util::details::AssnsMap
  : public details::AssnsCrosserTypes<KeyType, TargetType>
{
  
    public:
  
  using This_t = AssnsMap<KeyType, TargetType>;
  
  using KeyPtr_t = typename This_t::KeyPtr_t;
  using TargetPtr_t = typename This_t::TargetPtr_t;
  using TargetPtrs_t = typename This_t::TargetPtrs_t;
  
  using AssnsMap_t = typename This_t::AssnsMap_t;
  
  
  // --- BEGIN -- Modify interface ---------------------------------------------
  ///@name Modify interface
  ///@{
  
  /// Add a `targetPtr` associated to a `keyPtr` (duplicates not checked).
  This_t& add(KeyPtr_t const& keyPtr, TargetPtr_t const& targetPtr);
  
  /// Add all `targetPtrs` associated to a `keyPtr` (duplicates not checked).
  This_t& add(KeyPtr_t const& keyPtr, TargetPtrs_t const& targetPtrs);
  
  /// Add all `targetPtrs` associated to a `keyPtr` (duplicates not checked).
  /// The content of `targetPtrs` is lost.
  This_t& add(KeyPtr_t const& keyPtr, TargetPtrs_t&& targetPtrs);
  
  /// Returns the pointers associated to `keyPtr` (empty if none).
  /// The key is not associated with them any more.
  TargetPtrs_t yieldAssPtrs(KeyPtr_t const& keyPtr);
  
  /// Returns an iterable of pairs key/targets, which can be modified
  /// (do **not** modify the key value!).
  AssnsMap_t& assnsMap() { return fAssnsMap; }
  
  /// Removes all the stored associations and keys.
  void clear() { fAssnsMap.clear(); }
  
  ///@}
  // --- END ---- Modify interface ---------------------------------------------
  
  
  // --- BEGIN -- Query interface ----------------------------------------------
  ///@name Query interface
  ///@{
  
  /// Returns whether there is data in the map.
  bool empty() const noexcept { return fAssnsMap.empty(); }
  
  /// Returns the pointers associated to `keyPtr` (empty if none).
  TargetPtrs_t const& assPtrs(KeyPtr_t const& keyPtr) const;
  
  /// Returns a map of key pointers to a sequence of associated target pointers.
  AssnsMap_t const& assnsMap() const { return fAssnsMap; }
  
  /// Returns a sorted list of all the product IDs in the key pointers.
  std::vector<art::ProductID> keyProductIDs() const;
  
  /// Returns a sorted list of all the product IDs in the target pointers.
  std::vector<art::ProductID> targetProductIDs() const;
  
  ///@}
  // --- END ---- Query interface ----------------------------------------------
  
  
  /// Returns a map from target to key describing the same associations as this.
  AssnsMap<TargetType, KeyType> flip() const;
  
  
    private:
  
  static TargetPtrs_t const EmptyColl;
  
  AssnsMap_t fAssnsMap; ///< Key pointer -> all associated target pointers.
  
}; // icarus::ns::util::details::AssnsMap


// -----------------------------------------------------------------------------
/// Instructions on which pointers of type T to select.
template <typename T>
struct icarus::ns::util::details::PointerSelector {
  using Ptr_t = art::Ptr<T>;
  
  PointerSelector(std::vector<Ptr_t> ptrs, std::vector<art::ProductID> IDs);
  
  bool operator() (Ptr_t const& ptr) const;
  
    private:
  std::vector<Ptr_t> fPtrs; ///< Listed pointers pass.
  std::vector<art::ProductID> fIDs; ///< All objects with these ID pass.
  
}; // icarus::ns::util::details::PointerSelector


// -----------------------------------------------------------------------------
// ---  template implementation
// -----------------------------------------------------------------------------
template <typename Dest, typename Src>
Dest& icarus::ns::util::details::append(Dest& dest, Src const& src) {
  using std::begin, std::end;
  dest.insert(end(dest), begin(src), end(src));
  return dest;
} // icarus::ns::util::details::append(Src const&)


template <typename Dest, typename Src>
Dest& icarus::ns::util::details::append(Dest& dest, Src&& src) {
  using std::empty, std::begin, std::end;
  if (empty(dest)) dest = std::move(src);
  else {
    dest.insert(
      end(dest), std::move_iterator(begin(src)), std::move_iterator(end(src))
      );
    src.clear();
  }
  return dest;
} // icarus::ns::util::details::append(Src&&)


// -----------------------------------------------------------------------------
template <int Index, typename... Ts>
auto const& icarus::ns::util::details::getElement(Ts const&... data) {
  
  auto access = std::forward_as_tuple(data...);
  
  constexpr std::size_t index = (Index < 0)? sizeof...(data) + Index: Index;
  return std::get<index>(access);
  
} // icarus::ns::util::details::getElement()


// -----------------------------------------------------------------------------
template <typename Minuend, typename Subtrahend>
std::vector<typename Minuend::value_type>
icarus::ns::util::details::set_difference
  (Minuend const& minuend, Subtrahend const& subtrahend)
{
  using std::begin, std::end;
  std::vector<typename Minuend::value_type> diff;
  std::set_difference(
    begin(minuend), end(minuend), begin(subtrahend), end(subtrahend),
    back_inserter(diff)
    );
  return diff;
} // icarus::ns::util::details::set_difference()


// -----------------------------------------------------------------------------
// ---  icarus::ns::util::details::AssnsMap
// -----------------------------------------------------------------------------
template <typename KeyType, typename TargetType>
typename icarus::ns::util::details::AssnsMap<KeyType, TargetType>::TargetPtrs_t
const icarus::ns::util::details::AssnsMap<KeyType, TargetType>::EmptyColl;


// -----------------------------------------------------------------------------
template <typename KeyType, typename TargetType>
auto icarus::ns::util::details::AssnsMap<KeyType, TargetType>::add
  (KeyPtr_t const& keyPtr, TargetPtr_t const& targetPtr) -> This_t&
  { fAssnsMap[keyPtr].push_back(targetPtr); return *this; }


// -----------------------------------------------------------------------------
template <typename KeyType, typename TargetType>
auto icarus::ns::util::details::AssnsMap<KeyType, TargetType>::add
  (KeyPtr_t const& keyPtr, TargetPtrs_t const& targetPtrs) -> This_t&
{
  append(fAssnsMap[keyPtr], targetPtrs);
  return *this;
} // icarus::ns::util::details::AssnsMap<>::add(TargetPtrs_t&)


// -----------------------------------------------------------------------------
template <typename KeyType, typename TargetType>
auto icarus::ns::util::details::AssnsMap<KeyType, TargetType>::add
  (KeyPtr_t const& keyPtr, TargetPtrs_t&& targetPtrs) -> This_t&
{
  append(fAssnsMap[keyPtr], std::move(targetPtrs));
  return *this;
} // icarus::ns::util::details::AssnsMap<>::add(TargetPtrs_t&&)


// -----------------------------------------------------------------------------
template <typename KeyType, typename TargetType>
auto icarus::ns::util::details::AssnsMap<KeyType, TargetType>::assPtrs
  (KeyPtr_t const& keyPtr) const -> TargetPtrs_t const&
{
  auto const it = fAssnsMap.find(keyPtr);
  return (it == fAssnsMap.end())? EmptyColl: it->second;
} // icarus::ns::util::details::AssnsMap<>::assPtrs()


// -----------------------------------------------------------------------------
template <typename KeyType, typename TargetType>
auto icarus::ns::util::details::AssnsMap<KeyType, TargetType>::keyProductIDs()
  const -> std::vector<art::ProductID>
{
  std::vector<art::ProductID> IDs;
  for (auto const& pairs: fAssnsMap) {
    art::ProductID const ID = pairs.first.id();
    if (std::find(IDs.rbegin(), IDs.rend(), ID) == IDs.rend())
      IDs.push_back(ID);
  } // for
  std::sort(IDs.begin(), IDs.end());
  return IDs;
} // icarus::ns::util::details::AssnsMap::keyProductIDs()


// -----------------------------------------------------------------------------
template <typename KeyType, typename TargetType>
auto icarus::ns::util::details::AssnsMap<KeyType, TargetType>::targetProductIDs
  () const -> std::vector<art::ProductID>
{
  std::vector<art::ProductID> IDs;
  for (auto const& pairs: fAssnsMap) {
    for (art::Ptr<TargetType> const& ptr: pairs.second) {
      art::ProductID const ID = ptr.id();
      if (std::find(IDs.rbegin(), IDs.rend(), ID) == IDs.rend())
        IDs.push_back(ID);
    } // for pointers
  } // for pairs
  std::sort(IDs.begin(), IDs.end());
  return IDs;
} // icarus::ns::util::details::AssnsMap::targetProductIDs()


// -----------------------------------------------------------------------------
template <typename KeyType, typename TargetType>
auto icarus::ns::util::details::AssnsMap<KeyType, TargetType>::yieldAssPtrs
  (KeyPtr_t const& keyPtr) -> TargetPtrs_t
{
  auto it = fAssnsMap.find(keyPtr);
  return (it == fAssnsMap.end())
    ? EmptyColl: std::exchange(it->second, TargetPtrs_t{});
} // icarus::ns::util::details::AssnsMap<>::yieldAssPtrs()


// -----------------------------------------------------------------------------
template <typename KeyType, typename TargetType>
auto icarus::ns::util::details::AssnsMap<KeyType, TargetType>::flip() const
  -> AssnsMap<TargetType, KeyType>
{
  // brute force
  AssnsMap<TargetType, KeyType> map;
  for (auto const& [ key, targets ]: fAssnsMap) {
    for (art::Ptr<TargetType> const& target: targets) {
      map.add(target, key);
    } // for key targets
  } // for source keys
  return map;
} // icarus::ns::util::details::AssnsMap<>::flip()


// -----------------------------------------------------------------------------
template <typename KeyType, typename TargetType>
std::ostream& icarus::ns::util::details::operator<<
  (std::ostream& out, AssnsMap<KeyType, TargetType> const& map)
{
  auto const& assnsMap = map.assnsMap();
  if (assnsMap.empty()) {
    out << "no association";
  }
  else {
    out << "associations:";
    std::size_t nTargets = 0;
    for (auto const& [ key, targets ]: assnsMap) {
      out << "\n  " << key << ": " << targets.size() << " associated targets";
      if (targets.empty()) continue;
      nTargets += targets.size();
      for (auto const& [ iTarget, target ]: ::util::enumerate(targets))
        out << "\n    [" << iTarget << "] " << target;
    } // for keys
    out << "\n"
      << assnsMap.size() << " keys associated to " << nTargets << " targets";
  } // if ... else
  return out;
} // icarus::ns::util::details::operator<< (AssnsMap)


// -----------------------------------------------------------------------------
// --- icarus::ns::util::details::PointerSelector
// -----------------------------------------------------------------------------
template <typename T>
icarus::ns::util::details::PointerSelector<T>::PointerSelector
  (std::vector<Ptr_t> ptrs, std::vector<art::ProductID> IDs)
  : fPtrs{ std::move(ptrs) }, fIDs{ std::move( IDs ) }
{
  std::sort(fPtrs.begin(), fPtrs.end());
  std::sort(fIDs.begin(), fIDs.end());
}


// -----------------------------------------------------------------------------
template <typename T>
bool icarus::ns::util::details::PointerSelector<T>::operator()
  (Ptr_t const& ptr) const
{
  if (std::binary_search(fIDs.begin(), fIDs.end(), ptr.id())) return true;
  return std::binary_search(fPtrs.begin(), fPtrs.end(), ptr);
}


// -----------------------------------------------------------------------------
// ---  icarus::ns::util::InputSpec and related
// -----------------------------------------------------------------------------
bool icarus::ns::util::InputSpec::hasSpec() const noexcept {
  
  struct HasInputSpecTest: HasSpecTest {};
  
  return std::visit(HasInputSpecTest{}, spec());
} // icarus::ns::util::InputSpec::hasSpec()


// -----------------------------------------------------------------------------
template <typename T>
bool icarus::ns::util::StartSpec<T>::hasSpec() const noexcept {
  
  struct HasStartSpecTest: Base_t::HasSpecTest {
    using Base_t::HasSpecTest::operator();
    bool operator() (art::Ptr<Key_t> const& ptr) const
      { return ptr.isNonnull(); }
#if defined CANVAS_DEC_VERSION && (CANVAS_DEC_VERSION >= 531100)
    bool operator() (art::ProductPtr<Key_t> const& ptr) const
      { return (*this)(ptr.id()); }
#endif
    bool operator() (std::vector<art::Ptr<Key_t>> const& ptrs) const
      {
        return std::any_of
          (ptrs.begin(), ptrs.end(), std::mem_fn(&art::Ptr<Key_t>::isNonnull));
      }
  };
  
  return std::visit(HasStartSpecTest{}, Base_t::spec());
} // icarus::ns::util::StartSpec<>::hasSpec()


// -----------------------------------------------------------------------------
template <typename SpecType>
bool icarus::ns::util::details::InputSpecsBase<SpecType>::hasSpecs
  () const noexcept
{
  return std::any_of(begin(), end(), std::mem_fn(&SpecType::hasSpec));
}


// -----------------------------------------------------------------------------
template <typename SpecType>
bool icarus::ns::util::details::InputSpecsBase<SpecType>::hasEmptySpecs
  () const noexcept
{
  return !std::all_of(begin(), end(), std::mem_fn(&SpecType::hasSpec));
}


// -----------------------------------------------------------------------------
inline std::ostream& icarus::ns::util::operator<<
  (std::ostream& out, InputSpec const& spec)
{
  struct InputSpecDumper {
    std::ostream& out;
    
    InputSpecDumper(std::ostream& out): out(out) {}
    
    void operator() (std::monostate) const
      { out << "autodetect"; }
    void operator() (art::InputTag const& tag) const
      { out << "tag '" << tag.encode() << "'"; }
    void operator() (art::ProductID const& ID) const
      { out << "ProdID=" << ID; }
    
  }; // InputSpecDumper

  std::visit(InputSpecDumper{ out }, spec.spec());
  return out;
  
} // icarus::ns::util::operator<< (InputSpec<T>)


// -----------------------------------------------------------------------------
template <typename SpecType>
inline std::ostream& icarus::ns::util::details::operator<<
  (std::ostream& out, InputSpecsBase<SpecType> const& specs)
{
  std::size_t const nSpecs = specs.size();
  
  if (nSpecs == 0) {
    out << "no specs";
    return out;
  }
  
  std::size_t iSpec = 0;
  if (nSpecs > 1)
    out << specs.size() << " specs: [0] ";
  out << "{ " << specs[iSpec] << " }";
  while (++iSpec < nSpecs) {
    out << "[" << iSpec << "] {" << specs[iSpec] << "}";
  }
  
  return out;
} // icarus::ns::util::details::operator<< (InputSpecsBase)


// -----------------------------------------------------------------------------
template <typename T>
std::ostream& icarus::ns::util::operator<<
  (std::ostream& out, InputSpecs<T> const& specs)
{
  out << "<Target: " << lar::debug::demangle<T>() << "> "
    << static_cast<details::InputSpecsBase<InputSpec> const&>(specs);
  return out;
} // icarus::ns::util::details::operator<< (InputSpecs<T>)


// -----------------------------------------------------------------------------
template <typename T>
std::ostream& icarus::ns::util::operator<<
  (std::ostream& out, StartSpecs<T> const& specs)
{
  out << "<Key: " << lar::debug::demangle<T>() << "> "
    << static_cast<details::InputSpecsBase<InputSpec> const&>(specs);
  return out;
} // icarus::ns::util::details::operator<< (StartSpecs<T>)


// -----------------------------------------------------------------------------
// ---  icarus::ns::util::details::MapJoiner
// -----------------------------------------------------------------------------
template <typename KeyType, typename FirstHopType, typename... OtherHopTypes>
struct icarus::ns::util::details::MapJoiner {
  
  // the first and other hop types are kept separate because it's hard
  // to split the parameter pack of the others from the complete one otherwise
  
  using TargetType = typename last_type<FirstHopType, OtherHopTypes...>::type;
  
  struct NoSelector_t
    { template <typename T> bool operator() (T const&) const { return true; } };
  
  static constexpr std::size_t nHops = 1 + sizeof...(OtherHopTypes);
  
  static constexpr NoSelector_t NoSelector{};
  
  /**
   * @brief Returns a association map from `KeyType` to `TargetType`.
   * @tparam Event a data repository (`art::Event`-like interface)
   * @param event the event to read the associations from
   * @param firstHopInputSpec specification for the first hop associations
   * @param otherHopInputSpec specification for all other hop associations
   * @return a association map from `KeyType` to `TargetType`
   * 
   * The algorithm starts from the last hop (associations from the
   * previous-to-last of `OtherHopTypes` to the `TargetType`) and attached the
   * associations hopping backward.
   * 
   * This algorithm is slightly simpler than the forward one.
   */
  template <typename Event>
  static AssnsMap<KeyType, TargetType> joinBackward(
    Event const& event,
    InputSpecs<FirstHopType> firstHopInputSpec,
    InputSpecs<OtherHopTypes>... otherHopInputSpecs
  ) {
      
      if constexpr(nHops == 1) {
        std::vector<art::InputTag> const firstHopTags
          = extractTagList(std::move(firstHopInputSpec), event);
        return assnsToMap<KeyType, TargetType>(event, firstHopTags);
      }
      else {
        // 1 is the first hop (KeyType -> FirstHopType),
        // 2 is all the others (FirstHopType -> TargetType)
        auto assnsMap2 = MapJoiner<FirstHopType, OtherHopTypes...>::joinBackward
          (event, std::move(otherHopInputSpecs)...);
        return leftExtendMapWithAssns<KeyType>
          (std::move(assnsMap2), event, std::move(firstHopInputSpec));
      } // if more than one hop
    } // joinBackward()
  
  
  /**
   * @brief Returns a association map from `KeyType` to `TargetType`.
   * @tparam Event a data repository (`art::Event`-like interface)
   * @tparam Selector functor with `bool operator() const (art::Ptr<KeyType>)`
   * @param event the event to read the associations from
   * @param firstHopInputSpec specification for the first hop associations
   * @param otherHopInputSpec specification for all other hop associations
   * @param selector if specified, only keys passing the selector are considered
   * @return a association map from `KeyType` to `TargetType`
   * 
   * The algorithm starts from the first hop (associations from the
   * `KeyType` to the `FirstHopType`) and attached the associations hopping
   * forward.
   */
  template <typename Event, typename Selector>
  static AssnsMap<KeyType, TargetType> joinForward(
    Event const& event,
    InputSpecs<FirstHopType> firstHopInputSpec,
    InputSpecs<OtherHopTypes>... otherHopInputSpecs,
    std::optional<Selector> const& selector = NoSelector
  ) {
      std::vector<art::InputTag> const firstHopTags
        = extractTagList(std::move(firstHopInputSpec), event);
      
      auto leftMap
        = assnsToMap<KeyType, FirstHopType>(event, firstHopTags, selector);
      
      if constexpr(nHops == 1) {
        return leftMap;
      }
      else {
        return multiRightExtendMapWithAssns
          (std::move(leftMap), event, std::move(otherHopInputSpecs)...);
      }
    } // joinForward()
  
  
  /// Returns an association map from `tag` associations read from `event`.
  /// Only left entries passing `selector` are included.
  template <
    typename Left, typename Right,
    typename Event, typename InputTags, typename Selector = NoSelector_t
    >
  static AssnsMap<Left, Right> assnsToMap(
    Event const& event, InputTags const& tags,
    std::optional<Selector> const& selector = std::nullopt
    ) {
      AssnsMap<Left, Right> assnsMap;
      for (art::InputTag const& tag: tags)
        addAssnsToMap(assnsMap, event, tag, selector);
      return assnsMap;
    } // assnsToMap()
  
  /// Extends the association `map` with `tag` associations read from `event`,
  /// adding only the ones with left pointer listed in `selector`.
  template <
    typename Left, typename Right,
    typename Event, typename Selector = NoSelector_t
    >
  static AssnsMap<Left, Right>& addAssnsToMap(
    AssnsMap<Left, Right>& map, Event const& event, art::InputTag const& tag,
    std::optional<Selector> const& selector = std::nullopt
    ) {
      return addAssnsToMap(
        map, event.template getProduct<art::Assns<Left, Right>>(tag), selector
        );
    }
  
  /// Extends the association `map` with the specified _art_ associations,
  /// adding only the ones with left pointer passing `selector`.
  template <typename Left, typename Right, typename Selector = NoSelector_t>
  static AssnsMap<Left, Right>& addAssnsToMap(
    AssnsMap<Left, Right>& map, art::Assns<Left, Right> const& assns,
    std::optional<Selector> const& selector = std::nullopt
    ) {
      for (auto const& [ leftPtr, rightPtr ]: assns) {
        if (!selector || (*selector)(leftPtr)) map.add(leftPtr, rightPtr);
      }
      return map;
    } // addAssnsToMap()
  
  /**
   * @brief Returns a new association map extended on the key side
   * @tparam NewLeft (mandatory) type of key for the new map
   * @tparam Left type of key of the existing map
   * @tparam Right type of target of the existing map
   * @tparam Event type of the data repository
   * @tparam InputTags type of a collection of `art::InputTag`
   * @param map the map to be "extended"; it will be depleted of its content
   * @param event the data repository to read the associations from
   * @param specs the specification for the input of the extending association
   * @return a new association map
   * 
   * The returned association map has `NewLeft` as the new key type and the same
   * target type (`Right`) as the input `map`.
   * The resulting map is joining the key of the input `map` with the target of
   * the associations being read.
   * The map _may_ come out smaller than the two inputs.
   */
  template <
    typename NewLeft, typename Left, typename Right, typename Event, typename T
    >
  static AssnsMap<NewLeft, Right> leftExtendMapWithAssns(
    AssnsMap<Left, Right>&& map, Event const& event, InputSpecs<T> specs) {
      // read the associations with the material for the extension
      bool const bAutodetect = specs.hasEmptySpecs();
      std::vector<art::InputTag> const tags
        = extractTagList(std::move(specs), event);
      std::vector<art::ProductID> neededIDs;
      if (bAutodetect) neededIDs = map.keyProductIDs();
      auto const leftMap = mapExtensionPreparation<NewLeft, Left, 1>
        (event, tags, std::move(neededIDs));
      return joinMaps(leftMap, std::move(map));
    } // leftExtendMapWithAssns()
  
  
  template <
    typename Left, typename Right, typename NextRight, typename... MoreRights,
    typename Event
    >
  static AssnsMap<Left, typename last_type<NextRight, MoreRights...>::type>
  multiRightExtendMapWithAssns(
    AssnsMap<Left, Right>&& map,
    Event const& event,
    InputSpecs<NextRight> nextInputSpec,
    InputSpecs<MoreRights>... otherInputSpec
    )
    {
      AssnsMap<Left, NextRight> assnsMap = rightExtendMapWithAssns<NextRight>
        (std::move(map), event, std::move(nextInputSpec));
      
      if constexpr(sizeof...(MoreRights) == 0) {
        return assnsMap;
      }
      else {
        return multiRightExtendMapWithAssns
          (std::move(assnsMap), event, std::move(otherInputSpec)...);
      }
    } // multiRightExtendMapWithAssns()
  
  
  /**
   * @brief Returns a new association map extended on the target side
   * @tparam NewRight (mandatory) type of key for the new map
   * @tparam Left type of key of the existing map
   * @tparam Right type of target of the existing map
   * @tparam Event type of the data repository
   * @param map the map to be "extended"; it will be depleted of its content
   * @param event the data repository to read the associations from
   * @param specs the specification for the input of the extending association
   * @return a new association map
   * 
   * The returned association map has `NewRight` as the new target type and the
   * same key type (`Left`) as the input `map`.
   * The resulting map is joining the key of the association being read with the
   * key of the input `map`.
   * The map _may_ come out smaller than the two inputs.
   */
  template<
    typename NewRight, typename Left, typename Right, typename Event, typename T
    >
  static AssnsMap<Left, NewRight> rightExtendMapWithAssns
    (AssnsMap<Left, Right> map, Event const& event, InputSpecs<T> specs)
    {
      // read the associations with the material for the extension
      bool const bAutodetect = specs.hasEmptySpecs();
      std::vector<art::InputTag> const tags
        = extractTagList(std::move(specs), event);
      std::vector<art::ProductID> neededIDs;
      if (bAutodetect) neededIDs = map.targetProductIDs();
      auto rightMap = mapExtensionPreparation<Right, NewRight, 0U>
        (event, tags, std::move(neededIDs));
      return joinMaps(map, std::move(rightMap));
    }
  
  /// Joins two maps in the middle, stealing content from the right one.
  template <typename Left, typename Middle, typename Right>
  static AssnsMap<Left, Right> joinMaps
    (AssnsMap<Left, Middle> const& leftMap, AssnsMap<Middle, Right>&& rightMap)
    {
      AssnsMap<Left, Right> map;
      for (auto& [ leftPtr, middlePtrs ]: leftMap.assnsMap()) {
        for (art::Ptr<Middle> const& middlePtr: middlePtrs) {
          map.add(leftPtr, rightMap.yieldAssPtrs(middlePtr));
        } // for middle pointers
      } // for left map
      return map;
    } // joinMaps()
  
  
  /**
   * @brief Collects a map of Left-to-Right pointers.
   * @tparam Left type of key in the map
   * @tparam Right type of target in the map
   * @tparam JointSide `0` for `Left` side, `1` for `Right` side
   * @tparam Event type of data repository to read data from (`art::Event` I/F)
   * @tparam InputTags type of collection of `art::InputTag` objects
   * @param event the event to read the data from
   * @param tags the list of input tags to needed `Left`-to`Right` associations
   * @param requiredIDs list of product IDs needed for the the extension
   * @return a `Left`-to-`Right` association map
   * @throw art::Exception (code: `art::errors::ProductNotFound`) if a required
   *        association is not found
   * 
   * The returned map contains all the `Left`-to-`Right` associations specified
   * by `tags`; if any is missing, an exception is thrown.
   * 
   * After these associations are collected, the product ID of the pointers
   * in the `JointSide` side are compared with the ones specified in the
   * `requiredIDs` list. For each ID which is in `requiredIDs` but does not
   * appear in the associations collected so far, the algorithm attempts
   * to read another `Left`-to-`Right` association using the exact same input
   * tag as the one associated to that product ID. If such association data
   * product is found, its content is added to the map. Otherwise, the algorithm
   * moves on, not considering this a fatal error.
   * 
   * The only clear fatal error condition tested by this algorithm is when no
   * mandatory tag is specified, some IDs are present in `requiredIDs` _and_
   * no data product has been found from any of them. In that case, an exception
   * is thrown (still `art::errors::ProductNotFound` code).
   */
  template <
    typename Left, typename Right, std::size_t JointSide,
    typename Event, typename InputTags
    >
  static AssnsMap<Left, Right> mapExtensionPreparation(
    Event const& event, InputTags const& tags,
    std::vector<art::ProductID> const& requiredIDs
    ) {
      /*
       * First read all the associations with tags that are explicitly tagged;
       * then compare their ID with the IDs that we are required.
       * For each required ID not present in the original tags,
       * an association is read (failure is not an error).
       */
      using Assns_t = AssnsMap<Left, Right>;
      Assns_t map = assnsToMap<Left, Right>(event, tags);
      
      constexpr auto extractProductIDs = (JointSide == 0)
        ? &Assns_t::keyProductIDs: &Assns_t::targetProductIDs;
      std::vector<art::ProductID> const mapIDs = (map.*extractProductIDs)();
      
      std::vector<art::ProductID> const missingIDs
        = details::set_difference(requiredIDs, mapIDs);
      
      unsigned int nDiscovered = 0;
      for (art::ProductID const ID: missingIDs) {
        art::InputTag const tag = getInputTag(event, ID);
        auto handle = event.template getHandle<art::Assns<Left, Right>>(tag);
        if (!handle) continue;
        addAssnsToMap(map, *handle);
        ++nDiscovered;
      } // for
      
      // error check for an extreme case:
      using std::empty;
      if (empty(tags) && !missingIDs.empty() && (nDiscovered == 0)) {
        std::string const leftName = lar::debug::demangle<Left>();
        std::string const rightName = lar::debug::demangle<Right>();
        // even if this error is not triggered we may still be missing some
        throw art::Exception{ art::errors::ProductNotFound }
          << "During preparation of " << leftName << " <=> " << rightName
          << " associations to join on "
          << ((JointSide == 0)? leftName: rightName)
          << " couldn't find any of the needed association data products!"
          << " Some must be explicitly specified via input tag."
          << "\n";
      }
      
      return map;
    } // mapExtensionPreparation()
  
  /// Returns the input tag associated to the product `ID` (empty if not found).
  template <typename Event>
  static art::InputTag getInputTag(Event const& event, art::ProductID ID)
    {
      art::BranchDescription const* branchDescr
        = event.getProductDescription(ID).get();
      if (!branchDescr) return {};
      return { branchDescr->inputTag() };
    } // getInputTag()
  
  /// Helper returning a list of input tags out of a InputSpec
  template <typename Event>
  struct TagListExtractor {
    Event const* event;
    
    TagListExtractor(Event const* event): event{ event } {}
    
    std::vector<art::InputTag> operator()
      (std::monostate) const
      { return {}; }
    
    std::vector<art::InputTag> operator()
      (art::InputTag&& inputTag) const
      { return { std::move(inputTag) }; }
    
    std::vector<art::InputTag> operator()
      (art::ProductID ID) const
      { return { getInputTag(*event, ID) }; }
    
  }; // TagListExtractor
  
  template <typename Event, typename T>
  static std::vector<art::InputTag> extractTagList
    (InputSpecs<T>&& inputSpecs, Event const& event)
    {
      std::vector<art::InputTag> tags;
      for (InputSpec& spec: inputSpecs) {
        append(tags,
          visit(
            TagListExtractor<Event>{ &event },
            static_cast<SupportedInputSpecs&&>(spec)
          ));
      } // for
      return tags;
    }
  
}; // icarus::ns::util::details::MapJoiner


// -----------------------------------------------------------------------------
// ---  icarus::ns::util::AssnsCrosser
// -----------------------------------------------------------------------------
template <typename KeyType, typename... OtherTypes>
typename icarus::ns::util::AssnsCrosser<KeyType, OtherTypes...>::TargetPtr_t
const icarus::ns::util::AssnsCrosser<KeyType, OtherTypes...>::NullTargetPtr;


// -----------------------------------------------------------------------------
template <typename KeyType, typename... OtherTypes>
template <typename Event>
icarus::ns::util::AssnsCrosser<KeyType, OtherTypes...>::AssnsCrosser(
  Event const& event,
  InputSpecs<OtherTypes>... otherInputSpecs
)
  : AssnsCrosser{ event, StartSpecs<KeyType>{}, std::move(otherInputSpecs)... }
{}


// -----------------------------------------------------------------------------
template <typename KeyType, typename... OtherTypes>
template <typename Event>
icarus::ns::util::AssnsCrosser<KeyType, OtherTypes...>::AssnsCrosser(
  Event const& event,
  StartSpecs<KeyType> startSpecs,
  InputSpecs<OtherTypes>... otherInputSpecs
)
  : fAssnsMap
    { prepare(event, std::move(startSpecs), std::move(otherInputSpecs)... ) }
{}


// -----------------------------------------------------------------------------
template <typename KeyType, typename... OtherTypes>
auto icarus::ns::util::AssnsCrosser<KeyType, OtherTypes...>::assPtr
  (KeyPtr_t const& keyPtr) const -> TargetPtr_t const&
{
  TargetPtrs_t const& targets = assPtrs(keyPtr);
  if (targets.size() > 1) {
    // using LogicError because that's what art::FindOne does
    throw art::Exception{ art::errors::LogicError }
      << "AssnsCrosser::assPtr(): there are " << targets.size() << " "
      << lar::debug::demangle<Target_t>() << " objects associated to Ptr<"
      << lar::debug::demangle<Key_t>() << ">=" << keyPtr << "!\n";
  }
  return targets.empty()? NullTargetPtr: targets.front();
} // icarus::ns::util::AssnsCrosser<KeyType, OtherTypes...>::assPtr()


// -----------------------------------------------------------------------------
template <typename KeyType, typename... OtherTypes>
template <typename Event>
auto icarus::ns::util::AssnsCrosser<KeyType, OtherTypes...>::prepare(
  Event const& event,
  StartSpecs<KeyType> startSpecs, InputSpecs<OtherTypes>... otherInputSpecs
) const -> AssnsMap_t
{
  std::optional<details::PointerSelector<Key_t>> keySelector
    = keysFromSpecs(event, startSpecs);
  HoppingAlgo const algo
    = chooseTraversalAlgorithm(startSpecs, otherInputSpecs...);
  switch (algo) {
    case HoppingAlgo::forward:
      return details::MapJoiner<KeyType, OtherTypes...>::joinForward
        (event, std::move(otherInputSpecs)..., keySelector);
    case HoppingAlgo::backward:
      return details::MapJoiner<KeyType, OtherTypes...>::joinBackward
        (event, std::move(otherInputSpecs)... );
    default:
      throw std::logic_error
        { "Unexpected direction: " + std::to_string(static_cast<int>(algo)) };
  } // switch
} // icarus::ns::util::AssnsCrosser<>::prepare()


// -----------------------------------------------------------------------------
template <typename KeyType, typename... OtherTypes>
auto icarus::ns::util::AssnsCrosser<KeyType, OtherTypes...>
  ::chooseTraversalAlgorithm
(
  StartSpecs<KeyType> const& startSpecs,
  InputSpecs<OtherTypes> const&... otherInputSpecs
) const -> HoppingAlgo {
  /*
   * If there is a start specification, we need (and can) to go forward.
   * Otherwise, we go backward (faster) unless there is no specification for
   * the last hop (in which case we can't start from the back).
   * 
   * When both algorithms are available, the backward one is chosen.
   */
  
  bool const hasStartInfo = startSpecs.hasSpecs();
  
  bool const hasEndSpecs
    = details::getElement<-1>(otherInputSpecs...).hasSpecs();
  
  constexpr std::size_t nHops = sizeof...(OtherTypes);
  
#if 0
  // --- BEGIN -- DEBUG --------------------------------------------------------
  // print details about how the specifications are received:
  auto const formatter
    = [&out=std::cout](auto const& specs){ out << "\n  " << specs; };
  std::cout
    << "Start specs: " << startSpecs << ", hasStartInfo=" << hasStartInfo
    << "\n" << nHops << " hops:";
  (formatter(otherInputSpecs), ...);
  std::cout << "\nLast spec: " << details::getElement<-1>(otherInputSpecs...)
    << " -> hasEndSpecs=" << hasEndSpecs << std::endl;
  
  // --- END ---- DEBUG --------------------------------------------------------
#endif // 0
  if constexpr(nHops == 1) {
    if (hasStartInfo) return HoppingAlgo::forward;
    if (hasEndSpecs) return HoppingAlgo::backward;
    throw std::logic_error
      { "Insufficient specifications for single association traversal." };
  }
  else {
    bool const hasFirstSpecs
      = hasStartInfo || details::getElement<0>(otherInputSpecs...).hasSpecs();
    
    if (hasFirstSpecs) return HoppingAlgo::forward;
    if (hasEndSpecs) return HoppingAlgo::backward;
    
    throw std::logic_error{
      "Insufficient specifications for traversal of " + std::to_string(nHops)
      + " associations."
      };
    
  }
  
} // icarus::ns::util::AssnsCrosser<>::chooseTraversalAlgorithm()


// -----------------------------------------------------------------------------
template <typename KeyType, typename... OtherTypes>
template <typename T, typename Event>
auto icarus::ns::util::AssnsCrosser<KeyType, OtherTypes...>::keysFromSpecs
  (Event const& event, StartSpecs<T> const& specs) const
  -> std::optional<details::PointerSelector<T>>
{
  /*
   * An interface needs to be established.
   * The specification may follow the pattern of the InputSpecs, but can't
   * use InputSpecsBase as it is now, since the hosted data types may need a
   * templated type (see below).
   * A list of possible supported input:
   *  * a product ID: get the handle to the `std::vector<T>` data product and
   *    the product size (which unfortunately means to read the data product
   *    itself) and make a list of all pointers
   *  * a handle or valid handle: 
   *  * an input tag: get the handle of the `std::vector<T>` data product and
   *    proceed with that as above
   *  * a product pointer: get the product ID and proceed with that
   *  * a pointer to the key: require that pointer directly
   *  * a vector of pointers: require all the pointers in the vector
   * 
   * One can avoid reading the size of the data product, and possibly the data
   * product itself, by having a special value that denotes all possible
   * pointers from a data product, i.e. from a product ID.
   * This special value can be treated either as a variant (e.g. including
   * a `art::ProductPtr<T>` and a `art::Ptr<T>`) or assigning a special key to
   * an `art::Ptr<T>`, or keeping a separate list of the two types of
   * specifications (which is probably the most efficient way).
   */
  if (!specs.hasSpecs()) return std::nullopt;
  
  std::vector<art::Ptr<T>> ptrs;
  std::vector<art::ProductID> IDs;
  
  for (StartSpec<T> const& spec: specs) {
    
    if      (std::holds_alternative<art::InputTag>(spec)) {
      auto const& handle = event.template getValidHandle<std::vector<T>>
        (std::get<art::InputTag>(spec));
      IDs.push_back(handle.id());
    }
    else if (std::holds_alternative<art::Ptr<T>>(spec)) {
      ptrs.push_back(std::get<art::Ptr<T>>(spec));
    }
    else if (std::holds_alternative<std::vector<art::Ptr<T>>>(spec)) {
      details::append(ptrs, std::get<std::vector<art::Ptr<T>>>(spec));
    }
    else if (std::holds_alternative<art::ProductID>(spec)) {
      IDs.push_back(std::get<art::ProductID>(spec));
    }
#if defined CANVAS_DEC_VERSION && (CANVAS_DEC_VERSION >= 531100)
    else if (std::holds_alternative<art::ProductPtr<T>>(spec)) {
      IDs.push_back(std::get<art::ProductPtr<T>>(spec).id());
    }
#endif
    else if (std::holds_alternative<std::monostate>(spec)) {
      // ignored, since there are other specs (or `hasSpecs()` would be `false`)
    }
    else throw art::Exception{ art::errors::LogicError }
      << "Start spec holds an unexpected type (" << spec.index() << ").\n";
    
  } // for specs
  
  return std::optional<details::PointerSelector<T>>
    { std::in_place, std::move(ptrs), std::move(IDs) };
} // icarus::ns::util::AssnsCrosser<>::keysFromSpecs()


// -----------------------------------------------------------------------------
template <typename KeyType, typename... OtherTypes, typename Event>
auto icarus::ns::util::makeAssnsCrosser(
  Event const& event,
  InputSpecs<OtherTypes>... inputSpecs
) -> AssnsCrosser<KeyType, OtherTypes...>
{
  return AssnsCrosser<KeyType, OtherTypes...>(event, std::move(inputSpecs)...);
}


// -----------------------------------------------------------------------------
template <typename KeyType, typename... OtherTypes, typename Event>
auto icarus::ns::util::makeAssnsCrosser(
  Event const& event,
  StartSpecs<KeyType>,
  InputSpecs<OtherTypes>... inputSpecs
) -> AssnsCrosser<KeyType, OtherTypes...>
{
  return AssnsCrosser<KeyType, OtherTypes...>(event, std::move(inputSpecs)...);
}


// -----------------------------------------------------------------------------

#endif // ICARUSALG_UTILITIES_ASSNSCROSSER_H
