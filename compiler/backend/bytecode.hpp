#pragma once
#include "ir.hpp"
#include <string>

// Write an IRProgram to a .tlc binary file (compile once, run many times).
// Returns true on success.
bool writeBytecode(const IRProgram& prog, const std::string& filename);

// Read a .tlc binary file back into an IRProgram.
// Throws std::runtime_error on bad magic or version mismatch.
IRProgram readBytecode(const std::string& filename);
