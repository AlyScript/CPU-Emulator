#define CATCH_CONFIG_MAIN

#include "catch.hpp"
#include "instructions.h"
#include "emulator.h"

#include <cstdio>
#include <fcntl.h>
#include <inttypes.h>
#include <string_view>

// Argh, windows libc is a bit non-standard
#ifdef _WIN32
#define STDOUT_FILENO 1
#include <sys/stat.h>
#include <io.h>
#define dup _dup
#define dup2 _dup2
#define open _open
#define close _close
#define lseek _lseek
#define S_IRUSR _S_IREAD
#define S_IWUSR _S_IWRITE
#else
#include <unistd.h>
#endif

const void* get_location(std::string_view v) {
  return static_cast<const void*>(v.data());
}

bool same_memory(std::string_view s1, std::string_view s2) {
  return s1.data() == s2.data();
}

// -----------------------------------------------------------------------------
// -------------------------   INSTRUCTION OPCODES     -------------------------
// -----------------------------------------------------------------------------
TEST_CASE("InstructionOpcode values", "[instruction]") {
  InstructionOpcode op;

  op = ADD;
  REQUIRE(op == 0);
  op = AND;
  REQUIRE(op == 1);
  op = ORR;
  REQUIRE(op == 2);
  op = XOR;
  REQUIRE(op == 3);
  op = LDR;
  REQUIRE(op == 4);
  op = STR;
  REQUIRE(op == 5);
  op = JMP;
  REQUIRE(op == 6);
  op = JNE;
  REQUIRE(op == 7);
}


// -----------------------------------------------------------------------------
// -------------------------     PROCESSOR STATE       -------------------------
// -----------------------------------------------------------------------------
TEST_CASE("ProcessorState Initialisation", "[processor][init]") {
  ProcessorState state;
  CHECK(state.pc == 0);
  CHECK(state.acc == 0);
  for (int i = 0; i < 256; ++i)
    CHECK(state.memory[i] == 0);
}

TEST_CASE("ProcessorState Copy Operators", "[processor][copy]") {
  ProcessorState state1;
  state1.pc = 127;
  state1.acc = 255;
  for (int i = 0; i < 256; ++i)
    state1.memory[i] = rand() & 255;

  ProcessorState state3;
  state3.pc = 255;
  state1.acc = 127;
  for (int i = 0; i < 256; ++i)
    state1.memory[i] = 255 - (rand() & 255);

  // Trigger the copy constructor and check that state1 and state2 contain the same data
  ProcessorState state2{state1};
  CHECK(state2.pc == state1.pc);
  CHECK(state2.acc == state1.acc);
  for (int i = 0; i < 256; ++i)
    CHECK(state2.memory[i] == state1.memory[i]);

  // Make sure that state2 does not share data with state1
  state1.memory[0]++;
  CHECK(state2.memory[0] != state1.memory[0]);

  // Trigger the copy assignment operator and check that state3 and state2 contain the same data
  state2 = state3;
  CHECK(state2.pc == state3.pc);
  CHECK(state2.acc == state3.acc);
  for (int i = 0; i < 256; ++i)
    CHECK(state2.memory[i] == state3.memory[i]);

  // Make sure that state2 does not share data with state3
  state3.memory[0]++;
  CHECK(state2.memory[0] != state3.memory[0]);
}


TEST_CASE("ProcessorState Move Operators", "[processor][move]") {
  ProcessorState state1;
  state1.pc = 127;
  state1.acc = 255;
  for (int i = 0; i < 256; ++i)
    state1.memory[i] = rand() & 255;

  ProcessorState state3;
  state3.pc = 255;
  state1.acc = 127;
  for (int i = 0; i < 256; ++i)
    state1.memory[i] = 255 - (rand() & 255);

  // This will only work correctly if the copy constructors (above) work correctly too
  ProcessorState state1_cpy{state1};
  ProcessorState state3_cpy{state3};

  // Trigger the copy constructor and check that state1 and state2 contain the same data
  ProcessorState state2{std::move(state1_cpy)};
  CHECK(state2.pc == state1.pc);
  CHECK(state2.acc == state1.acc);
  for (int i = 0; i < 256; ++i)
    CHECK(state2.memory[i] == state1.memory[i]);

  // ProcessorState should not really have moveable state,
  // so state2 and state1_cpy are not expected to share data
  // So we don't check this here

  // Trigger the copy assignment operator and check that state3 and state2 contain the same data
  state2 = std::move(state3_cpy);
  CHECK(state2.pc == state3.pc);
  CHECK(state2.acc == state3.acc);
  for (int i = 0; i < 256; ++i)
    CHECK(state2.memory[i] == state3.memory[i]);
}

// Nothing to check about InstructionData

// -----------------------------------------------------------------------------
// -------------------------        INSTRUCTIONS       -------------------------
// -----------------------------------------------------------------------------
const char* classnames[8]{"ADD", "AND", "ORR", "XOR", "LDR", "STR", "JMP", "JNE"};
const InstructionOpcode opcodes[8]{ADD, AND, ORR, XOR, LDR, STR, JMP, JNE};

// This essentially creates 8 separate tests, one for each instruction type
// T -> template parameter holding the current instruction class
// V -> index of the class in the list of classes (used to access classnames)
TEMPLATE_TEST_CASE_SIG("Instruction Initialisation", 
                   "[instruction][init][getters]",
                   ((typename T, int V), T, V),
                   (Iadd, 0), (Iand, 1),
                   (Iorr, 2), (Ixor, 3),
                   (Ildr, 4), (Istr, 5),
                   (Ijmp, 6), (Ijne, 7)) {

  const char* name = classnames[V];
  const addr_t addresses[5]{0, 65, 254, 257, 2048};
  char buffer[MAX_NAME];

  for (int i = 0; i < 5; ++i) {
    addr_t address = addresses[i] + V;
    T obj(address);

    // Keep only the lower 8 bits
    address = address & 255;

    // Is the address initialised correctly (8-bit value)
    REQUIRE(obj.get_address() == address);

    // Does the name() method return a string-like object
    // that matches the expected value
    REQUIRE_THAT(obj.name(), Catch::Matchers::Equals(name));

    // This is basically the original code from instruction.cpp
    // and the test will be whether your code is producing the same string
    if (strncmp(name, "ADD", 3) == 0)
      sprintf(buffer, "%s: ACC <- ACC + [%d]", name, address);
    else if (strncmp(name, "AND", 3) == 0)
      sprintf(buffer, "%s: ACC <- ACC & [%d]", name, address);
    else if (strncmp(name, "ORR", 3) == 0)
      sprintf(buffer, "%s: ACC <- ACC | [%d]", name, address);
    else if (strncmp(name, "XOR", 3) == 0)
      sprintf(buffer, "%s: ACC <- ACC ^ [%d]", name, address);
    else if (strncmp(name, "LDR", 3) == 0)
      sprintf(buffer, "%s: ACC <- [%d]", name, address);
    else if (strncmp(name, "STR", 3) == 0)
      sprintf(buffer, "%s: ACC -> [%d]", name, address);
    else if (strncmp(name, "JMP", 3) == 0)
      sprintf(buffer, "%s: PC  <- %d", name, address);
    else if (strncmp(name, "JNE", 3) == 0)
      sprintf(buffer, "%s: PC  <- %d if ACC != 0", name, address);
    else
      FAIL("This should never happen, unless there is an error in name()");

    REQUIRE_THAT(obj.to_string(), Catch::Matchers::Equals(buffer));
  }
}

template <typename T>
concept InstructionPointer = requires (T obj, InstructionBase* ins) {
  ins = &(*obj);
};

// Expands to 8 tests, one for each instruction
TEMPLATE_TEST_CASE_SIG("Instruction: Factory", 
                   "[instruction][init]",
                   ((typename T, int V), T, V),
                   (Iadd, 0), (Iand, 1),
                   (Iorr, 2), (Ixor, 3),
                   (Ildr, 4), (Istr, 5),
                   (Ijmp, 6), (Ijne, 7)) {

  const addr_t addresses[5]{0, 64, 255, 256, 2050};
  const char* name = classnames[V];
  char buffer[MAX_NAME];

  InstructionData data;

  for (int i = 0; i < 5; ++i) {
    data.opcode = opcodes[V];
    data.address = addresses[i];

    // Parse the instruction data and create the instruction
    InstructionPointer auto obj = InstructionBase::generateInstruction(data);
    REQUIRE(obj->get_address() == ((data.address) & 255));

    // Does the name() method return a string-like object
    // that matches the expected value
    REQUIRE_THAT(obj->name(), Catch::Matchers::Equals(name));

    // This is basically the original code from instruction.cpp
    // and the test will be whether your code is producing the same string
    addr_t address = data.address & 255;
    if (strncmp(name, "ADD", 3) == 0)
      sprintf(buffer, "%s: ACC <- ACC + [%d]", name, address);
    else if (strncmp(name, "AND", 3) == 0)
      sprintf(buffer, "%s: ACC <- ACC & [%d]", name, address);
    else if (strncmp(name, "ORR", 3) == 0)
      sprintf(buffer, "%s: ACC <- ACC | [%d]", name, address);
    else if (strncmp(name, "XOR", 3) == 0)
      sprintf(buffer, "%s: ACC <- ACC ^ [%d]", name, address);
    else if (strncmp(name, "LDR", 3) == 0)
      sprintf(buffer, "%s: ACC <- [%d]", name, address);
    else if (strncmp(name, "STR", 3) == 0)
      sprintf(buffer, "%s: ACC -> [%d]", name, address);
    else if (strncmp(name, "JMP", 3) == 0)
      sprintf(buffer, "%s: PC  <- %d", name, address);
    else if (strncmp(name, "JNE", 3) == 0)
      sprintf(buffer, "%s: PC  <- %d if ACC != 0", name, address);
    else
      FAIL("This should never happen, unless there is an error in name()");

    REQUIRE_THAT(obj->to_string(), Catch::Matchers::Equals(buffer));

    // Is this executing the same thing as if we called execute() on the derived
    // type? This should never fail, unless you have done something weird with
    // the implementation of InstructionBase::execute()
    ProcessorState state1;
    state1.acc = 85;
    for (int i = 0; i < 256; ++i)
      state1.memory[i] = i;
    
    // Assumming the ProcessorState copy constructor works correctly
    ProcessorState state2{state1};

    // An instruction object with the same data as obj, but used without going through the base type
    T obj2(data.address);

    obj->execute(state1);
    obj2.execute(state2);
    REQUIRE(state1.acc == state2.acc);
    REQUIRE(state1.pc == state2.pc);
    for (int i = 0; i < 256; ++i)
      REQUIRE(state1.memory[i] == state2.memory[i]);

    // No delete for obj! This has to be a memory leak. Or not?
  }
}

TEST_CASE("Instruction: Factory, with invalid data", "[instruction][init]") {
  InstructionData data;
  data.address = 255;
  for (int idx = NUM_OPCODES; idx < 256; ++idx) {
    data.opcode = idx;
    CHECK(InstructionBase::generateInstruction(data) == NULL);
  }
} 

