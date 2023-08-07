/**
 * @file AssnsCrosser_test.cc
 * @brief Unit test for `icarus::ns::util::AssnsCrosser` class and utilities.
 * @author Gianluca Petrillo (petrillo@slac.stanford.edu)
 * @date June 9, 2023
 * @see icarusalg/Utilities/AssnsCrosser.h
 */


// Boost libraries
#define BOOST_TEST_MODULE AssnsCrosser
#include <cetlib/quiet_unit_test.hpp> // BOOST_AUTO_TEST_CASE()
#include <boost/test/test_tools.hpp> // BOOST_TEST()

// library to test
#include "icarusalg/Utilities/AssnsCrosser.h"

// ICARUS and LArSoft libraries
#include "test/FrameworkEventMockup.h"
#include "larcorealg/CoreUtils/enumerate.h"

// C/C++ standard libraries
#include <map>
#include <vector>
#include <any>
#include <stdexcept> // std::runtime_error
#include <typeindex>
#include <typeinfo>
#include <utility> // std::move()
#include <cstddef>


//------------------------------------------------------------------------------
// test data
template <std::size_t Tag>
struct DataType {
  
  static constexpr std::size_t tag = Tag;
  
  static constexpr std::size_t NoID = 0;
  
  std::size_t ID = NoID;
  
  DataType(std::size_t ID = NoID): ID(ID) {}
  
  operator std::string() const
    {
      return
        "DataType<" + std::to_string(tag) + ">[ID=" + std::to_string(ID) + "]";
    }
  
}; // DataType<>


struct DataTypeA: DataType<1> { using DataType<1>::DataType; };
struct DataTypeB: DataType<2> { using DataType<2>::DataType; };
struct DataTypeC: DataType<3> { using DataType<3>::DataType; };
struct DataTypeD: DataType<4> { using DataType<4>::DataType; };
struct DataTypeE: DataType<5> { using DataType<5>::DataType; };
struct DataTypeF: DataType<6> { using DataType<6>::DataType; };
struct DataTypeG: DataType<7> { using DataType<7>::DataType; };


