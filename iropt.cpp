#include "iropt.hpp"
#include "cfg.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Predicate helpers
// ─────────────────────────────────────────────────────────────────────────────

static bool isConstPush(IROp op) {
    return op == IROp::PUSH_INT || op == IROp::PUSH_FLOAT ||
           op == IROp::PUSH_STR || op == IROp::PUSH_CHAR || op == IROp::PUSH_BOOL;
}

static bool isBinOp(IROp op) {
    return op == IROp::ADD || op == IROp::SUB || op == IROp::MUL || op == IROp::DIV;
}

static bool hasSideEffect(IROp op) {
    return op == IROp::CALL || op == IROp::CALL_METHOD || op == IROp::CALL_SUPER ||
           op == IROp::PRINT || op == IROp::INPUT || op == IROp::READ_FILE ||
           op == IROp::NEW_OBJ || op == IROp::NEW_ARRAY;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pass 1 — Constant Propagation
// Track variables assigned constant values; replace LOAD of those variables
// with the constant push instruction directly.
// ─────────────────────────────────────────────────────────────────────────────
static std::vector<IRInstr> constantPropagation(const std::vector<IRInstr>& code) {
    std::unordered_map<std::string, IRInstr> constMap; // var → constant push instr
    std::vector<IRInstr> result;
    result.reserve(code.size());

    for (const IRInstr& ins : code) {
        switch (ins.op) {
        case IROp::LOAD: {
            auto it = constMap.find(ins.sval);
            result.push_back(it != constMap.end() ? it->second : ins);
            break;
        }
        case IROp::DECLARE:
        case IROp::STORE:
            if (!result.empty() && isConstPush(result.back().op))
                constMap[ins.sval] = result.back();
            else
                constMap.erase(ins.sval);
            result.push_back(ins);
            break;
        case IROp::LABEL:
        case IROp::EXIT_SCOPE:
            constMap.clear();
            result.push_back(ins);
            break;
        default:
            if (hasSideEffect(ins.op)) constMap.clear();
            result.push_back(ins);
            break;
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pass 2 — Dead Code Elimination
// Remove instructions in unreachable basic blocks (code after unconditional
// JUMP / RETURN / RETURN_VAL until the next LABEL).
// Also fold constant-condition JUMP_FALSE: if the condition is a known
// int/bool constant, replace with an unconditional JUMP or drop entirely.
// ─────────────────────────────────────────────────────────────────────────────
static std::vector<IRInstr> deadCodeElimination(const std::vector<IRInstr>& code) {
    std::vector<IRInstr> result;
    result.reserve(code.size());
    bool unreachable = false;

    for (const IRInstr& ins : code) {
        if (ins.op == IROp::LABEL) {
            unreachable = false;
            result.push_back(ins);
            continue;
        }
        if (unreachable) continue;

        // Constant-condition JUMP_FALSE → unconditional JUMP or dead branch
        if (ins.op == IROp::JUMP_FALSE && !result.empty() &&
            (result.back().op == IROp::PUSH_INT || result.back().op == IROp::PUSH_BOOL)) {
            int val = result.back().ival;
            result.pop_back(); // remove the constant push
            if (val == 0) {
                result.push_back({IROp::JUMP, ins.sval}); // always jumps
                unreachable = true;
            }
            // else always falls through → drop the JUMP_FALSE
            continue;
        }

        result.push_back(ins);
        if (ins.op == IROp::JUMP || ins.op == IROp::RETURN || ins.op == IROp::RETURN_VAL)
            unreachable = true;
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pass 3 — Copy Propagation
// When a = b (LOAD b; DECLARE/STORE a), replace subsequent LOAD a with LOAD b
// as long as neither a nor b has been reassigned.
// ─────────────────────────────────────────────────────────────────────────────
static std::vector<IRInstr> copyPropagation(const std::vector<IRInstr>& code) {
    std::unordered_map<std::string, std::string> copyOf; // a → b (a equals b)
    std::vector<IRInstr> result;
    result.reserve(code.size());

    for (const IRInstr& ins : code) {
        switch (ins.op) {
        case IROp::LOAD: {
            auto it = copyOf.find(ins.sval);
            result.push_back(it != copyOf.end() ? IRInstr{IROp::LOAD, it->second} : ins);
            break;
        }
        case IROp::DECLARE:
        case IROp::STORE:
            if (!result.empty() && result.back().op == IROp::LOAD) {
                copyOf[ins.sval] = result.back().sval;
            } else {
                copyOf.erase(ins.sval);
            }
            // Invalidate copies that sourced from the variable now being overwritten
            for (auto it = copyOf.begin(); it != copyOf.end(); ) {
                if (it->second == ins.sval && it->first != ins.sval)
                    it = copyOf.erase(it);
                else
                    ++it;
            }
            result.push_back(ins);
            break;
        case IROp::LABEL:
        case IROp::EXIT_SCOPE:
            copyOf.clear();
            result.push_back(ins);
            break;
        default:
            if (hasSideEffect(ins.op)) copyOf.clear();
            result.push_back(ins);
            break;
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pass 4 — Dead Store Elimination
// If a DECLARE/STORE x is followed (within the same basic block) by another
// DECLARE/STORE x with no intervening LOAD x, the first store is dead.
// Only eliminates pairs where the preceding push is a simple constant push,
// so we can safely remove both the push and the declare/store together.
// ─────────────────────────────────────────────────────────────────────────────
static std::vector<IRInstr> deadStoreElimination(const std::vector<IRInstr>& code) {
    size_t n = code.size();
    std::vector<bool> dead(n, false);

    for (size_t i = 0; i < n; ++i) {
        if (code[i].op != IROp::DECLARE && code[i].op != IROp::STORE) continue;
        if (i == 0 || !isConstPush(code[i-1].op) || dead[i-1]) continue;

        std::string var = code[i].sval;
        for (size_t j = i + 1; j < n; ++j) {
            IROp op = code[j].op;
            // Don't cross control flow or side effects
            if (op == IROp::LABEL || op == IROp::JUMP || op == IROp::JUMP_FALSE) break;
            if (hasSideEffect(op)) break;
            if (op == IROp::LOAD && code[j].sval == var) break; // var is read → not dead
            if ((op == IROp::DECLARE || op == IROp::STORE) && code[j].sval == var) {
                dead[i]   = true; // store is dead
                dead[i-1] = true; // preceding constant push is dead
                break;
            }
        }
    }

    std::vector<IRInstr> result;
    result.reserve(n);
    for (size_t i = 0; i < n; ++i)
        if (!dead[i]) result.push_back(code[i]);
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pass 5 — Common Subexpression Elimination
// Detect repeated (LOAD a; LOAD b; BINOP) triples within the same basic block.
// On the first occurrence, save the result to a temp variable and reload it.
// On subsequent occurrences, load the temp directly instead of recomputing.
// Only applies to expressions that appear more than once in the same block.
// ─────────────────────────────────────────────────────────────────────────────
static std::vector<IRInstr> commonSubexprElim(const std::vector<IRInstr>& code) {
    // Pre-scan: count per-block occurrences of each LOAD-LOAD-BINOP triple.
    std::unordered_map<std::string, int> blockExprCount;
    {
        int bb = 0;
        for (size_t i = 0; i + 2 < code.size(); ++i) {
            IROp op = code[i].op;
            if (op == IROp::LABEL || op == IROp::JUMP || op == IROp::JUMP_FALSE
                || hasSideEffect(op)) { ++bb; continue; }
            if (op == IROp::LOAD && code[i+1].op == IROp::LOAD && isBinOp(code[i+2].op)) {
                std::string key = std::to_string(bb) + "|"
                                  + std::to_string((int)code[i+2].op)
                                  + "|" + code[i].sval + "|" + code[i+1].sval;
                ++blockExprCount[key];
            }
        }
    }

    // Main pass: transform multi-occurrence expressions.
    std::unordered_map<std::string, std::string> exprToTemp;
    std::unordered_map<std::string, std::pair<std::string,std::string>> tempDeps;
    int cseCount = 0, bb = 0;
    std::vector<IRInstr> result;
    result.reserve(code.size() + 16);

    size_t i = 0;
    while (i < code.size()) {
        const IRInstr& ins = code[i];

        if (ins.op == IROp::LABEL || ins.op == IROp::JUMP || ins.op == IROp::JUMP_FALSE
            || hasSideEffect(ins.op)) {
            ++bb;
            exprToTemp.clear();
            tempDeps.clear();
            result.push_back(ins);
            ++i; continue;
        }

        if (ins.op == IROp::EXIT_SCOPE) {
            exprToTemp.clear();
            tempDeps.clear();
            result.push_back(ins);
            ++i; continue;
        }

        // Targeted invalidation: if a variable is overwritten, remove CSE entries
        // that depend on it as an operand.
        if (ins.op == IROp::STORE || ins.op == IROp::DECLARE) {
            const std::string& mod = ins.sval;
            for (auto it = exprToTemp.begin(); it != exprToTemp.end(); ) {
                auto& deps = tempDeps[it->second];
                if (deps.first == mod || deps.second == mod) it = exprToTemp.erase(it);
                else ++it;
            }
        }

        if (i + 2 < code.size() &&
            ins.op == IROp::LOAD && code[i+1].op == IROp::LOAD && isBinOp(code[i+2].op)) {
            const std::string& a = ins.sval;
            const std::string& b = code[i+1].sval;
            std::string key = std::to_string(bb) + "|"
                              + std::to_string((int)code[i+2].op) + "|" + a + "|" + b;

            if (blockExprCount[key] > 1) {
                auto it = exprToTemp.find(key);
                if (it != exprToTemp.end()) {
                    result.push_back({IROp::LOAD, it->second}); // reuse cached result
                } else {
                    // Compute, stash in temp, reload — preserves stack neutrality
                    std::string tmp = "__cse_" + std::to_string(cseCount++);
                    result.push_back(code[i]);
                    result.push_back(code[i+1]);
                    result.push_back(code[i+2]);
                    result.push_back({IROp::DECLARE, tmp});
                    result.push_back({IROp::LOAD, tmp});
                    exprToTemp[key] = tmp;
                    tempDeps[tmp]   = {a, b};
                }
                i += 3; continue;
            }
        }

        result.push_back(ins);
        ++i;
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pass 6 — Strength Reduction
// Replace expensive operations with cheaper equivalents:
//   x * 1  →  x         (remove PUSH_INT 1 + MUL)
//   x / 1  →  x         (remove PUSH_INT 1 + DIV)
//   x + 0  →  x         (remove PUSH_INT 0 + ADD)
//   x - 0  →  x         (remove PUSH_INT 0 + SUB)
//   x * 0  →  0         (POP x, push 0)
// ─────────────────────────────────────────────────────────────────────────────
static std::vector<IRInstr> strengthReduction(const std::vector<IRInstr>& code) {
    std::vector<IRInstr> result;
    result.reserve(code.size());

    size_t i = 0;
    while (i < code.size()) {
        if (i + 1 < code.size()) {
            const IRInstr& cur  = code[i];
            const IRInstr& next = code[i+1];

            if (cur.op == IROp::PUSH_INT) {
                if (cur.ival == 1 && next.op == IROp::MUL) { i += 2; continue; }
                if (cur.ival == 1 && next.op == IROp::DIV) { i += 2; continue; }
                if (cur.ival == 0 && next.op == IROp::ADD) { i += 2; continue; }
                if (cur.ival == 0 && next.op == IROp::SUB) { i += 2; continue; }
                if (cur.ival == 0 && next.op == IROp::MUL) {
                    result.push_back({IROp::POP});
                    result.push_back({IROp::PUSH_INT, "", 0});
                    i += 2; continue;
                }
            }
        }
        result.push_back(code[i++]);
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pass 7 — Loop-Invariant Code Motion
// Identify LABEL L … JUMP L back-edges (loops).
// Within the loop body, find 4-instruction sequences:
//   LOAD a; LOAD b; BINOP; DECLARE x
// where neither a nor b is written in the loop, and x is declared exactly once.
// Hoist those four instructions to just before the LABEL.
// ─────────────────────────────────────────────────────────────────────────────
static std::vector<IRInstr> loopInvariantCodeMotion(const std::vector<IRInstr>& code) {
    std::vector<IRInstr> result(code.begin(), code.end());
    bool changed = true;

    while (changed) {
        changed = false;
        size_t n = result.size();

        for (size_t ls = 0; ls < n && !changed; ++ls) {
            if (result[ls].op != IROp::LABEL) continue;
            const std::string& loopLabel = result[ls].sval;

            // Find the back-edge JUMP to this label
            size_t lj = n;
            for (size_t j = ls + 1; j < n; ++j) {
                if (result[j].op == IROp::JUMP && result[j].sval == loopLabel) { lj = j; break; }
            }
            if (lj == n) continue;

            // Collect all variables written in the loop body [ls+1, lj-1]
            std::unordered_set<std::string> written;
            for (size_t j = ls + 1; j < lj; ++j)
                if (result[j].op == IROp::STORE || result[j].op == IROp::DECLARE)
                    written.insert(result[j].sval);

            // Look for hoistable 4-instruction sequence
            for (size_t j = ls + 1; j + 3 < lj && !changed; ++j) {
                if (result[j].op   != IROp::LOAD)    continue;
                if (result[j+1].op != IROp::LOAD)    continue;
                if (!isBinOp(result[j+2].op))        continue;
                if (result[j+3].op != IROp::DECLARE) continue;

                const std::string& a = result[j].sval;
                const std::string& b = result[j+1].sval;
                const std::string& x = result[j+3].sval;

                if (written.count(a) || written.count(b)) continue;

                // x must be declared/stored exactly once in the loop
                int xWrites = 0;
                for (size_t k = ls + 1; k < lj; ++k)
                    if ((result[k].op == IROp::DECLARE || result[k].op == IROp::STORE)
                        && result[k].sval == x)
                        ++xWrites;
                if (xWrites != 1) continue;

                // Hoist: extract 4 instructions then insert before the LABEL
                std::vector<IRInstr> hoisted = { result[j], result[j+1], result[j+2], result[j+3] };
                result.erase(result.begin() + j, result.begin() + j + 4);
                result.insert(result.begin() + ls, hoisted.begin(), hoisted.end());
                changed = true;
            }
        }
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Pass 8 — SSA-based Global Value Numbering
//
// Renames variables into SSA form (so every definition is unique), then walks
// the dominator tree top-down.  Each node inherits the expression table of its
// immediate dominator: if the same LOAD/PUSH … BINOP … DECLARE pattern was
// already computed in a dominating block, it replaces the recomputation with a
// simple LOAD of the earlier result.  Because SSA guarantees that a variable
// defined in block A has the same value everywhere A dominates, no invalidation
// is needed — a stale entry is structurally impossible.
//
// After GVN the CFG is taken out of SSA form (phi elimination inserts LOAD/STORE
// copies), and copy propagation + DSE clean up any leftover identity copies.
// ─────────────────────────────────────────────────────────────────────────────

static void runGVNOnCFG(CFG& cfg) {
    using ExprTable = std::unordered_map<std::string, std::string>;

    auto getVN = [](const IRInstr& ins) -> std::string {
        switch (ins.op) {
        case IROp::LOAD:       return "V:" + ins.sval;
        case IROp::PUSH_INT:   return "I:" + std::to_string(ins.ival);
        case IROp::PUSH_FLOAT: return "F:" + std::to_string(ins.dval);
        case IROp::PUSH_STR:   return "S:" + ins.sval;
        case IROp::PUSH_BOOL:  return "B:" + std::to_string(ins.ival);
        case IROp::PUSH_CHAR:  return "C:" + std::string(1, ins.cval);
        default:               return "";
        }
    };

    // DFS on the dominator tree; each child inherits the parent's table by value
    // so sibling subtrees can't pollute each other.
    std::function<void(int, ExprTable)> visit = [&](int bid, ExprTable table) {
        BasicBlock& bb = cfg.blocks[bid];
        std::vector<IRInstr> result;
        result.reserve(bb.instrs.size());

        size_t i = 0;
        while (i < bb.instrs.size()) {
            // Pattern: [LOAD|PUSH_*] [LOAD|PUSH_*] BINOP DECLARE dest
            if (i + 4 <= bb.instrs.size()) {
                std::string vn0 = getVN(bb.instrs[i]);
                std::string vn1 = getVN(bb.instrs[i+1]);
                if (!vn0.empty() && !vn1.empty() &&
                    isBinOp(bb.instrs[i+2].op) &&
                    bb.instrs[i+3].op == IROp::DECLARE)
                {
                    const std::string& dest = bb.instrs[i+3].sval;
                    std::string key = std::to_string((int)bb.instrs[i+2].op)
                                    + "|" + vn0 + "|" + vn1;
                    auto it = table.find(key);
                    if (it != table.end()) {
                        // Redundant: replace with copy of the canonical result
                        result.push_back({IROp::LOAD,    it->second});
                        result.push_back({IROp::DECLARE, dest});
                    } else {
                        table[key] = dest;
                        result.push_back(bb.instrs[i]);
                        result.push_back(bb.instrs[i+1]);
                        result.push_back(bb.instrs[i+2]);
                        result.push_back(bb.instrs[i+3]);
                    }
                    i += 4;
                    continue;
                }
            }
            result.push_back(bb.instrs[i]);
            ++i;
        }
        bb.instrs = std::move(result);

        for (int child : bb.domChildren)
            visit(child, table); // child gets parent's accumulated table (by value)
    };

    if (!cfg.blocks.empty()) visit(0, {});
}

static std::vector<IRInstr> gvnPass(const std::vector<IRInstr>& code) {
    if (code.size() < 4) return code;

    CFG cfg = CFG::build("__gvn", code);
    if (cfg.blocks.size() < 2) return code; // single-block: flat CSE already covers it

    buildSSA(cfg);       // rename into SSA form; builds dominator tree internally
    runGVNOnCFG(cfg);    // eliminate cross-block redundancies
    destroySSA(cfg);     // phi elimination (LOAD/STORE copies in predecessors)

    auto result = cfg.flatten();
    result = copyPropagation(result);    // collapse identity copies from phi elim
    result = deadStoreElimination(result); // remove any newly dead stores
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Apply a pass to main code and every function in the program
// ─────────────────────────────────────────────────────────────────────────────

using PassFn = std::vector<IRInstr>(*)(const std::vector<IRInstr>&);

static void applyPass(IRProgram& prog, PassFn pass) {
    prog.main = pass(prog.main);
    for (auto& [key, fn] : prog.functions)
        fn.code = pass(fn.code);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public entry point — runs all 7 passes in order
// ─────────────────────────────────────────────────────────────────────────────

IRProgram runOptimizationPasses(IRProgram prog) {
    // ── Flat-list passes (Phase 3A) ──────────────────────────────────────────
    applyPass(prog, constantPropagation);
    applyPass(prog, deadCodeElimination);
    applyPass(prog, copyPropagation);
    applyPass(prog, deadStoreElimination);
    applyPass(prog, commonSubexprElim);
    applyPass(prog, strengthReduction);
    applyPass(prog, loopInvariantCodeMotion);

    // ── CFG-based passes (Phase 3B / 3C) ────────────────────────────────────
    // Global Value Numbering via SSA: eliminates cross-block redundant exprs.
    applyPass(prog, gvnPass);

    // Liveness-based dead store elimination (global, cross-block).
    applyPass(prog, livenessDSE);

    // Second round of flat passes to clean up copies inserted by livenessDSE
    // and any opportunities exposed by the global analysis.
    applyPass(prog, copyPropagation);
    applyPass(prog, deadStoreElimination);

    return prog;
}