// Check the actual functionality of the instructions
TEST_CASE("Iadd Execution", "[instruction][exec]") {
  ProcessorState state;
  state.acc = 5;
  state.pc = 2;

  // Initialise memory
  for (int i = 0; i < 256; ++i)
    state.memory[i] = 0b01010101; // 85

  state.memory[64] = 200;
  state.memory[65] = 255;
  state.memory[66] = 0;

  byte_t memory_copy[256];
  for (int i = 0; i < 256; ++i)
    memory_copy[i] = state.memory[i];

  SECTION("Basic use") {
    Iadd ins(64);
    ins.execute(state);
    REQUIRE(state.acc == 205);
    REQUIRE(state.pc == 4);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  SECTION("ACC overflow") {
    Iadd ins(65);
    ins.execute(state);
    REQUIRE(state.acc == 4);
    REQUIRE(state.pc == 4);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  state.pc = 254;
  SECTION("PC overflow") {
    Iadd ins(66);
    ins.execute(state);
    REQUIRE(state.acc == 5);
    REQUIRE(state.pc == 0);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }
}

TEST_CASE("Iand Execution", "[instruction][exec]") {
  ProcessorState state;
  state.acc = 0b01000101;
  state.pc = 2;

  // Initialise memory
  for (int i = 0; i < 256; ++i)
    state.memory[i] = 0b01010101; // 85

  state.memory[64] = 0b00010001;
  state.memory[65] = 0b00000000;
  state.memory[66] = 0b11111111;

  byte_t memory_copy[256];
  for (int i = 0; i < 256; ++i)
    memory_copy[i] = state.memory[i];

  SECTION("Basic use") {
    Iand ins(64);
    ins.execute(state);
    REQUIRE(state.acc == 1);
    REQUIRE(state.pc == 4);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  SECTION("ACC overflow") {
    Iand ins(65);
    ins.execute(state);
    REQUIRE(state.acc == 0);
    REQUIRE(state.pc == 4);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  state.pc = 254;
  SECTION("PC overflow") {
    Iand ins(66);
    ins.execute(state);
    REQUIRE(state.acc == 0b01000101);
    REQUIRE(state.pc == 0);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }
}

TEST_CASE("Iorr Execution", "[instruction][exec]") {
  ProcessorState state;
  state.acc = 0b01000101;
  state.pc = 2;

  // Initialise memory
  for (int i = 0; i < 256; ++i)
    state.memory[i] = 0b01010101; // 85

  state.memory[64] = 0b00010001;
  state.memory[65] = 0b00000000;
  state.memory[66] = 0b11111111;

  byte_t memory_copy[256];
  for (int i = 0; i < 256; ++i)
    memory_copy[i] = state.memory[i];

  SECTION("Basic use") {
    Iorr ins(64);
    ins.execute(state);
    REQUIRE(state.acc == 0b01010101);
    REQUIRE(state.pc == 4);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  SECTION("ACC overflow") {
    Iorr ins(65);
    ins.execute(state);
    REQUIRE(state.acc == 0b01000101);
    REQUIRE(state.pc == 4);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  state.pc = 254;
  SECTION("PC overflow") {
    Iorr ins(66);
    ins.execute(state);
    REQUIRE(state.acc == 0b11111111);
    REQUIRE(state.pc == 0);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }
}

TEST_CASE("Ixor Execution", "[instruction][exec]") {
  ProcessorState state;
  state.acc = 0b01000101;
  state.pc = 2;

  // Initialise memory
  for (int i = 0; i < 256; ++i)
    state.memory[i] = 0b01010101; // 85

  state.memory[64] = 0b00010001;
  state.memory[65] = 0b00000000;
  state.memory[66] = 0b11111111;

  byte_t memory_copy[256];
  for (int i = 0; i < 256; ++i)
    memory_copy[i] = state.memory[i];

  SECTION("Basic use") {
    Ixor ins(64);
    ins.execute(state);
    REQUIRE(state.acc == 0b01010100);
    REQUIRE(state.pc == 4);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  SECTION("ACC overflow") {
    Ixor ins(65);
    ins.execute(state);
    REQUIRE(state.acc == 0b01000101);
    REQUIRE(state.pc == 4);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  state.pc = 254;
  SECTION("PC overflow") {
    Ixor ins(66);
    ins.execute(state);
    REQUIRE(state.acc == 0b10111010);
    REQUIRE(state.pc == 0);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }
}

TEST_CASE("Ildr Execution", "[instruction][exec]") {
  ProcessorState state;
  state.acc = 5;
  state.pc = 2;

  // Initialise memory
  for (int i = 0; i < 256; ++i)
    state.memory[i] = 0b01010101; // 85

  state.memory[64] = 15;
  state.memory[65] = 255;
  state.memory[66] = 0;

  byte_t memory_copy[256];
  for (int i = 0; i < 256; ++i)
    memory_copy[i] = state.memory[i];

  SECTION("Basic use") {
    Ildr ins(64);
    ins.execute(state);
    REQUIRE(state.acc == 15);
    REQUIRE(state.pc == 4);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  SECTION("Basic use 2") {
    Ildr ins(65);
    ins.execute(state);
    REQUIRE(state.acc == 255);
    REQUIRE(state.pc == 4);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  state.pc = 254;
  SECTION("PC overflow") {
    Ildr ins(66);
    ins.execute(state);
    REQUIRE(state.acc == 0);
    REQUIRE(state.pc == 0);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }
}

TEST_CASE("Istr Execution", "[instruction][exec]") {
  ProcessorState state;
  state.acc = 170;
  state.pc = 2;

  // Initialise memory
  for (int i = 0; i < 256; ++i)
    state.memory[i] = 0b01010101; // 85

  state.memory[64] = 15;
  state.memory[65] = 255;
  state.memory[66] = 0;

  byte_t memory_copy[256];
  for (int i = 0; i < 256; ++i)
    memory_copy[i] = state.memory[i];

  SECTION("Basic use") {
    Istr ins(64);
    ins.execute(state);
    REQUIRE(state.acc == 170);
    REQUIRE(state.pc == 4);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) {
      if (i == 64) REQUIRE(state.memory[i] == 170);
      else REQUIRE(state.memory[i] == memory_copy[i]);
    }
  }

  SECTION("Basic use 2") {
    Istr ins(65);
    ins.execute(state);
    REQUIRE(state.acc == 170);
    REQUIRE(state.pc == 4);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) {
      if (i == 65) REQUIRE(state.memory[i] == 170);
      else REQUIRE(state.memory[i] == memory_copy[i]);
    }
  }

  state.pc = 254;
  SECTION("PC overflow") {
    Istr ins(66);
    ins.execute(state);
    REQUIRE(state.acc == 170);
    REQUIRE(state.pc == 0);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) {
      if (i == 66) REQUIRE(state.memory[i] == 170);
      else REQUIRE(state.memory[i] == memory_copy[i]);
    }
  }
}

TEST_CASE("Ijmp Execution", "[instruction][exec]") {
  ProcessorState state;
  state.acc = 170;
  state.pc = 2;

  // Initialise memory
  for (int i = 0; i < 256; ++i)
    state.memory[i] = 0b01010101; // 85

  SECTION("Basic use") {
    Ijmp ins(64);
    ins.execute(state);
    REQUIRE(state.acc == 170);
    REQUIRE(state.pc == 64);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i)
      REQUIRE(state.memory[i] == 0b01010101);
  }

  SECTION("Basic use 2") {
    Ijmp ins(256);
    ins.execute(state);
    REQUIRE(state.acc == 170);
    REQUIRE(state.pc == 0);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i)
      REQUIRE(state.memory[i] == 0b01010101);
  }
}

TEST_CASE("Ijne Execution", "[instruction][exec]") {
  ProcessorState state;
  state.acc = 170;
  state.pc = 2;

  // Initialise memory
  for (int i = 0; i < 256; ++i)
    state.memory[i] = 0b01010101; // 85

  SECTION("Taken") {
    Ijne ins(64);
    ins.execute(state);
    REQUIRE(state.acc == 170);
    REQUIRE(state.pc == 64);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i)
      REQUIRE(state.memory[i] == 0b01010101);
  }

  state.acc = 1;
  state.pc = 0;
  SECTION("Taken 2") {
    Ijne ins(0);
    ins.execute(state);
    REQUIRE(state.acc == 1);
    REQUIRE(state.pc == 0);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i)
      REQUIRE(state.memory[i] == 0b01010101);
  }

  state.acc = 1;
  state.pc = 250;
  SECTION("Taken 3") {
    Ijne ins(0);
    ins.execute(state);
    REQUIRE(state.acc == 1);
    REQUIRE(state.pc == 0);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i)
      REQUIRE(state.memory[i] == 0b01010101);
  }

  state.acc = 0;
  state.pc = 2;
  SECTION("Not taken") {
    Ijne ins(64);
    ins.execute(state);
    REQUIRE(state.acc == 0);
    REQUIRE(state.pc == 4);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i)
      REQUIRE(state.memory[i] == 0b01010101);
  }

  state.acc = 0;
  state.pc = 254;
  SECTION("Not taken 2") {
    Ijne ins(0);
    ins.execute(state);
    REQUIRE(state.acc == 0);
    REQUIRE(state.pc == 0);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i)
      REQUIRE(state.memory[i] == 0b01010101);
  }

}

