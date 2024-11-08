#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "instructions.h"
#include "emulator.h"

#include <cstdio>
#include <type_traits>

// In this file we only check whether your code has a more or less correct
// public interface. If not, it will probably not compile at all.
//
// The main testing file will also fail to compile if your code has broken
// the public interface. The advantage of using this file is that we provide
// the smallest possible test cases that will break and they will break early.
// This makes it easier to identify what the problem is. Consider running the
// tests in here after every change in the public interface.

struct unused{};

TEST_CASE("Constants", "[structural]") {
  STATIC_REQUIRE(ARCH_BITS == 8);
  STATIC_REQUIRE(ARCH_BITMASK == 0b11111111);
  STATIC_REQUIRE(ARCH_MAXVAL == 255);
  STATIC_REQUIRE(INSTRUCTION_SIZE == 2);
  STATIC_REQUIRE(MEMORY_SIZE == 256);
  STATIC_REQUIRE(MAX_NAME == 96);
}

TEST_CASE("Typedefs Structural Tests", "[structural]") {
  // Are the two typedefs equivalent to some kind of integer?
  STATIC_REQUIRE(std::is_integral_v<addr_t>);
  STATIC_REQUIRE(std::is_integral_v<data_t>);
}

TEST_CASE("Enums Structural Tests", "[structural]") {
  // enum types need to be plain, old-school enums
  //
  // Modern C++ has enum classes, which are, generally speaking, safer than
  // plain enums. In this context, though, enums and enum classes are not
  // syntactically equivalent, so you cannot make this change without
  // changing the testing code too. Since you cannot do that, stick with plain
  // enums.
  //
  // Also, we haven't touched enum classes in this unit, so this is beyond the
  // scope of this assignment.
  //
  // If you got here because ChatGPT really insists on turning enums into enum
  // classes, try to understand the limitations of your tools before relying
  // blindly on them.

  // InstructionOpcode:
  // 1) is enum,
  // 2) its underlying type is some kind of integer (needed for storing the opcode in emulated memory),
  // 3) is not an enum class (i.e., it can be implicitly converted to its underlying type)
  STATIC_REQUIRE(std::is_enum_v<InstructionOpcode>);
  STATIC_REQUIRE(std::is_integral_v<std::underlying_type_t<InstructionOpcode>>);
  STATIC_REQUIRE(std::is_convertible_v<InstructionOpcode, std::underlying_type_t<InstructionOpcode>>);
}

TEST_CASE("ProcessorState Structural Tests", "[processor][structural]") {
  // Check that acc and pc are data_t and addr_t respectively
  STATIC_REQUIRE(std::is_same_v<decltype(ProcessorState::acc), data_t>);
  STATIC_REQUIRE(std::is_same_v<decltype(ProcessorState::pc), addr_t>);

  // Check that:
  // 1) memory is something that can be indexed with []
  // 2) Indexing it returns a reference
  using indexedType = decltype(ProcessorState::memory[100]);
  STATIC_REQUIRE(std::is_reference_v<indexedType>);

  // 3) That reference is a reference to some kind of integer number
  using elementType = std::remove_reference_t<indexedType>;
  STATIC_REQUIRE(std::is_integral_v<elementType>);

  // Check that ProcessorState has a default constructor (implicit or explicit)
  STATIC_REQUIRE(std::is_default_constructible_v<ProcessorState>);
  
  // Check that ProcessorState has a copy constructor (implicit or explicit)
  STATIC_REQUIRE(std::is_copy_constructible_v<ProcessorState>);

  // Check that ProcessorState has a copy assignment operator (implicit or explicit)
  STATIC_REQUIRE(std::is_copy_assignable_v<ProcessorState>);

  // Check that ProcessorState has a move constructor (implicit or explicit)
  STATIC_REQUIRE(std::is_move_constructible_v<ProcessorState>);

  // Check that ProcessorState has a move assignment operator (implicit or explicit)
  STATIC_REQUIRE(std::is_move_assignable_v<ProcessorState>);

  // Check that ProcessorState has a destructor (implicit or explicit)
  STATIC_REQUIRE(std::is_destructible_v<ProcessorState>);
}

TEST_CASE("InstructionData Structural Tests", "[structural]") {
  // InstructionData should be just a bucket of two numbers.
  // This is not a hard requirement, but it's a bad idea to make
  // this struct non trivial. Also limiting the solution space here
  // makes my life easier
  STATIC_REQUIRE(std::is_trivial_v<InstructionData>);
  STATIC_REQUIRE(std::is_standard_layout_v<InstructionData>);

  // Check that the members of InstructionData are some kind of integers
  STATIC_REQUIRE(std::is_integral_v<decltype(InstructionData::address)>);
  STATIC_REQUIRE(std::is_integral_v<decltype(InstructionData::opcode)>);
}


