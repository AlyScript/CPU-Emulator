#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>
#include "emulator.h"

// ============= Breakpoint ==============
Breakpoint::Breakpoint() : _address(0), _name("") { }

Breakpoint::Breakpoint(addr_t address, const std::string& name) 
    : _address(address & ARCH_BITMASK), _name(name) {
  
}

// Copy constructor
Breakpoint::Breakpoint(const Breakpoint& other) 
    : _address(other._address), _name(other._name) {
  
}

// Move constructor
Breakpoint::Breakpoint(Breakpoint&& other) noexcept 
    : _address(other._address), _name(std::move(other._name)) {
  other._address = 0;
}

// Copy assignment
Breakpoint& Breakpoint::operator=(const Breakpoint& other) {
  if (this == &other)
    return *this;
  _address = other.get_address();
  _name = other.get_name();
  return *this;
}

// Move assignment
Breakpoint& Breakpoint::operator=(Breakpoint&& other) noexcept {
  if (this != &other) {
    _address = std::move(other._address);
    _name = std::move(other._name);

    other._address = 0;
    other._name.clear();
  }
  return *this;
}

const addr_t& Breakpoint::get_address() const {
  return _address;
}

const std::string& Breakpoint::get_name() const {
  return _name;
}

int Breakpoint::has(addr_t address) const {
  return _address == (address & ARCH_BITMASK);
}

int Breakpoint::has(const std::string& name) const {
  return _name == name;
}

// ============= Emulator ==============

// ----------> Initialisation
Emulator::Emulator() : breakpoints() {
  state = ProcessorState();

  // An array as big as the max number of instructions we could ever have
  // This is obviously an overkill. Initialising the array to be shorter
  // is okay, as long as you can handle the worst case of MAX_INSTRUCTIONS
  // breakpoints
  // breakpoints = new Breakpoint[MAX_INSTRUCTIONS];
}

// Copy Constructor
Emulator::Emulator(const Emulator& other)
  : state(other.state), breakpoints(other.breakpoints), breakpoints_sz(other.breakpoints_sz), total_cycles(other.total_cycles) {
  
}

// Move Constructor
Emulator::Emulator(Emulator&& other) noexcept
  : state(std::move(other.state)),
    breakpoints(std::move(other.breakpoints)),
    breakpoints_sz(other.breakpoints_sz),
    total_cycles(other.total_cycles) {
  
  other.breakpoints_sz = 0;
  other.total_cycles = 0;
}

// Copy Assignment Operator
Emulator& Emulator::operator=(const Emulator& other) {
  if (this == &other)
    return *this;

  state = other.state;
  breakpoints = other.breakpoints;
  breakpoints_sz = other.breakpoints_sz;
  total_cycles = other.total_cycles;

  return *this;
}

// Move Assignment Operator
Emulator& Emulator::operator=(Emulator&& other) noexcept {
  if (this == &other)
    return *this;

  state = std::move(other.state);
  breakpoints = std::move(other.breakpoints);
  breakpoints_sz = other.breakpoints_sz;
  total_cycles = other.total_cycles;

  other.breakpoints_sz = 0;
  other.total_cycles = 0;

  return *this;
}

// ----------> Main emulation loop

InstructionData Emulator::fetch() const {
  return {state.memory.at(state.pc), state.memory.at(state.pc + 1)};
}


std::unique_ptr<InstructionBase> Emulator::decode(InstructionData data) const {
  // decode here is just a thin wrapper around generateInstruction()
  // In a more complex emulator, more things would happen here
  return InstructionBase::generateInstruction(data);
}

int Emulator::execute(InstructionBase* instr) {
  // Again this is just a thin wrapper,
  // but this is a side-effect of having a simple emulator
  instr->execute(state);
  return 1;
}

int Emulator::run(int steps) {
  // No steps to execute
  if (steps == 0)
    return 1;

  // Repeat for the given number of steps
  // Break with return code 0, if we find an error
  // Break with return code 1, if we find a breakpoint
  // Keep track of the total number of cycles we've executed successfully
  for (; steps > 0; --steps) {
    // Instructions are supposed to be aligned on two-byte offsets:
    // PC should be even. Terminate if PC is odd.
    if ((state.pc % 2) == 1)
      return 0;

    // Fetch the next instruction from memory and transform it into an InstructionBase-derived object
    std::unique_ptr<InstructionBase> instr = decode(fetch());

    if (instr == NULL)
      return 0;

    // What the function name says
    int success = execute(instr.get());

    // Terminate if we didn't execute the instruction successfully
    if (success == 0)
      return 0;

    ++total_cycles;
    
    if (is_breakpoint() == 1)
      return 1;
  }

  return 1;
}

// ----------> Breakpoint management