// Check the actual functionality of the _execute() functions
TEST_CASE("Iadd Execution 2", "[instruction][exec]") {
  ProcessorState state;
  state.acc = 5;
  state.pc = 2;

  // Initialise memory
  for (int i = 0; i < 256; ++i)
    state.memory[i] = 0b01010101; // 85

  state.memory[64] = 200;
  state.memory[65] = 255;
  state.memory[66] = 0;

  byte_t memory_copy[256];
  for (int i = 0; i < 256; ++i)
    memory_copy[i] = state.memory[i];

  SECTION("Basic use") {
    Iadd ins(64);
    ins._execute(state);
    REQUIRE(state.acc == 205);
    REQUIRE(state.pc == 2);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  SECTION("ACC overflow") {
    Iadd ins(65);
    ins._execute(state);
    REQUIRE(state.acc == 260);
    REQUIRE(state.pc == 2);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  state.pc = 254;
  SECTION("PC overflow") {
    Iadd ins(66);
    ins._execute(state);
    REQUIRE(state.acc == 5);
    REQUIRE(state.pc == 254);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }
}

TEST_CASE("Iand Execution 2", "[instruction][exec]") {
  ProcessorState state;
  state.acc = 0b01000101;
  state.pc = 2;

  // Initialise memory
  for (int i = 0; i < 256; ++i)
    state.memory[i] = 0b01010101; // 85

  state.memory[64] = 0b00010001;
  state.memory[65] = 0b00000000;
  state.memory[66] = 0b11111111;

  byte_t memory_copy[256];
  for (int i = 0; i < 256; ++i)
    memory_copy[i] = state.memory[i];

  SECTION("Basic use") {
    Iand ins(64);
    ins._execute(state);
    REQUIRE(state.acc == 1);
    REQUIRE(state.pc == 2);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  SECTION("ACC overflow") {
    Iand ins(65);
    ins._execute(state);
    REQUIRE(state.acc == 0);
    REQUIRE(state.pc == 2);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  state.pc = 254;
  SECTION("PC overflow") {
    Iand ins(66);
    ins._execute(state);
    REQUIRE(state.acc == 0b01000101);
    REQUIRE(state.pc == 254);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }
}

TEST_CASE("Iorr Execution 2", "[instruction][exec]") {
  ProcessorState state;
  state.acc = 0b01000101;
  state.pc = 2;

  // Initialise memory
  for (int i = 0; i < 256; ++i)
    state.memory[i] = 0b01010101; // 85

  state.memory[64] = 0b00010001;
  state.memory[65] = 0b00000000;
  state.memory[66] = 0b11111111;

  byte_t memory_copy[256];
  for (int i = 0; i < 256; ++i)
    memory_copy[i] = state.memory[i];

  SECTION("Basic use") {
    Iorr ins(64);
    ins._execute(state);
    REQUIRE(state.acc == 0b01010101);
    REQUIRE(state.pc == 2);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  SECTION("ACC overflow") {
    Iorr ins(65);
    ins._execute(state);
    REQUIRE(state.acc == 0b01000101);
    REQUIRE(state.pc == 2);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  state.pc = 254;
  SECTION("PC overflow") {
    Iorr ins(66);
    ins._execute(state);
    REQUIRE(state.acc == 0b11111111);
    REQUIRE(state.pc == 254);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }
}

TEST_CASE("Ixor Execution 2", "[instruction][exec]") {
  ProcessorState state;
  state.acc = 0b01000101;
  state.pc = 2;

  // Initialise memory
  for (int i = 0; i < 256; ++i)
    state.memory[i] = 0b01010101; // 85

  state.memory[64] = 0b00010001;
  state.memory[65] = 0b00000000;
  state.memory[66] = 0b11111111;

  byte_t memory_copy[256];
  for (int i = 0; i < 256; ++i)
    memory_copy[i] = state.memory[i];

  SECTION("Basic use") {
    Ixor ins(64);
    ins._execute(state);
    REQUIRE(state.acc == 0b01010100);
    REQUIRE(state.pc == 2);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  SECTION("ACC overflow") {
    Ixor ins(65);
    ins._execute(state);
    REQUIRE(state.acc == 0b01000101);
    REQUIRE(state.pc == 2);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  state.pc = 254;
  SECTION("PC overflow") {
    Ixor ins(66);
    ins._execute(state);
    REQUIRE(state.acc == 0b10111010);
    REQUIRE(state.pc == 254);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }
}

TEST_CASE("Ildr Execution 2", "[instruction][exec]") {
  ProcessorState state;
  state.acc = 5;
  state.pc = 2;

  // Initialise memory
  for (int i = 0; i < 256; ++i)
    state.memory[i] = 0b01010101; // 85

  state.memory[64] = 15;
  state.memory[65] = 255;
  state.memory[66] = 0;

  byte_t memory_copy[256];
  for (int i = 0; i < 256; ++i)
    memory_copy[i] = state.memory[i];

  SECTION("Basic use") {
    Ildr ins(64);
    ins._execute(state);
    REQUIRE(state.acc == 15);
    REQUIRE(state.pc == 2);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  SECTION("Basic use 2") {
    Ildr ins(65);
    ins._execute(state);
    REQUIRE(state.acc == 255);
    REQUIRE(state.pc == 2);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }

  state.pc = 254;
  SECTION("PC overflow") {
    Ildr ins(66);
    ins._execute(state);
    REQUIRE(state.acc == 0);
    REQUIRE(state.pc == 254);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) 
      REQUIRE(state.memory[i] == memory_copy[i]);
  }
}

TEST_CASE("Istr Execution 2", "[instruction][exec]") {
  ProcessorState state;
  state.acc = 170;
  state.pc = 2;

  // Initialise memory
  for (int i = 0; i < 256; ++i)
    state.memory[i] = 0b01010101; // 85

  state.memory[64] = 15;
  state.memory[65] = 255;
  state.memory[66] = 0;

  byte_t memory_copy[256];
  for (int i = 0; i < 256; ++i)
    memory_copy[i] = state.memory[i];

  SECTION("Basic use") {
    Istr ins(64);
    ins._execute(state);
    REQUIRE(state.acc == 170);
    REQUIRE(state.pc == 2);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) {
      if (i == 64) REQUIRE(state.memory[i] == 170);
      else REQUIRE(state.memory[i] == memory_copy[i]);
    }
  }

  SECTION("Basic use 2") {
    Istr ins(65);
    ins._execute(state);
    REQUIRE(state.acc == 170);
    REQUIRE(state.pc == 2);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) {
      if (i == 65) REQUIRE(state.memory[i] == 170);
      else REQUIRE(state.memory[i] == memory_copy[i]);
    }
  }

  state.pc = 254;
  SECTION("PC overflow") {
    Istr ins(66);
    ins._execute(state);
    REQUIRE(state.acc == 170);
    REQUIRE(state.pc == 254);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i) {
      if (i == 66) REQUIRE(state.memory[i] == 170);
      else REQUIRE(state.memory[i] == memory_copy[i]);
    }
  }
}

TEST_CASE("Ijmp Execution 2", "[instruction][exec]") {
  ProcessorState state;
  state.acc = 170;
  state.pc = 2;

  // Initialise memory
  for (int i = 0; i < 256; ++i)
    state.memory[i] = 0b01010101; // 85

  SECTION("Basic use") {
    Ijmp ins(64);
    ins._execute(state);
    REQUIRE(state.acc == 170);
    REQUIRE(state.pc == 62);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i)
      REQUIRE(state.memory[i] == 0b01010101);
  }

  SECTION("Basic use 2") {
    Ijmp ins(256);
    ins._execute(state);
    REQUIRE(state.acc == 170);
    REQUIRE((state.pc == 254 || state.pc == -2));

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i)
      REQUIRE(state.memory[i] == 0b01010101);
  }
}

TEST_CASE("Ijne Execution 2", "[instruction][exec]") {
  ProcessorState state;
  state.acc = 170;
  state.pc = 2;

  // Initialise memory
  for (int i = 0; i < 256; ++i)
    state.memory[i] = 0b01010101; // 85

  SECTION("Taken") {
    Ijne ins(64);
    ins._execute(state);
    REQUIRE(state.acc == 170);
    REQUIRE(state.pc == 62);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i)
      REQUIRE(state.memory[i] == 0b01010101);
  }

  state.acc = 1;
  state.pc = 0;
  SECTION("Taken 2") {
    Ijne ins(0);
    ins._execute(state);
    REQUIRE(state.acc == 1);
    REQUIRE((state.pc == 254 || state.pc == -2));

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i)
      REQUIRE(state.memory[i] == 0b01010101);
  }

  state.acc = 1;
  state.pc = 250;
  SECTION("Taken 3") {
    Ijne ins(0);
    ins._execute(state);
    REQUIRE(state.acc == 1);
    REQUIRE((state.pc == 254 || state.pc == -2));

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i)
      REQUIRE(state.memory[i] == 0b01010101);
  }

  state.acc = 0;
  state.pc = 2;
  SECTION("Not taken") {
    Ijne ins(64);
    ins._execute(state);
    REQUIRE(state.acc == 0);
    REQUIRE(state.pc == 2);

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i)
      REQUIRE(state.memory[i] == 0b01010101);
  }

  state.acc = 0;
  state.pc = 254;
  SECTION("Not taken 2") {
    Ijne ins(0);
    ins._execute(state);
    REQUIRE(state.acc == 0);
    REQUIRE((state.pc == 254 || state.pc == -2));

    // Ensure memory is not corrupted
    for (int i = 0; i < 256; ++i)
      REQUIRE(state.memory[i] == 0b01010101);
  }
}

// -----------------------------------------------------------------------------
// This is beyond the lectures (not examinable, not needed for this assignment,
// probably you'll never need this in your lifetime). Read only, if you want 
// to never be able to sleep at night again.
//
// SFINAE (Substitution Failure Is Not An Error) is a C++ principle stating that
// when a template substitution fails (because the template argument has the
// wrong type), this DOES NOT cause automatically a compilation error. If there
// are multiple template definitions with the same name, the compiler will try
// another one, and if that fails then another one and so on. Compilation will
// only fail if ALL template definitions fail for our template arguments.
//
// The rules about the order template definitions are used are complicated, but
// they generally go from the more narrow definitions to the more wide
// definitions.
//
// While this is a sensible approach to instantiating templates, nerds quickly
// realised they could hack SFINAE to do more complex types of template
// metaprogramming. One such case is below.
//
// Our fundamental problem is that we want to instantiate Breakpoint without
// creating an unnecessary copy of our string. This could happen for example,
// if we pass a char* as string, but the Breakpoint constructor takes an
// std::string. In this case, the compiler will implicitly construct a new
// std::string from our char*. So, what we want to achieve is that if
// Breakpoint takes char*, we pass a char*, if Breakpoint takes std::string&,
// we pass an std::string. So, we need to execute different code depending on
// how Breakpoint was written.
//
// To do this, we create two different wrapper functions:
// create_bkp(N1/2, addr_t, std::string)
// The N1 version is instantiating Breakpoint using std::string, the N2 version
// is using char*. But, we cannot have both of them fully defined: at each
// point in time, one of the two definitions is illegal. But if we define them
// as templates, this is no problem. Definitions of create_bkp are allowed to be
// incorrect, as long as one of them is correct. Thanks to SFINAE, any incorrect
// version will just be ignored.
//
// After this, we need to disambiguate the two cases and order their evaluation.
// We create two empty structs: N1 and N2 which is a subclass of N1. The
// std::string version of create() expects an extra (and unused) N2 argument,
// the char* version expects an extra N1 argument. 
//
// We create a final wrapper that calls create_bkp<Breakpoint>() with a N2 argument.
// SFINAE starts with the most specific template definition: the one expecting a
// N2 argument (which instantiates Breakpoint with std::string). If Breakpoint
// expects std::string, this succeeds and we're done. If it fails, we try the
// less specific template definition: the one expecting N1 (the base class of N2).
// If that fails, then compilation fails.
//
// This testing code would be so much simpler, if I could just force you to use
// the right argument types.
//
// -----------------------------------------------------------------------------

struct N1 {};
struct N2 : N1 {};

template <class T>
auto create_bkp(N2, addr_t address, std::string& name) -> decltype(T(10, std::string{""})) {
	return T(address, name);
}

template <class T>
auto create_bkp(N1, addr_t address, std::string& name) {
	return T(address, name.c_str());
}

template <class T>
auto create_bkp(addr_t address, std::string& name) {
	return create_bkp<T>(N2{}, address, name);
}

