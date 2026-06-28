#include "bytecode.hpp"
#include <fstream>
#include <unordered_map>
#include <stdexcept>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// File format: TinyLang Bytecode (.tlc)
//
//   [4] magic  = 0x544C4243  ("TLBC")
//   [2] version = 1
//   [string pool]
//   [class table]
//   [function table]
//   [main instructions]
//
// All multi-byte integers are little-endian.
// Each IRInstr on disk:
//   [1] opcode   [4] sval_pool_idx  [4] ival  [8] dval  [1] cval  = 18 bytes
// ─────────────────────────────────────────────────────────────────────────────

static constexpr uint32_t MAGIC   = 0x544C4243u; // "TLBC"
static constexpr uint16_t VERSION = 1;
static constexpr uint32_t NO_STR  = 0xFFFFFFFFu; // sentinel for empty sval

// ── low-level I/O ─────────────────────────────────────────────────────────────

static void wu8 (std::ofstream& f, uint8_t  v) { f.write(reinterpret_cast<char*>(&v), 1); }
static void wu32(std::ofstream& f, uint32_t v) { f.write(reinterpret_cast<char*>(&v), 4); }
static void wu16(std::ofstream& f, uint16_t v) { f.write(reinterpret_cast<char*>(&v), 2); }
static void wf64(std::ofstream& f, double   v) { f.write(reinterpret_cast<char*>(&v), 8); }
static void wstr(std::ofstream& f, const std::string& s) {
    wu32(f, (uint32_t)s.size());
    f.write(s.data(), (std::streamsize)s.size());
}

static uint8_t  ru8 (std::ifstream& f) { uint8_t  v; f.read(reinterpret_cast<char*>(&v), 1); return v; }
static uint32_t ru32(std::ifstream& f) { uint32_t v; f.read(reinterpret_cast<char*>(&v), 4); return v; }
static uint16_t ru16(std::ifstream& f) { uint16_t v; f.read(reinterpret_cast<char*>(&v), 2); return v; }
static double   rf64(std::ifstream& f) { double   v; f.read(reinterpret_cast<char*>(&v), 8); return v; }
static std::string rstr(std::ifstream& f) {
    uint32_t len = ru32(f);
    std::string s(len, '\0');
    if (len > 0) f.read(s.data(), len);
    return s;
}

// ── String pool (constant pool for all string operands) ───────────────────────

struct StringPool {
    std::vector<std::string>                    strs;
    std::unordered_map<std::string, uint32_t>   idx;

    uint32_t intern(const std::string& s) {
        if (s.empty()) return NO_STR;
        auto it = idx.find(s);
        if (it != idx.end()) return it->second;
        uint32_t i = (uint32_t)strs.size();
        strs.push_back(s);
        idx[s] = i;
        return i;
    }
};

// ── Instruction encode / decode ───────────────────────────────────────────────

static void writeInstr(std::ofstream& f, const IRInstr& ins, StringPool& pool) {
    wu8 (f, (uint8_t)ins.op);
    wu32(f, pool.intern(ins.sval));
    wu32(f, (uint32_t)(int32_t)ins.ival);
    wf64(f, ins.dval);
    wu8 (f, (uint8_t)ins.cval);
}

static IRInstr readInstr(std::ifstream& f, const std::vector<std::string>& pool) {
    IRInstr ins;
    ins.op   = (IROp)ru8(f);
    uint32_t si = ru32(f);
    ins.sval = (si == NO_STR || si >= pool.size()) ? "" : pool[si];
    ins.ival = (int)(int32_t)ru32(f);
    ins.dval = rf64(f);
    ins.cval = (char)ru8(f);
    return ins;
}

static void writeCode(std::ofstream& f, const std::vector<IRInstr>& code, StringPool& pool) {
    wu32(f, (uint32_t)code.size());
    for (const IRInstr& ins : code) writeInstr(f, ins, pool);
}

static std::vector<IRInstr> readCode(std::ifstream& f, const std::vector<std::string>& pool) {
    uint32_t n = ru32(f);
    std::vector<IRInstr> code;
    code.reserve(n);
    for (uint32_t i = 0; i < n; ++i) code.push_back(readInstr(f, pool));
    return code;
}