//------------------------------------------------------------------------------
testing::mockup::Event makeTestEvent1() {
  std::vector<DataTypeA> dataA { // 16
    /*  0 */ DataTypeA{  0 },
    /*  1 */ DataTypeA{ 16 },
    /*  2 */ DataTypeA{ 32 },
    /*  3 */ DataTypeA{ 48 },
    /*  4 */ DataTypeA{ 64 }
    };
  std::vector<DataTypeA> dataA1 { // 16
    /*  0 */ DataTypeA{  0 },
    /*  1 */ DataTypeA{ 16 },
    };
  std::vector<DataTypeA> dataA2 { // 16
    /*  0 */ DataTypeA{ 32 },
    /*  1 */ DataTypeA{ 48 },
    /*  2 */ DataTypeA{ 64 }
    };
  
  std::vector<DataTypeB> dataB { //  8
    /*  0 */ DataTypeB{ 16 },
    /*  1 */ DataTypeB{ 24 },
    /*  2 */ DataTypeB{ 32 },
    /*  3 */ DataTypeB{ 48 },
    /*  4 */ DataTypeB{ 56 }
    };
  
  std::vector<DataTypeC> dataC { //  4
    /*  0 */ DataTypeC{ 16 },
    /*  1 */ DataTypeC{ 20 },
    /*  2 */ DataTypeC{ 24 },
    /*  3 */ DataTypeC{ 28 },
    /*  4 */ DataTypeC{ 32 },
    /*  5 */ DataTypeC{ 56 },
    /*  6 */ DataTypeC{ 60 },
    /*  7 */ DataTypeC{ 64 },
    /*  8 */ DataTypeC{ 72 }
    };
  
  std::vector<DataTypeD> dataD { //  2
    /*  0 */ DataTypeD{ 16 },
    /*  1 */ DataTypeD{ 18 },
    /*  2 */ DataTypeD{ 28 },
    /*  3 */ DataTypeD{ 36 },
    /*  4 */ DataTypeD{ 60 },
    /*  5 */ DataTypeD{ 64 },
    /*  6 */ DataTypeD{ 72 },
    /*  7 */ DataTypeD{ 76 },
    };
  
  testing::mockup::Event event;
  
  event.put(std::move(dataA),  art::InputTag{ "A" });
  event.put(std::move(dataA1), art::InputTag{ "A1" });
  event.put(std::move(dataA2), art::InputTag{ "A2" });
  event.put(std::move(dataB),  art::InputTag{ "B" });
  event.put(std::move(dataC),  art::InputTag{ "C" });
  event.put(std::move(dataD),  art::InputTag{ "D" });
  
  testing::mockup::PtrMaker<DataTypeA> makeAptr{ event, art::InputTag{ "A" } };
  testing::mockup::PtrMaker<DataTypeA> makeA1ptr
    { event, art::InputTag{ "A1" } };
  testing::mockup::PtrMaker<DataTypeA> makeA2ptr
    { event, art::InputTag{ "A2" } };
  testing::mockup::PtrMaker<DataTypeB> makeBptr{ event, art::InputTag{ "B" } };
  testing::mockup::PtrMaker<DataTypeC> makeCptr{ event, art::InputTag{ "C" } };
  testing::mockup::PtrMaker<DataTypeD> makeDptr{ event, art::InputTag{ "D" } };
  
  /*
   * The plan:
   *  A[0] <=> none
   *  A[1] <=> B[0], B[1]
   *  A[2] <=> B[2]
   *  A[3] <=> B[3]
   *  A[4] <=> none
   *  none <=> B[4]
   * 
   *  B[0] <=> C[0], C[1]
   *  B[1] <=> C[2], C[3]
   *  B[2] <=> C[4]
   *  B[3] <=> none
   *  B[4] <=> C[5], C[6]
   *  none <=> C[7]
   *  none <=> C[8]
   * 
   *  C[0] <=> D[0], D[1]
   *  C[1] <=> none
   *  C[2] <=> none
   *  C[3] <=> D[2]
   *  none <=> D[3]
   *  C[4] <=> none
   *  C[5] <=> none
   *  C[6] <=> D[4]
   *  C[7] <=> D[5]
   *  C[8] <=> D[6]
   *  none <=> D[7]
   * 
   *  A1[0] <=> none
   *  A1[1] <=> B[0], B[1]
   *  none  <=> B[2]
   *  none  <=> B[3]
   *  none  <=> B[4]
   * 
   *  none  <=> B[0], B[1]
   *  A2[0] <=> B[2]
   *  A2[1] <=> B[3]
   *  A2[2] <=> none
   *  none  <=> B[4]
   * 
   * 
   *  A[0] <=> none    <=> none    <=> none
   *  A[1] <=> B[0..1] <=> C[0..3] <=> D[0..2]
   *  A[2] <=> B[2]    <=> C[4]    <=> none
   *  A[3] <=> B[3]    <=> none    <=> none
   *  A[4] <=> none    <=> none    <=> none
   *  
   *  A1[0] <=> none    <=> none    <=> none
   *  A1[1] <=> B[0..1] <=> C[0..3] <=> D[0..2]
   *  A2[0] <=> B[2]    <=> C[4]    <=> none
   *  A2[1] <=> B[3]    <=> none    <=> none
   *  A2[2] <=> none    <=> none    <=> none
   *  
   */
  art::Assns<DataTypeA, DataTypeB> assnsAB;
  assnsAB.addSingle(makeAptr(1), makeBptr(0));
  assnsAB.addSingle(makeAptr(1), makeBptr(1));
  assnsAB.addSingle(makeAptr(2), makeBptr(2));
  assnsAB.addSingle(makeAptr(3), makeBptr(3));
  event.put(std::move(assnsAB), art::InputTag{ "B" });
  
  art::Assns<DataTypeA, DataTypeB> assnsA1B;
  assnsA1B.addSingle(makeA1ptr(1), makeBptr(0));
  assnsA1B.addSingle(makeA1ptr(1), makeBptr(1));
  event.put(std::move(assnsA1B), art::InputTag{ "B:1" });
  
  art::Assns<DataTypeA, DataTypeB> assnsA2B;
  assnsA2B.addSingle(makeA2ptr(0), makeBptr(2));
  assnsA2B.addSingle(makeA2ptr(1), makeBptr(3));
  event.put(std::move(assnsA2B), art::InputTag{ "B:2" });
  
  art::Assns<DataTypeB, DataTypeC> assnsBC;
  assnsBC.addSingle(makeBptr(0), makeCptr(0));
  assnsBC.addSingle(makeBptr(0), makeCptr(1));
  assnsBC.addSingle(makeBptr(1), makeCptr(2));
  assnsBC.addSingle(makeBptr(1), makeCptr(3));
  assnsBC.addSingle(makeBptr(2), makeCptr(4));
  assnsBC.addSingle(makeBptr(4), makeCptr(5));
  assnsBC.addSingle(makeBptr(4), makeCptr(6));
  event.put(std::move(assnsBC), art::InputTag{ "C" });
  
  art::Assns<DataTypeC, DataTypeD> assnsCD;
  assnsCD.addSingle(makeCptr(0), makeDptr(0));
  assnsCD.addSingle(makeCptr(0), makeDptr(1));
  assnsCD.addSingle(makeCptr(3), makeDptr(2));
  assnsCD.addSingle(makeCptr(6), makeDptr(4));
  assnsCD.addSingle(makeCptr(7), makeDptr(5));
  assnsCD.addSingle(makeCptr(8), makeDptr(6));
  event.put(std::move(assnsCD), art::InputTag{ "D" });
  
  return event;
} // makeTestEvent1()


