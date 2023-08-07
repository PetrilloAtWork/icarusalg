/**
 * @file test/FrameworkEventMockup.h
 * @brief Simple _art_-like event mockup.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date June 9, 2023
 * 
 * This library is header only (although there are a couple of inlined things
 * that would probably warrant an implementation file).
 */

#ifndef ICARUSALG_TEST_FRAMEWORKEVENTMOCKUP_H
#define ICARUSALG_TEST_FRAMEWORKEVENTMOCKUP_H

// framework libraries
#include "canvas/Persistency/Common/Assns.h"
#include "canvas/Persistency/Common/Ptr.h"
#include "canvas/Persistency/Provenance/ProductID.h"
#include "canvas/Persistency/Provenance/BranchDescription.h"
#include "canvas/Persistency/Provenance/ProcessConfiguration.h"
#include "canvas/Persistency/Provenance/TypeLabel.h"
#include "canvas/Utilities/TypeID.h"
#include "canvas/Utilities/InputTag.h"
#include "canvas/Utilities/Exception.h"
#include "cetlib/exempt_ptr.h"
#include "fhiclcpp/ParameterSetID.h"

// C/C++ standard libraries
#include <map>
#include <vector>
#include <any>
#include <typeindex>
#include <typeinfo>
#include <utility> // std::move()
#include <cstddef>


//------------------------------------------------------------------------------
namespace testing::mockup {
  class Event;
  template <typename T> class Handle;
  template <typename T> class ValidHandle;
  template <typename T> class PtrMaker;
  namespace details { template <typename T> class HandleBase; }
} // namespace testing::mockup

/**
 * @brief Mock-up class with a ridiculously small `art::Event`-like interface.
 * 
 * This "event" contains and owns data objects and can return a constant
 * reference to them on demand. It is intended to develop unit tests for
 * code that requires to read data from an event.
 * 
 * The interface is mimicking _art_'s and _gallery_'s `Event` classes, but it's
 * reduced to the very bare minimum.
 * 
 * Supported operations:
 *  * adding a data product associating it with an input tag (`art::InputTag`);
 *    the interface of this `put()` is inspired by _art_'s, but does not match
 *    it (nor it is intended to). In particular, this class does not currently
 *    use `std::unique_ptr` to store data products.
 *  * requesting a data product via `art::InputTag`: `getProduct()` mirrors the
 *    actual `art::Event` interface (it should be also in _gallery_, but as of
 *    `v1_20_02` that interface has not been added).
 *  * requesting the product ID (`art::ProductID`) of a data product specified
 *    by `art::InputTag` with `getProductID()`; this is _very different_ from
 *    `art::Event::getProductID()`, which returns ID only for data products
 *    from the current (producer?) module.
 * 
 * Pretty much everything else is _not_ supported, including also:
 *  * handles
 *  * product tokens
 *  * views
 *  * selectors
 *  * reading many data products at once
 * 
 * Example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 * testing::mockup::Event fillEvent() {
 *   testing::mockup::Event event;
 *   event.put(std::vector<float>{ 0.3, 0.6, 0.9 }, art::InputTag{ "A" });
 *   event.put(std::vector<int>{ 1, 6, 5, 9 }, art::InputTag{ "B" });
 *   return event;
 * }
 * 
 * testing::mockup::Event const event = fillEvent();
 * 
 * auto const& dataB = event.getProduct<std::vector<int>>(art::InputTag{ "B" });
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * 
 */
class testing::mockup::Event {
  
    public:
  
  static std::string const DefaultProcessName; ///< Default process name.
  
  /**
   * @brief Constructor.
   * @param processName sets the (default) process name for products without one
   */
  Event(std::string processName = DefaultProcessName)
    : fProcessName{ std::move(processName) }
    {}
  
  
  Event(Event&&) = default;
  Event& operator= (Event&&) = default;
  
  
  // --- BEGIN --- Data population interface -----------------------------------
  /// @name Data population interface
  /// @{
  