TEMPLATE_TEST_CASE("Instruction Structural Tests", 
                   "[instruction][structural]",
                   Iadd, Iand, Iorr, Ixor, Ildr, Istr, Ijmp, Ijne) {

  // Sanity check: Is the instruction still derived from InstructionBase?
  STATIC_REQUIRE(std::is_base_of_v<InstructionBase, TestType>);

  // We don't require a publically visible default constructor, or copy/move constructors/destructors?
  // But asan or clang tidy might expect some of those...
  
  // Does this class have a constructor taking a number?
  STATIC_REQUIRE(std::is_constructible_v<TestType, int>);

  // Does it have a get_address() which returns an integer kind of value
  STATIC_REQUIRE(std::is_invocable_r_v<int, decltype(&TestType::get_address), TestType>);

  // Does it have a name() which returns an string-like value (aka a value that can be converted to string view)?
  STATIC_REQUIRE(std::is_invocable_r_v<std::string_view, decltype(&TestType::name), TestType>);

  // Does it have an execute(ProcessorState&) which returns void?
  STATIC_REQUIRE(std::is_invocable_r_v<void, decltype(&TestType::execute), TestType, ProcessorState&>);

  // Same for _execute(ProcessorState&)
  STATIC_REQUIRE(std::is_invocable_r_v<void, decltype(&TestType::_execute), TestType, ProcessorState&>);
}

/**
 * Helper struct for implicitly converting pointer-like values to references
 *
 * @tparam U the type of the target reference
 */
template <typename U>
struct Dereferenced {
  U& ref;
  Dereferenced(const auto& obj) : ref(*obj) {};
  Dereferenced(auto& obj) : ref(*obj) {};
};

TEST_CASE("InstructionBase Structural Tests", "[instruction][structural]") {

  // Sanity check: Is the instruction still derived from InstructionBase?
  STATIC_REQUIRE(std::is_abstract_v<InstructionBase>);

  // We shouldn't have **publically** visible constructors
  STATIC_REQUIRE(not std::is_default_constructible_v<InstructionBase>);
  STATIC_REQUIRE(not std::is_copy_constructible_v<InstructionBase>);
  STATIC_REQUIRE(not std::is_move_constructible_v<InstructionBase>);
  STATIC_REQUIRE(not std::is_constructible_v<InstructionBase, int>);
  // Destructors might or might not be visible

  // Does it have a get_address() which returns an integer kind of value
  STATIC_REQUIRE(std::is_invocable_r_v<int, decltype(&InstructionBase::get_address), InstructionBase>);

  // Does it have a name() which returns an string-like value (aka a value that can be converted to string view)?
  STATIC_REQUIRE(std::is_invocable_r_v<std::string_view, decltype(&InstructionBase::name), InstructionBase>);

  // Does it have a to_string() which returns an string-like value (aka a value that can be converted to string view)?
  STATIC_REQUIRE(std::is_invocable_r_v<std::string_view, decltype(&InstructionBase::to_string), InstructionBase>);

  // Does it have an execute(ProcessorState&) which returns void?
  STATIC_REQUIRE(std::is_invocable_r_v<void, decltype(&InstructionBase::execute), InstructionBase, ProcessorState&>);

  // Same for _execute(ProcessorState&)
  STATIC_REQUIRE(std::is_invocable_r_v<void, decltype(&InstructionBase::_execute), InstructionBase, ProcessorState&>);

  // Does InstructionBase:
  // 1) have a static method generateInstruction(InstructionData)
  // 2) that returns an object that can be dereferenced to InstructionBase& through Dereferenced<InstructionBase>?
  // This tests forbids the InstructionData argument from being a reference.
  // While this could be an option, it's a bad option. Input-only data should be
  // either passed by value or const-reference. When the data is just two 32-bit
  // integers, passing by value and by const-reference has the same overhead,
  // with passing-by-value having clearer semantics.
  STATIC_REQUIRE(std::is_invocable_r_v<Dereferenced<InstructionBase>, decltype(&InstructionBase::generateInstruction), InstructionData>);
}