// -----------------------------------------------------------------------------
void AssnsCrosser1_test() {
  /*
   * Test with a single association.
   *
   * The plan:
   *  A[0] <=> none
   *  A[1] <=> B[0], B[1]
   *  A[2] <=> B[2]
   *  A[3] <=> B[3]
   *  A[4] <=> none
   *  none <=> B[4]
   */
  
  testing::mockup::Event const event = makeTestEvent1();
  
  testing::mockup::PtrMaker<DataTypeA> makeAptr{ event, art::InputTag{ "A" } };
  testing::mockup::PtrMaker<DataTypeB> makeBptr{ event, art::InputTag{ "B" } };
  
  icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB> AtoB
    { event, art::InputTag{ "B" } };
  
  {
    auto const& Bs = AtoB.assPtrs(makeAptr(0));
    static_assert
      (std::is_same_v<decltype(Bs), std::vector<art::Ptr<DataTypeB>> const&>);
    
    BOOST_TEST(Bs.empty());
  }
  
  {
    auto const& Bs = AtoB.assPtrs(makeAptr(1));
    
    BOOST_TEST(Bs.size() == 2);
    if (Bs.size() > 0) BOOST_TEST(Bs[0] == makeBptr(0));
    if (Bs.size() > 1) BOOST_TEST(Bs[1] == makeBptr(1));
  }
  
  {
    auto const& Bs = AtoB.assPtrs(makeAptr(2));
    
    BOOST_TEST(Bs.size() == 1);
    if (Bs.size() > 0) BOOST_TEST(Bs[0] == makeBptr(2));
  }
  
  {
    auto const& Bs = AtoB.assPtrs(makeAptr(3));
    
    BOOST_TEST(Bs.size() == 1);
    if (Bs.size() > 0) BOOST_TEST(Bs[0] == makeBptr(3));
  }
  
  {
    auto const& Bs = AtoB.assPtrs(makeAptr(4));
    
    BOOST_TEST(Bs.empty());
  }
  
  {
    auto const& Bs = AtoB.assPtrs(makeAptr(5));
    
    BOOST_TEST(Bs.empty());
  }
  
  {
    auto const& Bs = AtoB.assPtrs(makeAptr(6));
    static_assert
      (std::is_same_v<decltype(Bs), std::vector<art::Ptr<DataTypeB>> const&>);
    BOOST_TEST(Bs.empty());
  }
  
} // AssnsCrosser1_test()


//------------------------------------------------------------------------------
void AssnsCrosser2check(
  testing::mockup::Event const& event,
  icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const& AtoC
) {
  /*
   * Test with a three-hop association.
   *
   * The plan:
   *  A[0] <=> none
   *  A[1] <=> B[0], B[1]
   *  A[2] <=> B[2]
   *  A[3] <=> B[3]
   *  A[4] <=> none
   *  none <=> B[4]
   * 
   *  B[0] <=> C[0], C[1]
   *  B[1] <=> C[2], C[3]
   *  B[2] <=> C[4]
   *  B[3] <=> none
   *  B[4] <=> C[5], C[6]
   *  none <=> C[7]
   *  none <=> C[8]
   * 
   *  A[0] <=> none    <=> none    <=> none
   *  A[1] <=> B[0..1] <=> C[0..3] <=> D[0..2]
   *  A[2] <=> B[2]    <=> C[4]    <=> none
   *  A[3] <=> B[3]    <=> none    <=> none
   *  A[4] <=> none    <=> none    <=> none
   *  
   */
  
  testing::mockup::PtrMaker<DataTypeA> makeAptr{ event, art::InputTag{ "A" } };
  testing::mockup::PtrMaker<DataTypeC> makeCptr{ event, art::InputTag{ "C" } };
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(0));
    static_assert
      (std::is_same_v<decltype(Cs), std::vector<art::Ptr<DataTypeC>> const&>);
    
    BOOST_TEST(Cs.empty());
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(1));
    
    BOOST_TEST(Cs.size() == 4);
    if (Cs.size() > 0) BOOST_TEST(Cs[0] == makeCptr(0));
    if (Cs.size() > 1) BOOST_TEST(Cs[1] == makeCptr(1));
    if (Cs.size() > 2) BOOST_TEST(Cs[2] == makeCptr(2));
    if (Cs.size() > 3) BOOST_TEST(Cs[3] == makeCptr(3));
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(2));
    
    BOOST_TEST(Cs.size() == 1);
    if (Cs.size() > 0) BOOST_TEST(Cs[0] == makeCptr(4));
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(3));
    
    BOOST_TEST(Cs.empty());
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(4));
    
    BOOST_TEST(Cs.empty());
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(5));
    static_assert
      (std::is_same_v<decltype(Cs), std::vector<art::Ptr<DataTypeC>> const&>);
    BOOST_TEST(Cs.empty());
  }
  
} // AssnsCrosser2check()