  /**
   * @brief Moves and registers the specified data under the specified `tag`.
   * @param T type of the data product being put into the event
   * @param tag the input tag this data product will be registered under
   * @param data the content of the data product
   * @return the ID of the data product just added
   * @throw art::Exception (code: `aer::errors::ProductRegistrationFailure`)
   *        if a data product with this type and tag is already registered
   * 
   * The `data` is moved into the event and it will be owned by it from now on.
   * A product ID is assigned to the data product and returned.
   * 
   * Note that this interface is subtly different from `art::Event`: here we
   * must supply a full input `tag`, while _art_ supports just an optional
   * instance name; and _art_ (from v. 3.11) returns a handle which is
   * convertible to `art::ProductID` instead of the product ID itself.
   */
  template <typename T>
  art::ProductID put(T&& data, art::InputTag tag);
  
  /// @}
  // --- END ----- Data population interface -----------------------------------
  
  
  // --- BEGIN --- Query and retrieval interface -------------------------------
  /// @name Query and retrieval interface
  /// @{
  
  /// Returns the ID of data product of type `T` and specified input `tag`.
  template <typename T>
  art::ProductID getProductID(art::InputTag const& tag) const;
  
  /// Returns the data product of type `T` and specified input `tag`.
  /// @throw art::Exception (code: `art::errors::ProductNotFound`) if not found
  template <typename T>
  T const& getProduct(art::InputTag const& tag) const;
  
  /// Returns a handle to the data product of type `T` and specified `tag`.
  template <typename T>
  Handle<T> getHandle(art::InputTag const& tag) const;
  
  /// Returns a handle to the data product of type `T` and specified `tag`.
  /// @throws art::Exception (code: `art::errors::ProductNotFound`) if not found
  template <typename T>
  Handle<T> getValidHandle(art::InputTag const& tag) const;
  
  /**
   * @brief Returns the branch description for the specified product ID.
   * @param ID the product ID to query about
   * @return a branch description object, partially filled
   * 
   * Most of the information in the _art_ branch description either does not
   * apply or it is hard to discover in this mockup.
   * Currently the only information reliably stored is the input tag.
   */
  cet::exempt_ptr<art::BranchDescription const> getProductDescription
    (art::ProductID ID) const;
  
  
  /// @}
  // --- END ----- Query and retrieval interface -------------------------------
  
  
    private:
  
  struct ProductKey: std::pair<art::InputTag, std::type_index> {
    
    using std::pair<art::InputTag, std::type_index>::pair;
    
    static int comp(art::InputTag const& a, art::InputTag const& b) noexcept;
    
  }; // ProductKey
  
  friend bool operator< (ProductKey const& a, ProductKey const& b) noexcept;
  
  struct DataProductRecord_t {
    art::InputTag tag;
    art::ProductID id;
    std::any data;
  };
  
  struct BranchRecord_t {
    art::BranchDescription branchDescr;
  };
  
  
  // don't copy (not deleted because we may want in the future a helper to copy)
  Event(Event const&) = default;
  Event& operator= (Event const&) = default;
  
  
  // --- BEGIN --- Configuration -----------------------------------------------
  
  std::string fProcessName;
  
  // --- END ----- Configuration -----------------------------------------------
  
  
  // --- BEGIN --- Object data -------------------------------------------------
  
  std::size_t fLastProductID = art::ProductID{}.value(); // initialized invalid
  
  std::map<ProductKey, DataProductRecord_t> fDataPointers; ///< The data.
  
  /// Some "branch" information.
  std::map<art::ProductID, BranchRecord_t> fProductIDs;
  
  // --- END ----- Object data -------------------------------------------------
  
  
  /// Returns the pointer to the product information for `tag`.
  /// @returns pointer to the information record, `nullptr` if not available
  template <typename T>
  DataProductRecord_t const* getProductInfo(art::InputTag const& tag) const;
  
  /// Returns the pointer to the data in the record. Throws if wrong type.
  template <typename T>
  T const* getDataPointer(DataProductRecord_t const& dataRecord) const;
  
