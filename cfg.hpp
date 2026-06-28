#pragma once
#include "ir.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
// Phi node (SSA): at a join point, merge one definition per predecessor.
// origVar   — original variable name before SSA renaming
// dest      — renamed destination (e.g. x$2)
// srcs      — (renamed_source_var, predecessor_block_id)
// ─────────────────────────────────────────────────────────────────────────────
struct PhiNode {
    std::string origVar;
    std::string dest;
    std::vector<std::pair<std::string,int>> srcs;
};

// ─────────────────────────────────────────────────────────────────────────────
// Basic block: a maximal straight-line sequence of instructions.
// ─────────────────────────────────────────────────────────────────────────────
struct BasicBlock {
    int         id    = -1;
    std::string label;              // first LABEL name in this block (may be empty)

    std::vector<PhiNode>  phis;     // phi nodes at block entry (SSA only)
    std::vector<IRInstr>  instrs;   // non-LABEL IR instructions

    std::vector<int> succs;         // successor block IDs
    std::vector<int> preds;         // predecessor block IDs

    // Liveness (filled by CFG::computeLiveness)
    std::unordered_set<std::string> use;      // vars used before any def in block
    std::unordered_set<std::string> def;      // vars defined in block
    std::unordered_set<std::string> liveIn;
    std::unordered_set<std::string> liveOut;

    // Dominator info (filled by CFG::computeDominators / computeDomFrontiers)
    int              idom = -1;               // immediate dominator (-1 for entry)
    std::vector<int> domChildren;
    std::unordered_set<int> domFrontier;

    bool isExit() const { return succs.empty(); }
};

// ─────────────────────────────────────────────────────────────────────────────
// Control Flow Graph for one function (or main).
// ─────────────────────────────────────────────────────────────────────────────
struct CFG {
    std::string funcName;
    std::vector<BasicBlock> blocks;             // blocks[0] is always the entry
    std::unordered_map<std::string,int> labelToBlock;

    // Build from a flat instruction vector.
    static CFG build(const std::string& name, const std::vector<IRInstr>& code);

    // Flatten back to a linear instruction vector for VM execution.
    std::vector<IRInstr> flatten() const;

    // Print CFG to stdout (optional debugging aid).
    void dump() const;

    // ── Analysis passes (modify blocks in-place) ──────────────────────────
    // Backward dataflow: fills use/def/liveIn/liveOut.
    void computeLiveness();

    // Cooper et al. iterative dominators: fills idom + domChildren.
    void computeDominators();

    // Cytron et al.: fills domFrontier (requires computeDominators first).
    void computeDomFrontiers();

    // Blocks in reverse post-order (entry first).
    std::vector<int> reversePostOrder() const;
};

// ─────────────────────────────────────────────────────────────────────────────
// SSA construction and destruction (operate on a CFG).
// buildSSA  : inserts phi nodes + renames all variable defs/uses.
// destroySSA: phi elimination — inserts copies in predecessors, clears phis.
// ─────────────────────────────────────────────────────────────────────────────
void buildSSA(CFG& cfg);
void destroySSA(CFG& cfg);

// ─────────────────────────────────────────────────────────────────────────────
// Liveness-based dead store elimination.
// Builds a CFG internally, runs liveness, removes DECLARE x whose value is
// never read before it's overwritten or the block exits.
// Returns an optimised flat instruction vector.
// ─────────────────────────────────────────────────────────────────────────────
std::vector<IRInstr> livenessDSE(const std::vector<IRInstr>& code);