// ── Public: write ─────────────────────────────────────────────────────────────

bool writeBytecode(const IRProgram& prog, const std::string& filename) {
    std::ofstream f(filename, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return false;

    // Build string pool first (two-pass: scan then write)
    StringPool pool;
    auto scanCode = [&](const std::vector<IRInstr>& code) {
        for (const IRInstr& ins : code) pool.intern(ins.sval);
    };
    for (auto& [key, fn] : prog.functions) {
        pool.intern(fn.name); pool.intern(fn.className);
        for (auto& [t, n] : fn.params) { pool.intern(t); pool.intern(n); }
        scanCode(fn.code);
    }
    for (auto& [key, cls] : prog.classes) {
        pool.intern(cls.name); pool.intern(cls.baseClass);
        for (auto& [t, n] : cls.fields) { pool.intern(t); pool.intern(n); }
    }
    scanCode(prog.main);

    // Header
    wu32(f, MAGIC);
    wu16(f, VERSION);

    // String pool
    wu32(f, (uint32_t)pool.strs.size());
    for (const std::string& s : pool.strs) wstr(f, s);

    // Class table
    wu32(f, (uint32_t)prog.classes.size());
    for (auto& [key, cls] : prog.classes) {
        wu32(f, pool.intern(cls.name));
        wu32(f, pool.intern(cls.baseClass));
        wu32(f, (uint32_t)cls.fields.size());
        for (auto& [t, n] : cls.fields) { wu32(f, pool.intern(t)); wu32(f, pool.intern(n)); }
    }

    // Function table
    wu32(f, (uint32_t)prog.functions.size());
    for (auto& [key, fn] : prog.functions) {
        wu32(f, pool.intern(fn.name));
        wu32(f, pool.intern(fn.className));
        wu32(f, (uint32_t)fn.params.size());
        for (auto& [t, n] : fn.params) { wu32(f, pool.intern(t)); wu32(f, pool.intern(n)); }
        writeCode(f, fn.code, pool);
    }

    // Main
    writeCode(f, prog.main, pool);

    return true;
}

// ── Public: read ──────────────────────────────────────────────────────────────

IRProgram readBytecode(const std::string& filename) {
    std::ifstream f(filename, std::ios::binary);
    if (!f.is_open()) throw std::runtime_error("Cannot open: " + filename);

    // Header
    if (ru32(f) != MAGIC)   throw std::runtime_error("Not a .tlc file: bad magic");
    if (ru16(f) != VERSION) throw std::runtime_error("Unsupported .tlc version");

    // String pool
    uint32_t poolSize = ru32(f);
    std::vector<std::string> pool(poolSize);
    for (uint32_t i = 0; i < poolSize; ++i) pool[i] = rstr(f);

    auto ps = [&](uint32_t idx) -> std::string {
        return (idx == NO_STR || idx >= pool.size()) ? "" : pool[idx];
    };

    IRProgram prog;

    // Classes
    uint32_t nc = ru32(f);
    for (uint32_t i = 0; i < nc; ++i) {
        IRClass cls;
        cls.name      = ps(ru32(f));
        cls.baseClass = ps(ru32(f));
        uint32_t nf = ru32(f);
        for (uint32_t j = 0; j < nf; ++j)
            cls.fields.push_back({ps(ru32(f)), ps(ru32(f))});
        prog.classes[cls.name] = std::move(cls);
    }

    // Functions
    uint32_t nfn = ru32(f);
    for (uint32_t i = 0; i < nfn; ++i) {
        IRFunction fn;
        fn.name      = ps(ru32(f));
        fn.className = ps(ru32(f));
        uint32_t np = ru32(f);
        for (uint32_t j = 0; j < np; ++j)
            fn.params.push_back({ps(ru32(f)), ps(ru32(f))});
        fn.code = readCode(f, pool);
        std::string key = fn.className.empty() ? fn.name : fn.className + "::" + fn.name;
        prog.functions[key] = std::move(fn);
    }

    // Main
    prog.main = readCode(f, pool);

    return prog;
}