//------------------------------------------------------------------------------
void AssnsCrosser2_test() {
  /*
   * Test with a two-hop association.
   *
   * The plan:
   *  A[0] <=> none
   *  A[1] <=> B[0], B[1]
   *  A[2] <=> B[2]
   *  A[3] <=> B[3]
   *  A[4] <=> none
   *  none <=> B[4]
   * 
   *  B[0] <=> C[0], C[1]
   *  B[1] <=> C[2], C[3]
   *  B[2] <=> C[4]
   *  B[3] <=> none
   *  B[4] <=> C[5], C[6]
   *  none <=> C[7]
   *  none <=> C[8]
   * 
   *  A[0] <=> none
   *  A[1] <=> B[0..1] <=> C[0..3]
   *  A[2] <=> B[2]    <=> C[4]
   *  A[3] <=> B[3]    <=> none
   *  A[4] <=> none    <=> none
   *  
   */
  
  testing::mockup::Event const event = makeTestEvent1();
  
  icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> AtoC
    { event, art::InputTag{ "B" }, art::InputTag{ "C" } };
  
  BOOST_TEST_CONTEXT("Test: 2 hops with full InputTag specification") {
    AssnsCrosser2check(event, AtoC);
  }
  
} // AssnsCrosser2_test()


//------------------------------------------------------------------------------
void AssnsCrosserDiamond_test() {
  /*
   * Test with a diamond association.
   */
  
  std::vector<DataTypeA> dataA { DataTypeA{ 10 } };
  std::vector<DataTypeB> dataB { DataTypeB{ 20 }, DataTypeB{ 21 } };
  std::vector<DataTypeC> dataC { DataTypeC{ 30 } };
  
  testing::mockup::Event event;
  
  event.put(std::move(dataA), art::InputTag{ "A" });
  event.put(std::move(dataB), art::InputTag{ "B" });
  event.put(std::move(dataC), art::InputTag{ "C" });
  
  testing::mockup::PtrMaker<DataTypeA> makeAptr{ event, art::InputTag{ "A" } };
  testing::mockup::PtrMaker<DataTypeB> makeBptr{ event, art::InputTag{ "B" } };
  testing::mockup::PtrMaker<DataTypeC> makeCptr{ event, art::InputTag{ "C" } };
  
  /*
   * The plan:
   *  A[1] <=> B[0], B[1]
   * 
   *  B[0] <=> C[0],
   *  B[1] <=> C[0]
   * 
   *  A[0] <=> B[0..1] <=> C[0] (but via two paths)
   */
  art::Assns<DataTypeA, DataTypeB> assnsAB;
  assnsAB.addSingle(makeAptr(0), makeBptr(0));
  assnsAB.addSingle(makeAptr(0), makeBptr(1));
  event.put(std::move(assnsAB), art::InputTag{ "B" });
  
  art::Assns<DataTypeB, DataTypeC> assnsBC;
  assnsBC.addSingle(makeBptr(0), makeCptr(0));
  assnsBC.addSingle(makeBptr(1), makeCptr(0));
  event.put(std::move(assnsBC), art::InputTag{ "C" });
  
  using icarus::ns::util::hopTo;
  
  auto const AtoC = icarus::ns::util::makeAssnsCrosser<DataTypeA>
    (event, hopTo<DataTypeB>("B"), hopTo<DataTypeC>("C"));
  
  auto const& Cs = AtoC.assPtrs(makeAptr(0));
  
  BOOST_TEST(Cs.size() == 2);
  if (Cs.size() > 0) BOOST_TEST(Cs[0] == makeCptr(0));
  if (Cs.size() > 1) BOOST_TEST(Cs[1] == makeCptr(0));
  
} // AssnsCrosserDiamond_test()