int Emulator::insert_breakpoint(addr_t address, std::string name) {
  // breakpoints is full (should never happen though!)
  if (breakpoints_sz == MAX_INSTRUCTIONS)
    return 0;

  // Breakpoint already exists
  if (find_breakpoint(address) != NULL)
    return 0;

  // Breakpoint name already used
  if (find_breakpoint(name) != NULL)
    return 0;

  // Insert breakpoint and increment breakpoints_sz in a single step
  breakpoints.at(breakpoints_sz++) = Breakpoint(address, name);
  return 1;
}


const Breakpoint* Emulator::find_breakpoint(addr_t address) const {
  // iterate over all breakpoints
  for (int idx = 0; idx < breakpoints_sz; ++idx) {
    if (breakpoints.at(idx).has(address)) {
      // if this one has the address we're looking for return it
      return &breakpoints.at(idx);
    }
  }
  // indicates failure to find a breakpoint
  return NULL;
}

// Basically the same as above, but for the name
const Breakpoint* Emulator::find_breakpoint(const std::string name) const {
  for (int idx = 0; idx < breakpoints_sz; ++idx) {
    if (breakpoints.at(idx).has(name)) {
      return &breakpoints.at(idx);
    }
  }
  return NULL;
}

int Emulator::delete_breakpoint(addr_t address) {
  const Breakpoint* found = find_breakpoint(address);

  if (found == NULL)
    return 0;

  // Remove one breakpoint
  --breakpoints_sz;

  // Urghh: C pointer magic to find the index of the breakpoint from its pointer
  // `found` is a pointer in the `breakpoints` array, so the difference of
  // `found` and `breakpoints` is the index of `found` in the array.
  int found_idx = found - breakpoints.data();

  // Move all breakpoints above found one position to the left, to fill the gap 
  for (int idx = found_idx; idx < breakpoints_sz; ++idx) {
    // This is an object assignment operation, assigning to breakpoints[idx]
    // the object currently in breakpoints[idx + 1]. Without std::move, this
    // would cause a copy
    breakpoints.at(idx) = std::move(breakpoints.at(idx + 1));
  }

  return 1;
}

// Oh, look, this function is practically identical to the one above
int Emulator::delete_breakpoint(const char* name) {
  const Breakpoint* found = find_breakpoint(name);

  if (found == NULL)
    return 0;

  --breakpoints_sz;

  int found_idx = found - breakpoints.data();

  // Move all breakpoints above found, one position to the left to fill the gap 
  for (int idx = found_idx; idx < breakpoints_sz; ++idx) {
    breakpoints.at(idx) = std::move(breakpoints.at(idx + 1));
  }

  return 1;
}

int Emulator::num_breakpoints() const {
  return breakpoints_sz;
}

// ----------> Manage state

int Emulator::cycles() const {
  return total_cycles;
}

data_t Emulator::read_acc() const {
  return state.acc;
}

addr_t Emulator::read_pc() const {
  return state.pc;
}

addr_t Emulator::read_mem(addr_t address) const {
  // limit address to the allowed range of values
  address &= ARCH_BITMASK;
  return state.memory.at(address);
}

// ----------> Utilities

int Emulator::is_zero() const {
  return state.acc == 0;
}

int Emulator::is_breakpoint() const {
  return find_breakpoint(state.pc) != NULL;
}

int Emulator::print_program() const {
  for (int offset = 0; offset < MEMORY_SIZE; offset += INSTRUCTION_SIZE) {
    InstructionData data{state.memory.at(offset), state.memory.at(offset + 1)};
    std::unique_ptr<InstructionBase> instr = decode(data);

    if ((instr == NULL) || (data.opcode == 0 && data.address == 0)) {
      // printf("%d:\t%d\t%d\n", offset, data.opcode, data.address);
      std::cout << offset << ":\t" << static_cast<int>(data.opcode) << "\t" << static_cast<int>(data.address) << std::endl;
    }
    else 
      // printf("%d:\t%d\t%d\t:\t%s\n", offset, data.opcode, data.address, instr->to_string());
      std::cout << offset << ":\t" << static_cast<int>(data.opcode) << "\t" << static_cast<int>(data.address) << "\t:\t" << instr->to_string() << std::endl;
  }
  return 1;
}