  /// Returns the pointer to the product information for `tag`.
  /// @throws art::Exception (`art::errors::ProductNotFound`) if not available
  template <typename T>
  DataProductRecord_t const& getValidProductInfo
    (art::InputTag const& tag) const;
  
  /// Adds the default process name to the `tag` if it does not have any.
  art::InputTag completeTag(art::InputTag tag) const;
  
  template <typename T>
  static ProductKey makeKey(art::InputTag tag);
  
}; // testing::mockup::Event


/// Base class for mockup data product handles.
template <typename T>
class testing::mockup::details::HandleBase {
  art::ProductID fID;       ///< ID of this product.
  T const* fData = nullptr; ///< Pointer to the actual data.
  
    protected:
  void checkValidity() const;
  
    public:
  using element_type = T;
  class HandleTag {}; ///< Utility tag to recognise a handle.
  
  HandleBase() = default;
  HandleBase(art::ProductID ID, T const* data): fID{ ID }, fData{ data } {}
  
  T const& operator*() const { return *product(); }
  T const* operator->() const { return product(); }
  T const* product() const { return fData; }

  art::ProductID id() const { return fID; }
  
  explicit operator bool() const noexcept { return isValid(); }
  
  /// Returns whether the handle has actual data and from a valid source.
  bool isValid() const noexcept { return fData && (fID != art::ProductID{}); }
  
  /// Returns whether the handle has actual data.
  bool failedToGet() const { return fData == nullptr; }
  
}; // testing::mockup::details::HandleBase


/// Mockup class of data product handle. Acts like a "smart" pointer.
template <typename T>
class testing::mockup::Handle: public details::HandleBase<T> {
  using Base_t = details::HandleBase<T>;
  
    public:
  using Base_t::Base_t;
  
  T const* product() const
    { Base_t::checkValidity(); return Base_t::product(); }

}; // testing::mockup::Handle


/// Mockup class of data product valid handle. Acts like a "smart" pointer.
template <typename T>
class testing::mockup::ValidHandle: public details::HandleBase<T> {
  using Base_t = details::HandleBase<T>;
  
    public:
  ValidHandle(art::ProductID ID, T const* data): Base_t{ ID, data } {}
  
}; // testing::mockup::ValidHandle


// -----------------------------------------------------------------------------
/**
 * @brief Creates `art::Ptr` from the specified data product.
 * @tparam T data type of the pointers
 * 
 * This class is initialised with a data product (either product ID and data,
 * or event and input tag) of type `std::vector<T>` and can return functional
 * `art::Ptr` to the elements of that data product.
 * 
 * Example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 * testing::mockup::Event event;
 * event.put(std::vector<float>{ 0.3, 0.6, 0.9 }, art::InputTag{ "A" });
 * event.put(std::vector<int>{ 1, 6, 5, 9 }, art::InputTag{ "B" });
 * 
 * auto const& dataB = event.getProduct<std::vector<int>>(art::InputTag{ "B" });
 * 
 * testing::mockup::PtrMaker<float> const makeAptr{ event, art::InputTag{ "A" } };
 * testing::mockup::PtrMaker<int> const makeBptr{ event, art::InputTag{ "B" } };
 * 
 * art::Assns<float, int> assnsAB;
 * assnsAB.addSingle(makeAptr(1), makeBptr(1));
 * assnsAB.addSingle(makeAptr(2), makeBptr(3));
 * event.put(std::move(assnsAB, art::InputTag{ "B" });
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * 
 */
template <typename T>
class testing::mockup::PtrMaker {
  
    public:
  using Data_t = T;
  using ProdColl_t = std::vector<Data_t>;
  using Ptr_t = art::Ptr<Data_t>;
  