// -----------------------------------------------------------------------------
// -------------------------         BREAKPOINT        -------------------------
// -----------------------------------------------------------------------------
TEST_CASE("Breakpoint Constructors and Getters", "[breakpoint][init][getters]") {

  SECTION("Constructor: Basic Usage") {
    Breakpoint bkp(10, "BKP1");
    CHECK(bkp.get_address() == 10);
    CHECK_THAT(bkp.get_name(), Catch::Matchers::Equals("BKP1"));
  }

  SECTION("Constructor: Large Address") {
    Breakpoint bkp(1000, "BKP");
    CHECK(bkp.get_address() == (1000 & 255));
    CHECK_THAT(bkp.get_name(), Catch::Matchers::Equals("BKP"));
  }

  SECTION("Constructor: Very Large Name") {
    Breakpoint bkp(10, "BKP0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789");
    CHECK(bkp.get_address() == 10);
    CHECK_THAT(bkp.get_name(), Catch::Matchers::Equals("BKP0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"));
  }
  
  SECTION("Constructor: Zero-length Name") {
    Breakpoint bkp(10, "");
    CHECK(bkp.get_address() == 10);
    CHECK_THAT(bkp.get_name(), Catch::Matchers::Equals(""));
  }

  SECTION("Constructor: Copies the name argument") {
    std::string name{"BKP6"};
    Breakpoint bkp = create_bkp<Breakpoint>(10, name);
    CHECK(bkp.get_address() == 10);
    CHECK_THAT(bkp.get_name(), Catch::Matchers::Equals(name));
    name[0] = 'G';
    CHECK_THAT(bkp.get_name(), !Catch::Matchers::Equals(name));
    CHECK(not same_memory(bkp.get_name(), name));
  }

  SECTION("Copy Constructor") {
    Breakpoint bkp1(10, "BKP1");
    CHECK(bkp1.get_address() == 10);
    CHECK_THAT(bkp1.get_name(), Catch::Matchers::Equals("BKP1"));

    Breakpoint bkp2{bkp1};
    CHECK(bkp2.get_address() == 10);
    CHECK_THAT(bkp2.get_name(), Catch::Matchers::Equals("BKP1"));

    // Did we actually copy the name data, or did we copy the name pointer?
    CHECK(not same_memory(bkp1.get_name(), bkp2.get_name()));
  }

  SECTION("Copy Assignment Operator") {
    Breakpoint bkp1(12, "BKPx");
    CHECK(bkp1.get_address() == 12);
    CHECK_THAT(bkp1.get_name(), Catch::Matchers::Equals("BKPx"));

    Breakpoint bkp2(16, "BKP0");
    CHECK(bkp2.get_address() == 16);
    CHECK_THAT(bkp2.get_name(), Catch::Matchers::Equals("BKP0"));

    bkp2 = bkp1;
    CHECK(bkp2.get_address() == 12);
    CHECK_THAT(bkp2.get_name(), Catch::Matchers::Equals("BKPx"));

    // Did we actually copy the name data, or did we copy the name pointer?
    CHECK(not same_memory(bkp1.get_name(), bkp2.get_name()));
  }

  SECTION("Move Constructor") {
    // I found the hard way that glibc copies small strings even if we are doing a move
    // So, I am going to force it to do a move
    Breakpoint bkp1(20, "BKP11aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    CHECK(bkp1.get_address() == 20);
    CHECK_THAT(bkp1.get_name(), Catch::Matchers::Equals("BKP11aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    const void* name_ptr1 = get_location(bkp1.get_name());

    Breakpoint bkp2{std::move(bkp1)};
    CHECK(bkp2.get_address() == 20);
    CHECK_THAT(bkp2.get_name(), Catch::Matchers::Equals("BKP11aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    const void* name_ptr2 = get_location(bkp2.get_name());

    // If we did it right, we shouldn't have created a copy of the name data,
    // we should have just moved the pointer to the new object
    CHECK(name_ptr1 == name_ptr2);
  }

  SECTION("Move Assignment Operator") {
    // I found the hard way that glibc copies small strings even if we are doing a move
    // So, I am going to force it to do a move
    Breakpoint bkp1(22, "BKPxxaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    CHECK(bkp1.get_address() == 22);
    CHECK_THAT(bkp1.get_name(), Catch::Matchers::Equals("BKPxxaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    // location of the internal string of the first breakpoint
    const void* name_ptr1 = get_location(bkp1.get_name());

    Breakpoint bkp2(16, "BKP0");
    CHECK(bkp2.get_address() == 16);
    CHECK_THAT(bkp2.get_name(), Catch::Matchers::Equals("BKP0"));

    // Move the internals of the first breakpoint into the second
    bkp2 = std::move(bkp1);
    // Address is correct
    CHECK(bkp2.get_address() == 22);
    // Name is correct
    CHECK_THAT(bkp2.get_name(), Catch::Matchers::Equals("BKPxxaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    // location of the internal string of the second breakpoint
    const void* name_ptr2 = get_location(bkp2.get_name());

    // If we did it right, we shouldn't have created a copy of the name data,
    // we should have just moved the pointer to the new object, so these
    // two pointers should point to the same location
    CHECK(name_ptr1 == name_ptr2);
  }
}

TEST_CASE("Breakpoint has()", "[breakpoint][exec]") {
  Breakpoint bkp(128, "BKP2000");
  CHECK(bkp.has(128));
  CHECK(bkp.has("BKP2000"));

  CHECK(!bkp.has("127"));
  CHECK(!bkp.has("129"));
  CHECK(!bkp.has("384"));
  CHECK(!bkp.has("BKP200"));
  CHECK(!bkp.has("KP2000"));
  CHECK(!bkp.has("KP"));

  Breakpoint bkp2(256, "");
  CHECK(bkp2.has(0));
  CHECK(bkp2.has(""));

  CHECK(!bkp2.has("255"));
  CHECK(!bkp2.has("256"));
  CHECK(!bkp2.has("1"));
  CHECK(!bkp2.has(" "));
  CHECK(!bkp2.has("B"));
  CHECK(!bkp2.has("0"));
}

// Concept that applies only to types that have a unary operator*
// Essentially types that can be dereferenced
template <typename T>
concept Dereferencable = requires  (const T& obj) {
  *obj;
};

template <class T>
const T* get_address(Dereferencable auto ptr) {
	return &(*ptr);
}

// -----------------------------------------------------------------------------
// -------------------------          EMULATOR         -------------------------
// -----------------------------------------------------------------------------

TEST_CASE("Emulator Constructors, Initialisers, and Getters", "[emulator][init][getters]") {
  Emulator emulator;

  // Default Constructor
  REQUIRE(emulator.read_acc() == 0);
  REQUIRE(emulator.read_pc() == 0);
  for (int i = 0; i < 256; ++i)
    REQUIRE(emulator.read_mem(i) == 0);
  REQUIRE(emulator.cycles() == 0);
  REQUIRE(emulator.num_breakpoints() == 0);

  // Emulator::load_state()
  // This is the only way to precisely control the internal state of the
  // Emulator, so we have to test this very early. Most subsequent tests
  // rely on this part working correctly
  REQUIRE(fopen("data/state1.txt", "r") != NULL);
  REQUIRE(emulator.load_state("data/state1.txt"));
  REQUIRE(emulator.read_acc() == 10);
  REQUIRE(emulator.read_pc() == 4);
  REQUIRE(emulator.read_mem(0) == 0);
  REQUIRE(emulator.read_mem(1) == 0);
  REQUIRE(emulator.read_mem(2) == 0);
  REQUIRE(emulator.read_mem(3) == 0);
  REQUIRE(emulator.read_mem(4) == 1);
  REQUIRE(emulator.read_mem(5) == 10);
  REQUIRE(emulator.read_mem(6) == 3);
  REQUIRE(emulator.read_mem(7) == 6);
  REQUIRE(emulator.read_mem(33) == 32);
  REQUIRE(emulator.read_mem(34) == 5);
  REQUIRE(emulator.read_mem(35) == 0);
  REQUIRE(emulator.cycles() == 0);
  REQUIRE(emulator.num_breakpoints() == 1);
  REQUIRE_THAT(emulator.find_breakpoint(32)->get_name(), Catch::Matchers::Equals("END"));

  // Essentially a pointer to the beginning of the breakpoints storage
  const Breakpoint* breakpoints = get_address<Breakpoint>(emulator.find_breakpoint(32));

  SECTION("Copy Constructor") {
    Emulator emulator1{emulator};

    CHECK(emulator1.read_acc() == emulator.read_acc());
    CHECK(emulator1.read_pc() == emulator.read_pc());
    for (int i = 0; i < 256; ++i)
      CHECK(emulator1.read_mem(i) == emulator.read_mem(i));
    CHECK(emulator1.cycles() == emulator.cycles());
    CHECK(emulator1.num_breakpoints() == emulator.num_breakpoints());
    
    // Did we actually copy the breakpoints data,
    // or did we just copy the breakpoints pointer?
    const Breakpoint* breakpoints1 = get_address<Breakpoint>(emulator1.find_breakpoint(32));
    CHECK(breakpoints1 != breakpoints);

    // Double check that the internal data of the two objects are distinct
    REQUIRE(emulator.run(100));
    CHECK(emulator1.read_mem(35) != emulator.read_mem(35));
  }

  SECTION("Copy Assignment Operator") {
    Emulator emulator1;
    for (int i = 0; i < 256; ++i)
      REQUIRE(emulator1.read_mem(i) == 0);
    REQUIRE(emulator1.num_breakpoints() == 0);

    // Copy
    emulator1 = emulator;

    CHECK(emulator1.read_acc() == emulator.read_acc());
    CHECK(emulator1.read_pc() == emulator.read_pc());
    for (int i = 0; i < 256; ++i)
      CHECK(emulator1.read_mem(i) == emulator.read_mem(i));
    CHECK(emulator1.cycles() == emulator.cycles());
    CHECK(emulator1.num_breakpoints() == emulator.num_breakpoints());
    
    // Did we actually copy the breakpoints data,
    // or did we just copy the breakpoints pointer?
    const Breakpoint* breakpoints1 = get_address<Breakpoint>(emulator1.find_breakpoint(32));
    CHECK(breakpoints1 != breakpoints);

    // Double check that the internal data of the two objects are distinct
    REQUIRE(emulator.run(100));
    CHECK(emulator1.read_mem(35) != emulator.read_mem(35));
  }

  SECTION("Copy Assignment Operator: Same objects") {
    Emulator emulator1{emulator};

    // Check that the copy constructor did the right thing
    CHECK(emulator1.read_acc() == emulator.read_acc());
    CHECK(emulator1.read_pc() == emulator.read_pc());
    for (int i = 0; i < 256; ++i)
      CHECK(emulator1.read_mem(i) == emulator.read_mem(i));
    CHECK(emulator1.cycles() == emulator.cycles());
    CHECK(emulator1.num_breakpoints() == emulator.num_breakpoints());

    // Essentially a pointer to the beginning of the breakpoints storage
    const Breakpoint* breakpoints1 = get_address<Breakpoint>(emulator1.find_breakpoint(32));

    // Just a reference to the same object
    Emulator& emulator_handle{emulator1};

    // Copy the object to itself
    // Ideally the assignment operator should return without
    // doing anything
    emulator1 = emulator_handle;

    // The data should not have changed
    CHECK(emulator1.read_acc() == emulator.read_acc());
    CHECK(emulator1.read_pc() == emulator.read_pc());
    for (int i = 0; i < 256; ++i)
      CHECK(emulator1.read_mem(i) == emulator.read_mem(i));
    CHECK(emulator1.cycles() == emulator.cycles());
    CHECK(emulator1.num_breakpoints() == emulator.num_breakpoints());

    // The breakpoints storage should have remained the same
    const Breakpoint* breakpoints2 = get_address<Breakpoint>(emulator1.find_breakpoint(32));
    CHECK(breakpoints1 == breakpoints2);
  }

  SECTION("Move Constructor") {
    Emulator emulator1{std::move(emulator)};

    // Same checks as originally for emulator
    CHECK(emulator1.read_acc() == 10);
    CHECK(emulator1.read_pc() == 4);
    CHECK(emulator1.read_mem(0) == 0);
    CHECK(emulator1.read_mem(1) == 0);
    CHECK(emulator1.read_mem(2) == 0);
    CHECK(emulator1.read_mem(3) == 0);
    CHECK(emulator1.read_mem(4) == 1);
    CHECK(emulator1.read_mem(5) == 10);
    CHECK(emulator1.read_mem(6) == 3);
    CHECK(emulator1.read_mem(7) == 6);
    CHECK(emulator1.read_mem(33) == 32);
    CHECK(emulator1.read_mem(34) == 5);
    CHECK(emulator1.read_mem(35) == 0);
    CHECK(emulator1.cycles() == 0);
    CHECK(emulator1.num_breakpoints() == 1);
    CHECK_THAT(emulator1.find_breakpoint(32)->get_name(), Catch::Matchers::Equals("END"));

    // We should have moved the breakpoints data instead of copying them
    const Breakpoint* breakpoints1 = get_address<Breakpoint>(emulator1.find_breakpoint(32));
    CHECK(breakpoints1 == breakpoints);
  }

  SECTION("Move Assignment Operator") {
    Emulator emulator1;
    for (int i = 0; i < 256; ++i)
      REQUIRE(emulator1.read_mem(i) == 0);
    REQUIRE(emulator1.num_breakpoints() == 0);

    emulator1 = std::move(emulator);

    // Same checks as originally for emulator
    CHECK(emulator1.read_acc() == 10);
    CHECK(emulator1.read_pc() == 4);
    CHECK(emulator1.read_mem(0) == 0);
    CHECK(emulator1.read_mem(1) == 0);
    CHECK(emulator1.read_mem(2) == 0);
    CHECK(emulator1.read_mem(3) == 0);
    CHECK(emulator1.read_mem(4) == 1);
    CHECK(emulator1.read_mem(5) == 10);
    CHECK(emulator1.read_mem(6) == 3);
    CHECK(emulator1.read_mem(7) == 6);
    CHECK(emulator1.read_mem(33) == 32);
    CHECK(emulator1.read_mem(34) == 5);
    CHECK(emulator1.read_mem(35) == 0);
    CHECK(emulator1.cycles() == 0);
    CHECK(emulator1.num_breakpoints() == 1);
    CHECK_THAT(emulator1.find_breakpoint(32)->get_name(), Catch::Matchers::Equals("END"));

    // We should have moved the breakpoints data instead of copying them
    const Breakpoint* breakpoints1 = get_address<Breakpoint>(emulator1.find_breakpoint(32));
    CHECK(breakpoints1 == breakpoints);
  }
}

TEST_CASE("Emulator::fetch1", "[emulator][exec]") {
  Emulator emulator;

  InstructionData data = emulator.fetch();
  CHECK(data.address == 0);
  CHECK(data.opcode == ADD);

  // Emulator::load_state()
  // This is the only way to precisely control the internal state of the
  // Emulator, so we have to test this very early. Most subsequent tests
  // rely on this part working correctly
  REQUIRE(fopen("data/state1.txt", "r") != NULL);
  REQUIRE(emulator.load_state("data/state1.txt"));
  REQUIRE(emulator.read_acc() == 10);
  REQUIRE(emulator.read_pc() == 4);
  REQUIRE(emulator.read_mem(0) == 0);
  REQUIRE(emulator.read_mem(1) == 0);
  REQUIRE(emulator.read_mem(2) == 0);
  REQUIRE(emulator.read_mem(3) == 0);
  REQUIRE(emulator.read_mem(4) == 1);
  REQUIRE(emulator.read_mem(5) == 10);
  REQUIRE(emulator.read_mem(6) == 3);
  REQUIRE(emulator.read_mem(7) == 6);
  REQUIRE(emulator.read_mem(33) == 32);
  REQUIRE(emulator.read_mem(34) == 5);
  REQUIRE(emulator.read_mem(35) == 0);
  REQUIRE(emulator.cycles() == 0);
  REQUIRE(emulator.num_breakpoints() == 1);
  REQUIRE_THAT(emulator.find_breakpoint(32)->get_name(), Catch::Matchers::Equals("END"));
  
  // pc is 4
  data = emulator.fetch();
  CHECK(data.opcode == AND);
  CHECK(data.address == 10);

  // Next instruction
  emulator.run(1);
  REQUIRE(emulator.read_pc() == 6); 

  data = emulator.fetch();
  CHECK(data.opcode == XOR);
  CHECK(data.address == 6);

  // Next instruction
  emulator.run(1);
  REQUIRE(emulator.read_pc() == 8); 

  data = emulator.fetch();
  CHECK(data.opcode == LDR);
  CHECK(data.address == 34);

  // Run to the end
  emulator.run(500);
  REQUIRE(emulator.read_pc() == 32); 

  // Verify that the program has executed correctly
  REQUIRE(emulator.read_mem(35) == 12);

  // At this point it should be stuck at position 32
  data = emulator.fetch();
  CHECK(data.opcode == JMP);
  CHECK(data.address == 32);
}

TEST_CASE("Emulator::fetch2", "[emulator][exec]") {
  Emulator emulator;

  InstructionData data = emulator.fetch();
  REQUIRE(data.address == 0);
  REQUIRE(data.opcode == ADD);

  // Emulator::load_state()
  // This is the only way to precisely control the internal state of the
  // Emulator, so we have to test this very early. Most subsequent tests
  // rely on this part working correctly
  REQUIRE(fopen("data/state2.txt", "r") != NULL);
  REQUIRE(emulator.load_state("data/state2.txt"));

  REQUIRE(emulator.read_acc() == 0);
  REQUIRE(emulator.read_pc() == 0);
  int sum = 0;
  for (int i = 64; i < 96; ++i)
    sum += emulator.read_mem(i);
  REQUIRE(sum == 48);
  REQUIRE(emulator.cycles() == 5);
  REQUIRE(emulator.num_breakpoints() == 0);

  // pc is 0, so the fetch should be straightforward
  data = emulator.fetch();
  CHECK(data.opcode == LDR);
  CHECK(data.address == 63);

  // Next instruction
  emulator.run(1);
  REQUIRE(emulator.read_pc() == 2); 

  data = emulator.fetch();
  CHECK(data.opcode == ADD);
  CHECK(data.address == 64);

  // Next instruction
  emulator.run(1);
  REQUIRE(emulator.read_pc() == 4); 

  data = emulator.fetch();
  CHECK(data.opcode == STR);
  CHECK(data.address == 63);

  // Run to the end
  emulator.run(500);
  REQUIRE(emulator.read_pc() == 20); 

  // Verify that the program has executed correctly
  REQUIRE(emulator.read_mem(63) == 48);

  // At this point it should be stuck at position 20
  data = emulator.fetch();
  CHECK(data.opcode == JMP);
  CHECK(data.address == 20);
}

// Emulator::decode is currently just a thin wrapper around 
// InstructionBase::generateInstruction, so we run the exact same tests.
// This expands to 8 tests, one for each instruction type
TEMPLATE_TEST_CASE_SIG("Emulator::decode", 
                   "[emulator][exec]",
                   ((typename T, int V), T, V),
                   (Iadd, 0), (Iand, 1),
                   (Iorr, 2), (Ixor, 3),
                   (Ildr, 4), (Istr, 5),
                   (Ijmp, 6), (Ijne, 7)) {

  Emulator emulator;
  const addr_t addresses[5]{1, 60, 1023, 1024, 4100};
  char buffer[MAX_NAME];

  InstructionData data;

  for (int i = 0; i < 5; ++i) {
    data.opcode = opcodes[V];
    data.address = addresses[i];

    // Parse the instruction data and create the instruction
    InstructionPointer auto obj = emulator.decode(data);
    REQUIRE(obj->get_address() == ((data.address) & 255));

    // Does the name() method return a string-like object
    // that matches the expected value
    REQUIRE_THAT(obj->name(), Catch::Matchers::Equals(classnames[V]));

    // Is this executing the same thing as if we called execute() on the derived
    // type? This should never fail, unless you have done something weird with
    // the implementation of InstructionBase::execute()
    ProcessorState state1;
    state1.acc = 85;
    for (int i = 0; i < 256; ++i)
      state1.memory[i] = i;
    
    // Assumming the ProcessorState copy constructor works correctly
    ProcessorState state2{state1};

    // An instruction object with the same data as obj, but used without going through the base type
    T obj2(data.address);

    obj->execute(state1);
    obj2.execute(state2);
    REQUIRE(state1.acc == state2.acc);
    REQUIRE(state1.pc == state2.pc);
    for (int i = 0; i < 256; ++i)
      REQUIRE(state1.memory[i] == state2.memory[i]);

    // No delete for obj! This has to be a memory leak. Or not?
  }
}

TEST_CASE("Emulator::decode with invalid data", "[emulator][exec]") {
  Emulator emulator;
  InstructionData data;
  data.address = 255;
  for (int idx = NUM_OPCODES; idx < 256; ++idx) {
    data.opcode = idx;
    CHECK(nullptr == NULL);
    CHECK(emulator.decode(data) == NULL);
  }
} 

TEST_CASE("Emulator::execute", "[emulator][exec]") {
  Emulator emulator;

  // State3 memory is filled with successive values
  // Position 0 -> 0
  // Position i -> i
  // Position 255 -> 255
  REQUIRE(fopen("data/state3.txt", "r") != NULL);
  REQUIRE(emulator.load_state("data/state3.txt"));
  for (int i = 0; i < 256; ++i)
    REQUIRE(emulator.read_mem(i) == i);

  Ildr i1(34);
  CHECK(emulator.execute(&i1));
  CHECK(emulator.read_acc() == 34);
  CHECK(emulator.read_pc() == 2);

  Iadd i2(36);
  CHECK(emulator.execute(&i2));
  CHECK(emulator.read_acc() == 70);
  CHECK(emulator.read_pc() == 4);

  Istr i3(34);
  CHECK(emulator.execute(&i3));
  CHECK(emulator.read_acc() == 70);
  CHECK(emulator.read_mem(34) == 70);
  CHECK(emulator.read_pc() == 6);

  Ijne i4(18);
  CHECK(emulator.execute(&i4));
  CHECK(emulator.read_acc() == 70);
  CHECK(emulator.read_pc() == 18);

  Ildr i5(35);
  CHECK(emulator.execute(&i5));
  CHECK(emulator.read_acc() == 35);
  CHECK(emulator.read_pc() == 20);

  Iadd i6(37);
  CHECK(emulator.execute(&i6));
  CHECK(emulator.read_acc() == 72);
  CHECK(emulator.read_pc() == 22);

  Istr i7(35);
  CHECK(emulator.execute(&i7));
  CHECK(emulator.read_acc() == 72);
  CHECK(emulator.read_mem(35) == 72);
  CHECK(emulator.read_pc() == 24);

  Ijmp i8(8);
  CHECK(emulator.execute(&i8));
  CHECK(emulator.read_acc() == 72);
  CHECK(emulator.read_pc() == 8);
}

TEST_CASE("Emulator::run1", "[emulator][exec]") {
  REQUIRE(fopen("data/state1.txt", "r") != NULL);

  Emulator emulator;
  REQUIRE(emulator.load_state("data/state1.txt"));

  // run with zero steps -> don't run but return 1
  CHECK(emulator.run(0));
  CHECK(emulator.cycles() == 0);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 6);
  CHECK(emulator.read_acc() == 0);
  CHECK(emulator.cycles() == 1);
  CHECK(emulator.read_mem(35) == 0);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 8);
  CHECK(emulator.read_acc() == 3);
  CHECK(emulator.cycles() == 2);
  CHECK(emulator.read_mem(35) == 0);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 10);
  CHECK(emulator.read_acc() == 5);
  CHECK(emulator.cycles() == 3);
  CHECK(emulator.read_mem(35) == 0);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 12);
  CHECK(emulator.read_acc() == 4);
  CHECK(emulator.cycles() == 4);
  CHECK(emulator.read_mem(35) == 0);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 14);
  CHECK(emulator.read_acc() == 4);
  CHECK(emulator.cycles() == 5);
  CHECK(emulator.read_mem(35) == 0);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 18);
  CHECK(emulator.read_acc() == 4);
  CHECK(emulator.cycles() == 6);
  CHECK(emulator.read_mem(35) == 0);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 20);
  CHECK(emulator.read_acc() == 0);
  CHECK(emulator.cycles() == 7);
  CHECK(emulator.read_mem(35) == 0);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 22);
  CHECK(emulator.read_acc() == 3);
  CHECK(emulator.cycles() == 8);
  CHECK(emulator.read_mem(35) == 0);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 24);
  CHECK(emulator.read_acc() == 3);
  CHECK(emulator.cycles() == 9);
  CHECK(emulator.read_mem(35) == 3);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 8);
  CHECK(emulator.read_acc() == 3);
  CHECK(emulator.cycles() == 10);
  CHECK(emulator.read_mem(35) == 3);

  // Fast forward a bit more 
  CHECK(emulator.run(16));
  CHECK(emulator.read_pc() == 8);
  CHECK(emulator.read_acc() == 9);
  CHECK(emulator.cycles() == 26);
  CHECK(emulator.read_mem(35) == 9);

  // Run to the end
  emulator.run(50);
  CHECK(emulator.read_pc() == 32); 
  CHECK(emulator.read_acc() == 0); 
  CHECK(emulator.cycles() == 39);

  // Verify that the program has executed correctly
  CHECK(emulator.read_mem(35) == 12);
}

