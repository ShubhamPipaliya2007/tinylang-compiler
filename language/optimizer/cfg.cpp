#include "cfg.hpp"
#include <algorithm>
#include <queue>
#include <set>
#include <stack>
#include <functional>
#include <iostream>
#include <cassert>

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static bool isConstPush(IROp op) {
    return op == IROp::PUSH_INT || op == IROp::PUSH_FLOAT ||
           op == IROp::PUSH_STR || op == IROp::PUSH_CHAR || op == IROp::PUSH_BOOL;
}

static bool isTerminator(IROp op) {
    return op == IROp::JUMP || op == IROp::JUMP_FALSE ||
           op == IROp::RETURN || op == IROp::RETURN_VAL;
}

static void addEdge(CFG& cfg, int from, int to) {
    for (int s : cfg.blocks[from].succs) if (s == to) return; // no duplicate edges
    cfg.blocks[from].succs.push_back(to);
    cfg.blocks[to].preds.push_back(from);
}

// ─────────────────────────────────────────────────────────────────────────────
// CFG::build — convert a flat instruction vector into a graph of basic blocks.
//
// Leader heuristic:
//   • instruction 0
//   • any LABEL instruction (jump targets always start a new block)
//   • instruction immediately after a terminator (JUMP/JUMP_FALSE/RETURN*)
// ─────────────────────────────────────────────────────────────────────────────
CFG CFG::build(const std::string& name, const std::vector<IRInstr>& code) {
    CFG cfg;
    cfg.funcName = name;
    if (code.empty()) return cfg;

    // Step 1: collect leader positions
    std::set<size_t> leaderSet;
    leaderSet.insert(0);
    for (size_t i = 0; i < code.size(); ++i) {
        IROp op = code[i].op;
        if (op == IROp::LABEL)
            leaderSet.insert(i);
        if (isTerminator(op) && i + 1 < code.size())
            leaderSet.insert(i + 1);
    }
    std::vector<size_t> leaders(leaderSet.begin(), leaderSet.end());

    // Step 2: build blocks — strip LABEL instructions into bb.label
    for (size_t li = 0; li < leaders.size(); ++li) {
        size_t start = leaders[li];
        size_t end   = (li + 1 < leaders.size()) ? leaders[li+1] : code.size();

        BasicBlock bb;
        bb.id = (int)cfg.blocks.size();

        for (size_t i = start; i < end; ++i) {
            if (code[i].op == IROp::LABEL) {
                if (bb.label.empty()) {
                    bb.label = code[i].sval;
                    cfg.labelToBlock[code[i].sval] = bb.id;
                }
                // extra LABEL in same range: just record it
                else cfg.labelToBlock[code[i].sval] = bb.id;
            } else {
                bb.instrs.push_back(code[i]);
            }
        }
        cfg.blocks.push_back(std::move(bb));
    }

    // Step 3: wire edges based on terminators
    for (int bid = 0; bid < (int)cfg.blocks.size(); ++bid) {
        const BasicBlock& bb = cfg.blocks[bid];

        if (bb.instrs.empty()) {
            // Empty block (e.g. a LABEL with no instructions before next leader)
            if (bid + 1 < (int)cfg.blocks.size()) addEdge(cfg, bid, bid + 1);
            continue;
        }

        const IRInstr& term = bb.instrs.back();
        switch (term.op) {
        case IROp::JUMP: {
            auto it = cfg.labelToBlock.find(term.sval);
            if (it != cfg.labelToBlock.end()) addEdge(cfg, bid, it->second);
            break;
        }
        case IROp::JUMP_FALSE: {
            if (bid + 1 < (int)cfg.blocks.size()) addEdge(cfg, bid, bid + 1);
            auto it = cfg.labelToBlock.find(term.sval);
            if (it != cfg.labelToBlock.end()) addEdge(cfg, bid, it->second);
            break;
        }
        case IROp::RETURN:
        case IROp::RETURN_VAL:
            break; // exit blocks have no successors
        default:
            if (bid + 1 < (int)cfg.blocks.size()) addEdge(cfg, bid, bid + 1);
            break;
        }
    }

    return cfg;
}

