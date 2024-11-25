#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <functional>
#include <memory>
#include "instructions.h"

// ========== InstructionBase ==========


void InstructionBase::execute(ProcessorState& state) const {
  // virtual call that implements the actual functionality of the instruction
  _execute(state);

  // move the pc forward
  state.pc += INSTRUCTION_SIZE;

  // trim the accumulator and the PC to fit in number of bits of the architecture
  state.acc &= ARCH_BITMASK;
  state.pc &= ARCH_BITMASK;
}

addr_t InstructionBase::get_address() const {
  return _address;
}

void InstructionBase::_set_address(addr_t address) {
  _address = address & ARCH_BITMASK;
}

std::string InstructionBase::to_string() const {
  // Having a malloc is definitely a bad sign
  std::stringstream result;


  // Figure out what the instruction actually is based on the return value of name()
  // and then generate an appropriate instruction-specific string
  if (name() == "ADD") {
    result << name() << ": ACC <- " << "ACC " << "+ [" << get_address() << "]";
  }
  else if (name() == "AND") {
    result << name() << ": ACC <- " << "ACC " << "& [" << get_address() << "]";
  }
  else if (name() == "ORR") {
    result << name() << ": ACC <- " << "ACC " << "| [" << get_address() << "]";
  }
  else if (name() == "XOR") {
    result << name() << ": ACC <- " << "ACC " << "^ [" << get_address() << "]";
  }
  else if (name() == "LDR") {
    result << name() << ": ACC <- [" << get_address() << "]";
  }
  else if (name() == "STR") {
    result << name() << ": ACC -> [" << get_address() << "]";
  }
  else if (name() == "JMP") {
    result << name() << ": PC  <- " << get_address() << "";
  }
  else if (name() == "JNE") {
    result << name() << ": PC  <- " << get_address() << " if ACC != 0";
  }
  else {
    std::cout << "issue";
    assert(0);
  }

  // if (strncmp(name(), "ADD", 3) == 0)
  //   sprintf(buffer, "%s: ACC <- ACC + [%d]", name(), get_address()); // cout << name(): "ACC <- " << "ACC + " << get_address();
  // else if (strncmp(name(), "AND", 3) == 0)
  //   sprintf(buffer, "%s: ACC <- ACC & [%d]", name(), get_address());
  // else if (strncmp(name(), "ORR", 3) == 0)
  //   sprintf(buffer, "%s: ACC <- ACC | [%d]", name(), get_address());
  // else if (strncmp(name(), "XOR", 3) == 0)
  //   sprintf(buffer, "%s: ACC <- ACC ^ [%d]", name(), get_address());
  // else if (strncmp(name(), "LDR", 3) == 0)
    // sprintf(buffer, "%s: ACC <- [%d]", name(), get_address());
  // else if (strncmp(name(), "STR", 3) == 0)
    // sprintf(buffer, "%s: ACC -> [%d]", name(), get_address());
  // else if (strncmp(name(), "JMP", 3) == 0)
    // sprintf(buffer, "%s: PC  <- %d", name(), get_address());
  // else if (strncmp(name(), "JNE", 3) == 0)
    // sprintf(buffer, "%s: PC  <- %d if ACC != 0", name(), get_address());
  // else
  //   // This should never happen unless we have an error in name() or one of the strncmp's above
  //   // i.e. the tests will never try to trigger this code
  //   assert(0);
  return result.str();
}

std::unique_ptr<InstructionBase> InstructionBase::generateInstruction(InstructionData data) {
    if (data.opcode == ADD)
        return std::make_unique<Iadd>(data.address);
    if (data.opcode == AND)
        return std::make_unique<Iand>(data.address);
    if (data.opcode == ORR)
        return std::make_unique<Iorr>(data.address);
    if (data.opcode == XOR)
        return std::make_unique<Ixor>(data.address);
    if (data.opcode == LDR)
        return std::make_unique<Ildr>(data.address);
    if (data.opcode == STR)
        return std::make_unique<Istr>(data.address);
    if (data.opcode == JMP)
        return std::make_unique<Ijmp>(data.address);
    if (data.opcode == JNE)
        return std::make_unique<Ijne>(data.address);

    return nullptr;  // Return nullptr if the opcode is not found
}

// ========== ADD Instruction ==========
Iadd::Iadd(addr_t address) {
  _set_address(address);
}

void Iadd::_execute(ProcessorState& state) const {
  state.acc += state.memory.at(get_address());
}

const std::string Iadd::name() const {
  return "ADD";
}

// ========== AND Instruction ==========
Iand::Iand(addr_t address) {
  _set_address(address);
}

void Iand::_execute(ProcessorState& state) const {
  state.acc &= state.memory.at(get_address());
}

const std::string Iand::name() const {
  return "AND";
}

// ========== ORR Instruction ==========
Iorr::Iorr(addr_t address) {
  _set_address(address);
}

void Iorr::_execute(ProcessorState &state) const
{
    state.acc |= state.memory.at(get_address());
}

const std::string Iorr::name() const {
  return "ORR";
}

// ========== XOR Instruction ==========
Ixor::Ixor(addr_t address) {
  _set_address(address);
}


void Ixor::_execute(ProcessorState& state) const {
  state.acc ^= state.memory.at(get_address());
}

const std::string Ixor::name() const {
  return "XOR";
}

// ========== LDR Instruction ==========
Ildr::Ildr(addr_t address) {
  _set_address(address);
}

void Ildr::_execute(ProcessorState& state) const {
  state.acc = state.memory.at(get_address());
}

const std::string Ildr::name() const {
  return "LDR";
}

// ========== STR Instruction ==========
Istr::Istr(addr_t address) {
  _set_address(address);
}

void Istr::_execute(ProcessorState& state) const {
  state.memory.at(get_address()) = state.acc;
}

const std::string Istr::name() const {
  return "STR";
}

// ========== JMP Instruction ==========
Ijmp::Ijmp(addr_t address) {
  _set_address(address);
}

void Ijmp::_execute(ProcessorState& state) const {
  // Why minus two? Because execute() will increment PC by two,
  // so to make the PC take (eventually) the value `address`
  // I need to subtract two here. Same applies for JNE below
  // This kind of unintuitive behaviour is a clear sign of bad
  // class hierarchy design 
  state.pc = get_address() - 2;
}

const std::string Ijmp::name() const {
  return "JMP";
}

// ========== JNE Instruction ==========
Ijne::Ijne(addr_t address) {
  _set_address(address);
}

void Ijne::_execute(ProcessorState& state) const {
  // Same hack as above
  if (state.acc != 0)
    state.pc = get_address() - 2;
}

const std::string Ijne::name() const {
  return "JNE";
}