TEST_CASE("Emulator::run2", "[emulator][exec]") {
  REQUIRE(fopen("data/state2.txt", "r") != NULL);

  Emulator emulator;
  REQUIRE(emulator.load_state("data/state2.txt"));

  // run with zero steps -> don't run but return 1
  CHECK(emulator.run(0));
  CHECK(emulator.cycles() == 5);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 2);
  CHECK(emulator.read_acc() == 0);
  CHECK(emulator.cycles() == 6);
  CHECK(emulator.read_mem(63) == 0);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 4);
  CHECK(emulator.read_acc() == 1);
  CHECK(emulator.cycles() == 7);
  CHECK(emulator.read_mem(63) == 0);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 6);
  CHECK(emulator.read_acc() == 1);
  CHECK(emulator.cycles() == 8);
  CHECK(emulator.read_mem(63) == 1);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 8);
  CHECK(emulator.read_acc() == 64);
  CHECK(emulator.cycles() == 9);
  CHECK(emulator.read_mem(63) == 1);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 10);
  CHECK(emulator.read_acc() == 65);
  CHECK(emulator.cycles() == 10);
  CHECK(emulator.read_mem(63) == 1);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 12);
  CHECK(emulator.read_acc() == 65);
  CHECK(emulator.cycles() == 11);
  CHECK(emulator.read_mem(63) == 1);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 14);
  CHECK(emulator.read_acc() == 32);
  CHECK(emulator.cycles() == 12);
  CHECK(emulator.read_mem(63) == 1);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 16);
  CHECK(emulator.read_acc() == 31);
  CHECK(emulator.cycles() == 13);
  CHECK(emulator.read_mem(63) == 1);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 18);
  CHECK(emulator.read_acc() == 31);
  CHECK(emulator.cycles() == 14);
  CHECK(emulator.read_mem(63) == 1);

  // Next instruction
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 0);
  CHECK(emulator.read_acc() == 31);
  CHECK(emulator.cycles() == 15);
  CHECK(emulator.read_mem(63) == 1);

  // Fast forward a bit more 
  CHECK(emulator.run(10));
  CHECK(emulator.read_pc() == 0);
  CHECK(emulator.read_acc() == 30);
  CHECK(emulator.cycles() == 25);
  CHECK(emulator.read_mem(63) == 2);

  // Fast forward a bit more 
  CHECK(emulator.run(10));
  CHECK(emulator.read_pc() == 0);
  CHECK(emulator.read_acc() == 29);
  CHECK(emulator.cycles() == 35);
  CHECK(emulator.read_mem(63) == 3);

  // Fast forward a bit more 
  CHECK(emulator.run(70));
  CHECK(emulator.read_pc() == 0);
  CHECK(emulator.read_acc() == 22);
  CHECK(emulator.cycles() == 105);
  CHECK(emulator.read_mem(63) == 12);

  // Fast forward a bit more 
  CHECK(emulator.run(100));
  CHECK(emulator.read_pc() == 0);
  CHECK(emulator.read_acc() == 12);
  CHECK(emulator.cycles() == 205);
  CHECK(emulator.read_mem(63) == 36);

  // Run to the end
  CHECK(emulator.run(195));
  CHECK(emulator.read_pc() == 20); 
  CHECK(emulator.read_acc() == 0); 
  CHECK(emulator.cycles() == 400);

  // Verify that the program has executed correctly
  CHECK(emulator.read_mem(63) == 48);
}