// ─────────────────────────────────────────────────────────────────────────────
// CFG::flatten — emit each block (label first, then instructions).
// ─────────────────────────────────────────────────────────────────────────────
std::vector<IRInstr> CFG::flatten() const {
    std::vector<IRInstr> result;
    for (const BasicBlock& bb : blocks) {
        if (!bb.label.empty())
            result.push_back({IROp::LABEL, bb.label});
        // Emit phi-elimination copies (they were appended to instrs by destroySSA)
        for (const IRInstr& ins : bb.instrs)
            result.push_back(ins);
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// CFG::dump — human-readable debug output
// ─────────────────────────────────────────────────────────────────────────────
static const char* opName(IROp op) {
    switch (op) {
    case IROp::PUSH_INT:   return "PUSH_INT";
    case IROp::PUSH_FLOAT: return "PUSH_FLOAT";
    case IROp::PUSH_STR:   return "PUSH_STR";
    case IROp::PUSH_CHAR:  return "PUSH_CHAR";
    case IROp::PUSH_BOOL:  return "PUSH_BOOL";
    case IROp::LOAD:       return "LOAD";
    case IROp::DECLARE:    return "DECLARE";
    case IROp::STORE:      return "STORE";
    case IROp::POP:        return "POP";
    case IROp::ADD:        return "ADD";
    case IROp::SUB:        return "SUB";
    case IROp::MUL:        return "MUL";
    case IROp::DIV:        return "DIV";
    case IROp::NEG:        return "NEG";
    case IROp::CMP_EQ:     return "CMP_EQ";
    case IROp::CMP_NEQ:    return "CMP_NEQ";
    case IROp::CMP_LT:     return "CMP_LT";
    case IROp::CMP_GT:     return "CMP_GT";
    case IROp::AND:        return "AND";
    case IROp::OR:         return "OR";
    case IROp::NOT:        return "NOT";
    case IROp::JUMP:       return "JUMP";
    case IROp::JUMP_FALSE: return "JUMP_FALSE";
    case IROp::LABEL:      return "LABEL";
    case IROp::CALL:       return "CALL";
    case IROp::RETURN:     return "RETURN";
    case IROp::RETURN_VAL: return "RETURN_VAL";
    case IROp::PRINT:      return "PRINT";
    case IROp::ENTER_SCOPE:return "ENTER_SCOPE";
    case IROp::EXIT_SCOPE: return "EXIT_SCOPE";
    case IROp::NOP:        return "NOP";
    default:               return "???";
    }
}

void CFG::dump() const {
    std::cout << "=== CFG: " << funcName << " ===\n";
    for (const BasicBlock& bb : blocks) {
        std::cout << "BB" << bb.id;
        if (!bb.label.empty()) std::cout << " [" << bb.label << "]";
        std::cout << "  preds:{";
        for (int p : bb.preds) std::cout << p << ",";
        std::cout << "} succs:{";
        for (int s : bb.succs) std::cout << s << ",";
        std::cout << "}\n";
        std::cout << "  liveIn: ";
        for (const auto& v : bb.liveIn) std::cout << v << " ";
        std::cout << "\n  liveOut: ";
        for (const auto& v : bb.liveOut) std::cout << v << " ";
        std::cout << "\n";
        for (const PhiNode& phi : bb.phis) {
            std::cout << "  PHI " << phi.dest << " = phi(";
            for (auto& [v,p] : phi.srcs) std::cout << v << "@BB" << p << " ";
            std::cout << ")\n";
        }
        for (const IRInstr& ins : bb.instrs) {
            std::cout << "    " << opName(ins.op);
            if (!ins.sval.empty()) std::cout << " " << ins.sval;
            else if (ins.op == IROp::PUSH_INT || ins.op == IROp::PUSH_BOOL)
                std::cout << " " << ins.ival;
            else if (ins.op == IROp::PUSH_FLOAT) std::cout << " " << ins.dval;
            else if (ins.op == IROp::PUSH_CHAR)  std::cout << " '" << ins.cval << "'";
            else if (ins.op == IROp::CALL) std::cout << " " << ins.sval << " argc=" << ins.ival;
            std::cout << "\n";
        }
        if (bb.idom >= 0) std::cout << "  idom=BB" << bb.idom << "\n";
        if (!bb.domFrontier.empty()) {
            std::cout << "  domFrontier:{";
            for (int df : bb.domFrontier) std::cout << df << ",";
            std::cout << "}\n";
        }
    }
    std::cout << "========================\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// CFG::reversePostOrder — DFS from entry, return blocks in RPO.
// ─────────────────────────────────────────────────────────────────────────────
std::vector<int> CFG::reversePostOrder() const {
    int n = blocks.size();
    std::vector<bool> visited(n, false);
    std::vector<int> post;
    post.reserve(n);

    std::function<void(int)> dfs = [&](int b) {
        visited[b] = true;
        for (int s : blocks[b].succs)
            if (!visited[s]) dfs(s);
        post.push_back(b);
    };

    if (n > 0) dfs(0);
    std::reverse(post.begin(), post.end());
    return post;
}

// ─────────────────────────────────────────────────────────────────────────────
// CFG::computeDominators — Cooper, Harvey, Kennedy (2001)
// "A Simple, Fast Dominance Algorithm"
// ─────────────────────────────────────────────────────────────────────────────
void CFG::computeDominators() {
    int n = blocks.size();
    if (n == 0) return;

    // Clear previous results
    for (BasicBlock& bb : blocks) { bb.idom = -1; bb.domChildren.clear(); }

    std::vector<int> rpo = reversePostOrder();
    std::vector<int> rpoNum(n, -1);
    for (int i = 0; i < (int)rpo.size(); ++i) rpoNum[rpo[i]] = i;

    std::vector<int> idom(n, -1);
    idom[rpo[0]] = rpo[0]; // entry dominates itself

    // Walk up the dominator tree to the LCA of a and b
    auto intersect = [&](int a, int b) -> int {
        while (a != b) {
            while (rpoNum[a] > rpoNum[b]) a = idom[a];
            while (rpoNum[b] > rpoNum[a]) b = idom[b];
        }
        return a;
    };

    bool changed = true;
    while (changed) {
        changed = false;
        // Process in RPO, skip entry (index 0)
        for (int i = 1; i < (int)rpo.size(); ++i) {
            int b = rpo[i];
            int new_idom = -1;
            for (int p : blocks[b].preds) {
                if (idom[p] == -1) continue; // predecessor not yet processed
                new_idom = (new_idom == -1) ? p : intersect(new_idom, p);
            }
            if (new_idom != -1 && idom[b] != new_idom) {
                idom[b] = new_idom;
                changed = true;
            }
        }
    }

    // Store results: entry has no immediate dominator
    blocks[rpo[0]].idom = -1;
    for (int i = 1; i < (int)rpo.size(); ++i) {
        int b = rpo[i];
        blocks[b].idom = idom[b];
        if (idom[b] >= 0) blocks[idom[b]].domChildren.push_back(b);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// CFG::computeDomFrontiers — Cytron et al.
// DF(b) = { y | ∃ pred p of y : b dom p  and  b ⋢ sdom y }
// Requires computeDominators() first.
// ─────────────────────────────────────────────────────────────────────────────
void CFG::computeDomFrontiers() {
    for (BasicBlock& bb : blocks) bb.domFrontier.clear();

    for (int y = 0; y < (int)blocks.size(); ++y) {
        if (blocks[y].preds.size() < 2) continue;
        for (int p : blocks[y].preds) {
            int runner = p;
            while (runner != blocks[y].idom && runner != -1) {
                blocks[runner].domFrontier.insert(y);
                runner = blocks[runner].idom;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// CFG::computeLiveness — backward dataflow worklist.
// Fills use/def/liveIn/liveOut for every block.
// ─────────────────────────────────────────────────────────────────────────────
void CFG::computeLiveness() {
    int n = blocks.size();

    // Compute per-block gen (use) and kill (def)
    for (BasicBlock& bb : blocks) {
        bb.use.clear(); bb.def.clear();
        for (const IRInstr& ins : bb.instrs) {
            if (ins.op == IROp::LOAD && !bb.def.count(ins.sval))
                bb.use.insert(ins.sval);
            if (ins.op == IROp::DECLARE || ins.op == IROp::STORE)
                bb.def.insert(ins.sval);
        }
    }

    // Worklist: start with all blocks (process in reverse order for efficiency)
    std::vector<bool> inQueue(n, true);
    std::queue<int> wl;
    for (int i = n - 1; i >= 0; --i) wl.push(i);

    while (!wl.empty()) {
        int b = wl.front(); wl.pop();
        inQueue[b] = false;

        BasicBlock& bb = blocks[b];

        // liveOut[b] = ∪ liveIn[s] for each successor s
        std::unordered_set<std::string> newOut;
        for (int s : bb.succs)
            for (const std::string& v : blocks[s].liveIn) newOut.insert(v);

        // liveIn[b] = use[b] ∪ (liveOut[b] − def[b])
        std::unordered_set<std::string> newIn = bb.use;
        for (const std::string& v : newOut)
            if (!bb.def.count(v)) newIn.insert(v);

        if (newIn != bb.liveIn || newOut != bb.liveOut) {
            bb.liveIn  = std::move(newIn);
            bb.liveOut = std::move(newOut);
            for (int p : bb.preds)
                if (!inQueue[p]) { inQueue[p] = true; wl.push(p); }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// buildSSA — Cytron et al. SSA construction.
//
// Step 1: find def-sites for every variable.
// Step 2: insert phi nodes at iterated dominance frontiers.
// Step 3: rename variables with version suffixes ($N).
//
// Variables are renamed: x → x$0, x$1, …
// Phi node destinations use the ORIGINAL variable name (for ease of phi
// elimination later — copies in predecessors write back to the original name).
// ─────────────────────────────────────────────────────────────────────────────
void buildSSA(CFG& cfg) {
    cfg.computeDominators();
    cfg.computeDomFrontiers();

    int n = cfg.blocks.size();

    // ── Step 1: collect def-sites ────────────────────────────────────────────
    std::unordered_map<std::string, std::unordered_set<int>> defsites;
    for (int b = 0; b < n; ++b)
        for (const IRInstr& ins : cfg.blocks[b].instrs)
            if (ins.op == IROp::DECLARE || ins.op == IROp::STORE)
                defsites[ins.sval].insert(b);

    // ── Step 2: insert phi nodes at IDF ──────────────────────────────────────
    for (auto& [var, defs] : defsites) {
        if (defs.size() < 2) continue; // single def site: no phi needed

        std::queue<int> wl;
        std::unordered_set<int> inWl, placed;
        for (int b : defs) { wl.push(b); inWl.insert(b); }

        while (!wl.empty()) {
            int b = wl.front(); wl.pop(); inWl.erase(b);
            for (int df : cfg.blocks[b].domFrontier) {
                if (placed.count(df)) continue;
                PhiNode phi;
                phi.origVar = var;
                phi.dest    = var; // renamed in step 3
                for (int p : cfg.blocks[df].preds)
                    phi.srcs.push_back({var, p}); // sources renamed in step 3
                cfg.blocks[df].phis.push_back(std::move(phi));
                placed.insert(df);
                // Phi itself is a new def-site: propagate
                if (!inWl.count(df)) { wl.push(df); inWl.insert(df); }
            }
        }
    }

    // ── Step 3: rename (DFS on dominator tree) ───────────────────────────────
    std::unordered_map<std::string, int>                  counter;
    std::unordered_map<std::string, std::vector<std::string>> stk; // var → version stack

    auto newVer = [&](const std::string& var) -> std::string {
        int v = counter[var]++;
        std::string name = var + "$" + std::to_string(v);
        stk[var].push_back(name);
        return name;
    };

    auto topVer = [&](const std::string& var) -> std::string {
        auto it = stk.find(var);
        if (it == stk.end() || it->second.empty()) return var;
        return it->second.back();
    };

    // Track what was pushed per block so we can pop on exit
    std::function<void(int)> rename = [&](int b) {
        std::vector<std::pair<std::string,int>> pushed; // (orig_var, push_count)

        // Rename phi destinations
        for (PhiNode& phi : cfg.blocks[b].phis) {
            phi.dest = newVer(phi.origVar);
            pushed.push_back({phi.origVar, 1});
        }

        // Rename instructions: uses before defs
        for (IRInstr& ins : cfg.blocks[b].instrs) {
            if (ins.op == IROp::LOAD) {
                ins.sval = topVer(ins.sval);
            } else if (ins.op == IROp::DECLARE || ins.op == IROp::STORE) {
                std::string orig = ins.sval;
                ins.sval = newVer(orig);
                pushed.push_back({orig, 1});
            }
        }

        // Fill in phi sources for successors
        for (int s : cfg.blocks[b].succs) {
            for (PhiNode& phi : cfg.blocks[s].phis) {
                for (auto& [srcVar, srcBlock] : phi.srcs) {
                    if (srcBlock == b)
                        srcVar = topVer(phi.origVar);
                }
            }
        }

        // Recurse to dominator children
        for (int child : cfg.blocks[b].domChildren) rename(child);

        // Pop all versions introduced in this block
        for (auto& [var, cnt] : pushed)
            for (int i = 0; i < cnt; ++i)
                if (!stk[var].empty()) stk[var].pop_back();
    };

    if (n > 0) rename(0);
}

// ─────────────────────────────────────────────────────────────────────────────
// destroySSA — phi elimination (Briggs et al. lost-copy strategy).
// For each phi dest = phi(src0@pred0, src1@pred1, …):
//   insert LOAD src_i; STORE dest at the end of pred_i (before terminator).
// Then clear all phi nodes.
// ─────────────────────────────────────────────────────────────────────────────
void destroySSA(CFG& cfg) {
    for (BasicBlock& bb : cfg.blocks) {
        for (const PhiNode& phi : bb.phis) {
            for (const auto& [srcVar, predId] : phi.srcs) {
                BasicBlock& pred = cfg.blocks[predId];
                size_t insertAt = pred.instrs.size();
                if (!pred.instrs.empty() && isTerminator(pred.instrs.back().op))
                    insertAt = pred.instrs.size() - 1;
                pred.instrs.insert(pred.instrs.begin() + insertAt,
                                   IRInstr{IROp::STORE, phi.dest});
                pred.instrs.insert(pred.instrs.begin() + insertAt,
                                   IRInstr{IROp::LOAD,  srcVar});
            }
        }
        bb.phis.clear();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// livenessDSE — global dead store elimination using liveness analysis.
// Eliminates DECLARE y where y is not live-out and not used later in the block,
// provided the preceding instruction is a pure value producer (constant push or
// LOAD).  The LOAD case covers copies inserted by GVN / phi elimination.
// ─────────────────────────────────────────────────────────────────────────────
std::vector<IRInstr> livenessDSE(const std::vector<IRInstr>& code) {
    if (code.empty()) return code;

    CFG cfg = CFG::build("__ldse", code);
    cfg.computeLiveness();

    for (BasicBlock& bb : cfg.blocks) {
        size_t m = bb.instrs.size();
        std::vector<bool> dead(m, false);

        for (size_t i = 0; i < m; ++i) {
            if (bb.instrs[i].op != IROp::DECLARE) continue;
            if (i == 0) continue;
            // The preceding instruction must be a pure value producer with no
            // observable effects of its own.
            IROp prevOp = bb.instrs[i-1].op;
            bool pureProducer = isConstPush(prevOp) || prevOp == IROp::LOAD;
            if (!pureProducer || dead[i-1]) continue;

            const std::string& var = bb.instrs[i].sval;

            // Is var read after this point in the block?
            bool usedLater = false;
            for (size_t j = i + 1; j < m; ++j) {
                IROp op = bb.instrs[j].op;
                if (op == IROp::LOAD && bb.instrs[j].sval == var)
                    { usedLater = true; break; }
                if ((op == IROp::DECLARE || op == IROp::STORE) && bb.instrs[j].sval == var)
                    break; // overwritten before being read
            }

            if (!usedLater && !bb.liveOut.count(var)) {
                dead[i]   = true;
                dead[i-1] = true; // drop the producer too
            }
        }

        std::vector<IRInstr> kept;
        kept.reserve(m);
        for (size_t i = 0; i < m; ++i)
            if (!dead[i]) kept.push_back(bb.instrs[i]);
        bb.instrs = std::move(kept);
    }

    return cfg.flatten();
}
