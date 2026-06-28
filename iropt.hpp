#pragma once
#include "ir.hpp"

// Run all 7 optimization passes on an IRProgram.
// Each pass operates on the flat instruction vectors in prog.main
// and in every function's code vector — never inside the IR generator.
//
// Pipeline: constantPropagation → deadCodeElimination → copyPropagation
//         → deadStoreElimination → commonSubexprElim → strengthReduction
//         → loopInvariantCodeMotion
IRProgram runOptimizationPasses(IRProgram prog);