// state3 does not contain any meaningful program and is meant to fail
// Contents of memory are the same as the address of the data
// read_mem(i) == i
TEST_CASE("Emulator::run3", "[emulator][exec]") {
  REQUIRE(fopen("data/state3.txt", "r") != NULL);

  Emulator emulator;
  REQUIRE(emulator.load_state("data/state3.txt"));

  // run with zero steps -> don't run but return 1
  CHECK(emulator.run(0));
  CHECK(emulator.cycles() == 0);

  // Next instruction: (0,1) -> ADD 1
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 2);
  CHECK(emulator.read_acc() == 1);
  CHECK(emulator.cycles() == 1);

  // Next instruction: (2,3) -> ORR 3
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 4);
  CHECK(emulator.read_acc() == 3);
  CHECK(emulator.cycles() == 2);

  // Next instruction: (4,5) -> LDR 5
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 6);
  CHECK(emulator.read_acc() == 5);
  CHECK(emulator.cycles() == 3);

  // Next instruction: (6,7) -> JMP to 7
  // This is introduces an error because pc ends up with an odd value
  // The emulated system expects instructions aligned at two-byte offsets
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 7);
  CHECK(emulator.read_acc() == 5);
  CHECK(emulator.cycles() == 4);

  // Next instruction: (7,8) ->  This will trigger the error
  CHECK(not emulator.run(1));
  CHECK(emulator.read_pc() == 7);
  CHECK(emulator.read_acc() == 5);
  CHECK(emulator.cycles() == 4);
}

// state4 does not contain any meaningful program and is meant to fail
TEST_CASE("Emulator::run4", "[emulator][exec]") {
  REQUIRE(fopen("data/state4.txt", "r") != NULL);

  Emulator emulator;
  REQUIRE(emulator.load_state("data/state4.txt"));

  // run with zero steps -> don't run but return 1
  CHECK(emulator.run(0));
  CHECK(emulator.cycles() == 0);

  // Next instruction: (0,1) -> ADD 1
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 2);
  CHECK(emulator.read_acc() == 1);
  CHECK(emulator.cycles() == 1);

  // Next instruction: (1,3) -> AND 3
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 4);
  CHECK(emulator.read_acc() == 1);
  CHECK(emulator.cycles() == 2);

  // Next instruction: (2,5) -> ORR 5
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 6);
  CHECK(emulator.read_acc() == 5);
  CHECK(emulator.cycles() == 3);

  // Next instruction: (3,7) -> XOR 7
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 8);
  CHECK(emulator.read_acc() == 2);
  CHECK(emulator.cycles() == 4);

  // Next instruction: (4,9) -> LDR 9
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 10);
  CHECK(emulator.read_acc() == 9);
  CHECK(emulator.cycles() == 5);

  // Next instruction: (5,11) -> STR [11]
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 12);
  CHECK(emulator.read_acc() == 9);
  CHECK(emulator.cycles() == 6);
  CHECK(emulator.read_mem(11) == 9);

  // Next instruction: (6,14) -> JMP 14
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 14);
  CHECK(emulator.read_acc() == 9);
  CHECK(emulator.cycles() == 7);

  // Next instruction: (7,20) -> JNE 20
  CHECK(emulator.run(1));
  CHECK(emulator.read_pc() == 20);
  CHECK(emulator.read_acc() == 9);
  CHECK(emulator.cycles() == 8);

  // Next instruction: (10,4) -> Invalid instruction
  // The cycles counter should not have been incremented
  CHECK(not emulator.run(1));
  CHECK(emulator.read_pc() == 20);
  CHECK(emulator.read_acc() == 9);
  CHECK(emulator.cycles() == 8);
}

// -----------------------------------------------------------------------------
// -------------------------    BREAKPOINT MANAGEMENT  -------------------------
// -----------------------------------------------------------------------------