  /**
   * @brief Constructor: pointers to a product of specified `tag` from `event`.
   * @param event the "event" to read the data product from
   * @param tag the input tag of the data product
   * 
   * The data product with the specified `tag` is read from `event`, and an
   * `art::Ptr` is created out of it.
   * The pointer can actually dereference to the data.
   * 
   * This interface is not compatible with `art::PtrMaker`.
   * 
   * The data product must exist in `event`.
   */
  PtrMaker(Event const& event, art::InputTag const& tag)
    : PtrMaker
      { event.getProductID<ProdColl_t>(tag), event.getProduct<ProdColl_t>(tag) }
    {}
  
  /**
   * @brief Constructor: pointers with product `prodID` and pointing to `data`.
   * @param prodID product ID to be assigned to the pointers
   * @param data data the pointers will be pointing to
   * 
   * Pointers will have the specified product ID and will point to elements of
   * the `data` collection.
   * The pointer can actually dereference to the data.
   * Pointers may becoming dangling if the underlying content of `data` is
   * deleted or moved away (which is normally not possible in `art::Event`).
   * They may also point to non-existing elements after the end of the data
   * collection, condition which is not checked.
   * 
   * This interface is not compatible with `art::PtrMaker`.
   */
  PtrMaker(art::ProductID prodID, ProdColl_t const& data)
    : fProdID{ prodID }, fData{ data } {}
  
  // @{
  /**
   * @brief Creates a pointer to the specified element of the data product.
   * @param index the index of the element in the data product
   * @return a pointer
   * 
   * No check is performed on the index, which may point beyond the end of the
   * data product.
   */
  Ptr_t make(std::size_t index) const
    { return Ptr_t{ fProdID, &(fData[index]), index }; }
  Ptr_t operator() (std::size_t index) const { return make(index); }
  // @}
  
    private:
  
  art::ProductID const fProdID; ///< Product ID to record in the pointers.
  ProdColl_t const& fData; ///< Pointer to the original data product.
  
}; // testing::mockup::PtrMaker


// -----------------------------------------------------------------------------
// ---  template implementation
// -----------------------------------------------------------------------------
// ---  testing::mockup::Event
// -----------------------------------------------------------------------------
inline std::string const testing::mockup::Event::DefaultProcessName{ "mockup" };


// -----------------------------------------------------------------------------
int testing::mockup::Event::ProductKey::comp
  (art::InputTag const& a, art::InputTag const& b) noexcept
{
  if (int cmp = a.process().compare(b.process())) return cmp;
  if (int cmp = a.label().compare(b.label())) return cmp;
  return a.instance().compare(b.instance());
}


// -----------------------------------------------------------------------------
namespace testing::mockup {
  
  bool operator<
    (Event::ProductKey const& a, Event::ProductKey const& b) noexcept
  {
    if (int cmp = Event::ProductKey::comp(a.first, b.first)) return cmp < 0;
    return a.second < b.second;
  } // operator< (Event::ProductKey, Event::ProductKey)
  
} // namespace testing::mockup


// -----------------------------------------------------------------------------
template <typename T>
art::ProductID testing::mockup::Event::put(T&& data, art::InputTag tag) {
  
  tag = completeTag(tag);
  auto key = makeKey<T>(tag);
  if (fDataPointers.find(key) != fDataPointers.end()) {
    throw art::Exception{ art::errors::ProductRegistrationFailure }
      << "Data product '" << tag.encode() << "' already registered.\n";
  }
  
  art::ProductID ID{ ++fLastProductID };
  fhicl::ParameterSetID const PSetID{}; // no parameter set ID
  
  fProductIDs.emplace(
    ID,
    BranchRecord_t{
      art::BranchDescription{
          art::InEvent // branch type
        , art::TypeLabel{ art::TypeID{ typeid(T) }, tag.instance(), true }
        , tag.label()
        , PSetID
        , art::ProcessConfiguration{ tag.process(), PSetID, "" }
      }
    }
    );
  
  fDataPointers.emplace(
    std::move(key),
    DataProductRecord_t{
      std::move(tag),
      ID,
      std::move(data)
    }
    );
  
  return ID;
} // testing::mockup::Event::put()