int Emulator::load_state(const std::string filename) {
  // Delete all breakpoints
  breakpoints_sz = 0;

  int read = 0;

  std::ifstream file(filename);

  if (!file)
    return 0;

  std::string line;
  if (std::getline(file, line) && !line.empty()) {
    std::istringstream iss(line);
    if (!(iss >> total_cycles) || total_cycles < 0) 
      return 0;
  } else
      return 0;

  if (std::getline(file, line)) {
    std::istringstream iss(line);
    if (!(iss >> state.acc) || state.acc > ARCH_MAXVAL || state.acc < 0) 
      return 0;
  } else 
      return 0;

  if (std::getline(file, line)) {
    std::istringstream iss(line);
    if (!(iss >> state.pc) || state.pc >= MEMORY_SIZE || state.pc < 0) 
      return 0;
    
  } else      
      return 0;

  int num = 0;
  for (int offset = 0; offset < MEMORY_SIZE; ++offset) {
    if (!std::getline(file, line) || line.empty()) {
        return 0;
    }

    std::istringstream iss(line);
    if (!(iss >> num) || !(iss >> std::ws).eof() || num > ARCH_MAXVAL || num < 0) {
        return 0;
    }

    state.memory.at(offset) = num;
  }

  while (true) {
    std::string name;
    if (!(file >> num >> name)) {
        if (file.eof()) break;
        return 0;
    }

    if (num < 0 || num >= MEMORY_SIZE) 
        return 0;
    
    if (!insert_breakpoint(num, name))
      return 0;
  }

  // while (1) {
  //   char name[MAX_NAME];
  //   int read = fscanf(fp, "%d %s\n", &num, name);
  //   // End of the file
  //   if (read != 2)
  //     break;

  //   // Wrong data (num is supposed to be an address)
  //   if ((num < 0) || (num >= MEMORY_SIZE))
  //     return 0;

  //   // Try to insert the breakpoint and return fail if unsuccessful
  //   if (!insert_breakpoint(num, name))
  //     return 0;
  // }

  // FILE *fp = fopen(filename, "r");

  // if (fp == NULL)
  //   return 0;

  // // Make sure that each fscanf reads the right number of items
  // read = fscanf(fp, "%d\n", &total_cycles);
  // if ((read != 1) || (total_cycles < 0))
  //   return 0;

  // read = fscanf(fp, "%d\n", &state.acc);
  // if ((read != 1) || (state.acc > ARCH_MAXVAL) || (state.acc < 0))
  //   return 0;

  // read = fscanf(fp, "%d\n", &state.pc);
  // if ((read != 1) || (state.pc >= MEMORY_SIZE) || (state.pc < 0))
  //   return 0;

  // int num = 0;
  // for (int offset = 0; offset < MEMORY_SIZE; ++offset) {
  //   // PP: Why not read a number directly into a state.memory location?
  //   // C++ by default assumes that your byte sized variable (e.g. state.memory[offset])
  //   // is supposed to hold a character, so it might assume that you are trying to read an
  //   // individual character from the file, not a number that fits in 8-bits.
  //   // E.g. if your next number is '0', it might load memory[offset] = '0' (48).
  //   // There are ways to force fscanf to read a number, but it will cause fewer
  //   // issues down the line, if you use integers as temporary storage, which forces C++
  //   // to read a number
  //   read = fscanf(fp, "%d\n", &num);
  //   if ((read != 1) || (num > ARCH_MAXVAL) || (num < 0))
  //     return 0;
  //   state.memory[offset] = num;
  // }

  // while (1) {
  //   char name[MAX_NAME];
  //   int read = fscanf(fp, "%d %s\n", &num, name);
  //   // End of the file
  //   if (read != 2)
  //     break;

  //   // Wrong data (num is supposed to be an address)
  //   if ((num < 0) || (num >= MEMORY_SIZE))
  //     return 0;

  //   // Try to insert the breakpoint and return fail if unsuccessful
  //   if (!insert_breakpoint(num, name))
  //     return 0;
  // }

  // fclose(fp);
  return 1;
}

int Emulator::save_state(std::string filename) const {
  std::ofstream file(filename);
  
  if (!file) 
    return 0;
  
  file << total_cycles << std::endl;
  file << state.acc << std::endl;
  file << state.pc << std::endl;

  for (int offset = 0; offset < MEMORY_SIZE; ++offset) 
    file << state.memory.at(offset) << std::endl;
  
  for (int idx = 0; idx < breakpoints_sz; ++idx)
    file << breakpoints.at(idx).get_address() << " " << breakpoints.at(idx).get_name() << std::endl;
  

  file.close();
  
  // FILE* fp = fopen(filename, "w");

  // if (fp == NULL)
  //   return 0;

  // fprintf(fp, "%d\n", total_cycles);
  // fprintf(fp, "%d\n", state.acc);
  // fprintf(fp, "%d\n", state.pc);

  // for (int offset = 0; offset < MEMORY_SIZE; ++offset) {
  //   // PP: Use temporary variables for the same reason as in load_state
  //   int num = state.memory[offset];
  //   fprintf(fp, "%d\n", num);
  // }

  // for (int idx = 0; idx < breakpoints_sz; ++idx)
  //   fprintf(fp, "%d %s\n", breakpoints[idx].get_address(), breakpoints[idx].get_name());

  // fclose(fp);
  
  return 1;
}