//------------------------------------------------------------------------------
void AssnsCrosser3check(
  testing::mockup::Event const& event,
  icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC, DataTypeD> const& AtoD
) {
  /*
   * Test with a three-hop association.
   *
   * The plan:
   *  A[0] <=> none
   *  A[1] <=> B[0], B[1]
   *  A[2] <=> B[2]
   *  A[3] <=> B[3]
   *  A[4] <=> none
   *  none <=> B[4]
   * 
   *  B[0] <=> C[0], C[1]
   *  B[1] <=> C[2], C[3]
   *  B[2] <=> C[4]
   *  B[3] <=> none
   *  B[4] <=> C[5], C[6]
   *  none <=> C[7]
   *  none <=> C[8]
   * 
   *  C[0] <=> D[0], D[1]
   *  C[1] <=> none
   *  C[2] <=> none
   *  C[3] <=> D[2]
   *  none <=> D[3]
   *  C[4] <=> none
   *  C[5] <=> none
   *  C[6] <=> D[4]
   *  C[7] <=> D[5]
   *  C[8] <=> D[6]
   *  none <=> D[7]
   * 
   *  A[0] <=> none    <=> none    <=> none
   *  A[1] <=> B[0..1] <=> C[0..3] <=> D[0..2]
   *  A[2] <=> B[2]    <=> C[4]    <=> none
   *  A[3] <=> B[3]    <=> none    <=> none
   *  A[4] <=> none    <=> none    <=> none
   *  
   */
  
  testing::mockup::PtrMaker<DataTypeA> makeAptr{ event, art::InputTag{ "A" } };
  testing::mockup::PtrMaker<DataTypeD> makeDptr{ event, art::InputTag{ "D" } };
  
  {
    auto const& Ds = AtoD.assPtrs(makeAptr(0));
    static_assert
      (std::is_same_v<decltype(Ds), std::vector<art::Ptr<DataTypeD>> const&>);
    
    BOOST_TEST(Ds.empty());
  }
  
  {
    auto const& Ds = AtoD.assPtrs(makeAptr(1));
    
    BOOST_TEST(Ds.size() == 3);
    if (Ds.size() > 0) BOOST_TEST(Ds[0] == makeDptr(0));
    if (Ds.size() > 1) BOOST_TEST(Ds[1] == makeDptr(1));
    if (Ds.size() > 2) BOOST_TEST(Ds[2] == makeDptr(2));
  }
  
  {
    auto const& Ds = AtoD.assPtrs(makeAptr(2));
    BOOST_TEST(Ds.empty());
  }
  
  {
    auto const& Ds = AtoD.assPtrs(makeAptr(3));
    BOOST_TEST(Ds.empty());
  }
  
  {
    auto const& Ds = AtoD.assPtrs(makeAptr(4));
    BOOST_TEST(Ds.empty());
  }
  
  {
    auto const& Ds = AtoD.assPtrs(makeAptr(5));
    static_assert
      (std::is_same_v<decltype(Ds), std::vector<art::Ptr<DataTypeD>> const&>);
    BOOST_TEST(Ds.empty());
  }
  
} // AssnsCrosser3check()


//------------------------------------------------------------------------------
void AssnsCrosser3_test() {
  
  testing::mockup::Event const event = makeTestEvent1();
  
  icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC, DataTypeD>
  const AtoD
    { event, art::InputTag{ "B" }, art::InputTag{ "C" }, art::InputTag{ "D" } };
  
  AssnsCrosser3check(event, AtoD);
  
} // AssnsCrosser3_test()


//------------------------------------------------------------------------------
void AssnsCrosser3withID_test() {
  
  testing::mockup::Event const event = makeTestEvent1();
  
  // the tag of B <=> C associations is the same as the one of the C data;
  // we have lost track of the ID of the latter, but we can still ask the event
  art::ProductID const dataC_ID
    = event.getProductID<std::vector<DataTypeC>>("C");
  BOOST_TEST_REQUIRE(dataC_ID != art::ProductID{});
  
  icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC, DataTypeD>
  const AtoD
    { event, art::InputTag{ "B" }, dataC_ID, art::InputTag{ "D" } };
  
  BOOST_TEST_CONTEXT("Test: 3 hops with a product ID") {
    AssnsCrosser3check(event, AtoD);
  }
  
} // AssnsCrosser3withID_test()


//------------------------------------------------------------------------------
void AssnsCrosser3withJump_test() {
  /*
   * In this test, the first hop should be discovered.
   * 
   * The selected algorithm should be the backward one
   *   (because the forward one has no starting point)
   * and the "D" associations should point to "C" data,
   * the (implicitly converted) "C" input tag should point to "B" data, 
   * and "B" tag should also denote an association to "A".
   * The tag "B" should be discovered from the left pointers of the "C"
   * association.
   */
  
  testing::mockup::Event const event = makeTestEvent1();
  
  icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC, DataTypeD>
  const AtoD
    { event, {}, "C", art::InputTag{ "D" } };
  
  BOOST_TEST_CONTEXT("Test: 3 hops with autodetection of first hop") {
    AssnsCrosser3check(event, AtoD);
  }
  
} // AssnsCrosser3withJump_test()