// -----------------------------------------------------------------------------
template <typename T>
art::ProductID testing::mockup::Event::getProductID
  (art::InputTag const& tag) const
  { return getValidProductInfo<T>(tag).id; }


// -----------------------------------------------------------------------------
template <typename T>
T const& testing::mockup::Event::getProduct(art::InputTag const& tag) const {
  return *getDataPointer<T>(getValidProductInfo<T>(tag));
} // testing::mockup::Event::getProduct()


// -----------------------------------------------------------------------------
template <typename T>
auto testing::mockup::Event::getHandle(art::InputTag const& tag) const
  -> Handle<T>
{
  if (DataProductRecord_t const* dataRecord = getProductInfo<T>(tag))
    return { dataRecord->id, getDataPointer<T>(*dataRecord) };
  return {};
} // testing::mockup::Event::getHandle()


// -----------------------------------------------------------------------------
template <typename T>
auto testing::mockup::Event::getValidHandle(art::InputTag const& tag) const
  -> Handle<T>
{
  DataProductRecord_t const& dataRecord = getValidProductInfo<T>(tag);
  return { dataRecord.id, getDataPointer<T>(dataRecord) };
} // testing::mockup::Event::getValidHandle()


// -----------------------------------------------------------------------------
template <typename T>
auto testing::mockup::Event::getProductInfo(art::InputTag const& tag) const
  -> DataProductRecord_t const*
{
  auto const it = fDataPointers.find(makeKey<T>(completeTag(tag)));
  return (it == fDataPointers.end())? nullptr: &(it->second);
} // testing::mockup::Event::getProductInfo()


// -----------------------------------------------------------------------------
template <typename T>
auto testing::mockup::Event::getValidProductInfo(art::InputTag const& tag) const
  -> DataProductRecord_t const&
{
  DataProductRecord_t const* dataRecord = getProductInfo<T>(tag);
  if (dataRecord) return *dataRecord;
  throw art::Exception{ art::errors::ProductNotFound }
    << "Data product '" << tag.encode() << "' not registered or wrong type.\n";
} // testing::mockup::Event::getValidProductInfo()


// -----------------------------------------------------------------------------
template <typename T>
T const* testing::mockup::Event::getDataPointer
  (DataProductRecord_t const& dataRecord) const
{
  try { return &std::any_cast<T const&>(dataRecord.data); }
  catch (std::bad_any_cast const&) {
    throw art::Exception{ art::errors::LogicError }
      << "Data product '" << dataRecord.tag.encode()
      << "' not of requested type.\n";
  }
} // testing::mockup::Event::getDataPointer()


// -----------------------------------------------------------------------------
cet::exempt_ptr<art::BranchDescription const>
inline testing::mockup::Event::getProductDescription
  (art::ProductID ID) const
{
  auto const it = fProductIDs.find(ID);
  return (it == fProductIDs.end())? nullptr: &it->second.branchDescr;
}


// -----------------------------------------------------------------------------
template <typename T>
auto testing::mockup::Event::makeKey(art::InputTag tag) -> ProductKey
  { return { std::move(tag), std::type_index{ typeid(T) } }; }


// -----------------------------------------------------------------------------
inline art::InputTag testing::mockup::Event::completeTag
  (art::InputTag tag) const
{
  if (tag.process().empty())
    return art::InputTag{ tag.label(), tag.instance(), fProcessName };
  else return tag;
} // art::InputTag testing::mockup::Event::completeTag()


// -----------------------------------------------------------------------------
// ---  testing::mockup::Handle and related
// -----------------------------------------------------------------------------
template <typename T>
void testing::mockup::details::HandleBase<T>::checkValidity() const {
  if (fData) return;
  throw art::Exception(art::errors::NullPointerError)
    << "Attempt to de-reference product that points to 'nullptr'.\n";
}


// -----------------------------------------------------------------------------

#endif // ICARUSALG_TEST_FRAMEWORKEVENTMOCKUP_H