// We generally rely on std::is_invocable to check whether a function exists and
// is callable with a certain type of arguments but decltype (that we use to get
// the function type and pass it to std::is_invocable) does not like overloaded
// functions. In general, overloaded functions don't work well as template
// arguments. So, we use the pattern below to detect whether an explicitly
// named function exists:
// 1) We define a template, whose default case is that it represents a false value
// 2) We define a more specific template case that takes a single parameter -> our main template
// 3) That template will be selected by SFINAE only if the following statement is
//    syntactically valid:
//      std::declval<Breakpoint>().has(std::declval<T>())
//    std::declval pretends to create an object of the given type, so this
//    statement is somewhat like:
//      Breakpoint b; T var{}; b.has(var);
// 4) If that template is selected, its value will be the true_type and its
//    retType will be the actual return type of the overload of has()

template <typename T, typename = void>
struct is_has_available : std::false_type {
  using retType = unused;
};

template <typename T>
struct is_has_available<T,
  std::void_t<decltype(std::declval<Breakpoint>().has(std::declval<T>()))>> : std::true_type {
    using retType = decltype(std::declval<Breakpoint>().has(std::declval<T>()));
};

TEST_CASE("Breakpoint Structural Tests", "[breakpoint][structural]") {
  // The original implementation has a default constructor but we don't require it 

  // There is a constructor that can take an address (as int) and a name (as const char*)
  // The arguments don't need to be int and const char*, they only need to be constructible
  // from int and const char* respectively.
  STATIC_REQUIRE(std::is_constructible_v<Breakpoint, int, const char*>);
  
  // Breakpoint has a copy constructor and a copy assignment operator (implicit or explicit)
  STATIC_REQUIRE(std::is_copy_constructible_v<Breakpoint>);
  STATIC_REQUIRE(std::is_copy_assignable_v<Breakpoint>);

  // Breakpoint has a move constructor and a move assignment operator (implicit or explicit)
  STATIC_REQUIRE(std::is_move_constructible_v<Breakpoint>);
  STATIC_REQUIRE(std::is_move_assignable_v<Breakpoint>);

  // Breakpoint has a destructor (implicit or explicit)
  STATIC_REQUIRE(std::is_destructible_v<Breakpoint>);

  // Breakpoint has a get_address() that returns something convertible to int
  STATIC_REQUIRE(std::is_invocable_r_v<int, decltype(&Breakpoint::get_address), Breakpoint>);

  // Breakpoint has a get_name() that returns something string-like (aka convertible to std::string_view)
  STATIC_REQUIRE(std::is_invocable_r_v<std::string_view, decltype(&Breakpoint::get_name), Breakpoint>);

  // Breakpoint has a has() that takes an integer and returns something convertible to int
  {
    using available = is_has_available<addr_t>;
    STATIC_REQUIRE(available::value);
    STATIC_REQUIRE(std::is_convertible_v<available::retType, int>);
  }

  // Breakpoint has a has() that takes a string-like value and returns something convertible to int
  {
    using available = is_has_available<const char*>;
    STATIC_REQUIRE(available::value);
    STATIC_REQUIRE(std::is_convertible_v<available::retType, int>);
  }

  // We don't care about the internal implementation of Breakpoint
}

// Existence testing workarounds for find_breakpoint() and delete_breakpoint()
// Similar reasons and strategies as for Breakpoint::has() earlier

template <typename T, typename = void>
struct is_findbreakpoint_available : std::false_type {
  using retType = unused;
};

template <typename T>
struct is_findbreakpoint_available<T,
  std::void_t<decltype(std::declval<Emulator>().find_breakpoint(std::declval<T>()))>> : std::true_type {
    using retType = decltype(std::declval<Emulator>().find_breakpoint(std::declval<T>()));
};

template <typename T, typename = void>
struct is_delbreakpoint_available : std::false_type {
  using retType = unused;
};

template <typename T>
struct is_delbreakpoint_available<T,
  std::void_t<decltype(std::declval<Emulator>().delete_breakpoint(std::declval<T>()))>> : std::true_type {
    using retType = decltype(std::declval<Emulator>().delete_breakpoint(std::declval<T>()));
};