//------------------------------------------------------------------------------
void AssnsCrosser3with2jumps_test() {
  /*
   * In this test, the first and second hops should be discovered.
   * 
   * The selected algorithm should be the backward one
   *   (because the forward one has no starting point)
   * and the "D" associations should point to "C" data,
   * and "C" tag should also denote an association to "B".
   * The tag "C" should be discovered from the left pointers of the "D"
   * association.
   * The same should afterward happen from "C" to "B".
   */
  
  testing::mockup::Event const event = makeTestEvent1();
  
  icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC, DataTypeD>
  const AtoD
    { event, {}, {}, "D" };
  
  BOOST_TEST_CONTEXT("Test: 3 hops with autodetection of first and second hop")
  {
    AssnsCrosser3check(event, AtoD);
  }
  
} // AssnsCrosser3with2jumps_test()


//------------------------------------------------------------------------------
void AssnsCrosserInputList1_test() {
  /*
   * Test with a two-hop association.
   *
   * The plan:
   *  A1[0] <=> none
   *  A1[1] <=> B[0], B[1]
   *  A2[0] <=> B[2]
   *  A2[1] <=> B[3]
   *  A2[2] <=> none
   *  none <=> B[4]
   * 
   *  B[0] <=> C[0], C[1]
   *  B[1] <=> C[2], C[3]
   *  B[2] <=> C[4]
   *  B[3] <=> none
   *  B[4] <=> C[5], C[6]
   *  none <=> C[7]
   *  none <=> C[8]
   * 
   *  A1[0] <=> none
   *  A1[1] <=> B[0..1] <=> C[0..3]
   *  A2[0] <=> B[2]    <=> C[4]
   *  A2[1] <=> B[3]    <=> none
   *  A2[2] <=> none    <=> none
   *  
   */
  testing::mockup::Event event = makeTestEvent1();
  
  testing::mockup::PtrMaker<DataTypeA> makeA1ptr
    { event, art::InputTag{ "A1" } };
  testing::mockup::PtrMaker<DataTypeA> makeA2ptr
    { event, art::InputTag{ "A2" } };
  testing::mockup::PtrMaker<DataTypeC> makeCptr{ event, art::InputTag{ "C" } };
  
  // note that the associations between A1/2 and B are called B:1 and B:2
  using icarus::ns::util::hopTo;
  auto const AtoC = icarus::ns::util::makeAssnsCrosser<DataTypeA>(
    event,
    hopTo<DataTypeB>{ "B:1", "B:2" }, hopTo<DataTypeC>{ "C" }
    );

  {
    auto const& Cs = AtoC.assPtrs(makeA1ptr(0));
    static_assert
      (std::is_same_v<decltype(Cs), std::vector<art::Ptr<DataTypeC>> const&>);
    
    BOOST_TEST(Cs.empty());
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeA1ptr(1));
    
    BOOST_TEST(Cs.size() == 4);
    if (Cs.size() > 0) BOOST_TEST(Cs[0] == makeCptr(0));
    if (Cs.size() > 1) BOOST_TEST(Cs[1] == makeCptr(1));
    if (Cs.size() > 2) BOOST_TEST(Cs[2] == makeCptr(2));
    if (Cs.size() > 3) BOOST_TEST(Cs[3] == makeCptr(3));
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeA1ptr(2));
    static_assert
      (std::is_same_v<decltype(Cs), std::vector<art::Ptr<DataTypeC>> const&>);
    BOOST_TEST(Cs.empty());
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeA2ptr(0));
    
    BOOST_TEST(Cs.size() == 1);
    if (Cs.size() > 0) BOOST_TEST(Cs[0] == makeCptr(4));
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeA2ptr(1));
    
    BOOST_TEST(Cs.empty());
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeA2ptr(2));
    
    BOOST_TEST(Cs.empty());
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeA2ptr(3));
    static_assert
      (std::is_same_v<decltype(Cs), std::vector<art::Ptr<DataTypeC>> const&>);
    BOOST_TEST(Cs.empty());
  }
  
} // AssnsCrosserInputList1_test()