TEST_CASE("Insert Breakpoint", "[emulator][breakpoint][exec]") {
  REQUIRE(fopen("data/state2.txt", "r") != NULL);

  Emulator emulator;

  // No breakpoints originally
  REQUIRE(emulator.load_state("data/state2.txt"));
  REQUIRE(emulator.num_breakpoints() == 0);

  // Add one
  CHECK(emulator.insert_breakpoint(0, "START"));
  CHECK(emulator.num_breakpoints() == 1);

  // Try (and fail) adding one with the same name
  CHECK(not emulator.insert_breakpoint(2, "START"));
  CHECK(emulator.num_breakpoints() == 1);

  // Try (and fail) adding one with the same address
  CHECK(not emulator.insert_breakpoint(0, "START2"));
  CHECK(emulator.num_breakpoints() == 1);

  // Add one with the non-conflicting address and name from the previous two attempts
  CHECK(emulator.insert_breakpoint(2, "START2"));
  CHECK(emulator.num_breakpoints() == 2);

  // Add one with an empty name
  CHECK(emulator.insert_breakpoint(16, ""));
  CHECK(emulator.num_breakpoints() == 3);

  // Fail to add one with an empty name again
  CHECK(not emulator.insert_breakpoint(4, ""));
  CHECK(emulator.num_breakpoints() == 3);

  // Add one with a very long name
  CHECK(emulator.insert_breakpoint(4, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
  CHECK(emulator.num_breakpoints() == 4);

  // Fail to add the same long name again
  CHECK(not emulator.insert_breakpoint(26, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
  CHECK(emulator.num_breakpoints() == 4);

  // Try (and fail) adding one whose address gets translated to 2
  CHECK(not emulator.insert_breakpoint(258, "OVERFLOW"));
  CHECK(emulator.num_breakpoints() == 4);

  // But succeed adding one that gets translated to an available address
  CHECK(emulator.insert_breakpoint(264, "OVERFLOW2"));
  CHECK(emulator.num_breakpoints() == 5);

  // Also succeed insert a breakpoint with different capitalisation of the same word
  CHECK(emulator.insert_breakpoint(266, "overflow2"));
  CHECK(emulator.num_breakpoints() == 6);
}

TEST_CASE("Find Breakpoint", "[emulator][breakpoint][exec]") {
  REQUIRE(fopen("data/state2.txt", "r") != NULL);

  const char* long_name1 = "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO";
  const char* long_name2 = "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO";
  
  Emulator emulator;

  // No breakpoints originally
  REQUIRE(emulator.load_state("data/state2.txt"));
  REQUIRE(emulator.num_breakpoints() == 0);

  int num_breakpoints = 7;
  const addr_t addresses[] = {254, 8, 0, 2, 38, 64, 128};
  const char* names[] = {"END", "", "START", "TWO", long_name1, "OFFBYONE", "MID"};

  // Add a few breakpoints
  for (int i = 0; i < num_breakpoints; ++i) 
    REQUIRE(emulator.insert_breakpoint(addresses[i], names[i]));
  REQUIRE(emulator.num_breakpoints() == 7);

  SECTION("find_breakpoint() finds existing breakpoints by address") {
    for (int i = 0; i < num_breakpoints; ++i) {
      auto bkp = emulator.find_breakpoint(addresses[i]);
      CHECK(bkp != NULL);
      CHECK(bkp->get_address() == addresses[i]);
      CHECK_THAT(bkp->get_name(), Catch::Matchers::Equals(names[i]));
    }

    // Also check the same for argument values > 256
    for (int i = 0; i < num_breakpoints; ++i) {
      auto bkp = emulator.find_breakpoint(addresses[i] + (256 * i));
      CHECK(bkp != NULL);
      CHECK(bkp->get_address() == addresses[i]);
      CHECK_THAT(bkp->get_name(), Catch::Matchers::Equals(names[i]));
    }
  }

  SECTION("find_breakpoint() finds existing breakpoints by name") {
    for (int i = 0; i < num_breakpoints; ++i) {
      auto bkp = emulator.find_breakpoint(names[i]);
      CHECK(bkp != NULL);
      CHECK(bkp->get_address() == addresses[i]);
      CHECK_THAT(bkp->get_name(), Catch::Matchers::Equals(names[i]));
    }
  }

  SECTION("The two find_breakpoint() functions return pointers to the same objects") {
    for (int i = 0; i < num_breakpoints; ++i) {
      auto bkp1 = emulator.find_breakpoint(addresses[i]);
      auto bkp2 = emulator.find_breakpoint(names[i]);
      CHECK(bkp1 != NULL);
      CHECK(bkp2 != NULL);
      CHECK(bkp1 == bkp2);
    }
  }

  SECTION("find_breakpoint DOES NOT find breakpoints with non-existing addresses") {
    CHECK(emulator.find_breakpoint(4) == NULL);
    CHECK(emulator.find_breakpoint(10) == NULL);
    CHECK(emulator.find_breakpoint(42) == NULL);
    CHECK(emulator.find_breakpoint(44) == NULL);
    CHECK(emulator.find_breakpoint(68) == NULL);
    CHECK(emulator.find_breakpoint(130) == NULL);
    CHECK(emulator.find_breakpoint(260) == NULL);
    CHECK(emulator.find_breakpoint(300) == NULL);
  }

  SECTION("find_breakpoint DOES NOT find breakpoints with non-existing names") {
    CHECK(emulator.find_breakpoint("STAR") == NULL);
    CHECK(emulator.find_breakpoint("TWOO") == NULL);
    CHECK(emulator.find_breakpoint("0") == NULL);
    CHECK(emulator.find_breakpoint(long_name2) == NULL);
    CHECK(emulator.find_breakpoint("A") == NULL);
    CHECK(emulator.find_breakpoint("D") == NULL);
    CHECK(emulator.find_breakpoint("EN") == NULL);
    CHECK(emulator.find_breakpoint("O") == NULL);
  }
}

TEST_CASE("Delete Breakpoint", "[emulator][breakpoint][exec]") {
  REQUIRE(fopen("data/state2.txt", "r") != NULL);

  const char* long_name1 = "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO";
  const char* long_name2 = "OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO";
  
  Emulator emulator;

  // No breakpoints originally
  REQUIRE(emulator.load_state("data/state2.txt"));
  REQUIRE(emulator.num_breakpoints() == 0);

  // Add a few breakpoints
  int num_breakpoints = 7;
  const addr_t addresses[] = {254, 8, 0, 2, 38, 64, 128};
  const char* names[] = {"END", "", "START", "TWO", long_name1, "OFFBYONE", "MID"};

  for (int i = 0; i < num_breakpoints; ++i) 
    REQUIRE(emulator.insert_breakpoint(addresses[i], names[i]));
  REQUIRE(emulator.num_breakpoints() == 7);

  // Redo the checks from above
  // We will only test delete_breakpoint if we are confident the breakpoints
  // data are in the expected state
  for (int i = 0; i < num_breakpoints; ++i) {
    auto bkp1 = emulator.find_breakpoint(addresses[i]);
    auto bkp2 = emulator.find_breakpoint(names[i]);
    REQUIRE(bkp1 != NULL);
    REQUIRE(bkp2 != NULL);
    REQUIRE(bkp1 == bkp2);
    REQUIRE(bkp1->get_address() == addresses[i]);
    REQUIRE_THAT(bkp1->get_name(), Catch::Matchers::Equals(names[i]));
  }

  SECTION("delete_breakpoint() deletes existing breakpoints by address") {
    for (int i = 0; i < num_breakpoints; ++i) {
      CHECK(emulator.delete_breakpoint(addresses[i]));
      CHECK(emulator.num_breakpoints() == num_breakpoints - i - 1);
    }
  }

  // Each section is executed independently of other sections
  // When entering this we still have seven checkpoints available
  // despite the section above
  SECTION("delete_breakpoint() deletes existing breakpoints by name") {
    for (int i = 0; i < num_breakpoints; ++i) {
      CHECK(emulator.delete_breakpoint(names[i]));
      CHECK(emulator.num_breakpoints() == num_breakpoints - i - 1);
    }
  }

  SECTION("delete_breakpoint() still works if deleting them from the end") {
    for (int i = num_breakpoints - 1; i >= 0; --i) {
      CHECK(emulator.delete_breakpoint(addresses[i]));
      CHECK(emulator.num_breakpoints() == i);
    }
  }

  SECTION("delete_checkpoint() does nothing when asked to delete non-existent checkpoints") {
    CHECK(not emulator.delete_breakpoint(4));
    CHECK(not emulator.delete_breakpoint(10));
    CHECK(not emulator.delete_breakpoint(42));
    CHECK(not emulator.delete_breakpoint(44));
    CHECK(not emulator.delete_breakpoint(68));
    CHECK(not emulator.delete_breakpoint(130));
    CHECK(not emulator.delete_breakpoint(260));
    CHECK(not emulator.delete_breakpoint(300));
    CHECK(not emulator.delete_breakpoint("STAR"));
    CHECK(not emulator.delete_breakpoint("TWOO"));
    CHECK(not emulator.delete_breakpoint("0"));
    CHECK(not emulator.delete_breakpoint(long_name2));
    CHECK(not emulator.delete_breakpoint("A"));
    CHECK(not emulator.delete_breakpoint("D"));
    CHECK(not emulator.delete_breakpoint("EN"));
    CHECK(not emulator.delete_breakpoint("O"));
    CHECK(emulator.num_breakpoints() == num_breakpoints);
  }

  SECTION("checkpoints are not findable after deleting them") {
    for (int i = 0; i < num_breakpoints; ++i) {
      REQUIRE(emulator.delete_breakpoint(addresses[i]));
      REQUIRE(emulator.num_breakpoints() == num_breakpoints - i - 1);

      CHECK(emulator.find_breakpoint(addresses[i]) == NULL);
      CHECK(emulator.find_breakpoint(names[i]) == NULL);

      // But the other breakpoints are still there
      for (int j = i + 1; j < num_breakpoints; ++j) {
        CHECK(emulator.find_breakpoint(addresses[j]) != NULL);
        CHECK(emulator.find_breakpoint(names[j]) != NULL);
      }
    }
  }

  SECTION("checkpoints are not findable after deleting them (in reverse)") {
    for (int i = num_breakpoints - 1; i >= 0; --i) {
      REQUIRE(emulator.delete_breakpoint(addresses[i]));
      REQUIRE(emulator.num_breakpoints() == i);

      CHECK(emulator.find_breakpoint(addresses[i]) == NULL);
      CHECK(emulator.find_breakpoint(names[i]) == NULL);

      // But the other breakpoints are still there
      for (int j = 0; j < i; ++j) {
        CHECK(emulator.find_breakpoint(addresses[j]) != NULL);
        CHECK(emulator.find_breakpoint(names[j]) != NULL);
      }
    }
  }

  SECTION("Cannot delete the same checkpoint twice") {
    for (int i = 0; i < num_breakpoints; ++i) {
      REQUIRE(emulator.delete_breakpoint(names[i]));
      CHECK(not emulator.delete_breakpoint(addresses[i]));
    }
  }

  SECTION("Can add a breakpoint after deleting it") {
    for (int i = 0; i < num_breakpoints; ++i) {
      REQUIRE(emulator.delete_breakpoint(names[i]));
      REQUIRE(emulator.num_breakpoints() == num_breakpoints - 1);
      REQUIRE(emulator.find_breakpoint(names[i]) == NULL);

      CHECK(emulator.insert_breakpoint(addresses[i], names[i]));
      CHECK(emulator.num_breakpoints() == num_breakpoints);
      CHECK(emulator.find_breakpoint(names[i]) != NULL);
    }
  }
}

// -----------------------------------------------------------------------------
// -------------------------        UTILITIES          -------------------------
// -----------------------------------------------------------------------------

// This mainly tests is_zero() and is_breakpoint(), but indirectly
// tests the whole emulator. If you get an error here, make sure
// you don't have another error in an earlier test
TEST_CASE("Emulator state helpers", "[emulator][exec]") {
  REQUIRE(fopen("data/state2.txt", "r") != NULL);

  Emulator emulator;
  REQUIRE(emulator.load_state("data/state2.txt"));

  // Check that state is as expected
  REQUIRE(emulator.read_acc() == 0);
  REQUIRE(emulator.read_pc() == 0);
  int sum = 0;
  for (int i = 64; i < 96; ++i)
    sum += emulator.read_mem(i);
  REQUIRE(sum == 48);
  REQUIRE(emulator.cycles() == 5);
  REQUIRE(emulator.num_breakpoints() == 0);

  // Add a few breakpoints
  int num_breakpoints = 4;
  const addr_t addresses[] = {0, 4, 18, 20};
  const char* names[] = {"START", "UPDATE", "LOOPEND", "END"};

  for (int i = 0; i < num_breakpoints; ++i) 
    REQUIRE(emulator.insert_breakpoint(addresses[i], names[i]));
  REQUIRE(emulator.num_breakpoints() == num_breakpoints);

  CHECK(emulator.is_zero());
  CHECK(emulator.is_breakpoint());

  // Run till next breakpoint
  REQUIRE(emulator.run(1000000));
  REQUIRE(emulator.read_pc() == 4);
  REQUIRE(emulator.read_acc() == 1);
  REQUIRE(emulator.cycles() == 7);
  REQUIRE(emulator.read_mem(63) == 0);

  CHECK(not emulator.is_zero());
  CHECK(emulator.is_breakpoint());

  // Run till next breakpoint
  REQUIRE(emulator.run(1000000));
  REQUIRE(emulator.read_pc() == 18);
  REQUIRE(emulator.read_acc() == 31);
  REQUIRE(emulator.cycles() == 14);
  REQUIRE(emulator.read_mem(63) == 1);

  CHECK(not emulator.is_zero());
  CHECK(emulator.is_breakpoint());

  // Run till next breakpoint
  REQUIRE(emulator.run(1000000));
  REQUIRE(emulator.read_pc() == 0);
  REQUIRE(emulator.read_acc() == 31);
  REQUIRE(emulator.cycles() == 15);
  REQUIRE(emulator.read_mem(63) == 1);

  CHECK(not emulator.is_zero());
  CHECK(emulator.is_breakpoint());

  // One step
  REQUIRE(emulator.run(1));
  REQUIRE(emulator.read_pc() == 2);
  REQUIRE(emulator.read_acc() == 1);
  REQUIRE(emulator.cycles() == 16);
  REQUIRE(emulator.read_mem(63) == 1);

  CHECK(not emulator.is_zero());
  CHECK(not emulator.is_breakpoint());

  // Leave only the last two breakpoints
  REQUIRE(emulator.delete_breakpoint(0));
  REQUIRE(emulator.delete_breakpoint(4));
  
  // Assuming we will only break at the LOOPEND breakpoint
  // the following is equivalent to executing the loop 30 times
  for (int i = 0; i < 30; ++i) {
    REQUIRE(emulator.run(1000));
    REQUIRE(emulator.read_pc() == 18);
    REQUIRE(emulator.read_acc() == 30 - i);
    REQUIRE(emulator.cycles() == 24 + 10 * i);
    CHECK(not emulator.is_zero());
    CHECK(emulator.is_breakpoint());
  }

  // This should be the last iteration
  REQUIRE(emulator.run(1000));
  REQUIRE(emulator.read_pc() == 18);
  REQUIRE(emulator.read_acc() == 0);
  REQUIRE(emulator.cycles() == 324);
  CHECK(emulator.is_zero());
  CHECK(emulator.is_breakpoint());

  // We will not the JNE jump, instead we will go to the next instruction
  // which is also a breakpoint (and an idle loop signifying the end of the program)
  REQUIRE(emulator.run(1000));
  REQUIRE(emulator.read_pc() == 20);
  REQUIRE(emulator.read_acc() == 0);
  REQUIRE(emulator.cycles() == 325);
  CHECK(emulator.is_zero());
  CHECK(emulator.is_breakpoint());

  // At this point, we stuck on instruction 20 forever
  // Running 1k steps, actually only runs for one cycle
  REQUIRE(emulator.run(1000));
  REQUIRE(emulator.read_pc() == 20);
  REQUIRE(emulator.read_acc() == 0);
  REQUIRE(emulator.cycles() == 326);
  CHECK(emulator.is_zero());
  CHECK(emulator.is_breakpoint());

  // Remove the breakpoint at the end
  REQUIRE(emulator.delete_breakpoint(20));

  // Now running for 1000 steps, should actually run for 1000 steps
  REQUIRE(emulator.run(1000));
  REQUIRE(emulator.read_pc() == 20);
  REQUIRE(emulator.read_acc() == 0);
  REQUIRE(emulator.cycles() == 1326);
  CHECK(emulator.is_zero());
  CHECK(not emulator.is_breakpoint());

  // Verify that the program has executed correctly
  CHECK(emulator.read_mem(63) == 48);
}

// ----------- Magic for capturing the printf --------------
int capture_stdout(const char* filename) {
  // Get a file descriptor handle for the original stdout
  int original_stdout = dup(STDOUT_FILENO);

  // Open the file where we will send the standard output
  int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1)
    return -1;

  // Make the standard output handle point to our file
  // Effectively standard output goes to the file
  fflush(stdout);
  dup2(fd, STDOUT_FILENO);

  // We don't need the original handle for now
  close(fd);
  return original_stdout;
}

#define MAX_FILENAME (1 * 1024)
#define MAX_LINE (1 * 1024)

// This is a bit complicated but the idea is that:
// 1) we call print_program() on various system states from the folder `data`,
// 2) we store the output in text files in the folder `output`
// 3) we compare these outputs against a set of reference outputs in `ref`
// If you can't figure out what happening in here, just compare the files
// in folders `output` and `ref` to see how your output differs from what
// was expected.
TEST_CASE("Program Print", "[emulator][exec]") {
  Emulator emulator;
  char infile[MAX_FILENAME];
  char outfile[MAX_FILENAME];
  char reffile[MAX_FILENAME];
  char line_out[MAX_LINE];
  char line_ref[MAX_LINE];

  for (int state_id = 1; state_id <= 4; ++state_id) {
    // File to be loaded
    int num = snprintf(infile, MAX_FILENAME, "data/state%d.txt", state_id);
    REQUIRE(num > 0);

    // File where the printed output will be saved
    num = snprintf(outfile, MAX_FILENAME, "output/print_program%d.txt", state_id);
    REQUIRE(num > 0);

    // File where the correct output is stored
    num = snprintf(reffile, MAX_FILENAME, "ref/print_program%d.txt", state_id);
    REQUIRE(num > 0);

    // Load the state
    REQUIRE(fopen(infile, "r") != NULL);
    REQUIRE(emulator.load_state(infile));
    int original_stdout = capture_stdout(outfile);
    if (original_stdout >= 0) {
      // Make sure we are at the start of the file
      lseek(STDOUT_FILENO, 0, SEEK_SET);

      // With the output captured, print the program
      emulator.print_program();

      // Make sure the printout is flushed to the file before we close it
      fflush(stdout);

      // Close our file
      close(STDOUT_FILENO);

      // Restore the original stdout
      dup2(original_stdout, STDOUT_FILENO);
    }
    REQUIRE(original_stdout >= 0);

    // Now compare the output against the reference
    FILE* fpout = fopen(outfile, "r");
    REQUIRE(fpout != NULL);

    FILE* fpref = fopen(reffile, "r");
    REQUIRE(fpref != NULL);

    int line_count = 0;
    while(true) {
      ++line_count;
      // read the next lines
      char* res1 = fgets(line_out, MAX_LINE, fpout);
      char* res2 = fgets(line_ref, MAX_LINE, fpref);

      if (res1 == NULL && res2 == NULL) {
        // both files reached the end at the same time
        break;
      } else if (res1 == NULL) {
        FAIL("Output is shorter than the reference for state file " << infile
            << "\nCompare files " << outfile << " and " << reffile << " for details");
      } else if (res2 == NULL) {
        FAIL("Output is longer than the reference for state file " << infile
            << "\nCompare files " << outfile << " and " << reffile << " for details");
      }

      // Normal case
      int comp = strncmp(line_out, line_ref, MAX_LINE);
      if (comp != 0) {
        FAIL("In the print_program() for " << infile
            << "\nOutput line " << line_count << " is:\n" << line_out 
            << "\nReference line " << line_count << " is:\n" << line_ref
            << "\nCompare files " << outfile << " and " << reffile << " for details");
      }
    }
    fclose(fpout);
    fclose(fpref);
  }
}

// We have indirectly tested load_state() multiple times,
// so here we'll keep this short
TEST_CASE("Load State: Correct cases", "[emulator][exec]") {
  REQUIRE(fopen("data/state1.txt", "r") != NULL);
  REQUIRE(fopen("data/state3.txt", "r") != NULL);
  REQUIRE(fopen("data/state_breakpoints.txt", "r") != NULL);
  Emulator emulator;

  SECTION("State1: Various random values and one breakpoint") {
    REQUIRE(emulator.load_state("data/state1.txt"));
    CHECK(emulator.read_acc() == 10);
    CHECK(emulator.read_pc() == 4);
    CHECK(emulator.read_mem(0) == 0);
    CHECK(emulator.read_mem(1) == 0);
    CHECK(emulator.read_mem(2) == 0);
    CHECK(emulator.read_mem(3) == 0);
    CHECK(emulator.read_mem(4) == 1);
    CHECK(emulator.read_mem(5) == 10);
    CHECK(emulator.read_mem(6) == 3);
    CHECK(emulator.read_mem(7) == 6);
    CHECK(emulator.read_mem(33) == 32);
    CHECK(emulator.read_mem(34) == 5);
    CHECK(emulator.read_mem(35) == 0);
    CHECK(emulator.cycles() == 0);
    CHECK(emulator.num_breakpoints() == 1);
    CHECK_THAT(emulator.find_breakpoint(32)->get_name(), Catch::Matchers::Equals("END"));
  }

  SECTION("State3: Successive values in memory") {
    REQUIRE(emulator.load_state("data/state3.txt"));
    CHECK(emulator.read_acc() == 0);
    CHECK(emulator.read_pc() == 0);
    CHECK(emulator.cycles() == 0);
    CHECK(emulator.num_breakpoints() == 0);
    for (int i = 0; i < 256; ++i)
      CHECK(emulator.read_mem(i) == i);
  }

  SECTION("State_breakpoints: A lot of breakpoints") {
    REQUIRE(emulator.load_state("data/state_breakpoints.txt"));
    CHECK(emulator.read_acc() == 0);
    CHECK(emulator.read_pc() == 0);
    CHECK(emulator.cycles() == 0);
    for (int i = 0; i < 256; ++i)
      CHECK(emulator.read_mem(i) == 0);

    CHECK(emulator.num_breakpoints() == 19);

    REQUIRE(emulator.find_breakpoint(0) != NULL);
    CHECK_THAT(emulator.find_breakpoint(0)->get_name(), Catch::Matchers::Equals("START"));
    
    REQUIRE(emulator.find_breakpoint(254) != NULL);
    CHECK_THAT(emulator.find_breakpoint(254)->get_name(), Catch::Matchers::Equals("END"));
    
    REQUIRE(emulator.find_breakpoint(128) != NULL);
    CHECK_THAT(emulator.find_breakpoint(128)->get_name(), Catch::Matchers::Equals("MID"));
    
    REQUIRE(emulator.find_breakpoint(60) != NULL);
    CHECK_THAT(emulator.find_breakpoint(60)->get_name(), Catch::Matchers::Equals("VERYLARGENAMEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE"));

    const char* names[15] = {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O"};
    for (int i = 0; i < 15; ++i) {
      REQUIRE(emulator.find_breakpoint(i*2 + 2) != NULL);
      CHECK_THAT(emulator.find_breakpoint(i*2 + 2)->get_name(), Catch::Matchers::Equals(names[i]));
    }
  }
}

// Let's see wheter load_state() can handle errors in the input correctly
TEST_CASE("Load State: Wrong cases", "[emulator][exec]") {
  REQUIRE(fopen("data/invalid0000000000000000000000.txt", "r") == NULL);
  REQUIRE(fopen("data/invalid1.txt", "r") != NULL);
  REQUIRE(fopen("data/invalid2.txt", "r") != NULL);
  REQUIRE(fopen("data/invalid3.txt", "r") != NULL);
  REQUIRE(fopen("data/invalid4.txt", "r") != NULL);
  REQUIRE(fopen("data/invalid5a.txt", "r") != NULL);
  REQUIRE(fopen("data/invalid5b.txt", "r") != NULL);
  REQUIRE(fopen("data/invalid5c.txt", "r") != NULL);
  REQUIRE(fopen("data/invalid6.txt", "r") != NULL);
  REQUIRE(fopen("data/invalid7.txt", "r") != NULL);
  REQUIRE(fopen("data/invalid8.txt", "r") != NULL);
  REQUIRE(fopen("data/invalid9.txt", "r") != NULL);

  Emulator emulator;

  SECTION("State file doesn't exist") {
    REQUIRE(not emulator.load_state("data/invalid0000000000000000000000.txt"));
  }

  SECTION("First word is not a number") {
    REQUIRE(not emulator.load_state("data/invalid1.txt"));
  }

  SECTION("Second word is not a number") {
    REQUIRE(not emulator.load_state("data/invalid2.txt"));
  }

  SECTION("Third word is not a number") {
    REQUIRE(not emulator.load_state("data/invalid3.txt"));
  }

  SECTION("Memory content lines have just a single number") {
    REQUIRE(not emulator.load_state("data/invalid4.txt"));
  }

  SECTION("Total Cycles has an illegal value") {
    REQUIRE(not emulator.load_state("data/invalid5a.txt"));
  }

  SECTION("ACC has an illegal value") {
    REQUIRE(not emulator.load_state("data/invalid5b.txt"));
  }

  SECTION("PC has an illegal value") {
    REQUIRE(not emulator.load_state("data/invalid5c.txt"));
  }

  SECTION("Breakpoint on a negative address") {
    REQUIRE(not emulator.load_state("data/invalid6.txt"));
  }

  SECTION("Breakpoint on a large address") {
    REQUIRE(not emulator.load_state("data/invalid7.txt"));
  }

  SECTION("Breakpoints with the same names") {
    REQUIRE(not emulator.load_state("data/invalid8.txt"));
  }

  SECTION("Breakpoints with the same addresses") {
    REQUIRE(not emulator.load_state("data/invalid9.txt"));
  }
}