TEST_CASE("Emulator Structural Tests", "[emulator][structural]") {
  // There is some kind of implicit or explicit default constructor
  STATIC_REQUIRE(std::is_default_constructible_v<Emulator>);

  // It has Copy/Move Constructors (implicit or explicit)
  STATIC_REQUIRE(std::is_copy_constructible_v<Emulator>);
  STATIC_REQUIRE(std::is_move_constructible_v<Emulator>);

  // It has Copy/Move Assignment Operators (implicit or explicit)
  STATIC_REQUIRE(std::is_copy_assignable_v<Emulator>);
  STATIC_REQUIRE(std::is_move_assignable_v<Emulator>);

  // And it has a destructor (again implicit or explicit)
  STATIC_REQUIRE(std::is_destructible_v<Emulator>);

  // Emulator has a fetch() that returns an InstructionData struct
  STATIC_REQUIRE(std::is_invocable_r_v<InstructionData, decltype(&Emulator::fetch), Emulator>);

  // Emulator has a decode() that takes an InstructionData struct and returns an InstructionBase pointer-like value
  // We don't allow an InstructionData& argument because it's kind of pointless. pass-by-reference is only useful
  // for large or non-trivial structs/classes. InstructionData is two integers in a trenchcoat.
  STATIC_REQUIRE(std::is_invocable_r_v<Dereferenced<InstructionBase>, decltype(&Emulator::decode), Emulator, InstructionData>);

  // Emulator has an execute() that takes an InstructionBase pointer and returns an integer-like value
  // If you are not passing an InsturctionBase pointer to execute(), you might be making a change that
  // is not necessary (and not a good idea)
  STATIC_REQUIRE(std::is_invocable_r_v<int, decltype(&Emulator::execute), Emulator, InstructionBase*>);

  // Emulator has a run() that returns an integer-like value
  STATIC_REQUIRE(std::is_invocable_r_v<int, decltype(&Emulator::run), Emulator, int>);

  // Emulator has an insert_breakpoint() that takes an integer-like address
  // and a string-like name (constructible from const char* originally) and returns an integer-like value
  STATIC_REQUIRE(std::is_invocable_r_v<int, decltype(&Emulator::insert_breakpoint), Emulator, int, const char*>);

  // Emulator has a find_breakpoint() that takes an integer-like address and
  // returns a pointer-like value dereferenceable to Breakpoint
  {
    using available = is_findbreakpoint_available<addr_t>;
    STATIC_REQUIRE(available::value);
    STATIC_REQUIRE(std::is_convertible_v<available::retType, Dereferenced<Breakpoint>>);
    // The return type needs to be pointer-like, including being able to hold NULL
    STATIC_REQUIRE(std::is_convertible_v<std::nullptr_t, available::retType>);
  }

  // Emulator has a find_breakpoint() that takes a string-like argument and
  // returns a pointer-like value dereferenceable to Breakpoint
  {
    using available = is_findbreakpoint_available<const char*>;
    STATIC_REQUIRE(available::value);
    STATIC_REQUIRE(std::is_convertible_v<available::retType, Dereferenced<Breakpoint>>);
    // The return type needs to be pointer-like, including being able to hold NULL
    STATIC_REQUIRE(std::is_convertible_v<std::nullptr_t, available::retType>);
  }

  // Emulator has a delete_breakpoint() that takes an integer-like address and returns integer-like value
  {
    using available = is_delbreakpoint_available<addr_t>;
    STATIC_REQUIRE(available::value);
    STATIC_REQUIRE(std::is_convertible_v<available::retType, int>);
  }

  // Emulator has a delete_breakpoint() that takes an string-like argument and returns a value dereferenceable to Breakpoint
  {
    using available = is_delbreakpoint_available<const char*>;
    STATIC_REQUIRE(available::value);
    STATIC_REQUIRE(std::is_convertible_v<available::retType, int>);
  }

  // Emulator has a read_acc() that returns an integer-like value
  STATIC_REQUIRE(std::is_invocable_r_v<int, decltype(&Emulator::read_acc), Emulator>);

  // Emulator has a read_pc() that returns an integer-like value
  STATIC_REQUIRE(std::is_invocable_r_v<int, decltype(&Emulator::read_pc), Emulator>);

  // Emulator has a read_mem(addr_t) that returns an integer-like value
  STATIC_REQUIRE(std::is_invocable_r_v<int, decltype(&Emulator::read_mem), Emulator, addr_t>);

  // Emulator has a is_zero() that returns an integer-like value
  STATIC_REQUIRE(std::is_invocable_r_v<int, decltype(&Emulator::is_zero), Emulator>);

  // Emulator has a is_breakpoint() that returns an integer-like value
  STATIC_REQUIRE(std::is_invocable_r_v<int, decltype(&Emulator::is_breakpoint), Emulator>);

  // Emulator has a print_program() that returns an integer-like value
  STATIC_REQUIRE(std::is_invocable_r_v<int, decltype(&Emulator::print_program), Emulator>);

  // Emulator has a load_state() that takes a string-like value and returns an integer-like value
  STATIC_REQUIRE(std::is_invocable_r_v<int, decltype(&Emulator::load_state), Emulator, const char*>);

  // Emulator has a save_state() that takes a string-like value and returns an integer-like value
  STATIC_REQUIRE(std::is_invocable_r_v<int, decltype(&Emulator::save_state), Emulator, const char*>);

  // We don't care about the internal implementation of Emulator
}