//------------------------------------------------------------------------------
void AssnsCrosserStartList1_test() {
  
  using icarus::ns::util::startFrom;
  
  testing::mockup::Event const event = makeTestEvent1();
  
  /*
  // the tag of B <=> C associations is the same as the one of the C data;
  // we have lost track of the ID of the latter, but we can still ask the event
  art::ProductID const dataC_ID
    = event.getProductID<std::vector<DataTypeC>>("C");
  BOOST_TEST_REQUIRE(dataC_ID != art::ProductID{});
  */
  
  testing::mockup::PtrMaker<DataTypeA> makeAptr{ event, art::InputTag{ "A" } };
  testing::mockup::PtrMaker<DataTypeC> makeCptr{ event, art::InputTag{ "C" } };
  
  icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const AtoC
    { event, { makeAptr(2), makeAptr(3) }, "B", "C" };
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(0));
    static_assert
      (std::is_same_v<decltype(Cs), std::vector<art::Ptr<DataTypeC>> const&>);
    
    BOOST_TEST(Cs.empty());
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(1));
    
    BOOST_TEST(Cs.empty());
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(2));
    
    BOOST_TEST(Cs.size() == 1);
    if (Cs.size() > 0) BOOST_TEST(Cs[0] == makeCptr(4));
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(3));
    
    BOOST_TEST(Cs.empty());
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(4));
    
    BOOST_TEST(Cs.empty());
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(5));
    static_assert
      (std::is_same_v<decltype(Cs), std::vector<art::Ptr<DataTypeC>> const&>);
    BOOST_TEST(Cs.empty());
  }
  
} // AssnsCrosserStartList1_test()


//------------------------------------------------------------------------------
void AssnsCrosserStartList2_test() {
  
  using icarus::ns::util::startFrom;
  
  testing::mockup::Event const event = makeTestEvent1();
  
  art::ProductID const dataA_ID
    = event.getProductID<std::vector<DataTypeA>>("A");
  BOOST_TEST_REQUIRE(dataA_ID != art::ProductID{});
  
  icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const AtoC
    { event, dataA_ID, "B", "C" };
  
  BOOST_TEST_CONTEXT("Test: 2 hops with a product ID") {
    AssnsCrosser2check(event, AtoC);
  }
  
} // AssnsCrosserStartList2_test()


//------------------------------------------------------------------------------
void AssnsCrosserStartList3_test() {
  
  using icarus::ns::util::startFrom;
  
  testing::mockup::Event const event = makeTestEvent1();
  
  // startFrom{} is not required, but it increases readability
  icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const AtoC
    { event, startFrom<DataTypeA>{ "A" }, "B", "C" };
  
  BOOST_TEST_CONTEXT("Test: 2 hops with an input tag start") {
    AssnsCrosser2check(event, AtoC);
  }
  
} // AssnsCrosserStartList3_test()


//------------------------------------------------------------------------------
void AssnsCrosserStartList4_test() {
  
  using icarus::ns::util::startFrom;
  
  testing::mockup::Event const event = makeTestEvent1();
  
  testing::mockup::PtrMaker<DataTypeA> makeAptr{ event, art::InputTag{ "A" } };
  testing::mockup::PtrMaker<DataTypeC> makeCptr{ event, art::InputTag{ "C" } };
  
  std::vector const startA{ makeAptr(2), makeAptr(3) };
  icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const AtoC
    { event, startA, "B", "C" };
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(0));
    static_assert
      (std::is_same_v<decltype(Cs), std::vector<art::Ptr<DataTypeC>> const&>);
    
    BOOST_TEST(Cs.empty());
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(1));
    
    BOOST_TEST(Cs.empty());
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(2));
    
    BOOST_TEST(Cs.size() == 1);
    if (Cs.size() > 0) BOOST_TEST(Cs[0] == makeCptr(4));
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(3));
    
    BOOST_TEST(Cs.empty());
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(4));
    
    BOOST_TEST(Cs.empty());
  }
  
  {
    auto const& Cs = AtoC.assPtrs(makeAptr(5));
    static_assert
      (std::is_same_v<decltype(Cs), std::vector<art::Ptr<DataTypeC>> const&>);
    BOOST_TEST(Cs.empty());
  }
  
} // AssnsCrosserStartList4_test()


//------------------------------------------------------------------------------
void AssnsCrosserClassDocumentation_test() {
  
  /*
   * The promise:
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
   */
  
  using ExpectedAtoC_t
    = icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const;
  
  testing::mockup::Event const event = makeTestEvent1();
  
  {
    icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const AtoC
      [[maybe_unused]]
      { event, art::InputTag{ "B" }, art::InputTag{ "C" } };
    static_assert(std::is_same_v<decltype(AtoC), ExpectedAtoC_t>);
  }
  
  {
    using icarus::ns::util::hopTo;
    auto const AtoC = icarus::ns::util::makeAssnsCrosser<DataTypeA>
      (event, hopTo<DataTypeB>{ "B" }, hopTo<DataTypeC>{ "C" });
    static_assert(std::is_same_v<decltype(AtoC), ExpectedAtoC_t>);
  }
  
  {
    using icarus::ns::util::startFrom, icarus::ns::util::hopTo;
    icarus::ns::util::AssnsCrosser const AtoC{ event
      , startFrom<DataTypeA>{}
      , hopTo<DataTypeB>{ "B" }
      , hopTo<DataTypeC>{ "C" }
    };
    static_assert(std::is_same_v<decltype(AtoC), ExpectedAtoC_t>);
  }
  
  {
    using icarus::ns::util::startFrom, icarus::ns::util::hopTo;
    auto const AtoC = makeAssnsCrosser(event
      , startFrom<DataTypeA>{}
      , hopTo<DataTypeB>{ "B" }
      , hopTo<DataTypeC>{ "C" }
      );
    static_assert(std::is_same_v<decltype(AtoC), ExpectedAtoC_t>);
  }
  
  {
    icarus::ns::util::AssnsCrosser<DataTypeA, DataTypeB, DataTypeC> const AtoC
      { event, { "B:1", "B:2" }, { "C" } };
    static_assert(std::is_same_v<decltype(AtoC), ExpectedAtoC_t>);
  }
  
  {
    using icarus::ns::util::hopTo;
    auto const AtoC = icarus::ns::util::makeAssnsCrosser<DataTypeA>(
      event,
      hopTo<DataTypeB>{ "B:1", "B:2" }, hopTo<DataTypeC>{ "C" }
      );
    static_assert(std::is_same_v<decltype(AtoC), ExpectedAtoC_t>);
  }
  
} // AssnsCrosserClassDocumentation_test()


//------------------------------------------------------------------------------
void InputSpecsClassDocumentation_test() {
  
  using icarus::ns::util::InputSpecs, icarus::ns::util::InputSpec;
  
  using AtoZ_t = icarus::ns::util::AssnsCrosser<
    DataTypeA, DataTypeB, DataTypeC, DataTypeD, DataTypeE, DataTypeF
    >;
  
  // the purpose is to confirm that this code compiles
  using instantiated [[maybe_unused]] = decltype(
    AtoZ_t{ std::declval<testing::mockup::Event const>()
      
      // implicit conversion to `art::InputTag`:
      , InputSpecs<DataTypeB>{ "TagB" }
      
      // implicit conversion to `art::InputTag` then to `InputSpecs<DataTypeC>`:
      , "TagC"
      
      // explicit vector of input tags (not recommended):
      , InputSpecs<DataTypeD>{ std::vector<InputSpec>{ "TagD1", "TagD2" } }
      
      // list of input tags, converted to `InputSpecs<DataTypeE>`:
      , InputSpecs<DataTypeE>{ "TagE1", "TagE2" }
      
      // implicit list of input tags, converted to `InputSpecs<DataTypeF>`:
      , { "TagF1", "TagF2" }
    
    }
    );
  
} // InputSpecsClassDocumentation_test()


//------------------------------------------------------------------------------
//---  The tests
//---
BOOST_AUTO_TEST_CASE( AssnsCrosser1_testCase ) {
  
  AssnsCrosser1_test();
  
} // BOOST_AUTO_TEST_CASE( AssnsCrosser1_testCase )


BOOST_AUTO_TEST_CASE( AssnsCrosser2_testCase ) {
  
  AssnsCrosser2_test();
  AssnsCrosserDiamond_test();
  
} // BOOST_AUTO_TEST_CASE( AssnsCrosser2_testCase )


BOOST_AUTO_TEST_CASE( AssnsCrosser3_testCase ) {
  
  AssnsCrosser3_test();
  
} // BOOST_AUTO_TEST_CASE( AssnsCrosser3_testCase )


BOOST_AUTO_TEST_CASE( AssnsCrosserInput_testCase ) {
  
  // tests with different input specification styles
  AssnsCrosserInputList1_test();
  AssnsCrosser3withID_test();
  AssnsCrosser3withJump_test();
  AssnsCrosser3with2jumps_test();
  
} // BOOST_AUTO_TEST_CASE( AssnsCrosserInput_testCase )


BOOST_AUTO_TEST_CASE( AssnsCrosserStart_testCase ) {
  
  // tests with different start specification styles
  AssnsCrosserStartList1_test();
  AssnsCrosserStartList2_test();
  AssnsCrosserStartList3_test();
  AssnsCrosserStartList4_test();
  
} // BOOST_AUTO_TEST_CASE( AssnsCrosserStart_testCase )


BOOST_AUTO_TEST_CASE( AssnsCrosserDocumentation_testCase ) {
  
  AssnsCrosserClassDocumentation_test();
  InputSpecsClassDocumentation_test();
  
} // BOOST_AUTO_TEST_CASE( AssnsCrosserDocumentation_testCase )


//------------------------------------------------------------------------------

