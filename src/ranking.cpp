// ============================================================
// Persistent agent Elo ranking (rank.exe) -- see ranking.h
// ============================================================
// Sections: SMALL UTILITIES, ID CODEC, ROSTER, MATCH STORE, SCHEDULER,
// BRADLEY-TERRY FIT, GAME RUNNER, RATE + REPORTS, HISTORY, GAUNTLET, EXTRACT, CHECK.
//
// Deliberately independent of ml_train.cpp (whose helpers are static): the tiny
// utilities it shares (ensureDir, fnv1a64, json extractors, registry lookups)
// are replicated here as statics so rank.exe never links the trainer.

// Windows headers MUST precede the project headers: globals.h does `#define SIZE 8`,
// which would otherwise mangle wingdi.h's `SIZE` struct (same pattern as ml_train.cpp).
// windows.h supplies GetProcessTimes for the per-move CPU-time measurement.
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "ranking.h"
#include "explorers.h"
#include "choosers.h"
#include "ai_eval.h"
#include "ai_random.h"
#include "ml_eval.h"
#include "datastore.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <istream>
#include <map>
#include <set>
#include <sstream>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

// ============================================================
// SMALL UTILITIES (replicated from ml_train.cpp, which keeps them static)
// ============================================================
static void ensureDir(const string& p) {
#ifdef _WIN32
    _mkdir(p.c_str());
#else
    mkdir(p.c_str(), 0755);
#endif
}

static unsigned long long fnv1a64(const char* p, size_t n, unsigned long long h) {
    for (size_t i = 0; i < n; i++) { h ^= (unsigned char)p[i]; h *= 1099511628211ULL; }
    return h;
}

static string utcTime(const char* fmt) {
    time_t t = time(nullptr);
    struct tm g;
#ifdef _WIN32
    gmtime_s(&g, &t);
#else
    gmtime_r(&t, &g);
#endif
    char buf[64];
    strftime(buf, sizeof(buf), fmt, &g);
    return string(buf);
}
static string nowUtc()   { return utcTime("%Y-%m-%dT%H:%M:%SZ"); }
static string runStamp() { return utcTime("%Y%m%dT%H%M%SZ"); }

// Minimal JSONL field extractors (our writer emits flat, simple objects).
static bool jsonStr(const string& line, const string& key, string& out) {
    size_t k = line.find("\"" + key + "\":\"");
    if (k == string::npos) return false;
    size_t s = k + key.size() + 4;
    size_t e = line.find('"', s);
    if (e == string::npos) return false;
    out = line.substr(s, e - s);
    return true;
}
static bool jsonNum(const string& line, const string& key, double& out) {
    size_t k = line.find("\"" + key + "\":");
    if (k == string::npos) return false;
    size_t s = k + key.size() + 3;
    if (s < line.size() && line[s] == '"') return false;   // it's a string field
    try { out = std::stod(line.substr(s)); } catch (...) { return false; }
    return true;
}

// Registry lookups by name (-1 = missing, so the codec can fail loudly).
static int explorerIndexByName(const char* n) {
    for (int i = 0; i < g_explorerCount; i++) if (string(g_explorers[i].name) == n) return i;
    return -1;
}
static int chooserIndexByName(const char* n) {
    for (int i = 0; i < g_chooserCount; i++) if (string(g_choosers[i].name) == n) return i;
    return -1;
}
static int evaluatorIndexByName(const char* n) {
    for (int i = 0; i < g_evalCount; i++) if (string(g_evaluators[i].name) == n) return i;
    return -1;
}

// Victor code -> 1 (White won), 2 (Black won), 0 (ongoing / draw).
static int gameOutcome(int victor) {
    if (victor >= WhiteWin) return 1;
    if (victor <= BlackWin) return 2;
    return 0;
}

// The model file behind a slot. Slots 0/1/2 are the project's fixed, named
// conventions; slots 3.. are a generic sweep/experiment convention so many
// independently-trained candidates can each get a permanent identity (a slot +
// file) and be rated together in one process, instead of one shared file being
// swapped serially between gauntlet calls.
static string slotFile(int slot) {
    if (slot == 0) return "models/lin_value.txt";
    if (slot == 1) return "models/lin_policy.txt";
    if (slot == 2) return "models/pst_value.txt";   // sparse piece-square value model (feature v2, incremental)
    if (slot >= 3 && slot < ML_SLOTS) return "models/sweep/slot" + std::to_string(slot) + ".txt";
    return "";
}

static string trimWs(const string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
    while (b > a && (s[b-1] == ' ' || s[b-1] == '\t')) b--;
    return s.substr(a, b - a);
}

static string fmtN(double v, int decimals) {
    char b[64];
    snprintf(b, sizeof(b), "%.*f", decimals, v);
    return string(b);
}

// ============================================================
// ID CODEC
// ============================================================
// Hand-maintained codec tables mapping registry names to ID tokens. Weight
// letters are explicit (not derived from first letters) so collisions are a
// deliberate choice; rankEvalCodecComplete() asserts coverage + uniqueness.
//
// MODULE VERSIONS: each row carries the current code version of that module,
// emitted as "@V" on its ID segment. Bump a row's version whenever that
// module's CODE changes behavior (e.g. the alpha-beta search is improved ->
// bump the "ab" row): every agent using the module then gets a new canonical
// ID, so a fresh history accumulates while other modules' agents keep theirs.
// A stale "@N" in a roster fails the canonical check and prints the fix.
struct RankNameCodec { const char* regName; const char* idName; int version; };
static const RankNameCodec g_rkChoosers[] = {
    { "UniformRandom", "rand",   1 },
    { "TieredRandom",  "tiered", 1 },
    { "SmartRandom",   "smart",  1 },
    { "LearnedPolicy", "policy", 1 },
};
static const int g_rkChooserCount = sizeof(g_rkChoosers) / sizeof(g_rkChoosers[0]);
static const RankNameCodec g_rkExplorers[] = {
    { "Greedy",    "greedy", 1 },
    { "AlphaBeta", "ab",     1 },
};
static const int g_rkExplorerCount = sizeof(g_rkExplorers) / sizeof(g_rkExplorers[0]);

struct RankEvalCodec { const char* regName; const char* idName; const char* letters; int version; };
static const RankEvalCodec g_rkEvals[] = {
    { "Classic",      "classic", "tcwl",  2 },   // turn, chip, wall, column (@2: neighbor-local structure delta)
    { "Experimental", "exp",     "tcwlf", 2 },   // + forward (@2: neighbor-local structure delta)
    { "LearnedValue", "learned", "",      1 },   // special arg form: s<slot>,<hash8>
    // Advanced: + support, center, mobility, hole, control(b), open, race,
    // overext(x), noise, noiseseed(s), racewin(g). See src/ai_eval.cpp ADV_*.
    { "Advanced",     "adv",     "tcwlfdemhborxnsg", 1 },
};
static const int g_rkEvalCount = sizeof(g_rkEvals) / sizeof(g_rkEvals[0]);
// The dilution wrapper is a module too (agentChooseMove's random-move coin).
static const int RK_DIL_VERSION = 1;
// The identity-level random opener is a module too (playOneGame/playoutCapture's
// per-agent ply-count check; see AgentSpec::openerPlies in src/agents.h).
static const int RK_OPENER_VERSION = 1;
// The linpol payload carries NO version: its model-content hash is its identity.

static const RankEvalCodec* evalCodecByRegName(const char* regName) {
    for (int i = 0; i < g_rkEvalCount; i++)
        if (string(g_rkEvals[i].regName) == regName) return &g_rkEvals[i];
    return nullptr;
}
static const RankEvalCodec* evalCodecByIdName(const string& idName) {
    for (int i = 0; i < g_rkEvalCount; i++)
        if (idName == g_rkEvals[i].idName) return &g_rkEvals[i];
    return nullptr;
}

bool rankEvalCodecComplete(string& err) {
    for (int i = 0; i < g_evalCount; i++) {
        const RankEvalCodec* row = evalCodecByRegName(g_evaluators[i].name);
        if (!row) {
            err = string("evaluator '") + g_evaluators[i].name
                + "' has no ID codec row (add one to g_rkEvals in src/ranking.cpp)";
            return false;
        }
        if (row->letters[0] == '\0') continue;   // special arg form (learned)
        int pc = g_evaluators[i].paramCount;
        if ((int)strlen(row->letters) != pc) {
            err = string("codec letters '") + row->letters + "' do not cover the "
                + std::to_string(pc) + " params of evaluator '" + g_evaluators[i].name + "'";
            return false;
        }
        for (int a = 0; a < pc; a++)
            for (int b = a + 1; b < pc; b++)
                if (row->letters[a] == row->letters[b]) {
                    err = string("duplicate weight letter '") + row->letters[a]
                        + "' in codec for evaluator '" + g_evaluators[i].name + "'";
                    return false;
                }
    }
    return true;
}

string rankFileHash8(const string& path) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f.is_open()) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    string c = ss.str();
    unsigned long long h = fnv1a64(c.data(), c.size(), 1469598103934665603ULL);
    char buf[24];
    snprintf(buf, sizeof(buf), "%016llx", h);
    return string(buf).substr(0, 8);
}

// Canonical budget rendering: multiples of a million get "m", of a thousand "k".
static string fmtBudget(unsigned long long b) {
    if (b % 1000000ULL == 0) return std::to_string(b / 1000000ULL) + "m";
    if (b % 1000ULL == 0)    return std::to_string(b / 1000ULL) + "k";
    return std::to_string(b);
}
// Canonical dilution percent: up to 2 decimals, trailing zeros and dot trimmed.
static string fmtPct(double prob) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", prob * 100.0);
    string s = buf;
    while (!s.empty() && s[s.size()-1] == '0') s.erase(s.size() - 1);
    if (!s.empty() && s[s.size()-1] == '.') s.erase(s.size() - 1);
    return s;
}

string rankAgentId(const AgentSpec& a) {
    string s;
    if (a.brain == BRAIN_POLICY) {
        const char* cname = (a.chooser >= 0 && a.chooser < g_chooserCount)
                          ? g_choosers[a.chooser].name : "";
        const RankNameCodec* row = nullptr;
        for (int i = 0; i < g_rkChooserCount; i++)
            if (string(g_rkChoosers[i].regName) == cname) row = &g_rkChoosers[i];
        string idn = row ? row->idName : "?";
        if (idn == "smart") s = "smart(" + std::to_string(a.chooserParam) + ")";
        else                s = idn;
        s += "@" + std::to_string(row ? row->version : 1);
        if (idn == "policy")
            s += ".linpol(s" + std::to_string(a.modelSlot) + ","
               + rankFileHash8(slotFile(a.modelSlot)) + ")";
    } else {
        const char* ename = (a.explorer >= 0 && a.explorer < g_explorerCount)
                          ? g_explorers[a.explorer].name : "";
        const RankNameCodec* row = nullptr;
        for (int i = 0; i < g_rkExplorerCount; i++)
            if (string(g_rkExplorers[i].regName) == ename) row = &g_rkExplorers[i];
        string idn = row ? row->idName : "?";
        if (idn == "ab") {
            s = "ab(d" + std::to_string(a.depth);
            if (!a.useAlphaBeta)        s += ",noab";
            if (a.useTT)                s += ",tt";
            if (a.useMoveOrder)         s += ",ord";
            if (a.useQuiescence)        s += ",qs";
            if (a.keepPartial)          s += ",part";
            if (a.aspirationWindow > 0) s += ",asp" + std::to_string(a.aspirationWindow);
            if (a.nodeBudget)           s += ",nb" + fmtBudget(a.nodeBudget);
            if (a.timeBudgetMs > 0.0)   s += ",tb" + std::to_string((long long)a.timeBudgetMs) + "ms";
            if (a.depthCap > 0)         s += ",cap" + std::to_string(a.depthCap);
            s += ")";
        } else {
            s = idn;   // greedy (always 1-ply, no arguments)
        }
        s += "@" + std::to_string(row ? row->version : 1);
        const char* vname = (a.evaluator >= 0 && a.evaluator < g_evalCount)
                          ? g_evaluators[a.evaluator].name : "";
        const RankEvalCodec* ev = evalCodecByRegName(vname);
        if (ev && ev->letters[0] == '\0') {
            s += ".learned(s" + std::to_string(a.modelSlot) + ","
               + rankFileHash8(slotFile(a.modelSlot)) + ")@" + std::to_string(ev->version);
        } else if (ev) {
            s += "." + string(ev->idName) + "(";
            int pc = g_evaluators[a.evaluator].paramCount;
            for (int i = 0; i < pc; i++) {
                if (i) s += ",";
                s += string(1, ev->letters[i]) + std::to_string(a.evalParams[i]);
            }
            s += ")@" + std::to_string(ev->version);
        } else {
            s += ".?";
        }
    }
    if (a.randomMoveProb > 0.0) {
        s += ".dil(r" + fmtPct(a.randomMoveProb);
        if (a.dilDepth > 0) s += ",d" + std::to_string(a.dilDepth);  // stochastic depth dilution
        s += ")@" + std::to_string(RK_DIL_VERSION);
    }
    if (a.openerKind >= 0 && a.openerKind < g_openerCount) {
        s += ".opener(" + string(g_openers[a.openerKind].idName);
        if (g_openers[a.openerKind].hasArg) s += "," + std::to_string(a.openerArg);
        s += ")@" + std::to_string(RK_OPENER_VERSION);
    }
    return s;
}

// ---- parsing helpers ----
// Split an ID into top-level dot-separated tokens (dots inside parens don't split).
static bool splitSegs(const string& id, std::vector<string>& segs, string& err) {
    segs.clear();
    string cur;
    int depth = 0;
    for (size_t i = 0; i < id.size(); i++) {
        char c = id[i];
        if (c == '(') depth++;
        if (c == ')') { depth--; if (depth < 0) { err = "unbalanced ')' in id"; return false; } }
        if (c == '.' && depth == 0) {
            if (cur.empty()) { err = "empty segment in id"; return false; }
            segs.push_back(cur);
            cur.clear();
        } else cur += c;
    }
    if (depth != 0) { err = "unbalanced '(' in id"; return false; }
    if (cur.empty()) { err = "id is empty or ends with '.'"; return false; }
    segs.push_back(cur);
    return true;
}

static bool lenientInt(const string& s, bool allowNeg, long long& v);

// Split "word(a,b,c)@V" into word + args + module version. A bare word gives
// hasParens=false; a missing "@V" gives atV=-1 (each call site decides whether
// the segment requires or forbids one).
static bool splitTok(const string& tok0, string& word, std::vector<string>& args,
                     bool& hasParens, long long& atV, string& err) {
    word.clear(); args.clear(); hasParens = false; atV = -1;
    string tok = tok0;
    size_t at = tok.rfind('@');
    if (at != string::npos) {
        long long n;
        if (!lenientInt(tok.substr(at + 1), false, n) || n < 1) {
            err = "bad module version after '@' in '" + tok0 + "' (expected @1, @2, ...)";
            return false;
        }
        atV = n;
        tok = tok.substr(0, at);
        if (tok.empty()) { err = "segment is only a version in '" + tok0 + "'"; return false; }
    }
    size_t p = tok.find('(');
    if (p == string::npos) {
        if (tok.find(')') != string::npos) { err = "stray ')' in '" + tok0 + "'"; return false; }
        word = tok;
        return !word.empty();
    }
    if (tok[tok.size()-1] != ')') { err = "expected ')' at the end of '" + tok0 + "'"; return false; }
    word = tok.substr(0, p);
    if (word.empty()) { err = "segment starts with '(' in '" + tok + "'"; return false; }
    hasParens = true;
    string inner = tok.substr(p + 1, tok.size() - p - 2);
    if (inner.find('(') != string::npos || inner.find(')') != string::npos) {
        err = "nested parens in '" + tok + "'";
        return false;
    }
    string cur;
    for (size_t i = 0; i < inner.size(); i++) {
        if (inner[i] == ',') {
            if (cur.empty()) { err = "empty argument in '" + tok + "'"; return false; }
            args.push_back(cur);
            cur.clear();
        } else cur += inner[i];
    }
    if (cur.empty()) { err = "empty argument list or trailing comma in '" + tok + "'"; return false; }
    args.push_back(cur);
    return true;
}

// Digits only (optional leading '-'), any leading zeros accepted here; the final
// canonical re-emit check is what rejects non-canonical spellings like "04".
static bool lenientInt(const string& s, bool allowNeg, long long& v) {
    if (s.empty()) return false;
    size_t i = 0;
    bool neg = false;
    if (s[0] == '-') {
        if (!allowNeg || s.size() == 1) return false;
        neg = true; i = 1;
    }
    long long acc = 0;
    for (; i < s.size(); i++) {
        if (s[i] < '0' || s[i] > '9') return false;
        acc = acc * 10 + (s[i] - '0');
        if (acc > 1000000000000LL) return false;
    }
    v = neg ? -acc : acc;
    return true;
}
static bool lenientBudget(const string& s, unsigned long long& v) {
    if (s.empty()) return false;
    unsigned long long mult = 1;
    string num = s;
    char last = s[s.size()-1];
    if (last == 'k')      { mult = 1000ULL;    num = s.substr(0, s.size()-1); }
    else if (last == 'm') { mult = 1000000ULL; num = s.substr(0, s.size()-1); }
    long long n;
    if (!lenientInt(num, false, n) || n <= 0) return false;
    v = (unsigned long long)n * mult;
    return true;
}
static bool lenientPct(const string& s, double& pct) {
    size_t dot = s.find('.');
    string ip = (dot == string::npos) ? s : s.substr(0, dot);
    string fp = (dot == string::npos) ? "" : s.substr(dot + 1);
    long long iv;
    if (!lenientInt(ip, false, iv)) return false;
    if (dot != string::npos && (fp.empty() || fp.size() > 2)) return false;
    double f = 0.0;
    for (size_t i = 0; i < fp.size(); i++) {
        if (fp[i] < '0' || fp[i] > '9') return false;
        f = f * 10.0 + (fp[i] - '0');
    }
    if (fp.size() == 1) f /= 10.0;
    if (fp.size() == 2) f /= 100.0;
    pct = (double)iv + f;
    return pct > 0.0 && pct < 100.0;
}
static bool isHash8(const string& s) {
    if (s.size() != 8) return false;
    for (size_t i = 0; i < s.size(); i++)
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return false;
    return true;
}

bool rankAgentFromId(const string& id, RankAgent& out, string& err) {
    err.clear();
    std::vector<string> segs;
    if (!splitSegs(id, segs, err)) return false;

    // Head segment (must carry its module version, e.g. rand@1 or ab(d6)@1).
    string headWord, word;
    std::vector<string> args;
    bool parens;
    long long atV;
    if (!splitTok(segs[0], headWord, args, parens, atV, err)) return false;
    if (atV < 1) {
        err = "head segment '" + segs[0] + "' needs a module version like @1";
        return false;
    }

    bool isSearch = false;
    int explorerIdx = -1, chooserIdx = -1, chooserParam = 0, depth = 1;
    bool fNoab = false, fTT = false, fOrd = false, fQs = false, fPart = false;
    bool haveAsp = false, haveCap = false, haveTb = false, haveNb = false;
    long long asp = 0, cap = 0, tbMs = 0;
    unsigned long long nb = 0;

    if (headWord == "rand" || headWord == "tiered" || headWord == "policy") {
        if (parens) { err = "'" + headWord + "' takes no arguments"; return false; }
        const char* reg = (headWord == "rand") ? "UniformRandom"
                        : (headWord == "tiered") ? "TieredRandom" : "LearnedPolicy";
        chooserIdx = chooserIndexByName(reg);
        if (chooserIdx < 0) { err = string("chooser '") + reg + "' not in registry"; return false; }
    } else if (headWord == "smart") {
        long long n;
        if (!parens || args.size() != 1 || !lenientInt(args[0], false, n) || n < 1) {
            err = "smart needs one positive argument, e.g. smart(4)";
            return false;
        }
        chooserIdx = chooserIndexByName("SmartRandom");
        if (chooserIdx < 0) { err = "chooser 'SmartRandom' not in registry"; return false; }
        chooserParam = (int)n;
    } else if (headWord == "greedy") {
        if (parens) { err = "'greedy' takes no arguments (it is always 1-ply)"; return false; }
        isSearch = true;
        explorerIdx = explorerIndexByName("Greedy");
        if (explorerIdx < 0) { err = "explorer 'Greedy' not in registry"; return false; }
        depth = 1;
    } else if (headWord == "ab") {
        if (!parens || args.empty()) { err = "ab needs arguments, e.g. ab(d6)"; return false; }
        isSearch = true;
        explorerIdx = explorerIndexByName("AlphaBeta");
        if (explorerIdx < 0) { err = "explorer 'AlphaBeta' not in registry"; return false; }
        long long d;
        if (args[0].size() < 2 || args[0][0] != 'd'
            || !lenientInt(args[0].substr(1), false, d) || d < 1 || d > 99) {
            err = "ab()'s first argument must be a depth like d6 (got '" + args[0] + "')";
            return false;
        }
        depth = (int)d;
        for (size_t i = 1; i < args.size(); i++) {
            const string& f = args[i];
            long long n;
            if (f == "noab") {
                if (fNoab) { err = "duplicate ab() flag 'noab'"; return false; }
                fNoab = true;
            } else if (f == "tt") {
                if (fTT) { err = "duplicate ab() flag 'tt'"; return false; }
                fTT = true;
            } else if (f == "ord") {
                if (fOrd) { err = "duplicate ab() flag 'ord'"; return false; }
                fOrd = true;
            } else if (f == "qs") {
                if (fQs) { err = "duplicate ab() flag 'qs'"; return false; }
                fQs = true;
            } else if (f == "part") {
                if (fPart) { err = "duplicate ab() flag 'part'"; return false; }
                fPart = true;
            } else if (f.size() > 3 && f.compare(0, 3, "asp") == 0) {
                if (haveAsp) { err = "duplicate ab() flag '" + f + "'"; return false; }
                if (!lenientInt(f.substr(3), false, n) || n <= 0) {
                    err = "bad aspiration window '" + f + "'";
                    return false;
                }
                asp = n; haveAsp = true;
            } else if (f.size() > 3 && f.compare(0, 3, "cap") == 0) {
                if (haveCap) { err = "duplicate ab() flag '" + f + "'"; return false; }
                if (!lenientInt(f.substr(3), false, n) || n <= 0) {
                    err = "bad depth cap '" + f + "'";
                    return false;
                }
                cap = n; haveCap = true;
            } else if (f.size() > 4 && f.compare(0, 2, "tb") == 0
                       && f.compare(f.size()-2, 2, "ms") == 0) {
                if (haveTb) { err = "duplicate ab() flag '" + f + "'"; return false; }
                if (!lenientInt(f.substr(2, f.size()-4), false, n) || n <= 0) {
                    err = "bad time budget '" + f + "' (expected like tb250ms)";
                    return false;
                }
                tbMs = n; haveTb = true;
            } else if (f.size() > 2 && f.compare(0, 2, "nb") == 0) {
                if (haveNb) { err = "duplicate ab() flag '" + f + "'"; return false; }
                if (!lenientBudget(f.substr(2), nb)) {
                    err = "bad node budget '" + f + "' (expected like nb200k, nb2m, nb1500)";
                    return false;
                }
                haveNb = true;
            } else {
                err = "unknown ab() flag '" + f + "'";
                return false;
            }
        }
    } else {
        err = "unknown head '" + headWord
            + "' (expected rand, tiered, smart(N), policy, greedy, or ab(...), each with @<version>)";
        return false;
    }

    // Remaining segments: evaluator / model / dilution, each at most once.
    // Evaluator and dil segments carry their module version; linpol does not
    // (its model-content hash is its identity).
    int evalIdx = -1, modelSlot = -1;
    std::vector<long long> weights;
    bool haveEval = false, haveModel = false, haveDil = false, haveOpener = false;
    string modelHash;
    double dilProb = 0.0;
    int dilDepth = 0;
    int openerKindVal = -1, openerArgVal = 0;

    for (size_t si = 1; si < segs.size(); si++) {
        if (!splitTok(segs[si], word, args, parens, atV, err)) return false;
        if (word == "opener") {
            if (haveOpener) { err = "duplicate opener() segment"; return false; }
            if (atV < 1) { err = "opener segment '" + segs[si] + "' needs a module version like @1"; return false; }
            if (!parens || args.empty() || args.size() > 2) {
                err = "opener() takes an opener name and an optional arg, e.g. opener(rand,6)@1";
                return false;
            }
            int ok = openerIndexByIdName(args[0].c_str());
            if (ok < 0) {
                string known;
                for (int i = 0; i < g_openerCount; i++) { if (i) known += "/"; known += g_openers[i].idName; }
                err = "unknown opener '" + args[0] + "' (known: " + known + ")";
                return false;
            }
            if (g_openers[ok].hasArg) {
                if (args.size() != 2) {
                    err = string("opener '") + args[0] + "' needs an arg, e.g. opener(" + args[0] + ",6)@1";
                    return false;
                }
                long long op;
                if (!lenientInt(args[1], false, op) || op < 1) {
                    err = "bad opener() arg '" + args[1] + "' (expected a positive integer, e.g. 6)";
                    return false;
                }
                openerArgVal = (int)op;
            } else if (args.size() != 1) {
                err = string("opener '") + args[0] + "' takes no arg (use opener(" + args[0] + ")@1)";
                return false;
            }
            openerKindVal = ok;
            haveOpener = true;
        } else if (word == "dil") {
            if (haveDil) { err = "duplicate dil() segment"; return false; }
            if (atV < 1) { err = "dil segment '" + segs[si] + "' needs a module version like @1"; return false; }
            if (!parens) { err = "dil needs an argument, e.g. dil(r5)@1"; return false; }
            if (args.size() > 2) {
                err = "dil() takes at most r<percent> and an optional d<depth>, got '"
                    + segs[si] + "'";
                return false;
            }
            double pct;
            if (args[0].size() < 2 || args[0][0] != 'r' || !lenientPct(args[0].substr(1), pct)) {
                err = "bad dil() argument '" + args[0] + "' (expected r<percent>, e.g. r5 or r2.5)";
                return false;
            }
            dilProb = pct / 100.0;
            // Optional second argument d<depth>: dilute with a shallower search instead of a
            // fully random move. Requires a search head and a depth strictly below the agent's.
            if (args.size() == 2) {
                long long dd;
                if (args[1].size() < 2 || args[1][0] != 'd'
                    || !lenientInt(args[1].substr(1), false, dd) || dd < 1) {
                    err = "bad dil() argument '" + args[1] + "' (expected d<depth>, e.g. d3)";
                    return false;
                }
                if (!isSearch) {
                    err = "dil() depth dilution '" + args[1] + "' needs a search head (ab/greedy)";
                    return false;
                }
                if (dd >= depth) {
                    err = "dil() depth dilution must be shallower than the agent depth d"
                        + std::to_string(depth) + " (got '" + args[1] + "')";
                    return false;
                }
                dilDepth = (int)dd;
            }
            haveDil = true;
        } else if (word == "learned" || word == "linpol") {
            if (word == "learned") {
                if (haveEval) { err = "more than one evaluator segment"; return false; }
                if (atV < 1) { err = "learned segment '" + segs[si] + "' needs a module version like @1"; return false; }
                haveEval = true;
                evalIdx = learnedValueIndex();
                if (evalIdx < 0) { err = "evaluator 'LearnedValue' not in registry"; return false; }
            } else {
                if (haveModel) { err = "duplicate linpol() segment"; return false; }
                if (atV >= 1) { err = "linpol carries no module version (its model hash is its identity)"; return false; }
                haveModel = true;
            }
            long long sl;
            if (!parens || args.size() != 2 || args[0].size() < 2 || args[0][0] != 's'
                || !lenientInt(args[0].substr(1), false, sl) || sl < 0 || sl >= ML_SLOTS) {
                err = word + " needs (s<slot>,<hash8>) with slot in 0.." + std::to_string(ML_SLOTS-1);
                return false;
            }
            if (!isHash8(args[1])) {
                err = "bad model hash '" + args[1] + "' (need 8 lowercase hex chars)";
                return false;
            }
            modelSlot = (int)sl;
            modelHash = args[1];
        } else {
            const RankEvalCodec* row = evalCodecByIdName(word);
            if (!row || row->letters[0] == '\0') { err = "unknown segment '" + segs[si] + "'"; return false; }
            if (haveEval) { err = "more than one evaluator segment"; return false; }
            if (atV < 1) { err = "evaluator segment '" + segs[si] + "' needs a module version like @1"; return false; }
            haveEval = true;
            evalIdx = evaluatorIndexByName(row->regName);
            if (evalIdx < 0) { err = string("evaluator '") + row->regName + "' not in registry"; return false; }
            int pc = g_evaluators[evalIdx].paramCount;
            if (!parens || (int)args.size() != pc) {
                err = word + " needs all " + std::to_string(pc) + " weights (letters '"
                    + row->letters + "' in that order)";
                return false;
            }
            weights.assign(pc, 0);
            std::vector<bool> seen(pc, false);
            for (int k = 0; k < pc; k++) {
                const string& a = args[k];
                const char* pos = (a.size() >= 2) ? strchr(row->letters, a[0]) : nullptr;
                if (!pos) { err = "bad weight '" + a + "' for " + word + " (letters '" + row->letters + "')"; return false; }
                int wi = (int)(pos - row->letters);
                if (seen[wi]) { err = string("duplicate weight letter '") + a[0] + "'"; return false; }
                long long v;
                if (!lenientInt(a.substr(1), true, v) || v < -100000 || v > 100000) {
                    err = "bad weight value '" + a + "'";
                    return false;
                }
                seen[wi] = true;
                weights[wi] = v;
            }
        }
    }

    // Cross rules between the head and the segments.
    if (isSearch) {
        if (!haveEval) { err = "search agent needs an evaluator segment (classic/exp/learned)"; return false; }
        if (haveModel) { err = "linpol() is only valid after the 'policy' head"; return false; }
    } else {
        if (haveEval) { err = "'" + headWord + "' takes no evaluator segment"; return false; }
        if (headWord == "policy" && !haveModel) {
            err = "'policy' needs a linpol(s<slot>,<hash8>) segment";
            return false;
        }
        if (headWord != "policy" && haveModel) {
            err = "linpol() is only valid after the 'policy' head";
            return false;
        }
    }

    // Learned agents: the model file on disk must match the ID's content hash, so
    // the match history stays truthful (a retrain is a new identity).
    if (modelSlot >= 0) {
        string mf = slotFile(modelSlot);
        if (mf.empty()) { err = "no model file convention for slot " + std::to_string(modelSlot); return false; }
        string actual = rankFileHash8(mf);
        if (actual.empty()) { err = "model file " + mf + " not found (needed by this id)"; return false; }
        if (actual != modelHash) {
            err = "model hash mismatch for " + mf + ": id says " + modelHash
                + " but the file hashes to " + actual
                + " (a retrain is a new identity: mint a new id, or restore the file)";
            return false;
        }
    }

    // Assemble the spec via the standard factories so registry defaults stay
    // single-sourced, then apply the ID's overrides.
    AgentSpec a;
    if (isSearch) {
        a = agentMakeSearch("", explorerIdx, evalIdx, depth, modelSlot >= 0 ? modelSlot : 0);
        for (size_t k = 0; k < weights.size(); k++) a.evalParams[k] = (int)weights[k];
        a.useAlphaBeta = !fNoab;
        a.useTT = fTT;
        a.useMoveOrder = fOrd;
        a.useQuiescence = fQs;
        a.keepPartial = fPart;
        a.aspirationWindow = (int)asp;
        a.nodeBudget = nb;
        a.timeBudgetMs = (double)tbMs;
        a.depthCap = (int)cap;
    } else {
        a = agentMakePolicy("", chooserIdx, chooserParam, modelSlot >= 0 ? modelSlot : 0);
    }
    a.randomMoveProb = dilProb;
    a.dilDepth = dilDepth;
    a.openerKind = openerKindVal;
    a.openerArg = openerArgVal;

    // Canonical form check: re-emitting must reproduce the input exactly. This
    // also rejects stale module versions, pointing at the current form.
    string canon = rankAgentId(a);
    if (canon != id) {
        err = "id is not canonical; use: " + canon;
        return false;
    }
    std::strncpy(a.name, id.c_str(), sizeof(a.name) - 1);
    a.name[sizeof(a.name) - 1] = '\0';
    out.spec = a;
    out.id = id;
    out.active = false;
    out.anchor = false;
    return true;
}

// ============================================================
// ROSTER
// ============================================================
bool rankLoadRoster(std::istream& in, std::vector<RankAgent>& out, string& err) {
    out.clear();
    std::set<string> seen;
    int anchors = 0, lineNo = 0;
    string line;
    while (std::getline(in, line)) {
        lineNo++;
        if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size() - 1);
        size_t h = line.find('#');
        if (h != string::npos) line = line.substr(0, h);
        line = trimWs(line);
        if (line.empty()) continue;
        size_t sp = line.find_first_of(" \t");
        if (sp == string::npos) {
            err = "line " + std::to_string(lineNo) + ": expected '<anchor|on|off> <id>'";
            return false;
        }
        string state = line.substr(0, sp);
        string id = trimWs(line.substr(sp));
        if (id.find_first_of(" \t") != string::npos) {
            err = "line " + std::to_string(lineNo) + ": unexpected text after id '" + id + "'";
            return false;
        }
        if (state != "anchor" && state != "on" && state != "off") {
            err = "line " + std::to_string(lineNo) + ": unknown state '" + state
                + "' (use anchor, on, or off)";
            return false;
        }
        RankAgent ag;
        string perr;
        if (!rankAgentFromId(id, ag, perr)) {
            err = "line " + std::to_string(lineNo) + ": " + perr;
            return false;
        }
        if (!seen.insert(id).second) {
            err = "line " + std::to_string(lineNo) + ": duplicate id " + id;
            return false;
        }
        ag.anchor = (state == "anchor");
        ag.active = (state != "off");
        if (ag.anchor) anchors++;
        out.push_back(ag);
    }
    if (anchors != 1) {
        err = "roster needs exactly one 'anchor' line (found " + std::to_string(anchors) + ")";
        return false;
    }
    return true;
}

bool rankLoadRosterFile(const string& path, std::vector<RankAgent>& out, string& err) {
    std::ifstream f(path.c_str());
    if (!f.is_open()) { err = "cannot open roster file " + path; return false; }
    if (!rankLoadRoster(f, out, err)) { err = path + ": " + err; return false; }
    return true;
}

// ============================================================
// MATCH STORE
// ============================================================
string rankFormatMatchRow(const RankMatchRow& m) {
    std::ostringstream o;
    o << "{\"t\":\"g\",\"w\":\"" << dsJsonEscape(m.w) << "\",\"b\":\"" << dsJsonEscape(m.b)
      << "\",\"r\":\"" << m.r << "\",\"plies\":" << m.plies
      << ",\"wms\":" << fmtN(m.wms, 3) << ",\"bms\":" << fmtN(m.bms, 3)
      << ",\"wcpu\":" << fmtN(m.wcpu, 3) << ",\"bcpu\":" << fmtN(m.bcpu, 3)
      << ",\"wmv\":" << m.wmv << ",\"bmv\":" << m.bmv
      << ",\"wnod\":" << (long long)m.wnod << ",\"bnod\":" << (long long)m.bnod
      << ",\"wpc\":" << m.wpc << ",\"bpc\":" << m.bpc
      << ",\"wed\":" << fmtN(m.wed, 2) << ",\"bed\":" << fmtN(m.bed, 2)
      << ",\"wsn\":" << m.wsn << ",\"bsn\":" << m.bsn
      << ",\"seed\":" << m.seed << ",\"board\":\"" << dsJsonEscape(m.board)
      << "\",\"par\":" << m.par << ",\"ts\":\"" << m.ts << "\",\"run\":\"" << m.run << "\"}";
    return o.str();
}

bool rankParseMatchRow(const string& line, RankMatchRow& out) {
    string t, r;
    double d;
    if (!jsonStr(line, "t", t) || t != "g") return false;
    if (!jsonStr(line, "w", out.w) || !jsonStr(line, "b", out.b)) return false;
    if (!jsonStr(line, "r", r) || r.size() != 1
        || (r[0] != 'W' && r[0] != 'B' && r[0] != 'D')) return false;
    out.r = r[0];
    if (!jsonNum(line, "plies", d)) return false;
    out.plies = (int)d;
    out.wms  = jsonNum(line, "wms", d)  ? d : 0.0;
    out.bms  = jsonNum(line, "bms", d)  ? d : 0.0;
    out.wmv  = jsonNum(line, "wmv", d)  ? (int)d : 0;
    out.bmv  = jsonNum(line, "bmv", d)  ? (int)d : 0;
    out.wnod = jsonNum(line, "wnod", d) ? d : 0.0;
    out.bnod = jsonNum(line, "bnod", d) ? d : 0.0;
    out.seed = jsonNum(line, "seed", d) ? (unsigned)d : 0u;
    out.par  = jsonNum(line, "par", d)  ? (int)d : 1;
    // Later-generation fields: -1 = not recorded (rows from before the field).
    out.wpc  = jsonNum(line, "wpc", d)  ? (int)d : -1;
    out.bpc  = jsonNum(line, "bpc", d)  ? (int)d : -1;
    out.wcpu = jsonNum(line, "wcpu", d) ? d : -1.0;
    out.bcpu = jsonNum(line, "bcpu", d) ? d : -1.0;
    out.wed  = jsonNum(line, "wed", d)  ? d : 0.0;
    out.bed  = jsonNum(line, "bed", d)  ? d : 0.0;
    out.wsn  = jsonNum(line, "wsn", d)  ? (int)d : 0;
    out.bsn  = jsonNum(line, "bsn", d)  ? (int)d : 0;
    out.board.clear(); out.ts.clear(); out.run.clear();
    jsonStr(line, "board", out.board);
    jsonStr(line, "ts", out.ts);
    jsonStr(line, "run", out.run);
    return true;
}

bool rankLoadMatches(const string& file, const string& board,
                     std::vector<RankMatchRow>& out, int& skipped) {
    out.clear();
    skipped = 0;
    std::ifstream f(file.c_str());
    if (!f.is_open()) return true;   // no store yet = empty history, not an error
    string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size() - 1);
        if (trimWs(line).empty()) continue;
        RankMatchRow m;
        if (!rankParseMatchRow(line, m)) { skipped++; continue; }
        if (!board.empty() && m.board != board) continue;
        out.push_back(m);
    }
    return true;
}

// ============================================================
// SCHEDULER
// ============================================================
// Per-game seed: self-contained, so any shard split or scheduling order plays
// identical games (rand() is consumed mid-game by dilution and random agents).
static unsigned gameSeed(const string& w, const string& b, long long ordinal, unsigned runSeed) {
    std::ostringstream s;
    s << w << "|" << b << "|" << ordinal << "|" << runSeed;
    string k = s.str();
    return (unsigned)(fnv1a64(k.data(), k.size(), 1469598103934665603ULL) & 0xffffffffULL);
}

std::vector<RankPendingGame> rankSchedule(const std::vector<RankAgent>& roster,
                                          const std::vector<RankMatchRow>& store,
                                          int gamesPerPair, unsigned runSeed) {
    std::vector<RankPendingGame> out;
    std::vector<string> ids;
    for (size_t i = 0; i < roster.size(); i++)
        if (roster[i].active) ids.push_back(roster[i].id);
    std::sort(ids.begin(), ids.end());

    std::map<std::pair<string,string>, long long> have;   // (white, black) -> games played
    for (size_t i = 0; i < store.size(); i++)
        have[std::make_pair(store[i].w, store[i].b)]++;

    for (size_t i = 0; i < ids.size(); i++)
        for (size_t j = i + 1; j < ids.size(); j++) {
            const string& a = ids[i];   // lexicographically smaller: White in ceil(G/2)
            const string& b = ids[j];
            long long haveAW = 0, haveBW = 0;
            std::map<std::pair<string,string>, long long>::iterator it;
            it = have.find(std::make_pair(a, b));
            if (it != have.end()) haveAW = it->second;
            it = have.find(std::make_pair(b, a));
            if (it != have.end()) haveBW = it->second;
            long long pendAW = (gamesPerPair + 1) / 2 - haveAW;
            long long pendBW = gamesPerPair / 2 - haveBW;
            if (pendAW < 0) pendAW = 0;
            if (pendBW < 0) pendBW = 0;
            long long ordinal = haveAW + haveBW;
            while (pendAW > 0 || pendBW > 0) {
                RankPendingGame g;
                if (pendAW >= pendBW) { g.w = a; g.b = b; pendAW--; }
                else                  { g.w = b; g.b = a; pendBW--; }
                g.seed = gameSeed(g.w, g.b, ordinal, runSeed);
                ordinal++;
                out.push_back(g);
            }
        }
    return out;
}

// ============================================================
// BRADLEY-TERRY FIT
// ============================================================
static const double ELO_PER_NAT = 400.0 / 2.302585092994045684;   // 400 / ln(10)

void rankFitBT(const std::vector<RankMatchRow>& rows, const string& anchorId, RankFit& out) {
    out.ids.clear(); out.elo.clear(); out.se.clear(); out.provisional.clear();
    out.anchored = false;

    // Index agents (sorted map = deterministic, order-independent).
    std::map<string,int> idx;
    for (size_t k = 0; k < rows.size(); k++) {
        if (rows[k].w == rows[k].b) continue;
        idx[rows[k].w] = 0;
        idx[rows[k].b] = 0;
    }
    int n = 0;
    for (std::map<string,int>::iterator it = idx.begin(); it != idx.end(); ++it) it->second = n++;
    if (n == 0) return;

    // Per-pair aggregates, i < j: (games, i's score). Draws count 0.5 each way.
    typedef std::map<std::pair<int,int>, std::pair<double,double> > PairMap;
    PairMap agg;
    for (size_t k = 0; k < rows.size(); k++) {
        if (rows[k].w == rows[k].b) continue;
        int wi = idx[rows[k].w], bi = idx[rows[k].b];
        double sWhite = (rows[k].r == 'W') ? 1.0 : (rows[k].r == 'B') ? 0.0 : 0.5;
        int i = wi < bi ? wi : bi, j = wi < bi ? bi : wi;
        double si = (i == wi) ? sWhite : 1.0 - sWhite;
        std::pair<double,double>& e = agg[std::make_pair(i, j)];
        e.first += 1.0;
        e.second += si;
    }
    // Prior: 0.5 virtual games (0.25 win each way) per pair that actually played.
    // Keeps undefeated agents finite without adding phantom edges.
    for (PairMap::iterator it = agg.begin(); it != agg.end(); ++it) {
        it->second.first += 0.5;
        it->second.second += 0.25;
    }

    // W_i = each agent's total score (real + prior); always > 0 thanks to the prior.
    std::vector<double> W(n, 0.0);
    for (PairMap::iterator it = agg.begin(); it != agg.end(); ++it) {
        W[it->first.first]  += it->second.second;
        W[it->first.second] += it->second.first - it->second.second;
    }

    // MM fixed point (Hunter's algorithm), simultaneous update, geometric-mean
    // normalized each sweep for numeric stability.
    std::vector<double> g(n, 1.0), gn(n, 0.0), denom(n, 0.0);
    for (int pass = 0; pass < 5000; pass++) {
        std::fill(denom.begin(), denom.end(), 0.0);
        for (PairMap::iterator it = agg.begin(); it != agg.end(); ++it) {
            int i = it->first.first, j = it->first.second;
            double d = it->second.first / (g[i] + g[j]);
            denom[i] += d;
            denom[j] += d;
        }
        for (int i = 0; i < n; i++) gn[i] = (denom[i] > 0.0) ? W[i] / denom[i] : g[i];
        double s = 0.0;
        for (int i = 0; i < n; i++) s += std::log(gn[i]);
        double scale = std::exp(-s / n);
        for (int i = 0; i < n; i++) gn[i] *= scale;
        double maxd = 0.0;
        for (int i = 0; i < n; i++) {
            double d = std::fabs(std::log(gn[i]) - std::log(g[i]));
            if (d > maxd) maxd = d;
        }
        g = gn;
        if (maxd < 1e-9) break;
    }

    // Union-find over played pairs (components disconnected from the anchor can
    // only be rated relative to themselves).
    std::vector<int> parent(n);
    for (int i = 0; i < n; i++) parent[i] = i;
    struct UF {
        static int find(std::vector<int>& p, int x) {
            while (p[x] != x) { p[x] = p[p[x]]; x = p[x]; }
            return x;
        }
    };
    for (PairMap::iterator it = agg.begin(); it != agg.end(); ++it) {
        int a = UF::find(parent, it->first.first), b = UF::find(parent, it->first.second);
        if (a != b) parent[a] = b;
    }

    std::vector<double> elo(n);
    for (int i = 0; i < n; i++) elo[i] = ELO_PER_NAT * std::log(g[i]);

    int anchorIdx = -1;
    {
        std::map<string,int>::iterator it = idx.find(anchorId);
        if (it != idx.end()) anchorIdx = it->second;
    }
    out.anchored = (anchorIdx >= 0);
    int anchorRoot = out.anchored ? UF::find(parent, anchorIdx) : -1;

    // Per-component shift: the anchor's component pins the anchor at 0; any other
    // component (or everything, when the anchor has no games) centers on mean 1000.
    std::map<int,int> compCount;
    std::map<int,double> compSum, compShift;
    for (int i = 0; i < n; i++) {
        int r = UF::find(parent, i);
        compCount[r]++;
        compSum[r] += elo[i];
    }
    for (std::map<int,int>::iterator it = compCount.begin(); it != compCount.end(); ++it) {
        int r = it->first;
        if (out.anchored && r == anchorRoot) compShift[r] = elo[anchorIdx];
        else compShift[r] = compSum[r] / it->second - 1000.0;
    }

    // Fisher-information diagonal at the fitted point (prior included).
    std::vector<double> info(n, 0.0);
    for (PairMap::iterator it = agg.begin(); it != agg.end(); ++it) {
        int i = it->first.first, j = it->first.second;
        double p = g[i] / (g[i] + g[j]);
        double c = it->second.first * p * (1.0 - p);
        info[i] += c;
        info[j] += c;
    }

    out.ids.resize(n); out.elo.resize(n); out.se.resize(n); out.provisional.resize(n);
    for (std::map<string,int>::iterator it = idx.begin(); it != idx.end(); ++it) {
        int i = it->second;
        int r = UF::find(parent, i);
        out.ids[i] = it->first;
        out.elo[i] = elo[i] - compShift[r];
        out.se[i]  = (info[i] > 0.0) ? ELO_PER_NAT / std::sqrt(info[i]) : 0.0;
        out.provisional[i] = (char)((out.anchored && r != anchorRoot) ? 1 : 0);
    }
}

double rankFitSingle(const std::vector<double>& oppElo, const std::vector<double>& score,
                     double& seOut) {
    std::vector<double> e = oppElo, s = score, wgt(score.size(), 1.0);
    // Same prior shape as the full fit: 0.5 virtual games at score 0.5 per
    // distinct opponent rating, so an undefeated candidate stays finite.
    std::set<double> uniq(oppElo.begin(), oppElo.end());
    for (std::set<double>::iterator it = uniq.begin(); it != uniq.end(); ++it) {
        e.push_back(*it);
        s.push_back(0.5);
        wgt.push_back(0.5);
    }
    double target = 0.0, totW = 0.0;
    for (size_t i = 0; i < s.size(); i++) { target += wgt[i] * s[i]; totW += wgt[i]; }
    if (totW <= 0.0) { seOut = 0.0; return 0.0; }
    // f(r) = expected total score is strictly increasing in r; bisect f(r) = target.
    double lo = -4000.0, hi = 6000.0;
    for (int it = 0; it < 200; it++) {
        double mid = 0.5 * (lo + hi), f = 0.0;
        for (size_t i = 0; i < e.size(); i++)
            f += wgt[i] / (1.0 + std::pow(10.0, (e[i] - mid) / 400.0));
        if (f < target) lo = mid; else hi = mid;
    }
    double r = 0.5 * (lo + hi), inf = 0.0;
    for (size_t i = 0; i < e.size(); i++) {
        double p = 1.0 / (1.0 + std::pow(10.0, (e[i] - r) / 400.0));
        inf += wgt[i] * p * (1.0 - p);
    }
    seOut = (inf > 0.0) ? ELO_PER_NAT / std::sqrt(inf) : 0.0;
    return r;
}

// ============================================================
// GAME RUNNER
// ============================================================
// Total CPU time (kernel + user, ms) this process has consumed, or -1 when the
// platform cannot say. Unlike wall time, deltas of this are contention-safe:
// a move that waited for a core does not get charged for the wait, so cpu/move
// stays honest in -Workers runs.
static double processCpuMs() {
#ifdef _WIN32
    FILETIME ct, et, kt, ut;
    if (!GetProcessTimes(GetCurrentProcess(), &ct, &et, &kt, &ut)) return -1.0;
    ULARGE_INTEGER k, u;
    k.LowPart = kt.dwLowDateTime; k.HighPart = kt.dwHighDateTime;
    u.LowPart = ut.dwLowDateTime; u.HighPart = ut.dwHighDateTime;
    return (double)(k.QuadPart + u.QuadPart) / 1e4;   // 100ns ticks -> ms
#else
    return -1.0;
#endif
}

// Play one game on the live engine board, filling a match row (timing, node
// totals, result). The caller has already srand()'d with the game's seed.
static bool playOneGame(const RankAgent& wa, const RankAgent& ba, const string& board,
                        RankMatchRow& m) {
    if (!reloadBoard(board)) return false;
    typedef std::chrono::steady_clock clk;
    m.w = wa.id; m.b = ba.id;
    m.plies = 0;
    m.wms = m.bms = 0.0;
    m.wmv = m.bmv = 0;
    m.wnod = m.bnod = 0.0;
    m.wed = m.bed = 0.0;
    m.wsn = m.bsn = 0;
    bool haveCpu = (processCpuMs() >= 0.0);
    m.wcpu = m.bcpu = haveCpu ? 0.0 : -1.0;
    int victor = None;
    for (int h = 0; h < 400; h++) {
        int side = (h % 2 == 0) ? White : Black;
        const RankAgent& ag = (side == White) ? wa : ba;
        g_lastNodes = 0;   // so non-search brains contribute 0 nodes
        double c0 = haveCpu ? processCpuMs() : 0.0;
        clk::time_point t0 = clk::now();
        // Identity-level opener: consult the agent's selected opener with its own
        // ply count so far (h/2, its Nth move regardless of color); if the opener
        // declines (or there is none), the brain plays.
        bool playedByOpener = false;
        if (ag.spec.openerKind >= 0 && ag.spec.openerKind < g_openerCount)
            playedByOpener = g_openers[ag.spec.openerKind].fn(side, h / 2, ag.spec.openerArg, victor);
        if (!playedByOpener)
            victor = agentChooseMove(ag.spec, side);
        double dt = std::chrono::duration<double, std::milli>(clk::now() - t0).count();
        if (haveCpu) {
            double dc = processCpuMs() - c0;
            if (side == White) m.wcpu += dc; else m.bcpu += dc;
        }
        if (side == White) { m.wms += dt; m.wmv++; }
        else               { m.bms += dt; m.bmv++; }
        if (ag.spec.brain == BRAIN_SEARCH && g_lastNodes > 1) {
            if (side == White) { m.wnod += (double)g_lastNodes; m.wed += g_lastEffDepth; m.wsn++; }
            else               { m.bnod += (double)g_lastNodes; m.bed += g_lastEffDepth; m.bsn++; }
        }
        m.plies = h + 1;
        if (gameOutcome(victor)) break;
    }
    m.wpc = g_whiteCount;
    m.bpc = g_blackCount;
    int oc = gameOutcome(victor);
    m.r = (oc == 1) ? 'W' : (oc == 2) ? 'B' : 'D';
    return true;
}

// Load every model slot referenced by these agents; hard error if one fails.
static bool loadModelSlots(const std::vector<const RankAgent*>& agents, string& err) {
    std::set<int> slots;
    for (size_t i = 0; i < agents.size(); i++) {
        const AgentSpec& a = agents[i]->spec;
        if (a.brain == BRAIN_POLICY && a.chooser == chooserIndexByName("LearnedPolicy"))
            slots.insert(a.modelSlot);
        if (a.brain == BRAIN_SEARCH && a.evaluator == learnedValueIndex())
            slots.insert(a.modelSlot);
    }
    for (std::set<int>::iterator it = slots.begin(); it != slots.end(); ++it) {
        string f = slotFile(*it);
        if (f.empty() || !mlLoadSlot(*it, f)) {
            err = "cannot load model " + f + " into slot " + std::to_string(*it);
            return false;
        }
    }
    return true;
}

// ============================================================
// PLAY
// ============================================================
int rankPlay(const string& rosterFile, const string& storeFile, const string& outFile,
             int gamesPerPair, int shard, int ofK, unsigned runSeed, const string& board) {
    std::vector<RankAgent> roster;
    string err;
    if (!rankLoadRosterFile(rosterFile, roster, err)) { cout << "ERROR: " << err << "\n"; return 1; }
    if (ofK < 1) ofK = 1;
    if (shard < 0 || shard >= ofK) { cout << "ERROR: --shard must be in [0, --of)\n"; return 1; }

    std::vector<RankMatchRow> store;
    int skipped = 0;
    rankLoadMatches(storeFile, board, store, skipped);
    if (skipped) cout << "WARNING: skipped " << skipped << " malformed line(s) in " << storeFile << "\n";

    std::vector<RankPendingGame> pending = rankSchedule(roster, store, gamesPerPair, runSeed);
    int nActive = 0;
    for (size_t i = 0; i < roster.size(); i++) if (roster[i].active) nActive++;

    string pre = (ofK > 1) ? ("[s" + std::to_string(shard) + "] ") : string("");
    cout << pre << "rank: " << nActive << " active agents, " << pending.size()
         << " pending games (target " << gamesPerPair << "/pair)";
    if (ofK > 1) cout << ", shard " << shard << "/" << ofK;
    cout << "\n" << flush;
    if (pending.empty()) {
        cout << pre << "nothing to play: every active pair is at target\n";
        return 0;
    }

    std::map<string, const RankAgent*> byId;
    std::vector<const RankAgent*> act;
    for (size_t i = 0; i < roster.size(); i++)
        if (roster[i].active) { byId[roster[i].id] = &roster[i]; act.push_back(&roster[i]); }
    if (!loadModelSlots(act, err)) { cout << "ERROR: " << err << "\n"; return 1; }

    PRNT = 0;
    ensureDir("ranking");
    string stamp = runStamp();
    struct Tally { long long w, l, d; };
    std::map<std::pair<string,string>, Tally> tally;
    long long played = 0;

    for (size_t p = 0; p < pending.size(); p++) {
        if ((long long)(p % (size_t)ofK) != (long long)shard) continue;
        const RankPendingGame& gm = pending[p];
        srand(gm.seed);
        RankMatchRow m;
        if (!playOneGame(*byId[gm.w], *byId[gm.b], board, m)) {
            cout << "ERROR: cannot load board " << board << "\n";
            return 1;
        }
        m.seed = gm.seed; m.board = board; m.par = ofK;
        m.ts = nowUtc(); m.run = stamp;
        dsAppendLine(outFile, rankFormatMatchRow(m));
        played++;

        // Session tally from the lexicographically smaller id's perspective.
        bool wSmall = (gm.w < gm.b);
        std::pair<string,string> key = wSmall ? std::make_pair(gm.w, gm.b)
                                              : std::make_pair(gm.b, gm.w);
        Tally& t = tally[key];
        if (m.r == 'D') t.d++;
        else if ((m.r == 'W') == wSmall) t.w++;
        else t.l++;

        std::ostringstream ln;
        ln << pre << "[" << std::setw(4) << (p + 1) << "/" << pending.size() << "] "
           << gm.w << " (W) vs " << gm.b << " : " << m.r << " in " << m.plies
           << " plies, " << fmtN((m.wms + m.bms) / 1000.0, 1) << "s | pair "
           << t.w << "-" << t.l;
        cout << ln.str() << "\n" << flush;
    }
    cout << pre << "played " << played << " game(s) -> " << outFile << "\n";
    return 0;
}

// ============================================================
// RATE + REPORTS
// ============================================================
struct AgentAgg {
    long long games = 0, wins = 0, losses = 0, draws = 0;
    long long winsW = 0, lossesW = 0, winsB = 0, lossesB = 0;   // split by color played
    double msSerial = 0.0, msAll = 0.0, nodSerial = 0.0, nodAll = 0.0;
    long long mvSerial = 0, mvAll = 0;
    long long pliesSum = 0;                    // over all games (avg game length)
    double marginSum = 0.0; long long marginGames = 0;   // own minus opp end pieces
    double cpuSum = 0.0; long long cpuMv = 0;  // CPU ms + moves, rows that recorded cpu
    double edSum = 0.0; long long edCnt = 0;   // effective search depth accumulators
};
struct PairAgg {   // from the lexicographically smaller id's perspective
    double n = 0.0, s = 0.0;
    long long w = 0, l = 0, d = 0;
    long long pliesSum = 0;
};

static void aggregateAgents(const std::vector<RankMatchRow>& rows, std::map<string, AgentAgg>& agg) {
    for (size_t k = 0; k < rows.size(); k++) {
        const RankMatchRow& m = rows[k];
        for (int side = 0; side < 2; side++) {
            bool meWhite = (side == 0);
            const string& id = meWhite ? m.w : m.b;
            AgentAgg& a = agg[id];
            a.games++;
            if (m.r == 'D') a.draws++;
            else if ((m.r == 'W') == meWhite) { a.wins++; if (meWhite) a.winsW++; else a.winsB++; }
            else { a.losses++; if (meWhite) a.lossesW++; else a.lossesB++; }
            a.pliesSum += m.plies;
            double ms  = meWhite ? m.wms : m.bms;
            double nod = meWhite ? m.wnod : m.bnod;
            long long mv = meWhite ? m.wmv : m.bmv;
            a.msAll += ms; a.nodAll += nod; a.mvAll += mv;
            if (m.par <= 1) { a.msSerial += ms; a.nodSerial += nod; a.mvSerial += mv; }
            if (m.wpc >= 0 && m.bpc >= 0) {
                a.marginSum += meWhite ? (m.wpc - m.bpc) : (m.bpc - m.wpc);
                a.marginGames++;
            }
            double cpu = meWhite ? m.wcpu : m.bcpu;
            if (cpu >= 0.0) { a.cpuSum += cpu; a.cpuMv += mv; }
            int sn = meWhite ? m.wsn : m.bsn;
            if (sn > 0) { a.edSum += meWhite ? m.wed : m.bed; a.edCnt += sn; }
        }
    }
}

static void aggregatePairs(const std::vector<RankMatchRow>& rows,
                           std::map<std::pair<string,string>, PairAgg>& pa) {
    for (size_t k = 0; k < rows.size(); k++) {
        const RankMatchRow& m = rows[k];
        bool wSmall = (m.w < m.b);
        std::pair<string,string> key = wSmall ? std::make_pair(m.w, m.b)
                                              : std::make_pair(m.b, m.w);
        double sSmall = (m.r == 'D') ? 0.5 : ((m.r == 'W') == wSmall) ? 1.0 : 0.0;
        PairAgg& e = pa[key];
        e.n += 1.0;
        e.s += sSmall;
        e.pliesSum += m.plies;
        if (m.r == 'D') e.d++;
        else if (sSmall == 1.0) e.w++;
        else e.l++;
    }
}

// Per-agent derived compute figures. cpuMsMove is -1 when no row recorded CPU.
static double cpuMsPerMove(const AgentAgg& a) {
    return (a.cpuMv > 0) ? a.cpuSum / a.cpuMv : -1.0;
}
// Elo per compute doubling: how much rating each doubling of per-move CPU buys.
// Undefined (returns "-") below 1us/move or at/below the anchor's strength.
static string effCol(double elo, double cpuMsMove) {
    if (cpuMsMove < 0.0) return "-";
    double us = cpuMsMove * 1000.0;
    if (us < 1.0 || elo <= 0.0) return "-";
    return fmtN(elo / std::log2(1.0 + us), 0);
}

static const AgentAgg& aggFor(const std::map<string, AgentAgg>& m, const string& id) {
    static const AgentAgg empty;
    std::map<string, AgentAgg>::const_iterator it = m.find(id);
    return (it == m.end()) ? empty : it->second;
}
static string stateFor(const std::map<string, string>& st, const string& id) {
    std::map<string, string>::const_iterator it = st.find(id);
    return (it == st.end()) ? string("gone") : it->second;
}

// ms/move + nodes/move, preferring uncontended serial rows; '*' marks a
// fallback that includes parallel-run (contended) moves.
static void timingCols(const AgentAgg& a, string& msS, string& nodS) {
    bool serial = (a.mvSerial > 0);
    long long mv = serial ? a.mvSerial : a.mvAll;
    double ms  = (mv > 0) ? (serial ? a.msSerial : a.msAll) / mv : 0.0;
    double nod = (mv > 0) ? (serial ? a.nodSerial : a.nodAll) / mv : 0.0;
    msS  = fmtN(ms, 2) + (serial ? "" : "*");
    nodS = fmtN(nod, 0) + (serial ? "" : "*");
}

static double eloExpectedScore(double ra, double rb) {
    return 1.0 / (1.0 + std::pow(10.0, (rb - ra) / 400.0));
}

static long long roundElo(double e) {
    return (long long)(e < 0 ? e - 0.5 : e + 0.5);
}

static void printConsoleTable(const RankFit& fit, const std::vector<int>& order,
                              const std::map<string, AgentAgg>& agg,
                              const std::map<string, string>& state) {
    cout << "\n rank    Elo    +/-   games   W-L asW   W-L asB   cpu ms/mv    eff  id\n";
    for (size_t r = 0; r < order.size(); r++) {
        int i = order[r];
        const string& id = fit.ids[i];
        const AgentAgg& a = aggFor(agg, id);
        string st = stateFor(state, id);
        string pm = (st == "anchor") ? "anchor" : fmtN(fit.se[i], 0);
        string wlW = std::to_string(a.winsW) + "-" + std::to_string(a.lossesW);
        string wlB = std::to_string(a.winsB) + "-" + std::to_string(a.lossesB);
        double cpu = cpuMsPerMove(a);
        std::ostringstream ln;
        ln << std::setw(5) << (r + 1) << "  " << std::setw(5) << roundElo(fit.elo[i])
           << "  " << std::setw(6) << pm << "  " << std::setw(6) << a.games
           << "  " << std::setw(8) << wlW << "  " << std::setw(8) << wlB
           << "  " << std::setw(10) << (cpu >= 0.0 ? fmtN(cpu, 2) : string("-"))
           << "  " << std::setw(5) << effCol(fit.elo[i], cpu) << "  " << id;
        if (fit.provisional[i]) ln << " ~provisional";
        if (st == "off") ln << " (off)";
        if (st == "gone") ln << " (retired)";
        cout << ln.str() << "\n";
    }
    cout << "\n";
}

static void writeRatingsTsv(const RankFit& fit, const std::vector<int>& order,
                            const std::map<string, AgentAgg>& agg,
                            const std::map<string, string>& state) {
    std::ofstream f("ranking/ratings.tsv");
    if (!f.is_open()) return;
    f << "rank\telo\tpm\tgames\twins\tlosses\twhite_wins\twhite_losses\tblack_wins\tblack_losses\t"
      << "avg_plies\tms_move\tcpu_ms_move\tnodes_move\teff\tactive\tid\n";
    for (size_t r = 0; r < order.size(); r++) {
        int i = order[r];
        const string& id = fit.ids[i];
        const AgentAgg& a = aggFor(agg, id);
        bool serial = (a.mvSerial > 0);
        long long mv = serial ? a.mvSerial : a.mvAll;
        double ms  = (mv > 0) ? (serial ? a.msSerial : a.msAll) / mv : 0.0;
        double nod = (mv > 0) ? (serial ? a.nodSerial : a.nodAll) / mv : 0.0;
        double cpu = cpuMsPerMove(a);
        f << (r + 1) << "\t" << roundElo(fit.elo[i]) << "\t" << roundElo(fit.se[i]) << "\t"
          << a.games << "\t" << a.wins << "\t" << a.losses << "\t"
          << a.winsW << "\t" << a.lossesW << "\t" << a.winsB << "\t" << a.lossesB << "\t"
          << fmtN(a.games > 0 ? (double)a.pliesSum / a.games : 0.0, 1) << "\t"
          << fmtN(ms, 3) << "\t" << (cpu >= 0.0 ? fmtN(cpu, 3) : string("")) << "\t"
          << fmtN(nod, 0) << "\t" << effCol(fit.elo[i], cpu) << "\t"
          << stateFor(state, id) << "\t" << id << "\n";
    }
}

// Machine-readable per-game export (one row per stored game, empty = unrecorded).
static void writeGamesTsv(const std::vector<RankMatchRow>& rows) {
    std::ofstream f("ranking/games.tsv");
    if (!f.is_open()) return;
    f << "ts\trun\tboard\twhite\tblack\tresult\tplies\twpc\tbpc\twms\tbms\twcpu\tbcpu\t"
      << "wmv\tbmv\twnod\tbnod\twed\tbed\twsn\tbsn\tseed\tpar\n";
    for (size_t k = 0; k < rows.size(); k++) {
        const RankMatchRow& m = rows[k];
        f << m.ts << "\t" << m.run << "\t" << m.board << "\t" << m.w << "\t" << m.b << "\t"
          << m.r << "\t" << m.plies << "\t"
          << (m.wpc >= 0 ? std::to_string(m.wpc) : string("")) << "\t"
          << (m.bpc >= 0 ? std::to_string(m.bpc) : string("")) << "\t"
          << fmtN(m.wms, 3) << "\t" << fmtN(m.bms, 3) << "\t"
          << (m.wcpu >= 0.0 ? fmtN(m.wcpu, 3) : string("")) << "\t"
          << (m.bcpu >= 0.0 ? fmtN(m.bcpu, 3) : string("")) << "\t"
          << m.wmv << "\t" << m.bmv << "\t"
          << (long long)m.wnod << "\t" << (long long)m.bnod << "\t"
          << fmtN(m.wed, 2) << "\t" << fmtN(m.bed, 2) << "\t" << m.wsn << "\t" << m.bsn << "\t"
          << m.seed << "\t" << m.par << "\n";
    }
}

static void writeReportMd(const RankFit& fit, const std::vector<int>& order,
                          const std::map<string, AgentAgg>& agg,
                          const std::map<string, string>& state,
                          const std::vector<RankAgent>& roster,
                          const std::map<std::pair<string,string>, PairAgg>& pairs,
                          const string& board, const string& storeFile,
                          size_t nRows, const string& anchorId) {
    std::ofstream f("ranking/report.md");
    if (!f.is_open()) return;

    std::map<string, double> eloBy;
    std::map<string, int> fitIdx;
    for (size_t i = 0; i < fit.ids.size(); i++) { eloBy[fit.ids[i]] = fit.elo[i]; fitIdx[fit.ids[i]] = (int)i; }

    f << "# Agent ranking report\n\n";
    f << "Generated " << nowUtc() << ". Board `" << board << "`. "
      << nRows << " games from `" << storeFile << "`, " << fit.ids.size() << " rated agents.\n\n";
    f << "Fit: Bradley-Terry MM refit over the full store, prior 0.5 virtual games per played pair, "
      << "anchor `" << anchorId << "` = Elo 0. `+/-` is one standard error. "
      << "`cpu/mv` is per-move process CPU time in ms (contention-safe, valid in parallel runs). "
      << "`eff` = Elo / log2(1 + cpu_us/move), the Elo bought per doubling of per-move compute. "
      << "`wall/mv` prefers serial games; `*` marks a fallback that includes contended parallel moves. "
      << "`margin` is the average end-of-game piece lead (own minus opponent). "
      << "`~` marks agents whose games do not connect to the anchor (rated relative to their own mean of 1000).\n\n";
    if (!fit.anchored)
        f << "**WARNING:** the anchor has no games yet, so all ratings are centered on mean 1000 instead of anchor = 0.\n\n";

    // Split ranked agents into active and inactive/retired.
    std::vector<int> activeOrder, otherOrder;
    for (size_t r = 0; r < order.size(); r++) {
        string st = stateFor(state, fit.ids[order[r]]);
        if (st == "anchor" || st == "on") activeOrder.push_back(order[r]);
        else otherOrder.push_back(order[r]);
    }

    struct Row {
        static void emit(std::ofstream& f, size_t rank, int i, const RankFit& fit,
                         const std::map<string, AgentAgg>& agg,
                         const std::map<string, string>& state) {
            const string& id = fit.ids[i];
            const AgentAgg& a = aggFor(agg, id);
            string st = stateFor(state, id);
            string pm = (st == "anchor") ? "(anchor)" : fmtN(fit.se[i], 0);
            string ms, nod;
            timingCols(a, ms, nod);
            double cpu = cpuMsPerMove(a);
            f << "| " << rank << " | " << roundElo(fit.elo[i]) << (fit.provisional[i] ? "~" : "")
              << " | " << pm << " | " << a.games
              << " | " << a.winsW << "-" << a.lossesW << " | " << a.winsB << "-" << a.lossesB
              << " | " << fmtN(a.games > 0 ? (double)a.pliesSum / a.games : 0.0, 0)
              << " | " << (a.marginGames > 0 ? fmtN(a.marginSum / a.marginGames, 1) : string("-"))
              << " | " << (cpu >= 0.0 ? fmtN(cpu, 2) : string("-"))
              << " | " << effCol(fit.elo[i], cpu)
              << " | " << ms << " | " << nod << " | " << st << " | `" << id << "` |\n";
        }
        static void head(std::ofstream& f) {
            f << "| rank | Elo | +/- | games | W-L as White | W-L as Black | avg plies | margin | cpu/mv | eff | wall/mv | nodes/mv | state | id |\n";
            f << "|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|\n";
        }
    };

    f << "## Ratings (active agents)\n\n";
    Row::head(f);
    for (size_t r = 0; r < activeOrder.size(); r++)
        Row::emit(f, r + 1, activeOrder[r], fit, agg, state);
    f << "\n";

    if (!otherOrder.empty()) {
        f << "## Inactive and retired agents\n\n";
        f << "Still rated from their stored games (history is never lost). `off` = in the roster "
          << "but benched, `gone` = no longer in the roster.\n\n";
        Row::head(f);
        for (size_t r = 0; r < otherOrder.size(); r++)
            Row::emit(f, r + 1, otherOrder[r], fit, agg, state);
        f << "\n";
    }

    // Compute efficiency: the Elo-vs-CPU surface a weight/feature hill climber
    // optimizes over. Frontier = no other agent is both stronger and cheaper.
    {
        std::vector<int> byCpu;
        for (size_t r = 0; r < activeOrder.size(); r++)
            if (cpuMsPerMove(aggFor(agg, fit.ids[activeOrder[r]])) >= 0.0)
                byCpu.push_back(activeOrder[r]);
        struct ByCpuAsc {
            const RankFit* fit;
            const std::map<string, AgentAgg>* agg;
            bool operator()(int x, int y) const {
                double cx = cpuMsPerMove(aggFor(*agg, fit->ids[x]));
                double cy = cpuMsPerMove(aggFor(*agg, fit->ids[y]));
                if (cx != cy) return cx < cy;
                return fit->elo[x] > fit->elo[y];
            }
        };
        ByCpuAsc cmp; cmp.fit = &fit; cmp.agg = &agg;
        std::sort(byCpu.begin(), byCpu.end(), cmp);
        if (!byCpu.empty()) {
            f << "## Compute efficiency (active agents)\n\n";
            f << "Sorted by per-move CPU time. `*` = on the Elo-vs-compute pareto frontier "
              << "(no other active agent is both stronger and cheaper).\n\n";
            f << "| cpu ms/mv | Elo | eff | frontier | id |\n";
            f << "|---:|---:|---:|:---:|---|\n";
            for (size_t x = 0; x < byCpu.size(); x++) {
                int i = byCpu[x];
                double ci = cpuMsPerMove(aggFor(agg, fit.ids[i]));
                bool frontier = true;
                for (size_t y = 0; y < byCpu.size(); y++) {
                    if (y == x) continue;
                    int j = byCpu[y];
                    double cj = cpuMsPerMove(aggFor(agg, fit.ids[j]));
                    if (cj <= ci && fit.elo[j] > fit.elo[i]) { frontier = false; break; }
                }
                f << "| " << fmtN(ci, 3) << " | " << roundElo(fit.elo[i]) << " | "
                  << effCol(fit.elo[i], ci) << " | " << (frontier ? "*" : "") << " | `"
                  << fit.ids[i] << "` |\n";
            }
            f << "\n";
        }
    }

    // Roster agents with no games yet.
    std::vector<string> unrated;
    for (size_t i = 0; i < roster.size(); i++)
        if (eloBy.find(roster[i].id) == eloBy.end()) unrated.push_back(roster[i].id);
    if (!unrated.empty()) {
        f << "## Unrated roster agents (no games yet)\n\n";
        for (size_t i = 0; i < unrated.size(); i++) f << "- `" << unrated[i] << "`\n";
        f << "\nRun `rank.exe play` to schedule their games.\n\n";
    }

    // Head-to-head matrix over active agents (row's score vs column).
    if (activeOrder.size() > 1) {
        f << "## Head-to-head matrix (active agents)\n\n";
        f << "Cell = row agent's score against the column agent, over n games.\n\n";
        f << "| # | agent |";
        for (size_t c = 0; c < activeOrder.size(); c++) f << " " << (c + 1) << " |";
        f << "\n|---|---|";
        for (size_t c = 0; c < activeOrder.size(); c++) f << "---:|";
        f << "\n";
        for (size_t r = 0; r < activeOrder.size(); r++) {
            const string& rid = fit.ids[activeOrder[r]];
            f << "| " << (r + 1) << " | `" << rid << "` |";
            for (size_t c = 0; c < activeOrder.size(); c++) {
                if (r == c) { f << " - |"; continue; }
                const string& cid = fit.ids[activeOrder[c]];
                bool rSmall = (rid < cid);
                std::pair<string,string> key = rSmall ? std::make_pair(rid, cid)
                                                      : std::make_pair(cid, rid);
                std::map<std::pair<string,string>, PairAgg>::const_iterator it = pairs.find(key);
                if (it == pairs.end() || it->second.n <= 0.0) { f << " . |"; continue; }
                double sc = it->second.s / it->second.n;
                if (!rSmall) sc = 1.0 - sc;
                f << " " << fmtN(sc * 100.0, 0) << "% (" << (long long)it->second.n << ") |";
            }
            f << "\n";
        }
        f << "\n";
    }

    // Per-agent match history: every opponent, actual vs expected score.
    f << "## Per-agent match history (active agents)\n\n";
    for (size_t r = 0; r < activeOrder.size(); r++) {
        int i = activeOrder[r];
        const string& id = fit.ids[i];
        string pm = (stateFor(state, id) == "anchor") ? string("(anchor)")
                  : ("+/- " + fmtN(fit.se[i], 0));
        f << "### " << (r + 1) << ". `" << id << "` (Elo " << roundElo(fit.elo[i]) << " " << pm << ")\n\n";

        // Collect this agent's opponents from the pair aggregates.
        struct OppRow { string opp; PairAgg pa; bool meSmall; };
        std::vector<OppRow> opps;
        for (std::map<std::pair<string,string>, PairAgg>::const_iterator it = pairs.begin();
             it != pairs.end(); ++it) {
            if (it->first.first != id && it->first.second != id) continue;
            OppRow o;
            o.meSmall = (it->first.first == id);
            o.opp = o.meSmall ? it->first.second : it->first.first;
            o.pa = it->second;
            opps.push_back(o);
        }
        // Sort by opponent Elo descending (rated opponents first), then id.
        struct ByElo {
            const std::map<string, double>* eloBy;
            bool operator()(const OppRow& a, const OppRow& b) const {
                std::map<string, double>::const_iterator ea = eloBy->find(a.opp), eb = eloBy->find(b.opp);
                double va = (ea == eloBy->end()) ? -1e18 : ea->second;
                double vb = (eb == eloBy->end()) ? -1e18 : eb->second;
                if (va != vb) return va > vb;
                return a.opp < b.opp;
            }
        };
        ByElo cmp; cmp.eloBy = &eloBy;
        std::sort(opps.begin(), opps.end(), cmp);

        f << "| opponent | games | W-L | score | expected | delta | avg plies |\n";
        f << "|---|---:|---:|---:|---:|---:|---:|\n";
        for (size_t k = 0; k < opps.size(); k++) {
            const OppRow& o = opps[k];
            double sc = o.pa.s / o.pa.n;
            long long w = o.pa.w, l = o.pa.l;
            if (!o.meSmall) { sc = 1.0 - sc; std::swap(w, l); }
            string expS = "-", dltS = "-";
            std::map<string, double>::const_iterator eo = eloBy.find(o.opp);
            if (eo != eloBy.end()) {
                double exp = eloExpectedScore(fit.elo[i], eo->second);
                double dlt = sc - exp;
                expS = fmtN(exp, 2);
                dltS = (dlt >= 0 ? "+" : "") + fmtN(dlt, 2);
            }
            f << "| `" << o.opp << "` | " << (long long)o.pa.n << " | " << w << "-" << l
              << " | " << fmtN(sc, 2) << " | " << expS << " | " << dltS
              << " | " << fmtN((double)o.pa.pliesSum / o.pa.n, 0) << " |\n";
        }
        f << "\n";
    }
}

int rankRate(const string& rosterFile, const string& storeFile, const string& board) {
    std::vector<RankAgent> roster;
    string err;
    if (!rankLoadRosterFile(rosterFile, roster, err)) { cout << "ERROR: " << err << "\n"; return 1; }
    string anchorId;
    for (size_t i = 0; i < roster.size(); i++) if (roster[i].anchor) anchorId = roster[i].id;

    std::vector<RankMatchRow> rows;
    int skipped = 0;
    rankLoadMatches(storeFile, board, rows, skipped);
    if (skipped) cout << "WARNING: skipped " << skipped << " malformed line(s) in " << storeFile << "\n";
    if (rows.empty()) {
        cout << "No games in " << storeFile << " for board " << board
             << ". Run 'rank.exe play' (or 'rank.exe run') first.\n";
        return 1;
    }

    RankFit fit;
    rankFitBT(rows, anchorId, fit);
    if (!fit.anchored)
        cout << "WARNING: anchor " << anchorId
             << " has no games; ratings centered on mean 1000 instead of anchor = 0\n";

    std::map<string, AgentAgg> agg;
    aggregateAgents(rows, agg);
    std::map<std::pair<string,string>, PairAgg> pairs;
    aggregatePairs(rows, pairs);

    std::map<string, string> state;
    for (size_t i = 0; i < roster.size(); i++)
        state[roster[i].id] = roster[i].anchor ? "anchor" : (roster[i].active ? "on" : "off");

    // Rank order: Elo descending, id ascending on ties (deterministic).
    std::vector<int> order(fit.ids.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
    struct ByEloDesc {
        const RankFit* fit;
        bool operator()(int a, int b) const {
            if (fit->elo[a] != fit->elo[b]) return fit->elo[a] > fit->elo[b];
            return fit->ids[a] < fit->ids[b];
        }
    };
    ByEloDesc cmp; cmp.fit = &fit;
    std::sort(order.begin(), order.end(), cmp);

    ensureDir("ranking");
    writeRatingsTsv(fit, order, agg, state);
    writeGamesTsv(rows);
    writeReportMd(fit, order, agg, state, roster, pairs, board, storeFile, rows.size(), anchorId);
    printConsoleTable(fit, order, agg, state);
    int unrated = 0;
    for (size_t i = 0; i < roster.size(); i++) {
        bool found = false;
        for (size_t k = 0; k < fit.ids.size(); k++) if (fit.ids[k] == roster[i].id) { found = true; break; }
        if (!found) unrated++;
    }
    if (unrated)
        cout << unrated << " roster agent(s) have no games yet (see report.md); run 'rank.exe play'\n";
    cout << "wrote ranking/ratings.tsv, ranking/games.tsv and ranking/report.md\n";
    return 0;
}

// ============================================================
// HISTORY
// ============================================================
int rankHistory(const string& storeFile, const string& agentQuery, int lastN, const string& board) {
    if (agentQuery.empty()) { cout << "ERROR: --agent <id or unique prefix> is required\n"; return 1; }
    std::vector<RankMatchRow> rows;
    int skipped = 0;
    rankLoadMatches(storeFile, board, rows, skipped);
    if (rows.empty()) { cout << "No games in " << storeFile << " for board " << board << "\n"; return 1; }

    std::set<string> ids;
    for (size_t k = 0; k < rows.size(); k++) { ids.insert(rows[k].w); ids.insert(rows[k].b); }

    string id;
    if (ids.count(agentQuery)) id = agentQuery;
    else {
        std::vector<string> hits;
        for (std::set<string>::iterator it = ids.begin(); it != ids.end(); ++it)
            if (it->compare(0, agentQuery.size(), agentQuery) == 0) hits.push_back(*it);
        if (hits.empty()) { cout << "No agent in the store matches '" << agentQuery << "'\n"; return 1; }
        if (hits.size() > 1) {
            cout << "Ambiguous prefix '" << agentQuery << "' matches:\n";
            for (size_t i = 0; i < hits.size(); i++) cout << "  " << hits[i] << "\n";
            return 1;
        }
        id = hits[0];
    }

    // Per-opponent aggregates.
    struct Opp { long long g, w, l, d; double s; };
    std::map<string, Opp> opp;
    std::vector<const RankMatchRow*> mine;
    for (size_t k = 0; k < rows.size(); k++) {
        const RankMatchRow& m = rows[k];
        if (m.w != id && m.b != id) continue;
        mine.push_back(&m);
        bool meWhite = (m.w == id);
        const string& other = meWhite ? m.b : m.w;
        Opp& o = opp[other];
        o.g++;
        double sc = (m.r == 'D') ? 0.5 : ((m.r == 'W') == meWhite) ? 1.0 : 0.0;
        o.s += sc;
        if (m.r == 'D') o.d++; else if (sc == 1.0) o.w++; else o.l++;
    }
    if (mine.empty()) { cout << "No games for " << id << "\n"; return 1; }

    cout << "\nHistory for " << id << " (" << mine.size() << " games, board " << board << ")\n\n";
    cout << " games      W-L   score  opponent\n";
    std::vector<std::pair<string, Opp> > ov(opp.begin(), opp.end());
    struct ByGames {
        bool operator()(const std::pair<string, Opp>& a, const std::pair<string, Opp>& b) const {
            if (a.second.g != b.second.g) return a.second.g > b.second.g;
            return a.first < b.first;
        }
    };
    std::sort(ov.begin(), ov.end(), ByGames());
    for (size_t i = 0; i < ov.size(); i++) {
        const Opp& o = ov[i].second;
        string wl = std::to_string(o.w) + "-" + std::to_string(o.l);
        std::ostringstream ln;
        ln << std::setw(6) << o.g << "  " << std::setw(7) << wl << "  "
           << std::setw(6) << fmtN(o.s / o.g, 2) << "  " << ov[i].first;
        cout << ln.str() << "\n";
    }

    if (lastN > 0) {
        size_t from = (mine.size() > (size_t)lastN) ? mine.size() - (size_t)lastN : 0;
        cout << "\nLast " << (mine.size() - from) << " games:\n";
        for (size_t k = from; k < mine.size(); k++) {
            const RankMatchRow& m = *mine[k];
            bool meWhite = (m.w == id);
            char res = (m.r == 'D') ? 'D' : (((m.r == 'W') == meWhite) ? 'W' : 'L');
            std::ostringstream ln;
            ln << "  " << (m.ts.empty() ? string("?") : m.ts) << "  " << res
               << " as " << (meWhite ? "White" : "Black") << " vs "
               << (meWhite ? m.b : m.w) << " (" << m.plies << " plies)";
            cout << ln.str() << "\n";
        }
    }
    cout << "\n";
    return 0;
}

// ============================================================
// GAUNTLET (rank one candidate vs the frozen pool, O(N) games)
// ============================================================
static bool readRatingsTsv(const string& path, std::map<string, double>& elo) {
    std::ifstream f(path.c_str());
    if (!f.is_open()) return false;
    string line;
    bool first = true;
    while (std::getline(f, line)) {
        if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size() - 1);
        if (first) { first = false; continue; }
        std::vector<string> cols;
        size_t i = 0;
        while (i <= line.size()) {
            size_t t = line.find('\t', i);
            size_t end = (t == string::npos) ? line.size() : t;
            cols.push_back(line.substr(i, end - i));
            if (t == string::npos) break;
            i = t + 1;
        }
        if (cols.size() < 3) continue;
        // The id is always the LAST column, so added metric columns never break this.
        try { elo[cols[cols.size()-1]] = std::stod(cols[1]); } catch (...) {}
    }
    return !elo.empty();
}

int rankGauntlet(const string& rosterFile, const string& storeFile, const string& candidateId,
                 int gamesPerOpp, bool keep, unsigned runSeed, const string& board) {
    if (candidateId.empty()) { cout << "ERROR: --id <candidate id> is required\n"; return 1; }
    std::vector<RankAgent> roster;
    string err;
    if (!rankLoadRosterFile(rosterFile, roster, err)) { cout << "ERROR: " << err << "\n"; return 1; }

    RankAgent cand;
    if (!rankAgentFromId(candidateId, cand, err)) {
        cout << "ERROR: bad candidate id: " << err << "\n";
        return 1;
    }
    for (size_t i = 0; i < roster.size(); i++)
        if (roster[i].id == candidateId && roster[i].active)
            cout << "NOTE: " << candidateId << " is already an active roster agent; "
                 << "a full 'rank.exe run' would rate it from the shared store\n";

    // Frozen pool ratings: prefer the last full fit, else refit in memory.
    std::map<string, double> pool;
    if (!readRatingsTsv("ranking/ratings.tsv", pool)) {
        std::vector<RankMatchRow> rows;
        int skipped = 0;
        rankLoadMatches(storeFile, board, rows, skipped);
        if (!rows.empty()) {
            string anchorId;
            for (size_t i = 0; i < roster.size(); i++) if (roster[i].anchor) anchorId = roster[i].id;
            RankFit fit;
            rankFitBT(rows, anchorId, fit);
            for (size_t i = 0; i < fit.ids.size(); i++) pool[fit.ids[i]] = fit.elo[i];
        }
    }

    std::vector<const RankAgent*> opps;
    int unratedActive = 0;
    for (size_t i = 0; i < roster.size(); i++) {
        if (!roster[i].active || roster[i].id == candidateId) continue;
        if (pool.find(roster[i].id) != pool.end()) opps.push_back(&roster[i]);
        else unratedActive++;
    }
    struct ById {
        bool operator()(const RankAgent* a, const RankAgent* b) const { return a->id < b->id; }
    };
    std::sort(opps.begin(), opps.end(), ById());
    if (opps.empty()) {
        cout << "ERROR: no rated active opponents (run 'rank.exe run' to build the pool first)\n";
        return 1;
    }
    if (unratedActive)
        cout << "NOTE: skipping " << unratedActive << " active agent(s) with no rating yet\n";

    // Existing pair counts drive the seed ordinals when appending to the store.
    std::map<std::pair<string,string>, long long> have;
    if (keep) {
        std::vector<RankMatchRow> rows;
        int skipped = 0;
        rankLoadMatches(storeFile, board, rows, skipped);
        for (size_t k = 0; k < rows.size(); k++) {
            std::pair<string,string> key = (rows[k].w < rows[k].b)
                ? std::make_pair(rows[k].w, rows[k].b) : std::make_pair(rows[k].b, rows[k].w);
            have[key]++;
        }
    }

    string outFile = keep ? storeFile : string("ranking/gauntlet.jsonl");
    ensureDir("ranking");
    if (!keep) { std::ofstream trunc(outFile.c_str(), std::ios::trunc); }

    std::vector<const RankAgent*> all = opps;
    all.push_back(&cand);
    if (!loadModelSlots(all, err)) { cout << "ERROR: " << err << "\n"; return 1; }

    PRNT = 0;
    string stamp = runStamp();
    std::vector<double> oppElo, score;
    long long total = (long long)opps.size() * gamesPerOpp, played = 0;
    cout << "gauntlet: " << candidateId << " vs " << opps.size() << " rated active agents, "
         << gamesPerOpp << " game(s) each = " << total << " games\n" << flush;

    for (size_t oi = 0; oi < opps.size(); oi++) {
        const RankAgent& opp = *opps[oi];
        std::pair<string,string> key = (cand.id < opp.id)
            ? std::make_pair(cand.id, opp.id) : std::make_pair(opp.id, cand.id);
        long long ordinal = keep ? have[key] : 0;
        long long w = 0, l = 0, d = 0;
        for (int g = 0; g < gamesPerOpp; g++) {
            bool candWhite = (g % 2 == 0);
            const RankAgent& wa = candWhite ? cand : opp;
            const RankAgent& ba = candWhite ? opp : cand;
            unsigned seed = gameSeed(wa.id, ba.id, ordinal, runSeed);
            ordinal++;
            srand(seed);
            RankMatchRow m;
            if (!playOneGame(wa, ba, board, m)) {
                cout << "ERROR: cannot load board " << board << "\n";
                return 1;
            }
            m.seed = seed; m.board = board; m.par = 1;
            m.ts = nowUtc(); m.run = stamp;
            dsAppendLine(outFile, rankFormatMatchRow(m));
            played++;
            double sCand = (m.r == 'D') ? 0.5 : ((m.r == 'W') == candWhite) ? 1.0 : 0.0;
            if (m.r == 'D') d++; else if (sCand == 1.0) w++; else l++;
            oppElo.push_back(pool[opp.id]);
            score.push_back(sCand);
            std::ostringstream ln;
            ln << "[" << std::setw(4) << played << "/" << total << "] "
               << m.w << " (W) vs " << m.b << " : " << m.r << " in " << m.plies
               << " plies | vs this opponent " << w << "-" << l;
            cout << ln.str() << "\n" << flush;
        }
    }

    double se = 0.0;
    double elo = rankFitSingle(oppElo, score, se);
    cout << "\ngauntlet result: " << candidateId << "\n";
    cout << "  Elo " << roundElo(elo) << " +/- " << roundElo(se)
         << " (pool ratings held fixed)\n";
    if (keep)
        cout << "  rows appended to " << storeFile << "; add 'on " << candidateId
             << "' to " << rosterFile << " so full refits include it\n";
    else
        cout << "  scratch rows in " << outFile << " (not part of the permanent store)\n";
    return 0;
}

// ============================================================
// EXTRACT
// ============================================================
// Replay a sample of historical matches from the store, capturing labeled value-
// model training positions instead of the summary-only match row. This reuses
// the existing, already-diverse, already-Elo-differentiated agent pool (every
// depth/dilution/evaluator/TT variant ever rated) as a training data source
// instead of a bespoke self-play generator. Games are replayed deterministically
// (same seed, same board) via the exact agents the id encodes; ml_train.cpp's
// selfplay-supervised --from-data fits a model on the file this writes.
// A raw board snapshot, cheap enough to keep one per half-move of a game.
struct BoardSnap { char sq[SIZE][SIZE]; };

static void snapBoard(BoardSnap& s) { std::memcpy(s.sq, board, sizeof(s.sq)); }

// Restore a snapshot and rebuild the incremental counters exactly the way
// reloadBoard seeds them, so play can resume mid-game from the snapshot.
static void restoreBoardSnapshot(const BoardSnap& s) {
    std::memcpy(board, s.sq, sizeof(s.sq));
    g_whiteCount = 0; g_blackCount = 0; g_chipDiff = 0;
    g_whiteAtEnd = 0; g_blackAtEnd = 0;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            if (board[x][y] == WHITE) {
                g_whiteCount++;
                g_chipDiff++;
                if (y == SIZE-1) g_whiteAtEnd++;
            } else if (board[x][y] == BLACK) {
                g_blackCount++;
                g_chipDiff--;
                if (y == 0) g_blackAtEnd++;
            }
        }
}

// Per-ply dilution probability for the pairgen override schedule: linear from
// `start` at ply 0 to `floorProb` at ply `decayPlies`, held at the floor after.
double rankDilutedProb(double start, double floorProb, int decayPlies, int ply) {
    if (decayPlies <= 0) return start;
    if (ply >= decayPlies) return floorProb;
    return start + (floorProb - start) * ((double)ply / (double)decayPlies);
}

// Continue play from the current board state at half-move index startHalf,
// capturing features (and optional board snapshots) before every move.
// dil, if present, is a COLOR mask override (1 = White, 2 = Black, 3 = both)
// that replaces that side's randomMoveProb per half-move; the caller-visible
// specs are never mutated (locals are). openSide is a COLOR mask (same encoding)
// of which side plays a uniform-random legal move for h < openPlies; the unmasked
// side consults its brain even inside the opener window (asymmetric opener). This
// pairgen-level opener composes (via OR) with each agent's own identity-level
// AgentSpec::openerPlies (its `.opener(N)@1` ID segment, if any) -- a move is
// random if EITHER source says so, so pairgen's --open-plies/--open-side and an
// agent's own rostered opener never conflict, they just stack.
static int playoutCapture(const RankAgent& wa, const RankAgent& ba, int startHalf,
                          int featVer, std::vector<int>& capSide,
                          std::vector<std::vector<float> >& capFeat,
                          const RankDilOverride* dil = nullptr, int openPlies = 0,
                          std::vector<BoardSnap>* snaps = nullptr, int openSide = 3) {
    AgentSpec wSpec = wa.spec, bSpec = ba.spec;
    int victor = None;
    for (int h = startHalf; h < 400; h++) {
        int side = (h % 2 == 0) ? White : Black;
        std::vector<float> feat(featVer == 2 ? MLV2_FEATURES : MLV_FEATURES);
        if (featVer == 2) mlExtractValueFeaturesV2(side, feat.data());
        else              mlExtractValueFeatures(side, feat.data());
        capSide.push_back(side);
        capFeat.push_back(feat);
        if (snaps) { snaps->push_back(BoardSnap()); snapBoard(snaps->back()); }
        const AgentSpec& moverSpec = (side == White) ? wSpec : bSpec;
        bool pairgenOpener = (h < openPlies && (openSide & (side == White ? 1 : 2)));
        // The agent's own identity-level opener composes (via OR) with pairgen's.
        bool playedByOpener = false;
        if (!pairgenOpener && moverSpec.openerKind >= 0 && moverSpec.openerKind < g_openerCount)
            playedByOpener = g_openers[moverSpec.openerKind].fn(side, h / 2, moverSpec.openerArg, victor);
        if (pairgenOpener) {
            victor = (side == White) ? pureRandomMoveWhite() : pureRandomMoveBlack();
        } else if (!playedByOpener) {
            if (dil && dil->apply) {
                if (dil->apply & 1)
                    wSpec.randomMoveProb = rankDilutedProb(dil->start, dil->floorProb, dil->decayPlies, h);
                if (dil->apply & 2)
                    bSpec.randomMoveProb = rankDilutedProb(dil->start, dil->floorProb, dil->decayPlies, h);
            }
            victor = agentChooseMove(side == White ? wSpec : bSpec, side);
        }
        if (gameOutcome(victor)) break;
    }
    return victor;
}

static int playOneGameCapture(const RankAgent& wa, const RankAgent& ba, const string& board,
                              int featVer, std::vector<int>& capSide,
                              std::vector<std::vector<float> >& capFeat,
                              const RankDilOverride* dil = nullptr, int openPlies = 0,
                              std::vector<BoardSnap>* snaps = nullptr, int openSide = 3) {
    if (!reloadBoard(board)) return None;
    return playoutCapture(wa, ba, 0, featVer, capSide, capFeat, dil, openPlies, snaps, openSide);
}

// Append one labeled training row per captured position, in the exact format
// ml_train.cpp's loadReplayDataset reads. Returns the number of rows written.
static int emitCapturedRows(std::ofstream& out, int featVer, float label,
                            const std::vector<int>& capSide,
                            const std::vector<std::vector<float> >& capFeat) {
    int positions = 0;
    for (size_t p = 0; p < capFeat.size(); p++) {
        const std::vector<float>& f = capFeat[p];
        std::ostringstream ln;
        if (featVer == 2) {
            ln << "{\"ver\":2,\"stm\":" << (capSide[p] == White ? 1 : -1) << ",\"label\":" << label << ",\"idx\":[";
            bool first = true;
            for (int i = 0; i < MLV2_STM; i++)
                if (f[i] != 0.0f) { if (!first) ln << ","; ln << i; first = false; }
            ln << "]}";
        } else {
            ln << "{\"ver\":1,\"label\":" << label << ",\"f\":[";
            for (int i = 0; i < MLV_FEATURES; i++) { if (i) ln << ","; ln << f[i]; }
            ln << "]}";
        }
        out << ln.str() << "\n";
        positions++;
    }
    return positions;
}

int rankExtract(const string& storeFile, const string& outFile, const string& board,
                int featVer, int sampleN, unsigned seed) {
    if (featVer != 1 && featVer != 2) featVer = 2;
    std::vector<RankMatchRow> rows;
    int skipped = 0;
    rankLoadMatches(storeFile, board, rows, skipped);
    if (rows.empty()) {
        cout << "ERROR: no matches for board " << board << " in " << storeFile << "\n";
        return 1;
    }
    cout << "Loaded " << rows.size() << " match rows (" << skipped << " store-parse skipped) for board " << board << "\n";

    // Deterministic shuffle so a re-run with the same --seed samples the same
    // games, and a bigger --sample is a superset-ish extension of a smaller one
    // (same prefix order) rather than an unrelated draw.
    std::vector<int> order(rows.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
    srand(seed);
    for (size_t i = order.size(); i > 1; i--) {
        size_t j = (size_t)(((double)rand() / ((double)RAND_MAX + 1.0)) * i);
        if (j >= i) j = i - 1;
        std::swap(order[i-1], order[j]);
    }
    int target = (sampleN > 0) ? sampleN : (int)order.size();

    ensureDir("data");
    std::ofstream out(outFile.c_str(), std::ios::trunc);
    if (!out.is_open()) { cout << "ERROR: cannot write " << outFile << "\n"; return 1; }

    int replayed = 0, idSkipped = 0, mismatchSkipped = 0, positions = 0;
    for (size_t k = 0; k < order.size() && replayed < target; k++) {
        const RankMatchRow& row = rows[order[k]];
        RankAgent wa, ba;
        string err;
        if (!rankAgentFromId(row.w, wa, err) || !rankAgentFromId(row.b, ba, err)) { idSkipped++; continue; }
        std::vector<const RankAgent*> pair;
        pair.push_back(&wa); pair.push_back(&ba);
        if (!loadModelSlots(pair, err)) { idSkipped++; continue; }

        srand(row.seed);
        std::vector<int> capSide;
        std::vector<std::vector<float> > capFeat;
        int victor = playOneGameCapture(wa, ba, board, featVer, capSide, capFeat);
        int oc = gameOutcome(victor);
        char r = (oc == 1) ? 'W' : (oc == 2) ? 'B' : 'D';
        if (r != row.r) { mismatchSkipped++; continue; }   // determinism drift guard: don't label an unreproduced game

        float label = (oc == 1) ? 1.0f : (oc == 2) ? 0.0f : 0.5f;
        positions += emitCapturedRows(out, featVer, label, capSide, capFeat);
        replayed++;
        if (replayed % 200 == 0) cout << "  replayed " << replayed << "/" << target << " games, " << positions << " positions\n";
    }
    out.close();
    cout << "Replayed " << replayed << " games (" << idSkipped << " unparseable/stale ids, "
         << mismatchSkipped << " determinism mismatches skipped), " << positions
         << " positions -> " << outFile << " (feature v" << featVer << ")\n";
    mlClearSlots();
    return replayed > 0 ? 0 : 1;
}

// ============================================================
// BOOKGEN
// ============================================================
// Recover the move a replayed half-move made by diffing the pre-move snapshot
// against the live board: the mover's source square lost its piece, the
// destination square gained one (capture or not). Returns false if no clean
// single-move diff is found.
static bool diffMoveFromSnap(const BoardSnap& before, int side, int& sx, int& sy, int& dx) {
    char me = (side == White) ? WHITE : BLACK;
    int fsx = -1, fsy = -1, fdx = -1;
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            if (before.sq[x][y] == me && board[x][y] != me) { fsx = x; fsy = y; }
            if (before.sq[x][y] != me && board[x][y] == me) { fdx = x; }
        }
    if (fsx < 0 || fdx < 0) return false;
    sx = fsx; sy = fsy; dx = fdx;
    return true;
}

int rankBookGen(const string& storeFile, const string& board, const string& idA,
                const string& idB, int maxPlies, const string& outFile) {
    if (idA.empty() || idB.empty()) { cout << "ERROR: bookgen needs --a (line owner) and --b (target)\n"; return 1; }
    if (outFile.empty()) { cout << "ERROR: bookgen needs --out (models/book<N>.txt)\n"; return 1; }
    RankAgent aAg, bAg;
    string err;
    if (!rankAgentFromId(idA, aAg, err)) { cout << "ERROR: --a: " << err << "\n"; return 1; }
    if (!rankAgentFromId(idB, bAg, err)) { cout << "ERROR: --b: " << err << "\n"; return 1; }
    std::vector<const RankAgent*> pairAgents;
    pairAgents.push_back(&aAg);
    pairAgents.push_back(&bAg);
    if (!loadModelSlots(pairAgents, err)) { cout << "ERROR: " << err << "\n"; return 1; }

    std::vector<RankMatchRow> rows;
    int skipped = 0;
    rankLoadMatches(storeFile, board, rows, skipped);
    std::vector<const RankMatchRow*> games;
    for (size_t i = 0; i < rows.size(); i++)
        if ((rows[i].w == idA && rows[i].b == idB) || (rows[i].w == idB && rows[i].b == idA))
            games.push_back(&rows[i]);
    if (games.empty()) {
        cout << "ERROR: no stored games between the two ids for board " << board << "\n";
        return 1;
    }
    cout << "bookgen: " << games.size() << " stored games between the pair, mining A's first "
         << maxPlies << " half-moves per game\n";

    struct BookRec { int sx, sy, dx; int seen; };
    std::map<unsigned long long, BookRec> book;
    int replayed = 0, mismatch = 0, conflicts = 0, diffFail = 0;

    for (size_t g = 0; g < games.size(); g++) {
        const RankMatchRow& row = *games[g];
        bool aIsWhite = (row.w == idA);
        if (!reloadBoard(board)) { cout << "ERROR: cannot load board " << board << "\n"; return 1; }
        srand(row.seed);
        AgentSpec wSpec = (aIsWhite ? aAg : bAg).spec;
        AgentSpec bSpec = (aIsWhite ? bAg : aAg).spec;
        std::vector<std::pair<unsigned long long, BookRec> > rec;
        int victor = None;
        for (int h = 0; h < 400; h++) {
            int side = (h % 2 == 0) ? White : Black;
            bool record = ((side == White) == aIsWhite) && h < maxPlies;
            unsigned long long key = 0;
            BoardSnap before;
            if (record) { key = (unsigned long long)positionKey(side, false).hash; snapBoard(before); }
            const AgentSpec& moverSpec = (side == White) ? wSpec : bSpec;
            bool playedByOpener = false;
            if (moverSpec.openerKind >= 0 && moverSpec.openerKind < g_openerCount)
                playedByOpener = g_openers[moverSpec.openerKind].fn(side, h / 2, moverSpec.openerArg, victor);
            if (!playedByOpener) victor = agentChooseMove(moverSpec, side);
            if (record) {
                BookRec br;
                br.seen = 1;
                if (diffMoveFromSnap(before, side, br.sx, br.sy, br.dx))
                    rec.push_back(std::make_pair(key, br));
                else
                    diffFail++;
            }
            if (gameOutcome(victor)) break;
        }
        int oc = gameOutcome(victor);
        char r = (oc == 1) ? 'W' : (oc == 2) ? 'B' : 'D';
        if (r != row.r) mismatch++;   // replay drift (cross-game state, theory 19b): informational only
        // Keep only winning lines: a drifted replay is still a real game between
        // the two agents, and its recordings are book-worthy iff A won IT.
        bool aWon = (oc == 1 && aIsWhite) || (oc == 2 && !aIsWhite);
        if (!aWon) continue;
        for (size_t i = 0; i < rec.size(); i++) {
            std::map<unsigned long long, BookRec>::iterator it = book.find(rec[i].first);
            if (it == book.end()) {
                book[rec[i].first] = rec[i].second;
            } else if (it->second.sx == rec[i].second.sx && it->second.sy == rec[i].second.sy
                       && it->second.dx == rec[i].second.dx) {
                it->second.seen++;
            } else {
                conflicts++;   // same position, different A move in another game: first-seen wins
            }
        }
        replayed++;
    }

    std::ofstream out(outFile.c_str(), std::ios::trunc);
    if (!out.is_open()) { cout << "ERROR: cannot write " << outFile << "\n"; return 1; }
    out << "# book v1 (rank.exe bookgen)\n";
    out << "# a (line owner): " << idA << "\n";
    out << "# b (target):     " << idB << "\n";
    out << "# board " << board << ", max half-moves " << maxPlies
        << ", A-won replays kept " << replayed << " of " << games.size()
        << " (replay drift vs stored result: " << mismatch
        << "), entries " << book.size() << ", move conflicts " << conflicts << "\n";
    char hex[24];
    for (std::map<unsigned long long, BookRec>::const_iterator it = book.begin(); it != book.end(); ++it) {
        snprintf(hex, sizeof(hex), "%016llx", it->first);
        out << hex << " " << it->second.sx << " " << it->second.sy << " " << it->second.dx << "\n";
    }
    out.close();
    cout << "Kept " << replayed << " A-won replays of " << games.size() << " stored games ("
         << mismatch << " replays drifted from the stored result, " << diffFail
         << " move-diff failures), " << book.size() << " book entries ("
         << conflicts << " conflicting duplicates dropped) -> " << outFile << "\n";
    mlClearSlots();
    return book.empty() ? 1 : 0;
}

// ============================================================
// PAIRGEN
// ============================================================
// Generate FRESH labeled training games between two named agents (the queued
// pool-pair generation idea), instead of replaying stored history (extract) or
// one teacher's self-play (train.exe). First use: the vs-champion training
// study, where agent B is the reigning champion and agent A is a generator,
// an oracle, or a diluted champion copy. See rankPairGen in ranking.h for the
// knob semantics (dilution override, open plies, winner filter, branch mining).

static const char* dilApplyName(int apply) {
    return (apply == 1) ? "a" : (apply == 2) ? "b" : (apply == 3) ? "both" : "none";
}
static const char* filterName(int fw) {
    return (fw == 1) ? "a" : (fw == 2) ? "b" : "any";
}
static const char* openSideName(int os) {
    return (os == 1) ? "a" : (os == 2) ? "b" : (os == 3) ? "both" : "none";
}

int rankPairGen(const string& idA, const string& idB, int games, const string& outFile,
                const string& board, int featVer, unsigned runSeed,
                const RankDilOverride& dil, int openPlies, int filterWinner,
                int branchTries, int shard, int ofK, int openSide) {
    if (featVer != 1 && featVer != 2) featVer = 2;
    if (ofK < 1) ofK = 1;
    if (shard < 0 || shard >= ofK) { cout << "ERROR: --shard must be in [0, --of)\n"; return 1; }
    if (games <= 0)                { cout << "ERROR: --games must be positive\n"; return 1; }
    if (idA.empty() || idB.empty()) { cout << "ERROR: pairgen needs --a <id> and --b <id>\n"; return 1; }

    RankAgent A, B;
    string err;
    if (!rankAgentFromId(idA, A, err)) { cout << "ERROR: --a: " << err << "\n"; return 1; }
    if (!rankAgentFromId(idB, B, err)) { cout << "ERROR: --b: " << err << "\n"; return 1; }
    std::vector<const RankAgent*> pairAgents;
    pairAgents.push_back(&A);
    pairAgents.push_back(&B);
    if (!loadModelSlots(pairAgents, err)) { cout << "ERROR: " << err << "\n"; return 1; }

    ensureDir("data");
    std::ofstream out(outFile.c_str(), std::ios::trunc);
    if (!out.is_open()) { cout << "ERROR: cannot write " << outFile << "\n"; return 1; }

    int played = 0, kept = 0, aWins = 0, bWins = 0, draws = 0, positions = 0;
    int brTried = 0, brKept = 0, brPositions = 0;
    // Color-stratified tallies over ALL played games (not just kept ones), so
    // the recipe's actual white/black split and per-color win rate can be
    // read back later instead of assumed. Tracked from A's perspective: which
    // color A held that game (aWhiteGames/aBlackGames) and how A did in each
    // (aWhiteWins/aBlackWins); B's per-color record is the complement (B was
    // Black whenever A was White, and vice versa).
    int aWhiteGames = 0, aWhiteWins = 0, aWhiteDraws = 0;
    int aBlackGames = 0, aBlackWins = 0, aBlackDraws = 0;
    for (int g = 0; g < games; g++) {
        if (g % ofK != shard) continue;
        bool aWhite = (g % 2 == 0);
        const RankAgent& wa = aWhite ? A : B;
        const RankAgent& ba = aWhite ? B : A;

        // Map the A/B dilution choice onto this game's colors.
        RankDilOverride cd = dil;
        cd.apply = (dil.apply == 1) ? (aWhite ? 1 : 2)
                 : (dil.apply == 2) ? (aWhite ? 2 : 1)
                 : dil.apply;

        // Same A/B -> color mapping for the opener-side mask.
        int gOpenSide = (openSide == 1) ? (aWhite ? 1 : 2)
                      : (openSide == 2) ? (aWhite ? 2 : 1)
                      : openSide;

        srand(gameSeed(wa.id, ba.id, g, runSeed));
        std::vector<int> capSide;
        std::vector<std::vector<float> > capFeat;
        std::vector<BoardSnap> snaps;
        int victor = playOneGameCapture(wa, ba, board, featVer, capSide, capFeat,
                                        cd.apply ? &cd : nullptr, openPlies,
                                        branchTries > 0 ? &snaps : nullptr, gOpenSide);
        int oc = gameOutcome(victor);
        played++;
        int winnerAB = (oc == 0) ? 0 : (((oc == 1) == aWhite) ? 1 : 2);
        if (winnerAB == 1) aWins++; else if (winnerAB == 2) bWins++; else draws++;
        if (aWhite) {
            aWhiteGames++;
            if (oc == 1) aWhiteWins++; else if (oc == 0) aWhiteDraws++;
        } else {
            aBlackGames++;
            if (oc == 2) aBlackWins++; else if (oc == 0) aBlackDraws++;
        }

        bool keep = (filterWinner == 0) || (winnerAB == filterWinner);
        if (keep) {
            float label = (oc == 1) ? 1.0f : (oc == 2) ? 0.0f : 0.5f;
            positions += emitCapturedRows(out, featVer, label, capSide, capFeat);
            kept++;
        }

        // Branch-from-win: mine alternative winning lines out of kept games
        // agent A won. Rewind to a random ply where A was to move (never the
        // game's final move), substitute a different legal move, play out
        // clean (no dilution override), and keep the tail only if A wins
        // again. The shared prefix is not re-emitted (the base game already
        // covers it), so only the positions after the divergence are new.
        if (branchTries > 0 && keep && winnerAB == 1 && capSide.size() >= 2) {
            int aColor = aWhite ? White : Black;
            std::vector<int> aPlies;
            for (size_t h = 0; h + 1 < capSide.size(); h++)
                if (capSide[h] == aColor) aPlies.push_back((int)h);
            for (int bi = 0; bi < branchTries && !aPlies.empty(); bi++) {
                brTried++;
                string bk = wa.id + "|" + ba.id + "|" + std::to_string(g) + "|br"
                          + std::to_string(bi) + "|" + std::to_string(runSeed);
                srand((unsigned)(fnv1a64(bk.data(), bk.size(), 1469598103934665603ULL) & 0xffffffffULL));

                int t = aPlies[rand() % aPlies.size()];
                restoreBoardSnapshot(snaps[t]);
                Move mv[ML_MAX_MOVES];
                int n = generateMoves(aColor, mv);
                if (n <= 1) continue;   // no alternative exists at this ply

                // Identify the originally played move: the unique candidate
                // that transforms snapshot t into snapshot t+1.
                int playedIdx = -1;
                for (int i = 0; i < n && playedIdx < 0; i++) {
                    BoardSnap sim = snaps[t];
                    sim.sq[mv[i].sx][mv[i].sy] = EMPTY;
                    sim.sq[mv[i].dx][mv[i].dy] = (aColor == White) ? WHITE : BLACK;
                    if (std::memcmp(sim.sq, snaps[t+1].sq, sizeof(sim.sq)) == 0) playedIdx = i;
                }
                int pick = rand() % (playedIdx >= 0 ? n - 1 : n);
                if (playedIdx >= 0 && pick >= playedIdx) pick++;

                int bv = (aColor == White)
                    ? playMoveWhite(mv[pick].sx, mv[pick].sy, mv[pick].dx)
                    : playMoveBlack(mv[pick].sx, mv[pick].sy, mv[pick].dx);
                std::vector<int> brSide;
                std::vector<std::vector<float> > brFeat;
                if (!gameOutcome(bv))
                    bv = playoutCapture(wa, ba, t + 1, featVer, brSide, brFeat);
                int boc = gameOutcome(bv);
                if (boc != 0 && ((boc == 1) == aWhite)) {   // A won the branch too
                    float label = (boc == 1) ? 1.0f : 0.0f;
                    brPositions += emitCapturedRows(out, featVer, label, brSide, brFeat);
                    brKept++;
                }
            }
        }

        if (played % 100 == 0)
            cout << "  played " << played << " games, kept " << kept
                 << ", A record " << aWins << "-" << bWins << "-" << draws
                 << ", " << (positions + brPositions) << " positions\n" << flush;
    }
    out.close();

    // Provenance sidecar: the full generation recipe plus outcome tallies, so
    // a model's teacher=replay:<file> line stays traceable to how the file
    // was made (the known provenance-gap fix for generated datasets).
    {
        std::ofstream meta((outFile + ".meta.json").c_str(), std::ios::trunc);
        if (meta.is_open()) {
            meta << "{\"a\":\"" << idA << "\",\"b\":\"" << idB << "\""
                 << ",\"games\":" << games << ",\"shard\":" << shard << ",\"of\":" << ofK
                 << ",\"seed\":" << runSeed << ",\"board\":\"" << board << "\""
                 << ",\"feature_version\":" << featVer
                 << ",\"dil_apply\":\"" << dilApplyName(dil.apply) << "\""
                 << ",\"dil_start\":" << dil.start << ",\"dil_floor\":" << dil.floorProb
                 << ",\"dil_decay_plies\":" << dil.decayPlies
                 << ",\"open_plies\":" << openPlies
                 << ",\"open_side\":\"" << openSideName(openSide) << "\""
                 << ",\"filter_winner\":\"" << filterName(filterWinner) << "\""
                 << ",\"branch_tries\":" << branchTries
                 << ",\"played\":" << played << ",\"kept\":" << kept
                 << ",\"a_wins\":" << aWins << ",\"b_wins\":" << bWins << ",\"draws\":" << draws
                 << ",\"a_white_games\":" << aWhiteGames << ",\"a_white_wins\":" << aWhiteWins
                 << ",\"a_white_draws\":" << aWhiteDraws
                 << ",\"a_black_games\":" << aBlackGames << ",\"a_black_wins\":" << aBlackWins
                 << ",\"a_black_draws\":" << aBlackDraws
                 << ",\"positions\":" << positions
                 << ",\"branch_tried\":" << brTried << ",\"branch_kept\":" << brKept
                 << ",\"branch_positions\":" << brPositions << "}\n";
        }
    }

    cout << "pairgen: " << played << " games played, " << kept << " kept ("
         << filterName(filterWinner) << " filter), A record " << aWins << "-" << bWins
         << "-" << draws << " (W-L-D), " << positions << " positions"
         << "; A as White " << aWhiteWins << "-" << (aWhiteGames - aWhiteWins - aWhiteDraws)
         << "-" << aWhiteDraws << " (" << aWhiteGames << " games), A as Black "
         << aBlackWins << "-" << (aBlackGames - aBlackWins - aBlackDraws) << "-" << aBlackDraws
         << " (" << aBlackGames << " games)";
    if (branchTries > 0)
        cout << "; branches " << brKept << "/" << brTried << " kept, +" << brPositions
             << " positions";
    cout << " -> " << outFile << " (feature v" << featVer << ")\n";
    mlClearSlots();
    return kept > 0 ? 0 : 1;
}

// ============================================================
// OPENER-BIAS MECHANISM MEASUREMENT
// ============================================================
// Directly measures whether the symmetric random opener leaves the deterministic
// champion (agent A) in a worse position than it would have chosen. For each
// seeded game it replays the both-random opener exactly as pairgen does (RNG
// faithful), then, for every ply where A was to move, uses A's own search as a
// neutral judge to score the line after A's forced random move against the line
// after A's own best move. delta = champRel(own) - champRel(random); a positive
// delta means the random opener cost A. Reported as mean delta and the fraction
// of A-plies/games past a one-chip materiality threshold, split by A's color.
int rankOpenerBias(const string& idA, const string& idB, int games,
                   const string& board, int openPlies, unsigned runSeed,
                   const string& judgeId) {
    if (games <= 0)     { cout << "ERROR: --games must be positive\n"; return 1; }
    if (openPlies <= 0) { cout << "ERROR: opener-bias needs --open-plies > 0\n"; return 1; }
    if (idA.empty() || idB.empty()) { cout << "ERROR: opener-bias needs --a <champion id> and --b <id>\n"; return 1; }

    string jId = judgeId.empty() ? idA : judgeId;
    RankAgent A, B, J;
    string err;
    if (!rankAgentFromId(idA, A, err)) { cout << "ERROR: --a: " << err << "\n"; return 1; }
    if (!rankAgentFromId(idB, B, err)) { cout << "ERROR: --b: " << err << "\n"; return 1; }
    if (!rankAgentFromId(jId, J, err)) { cout << "ERROR: --judge: " << err << "\n"; return 1; }
    std::vector<const RankAgent*> pairAgents;
    pairAgents.push_back(&A); pairAgents.push_back(&B); pairAgents.push_back(&J);
    if (!loadModelSlots(pairAgents, err)) { cout << "ERROR: " << err << "\n"; return 1; }

    // Sign-based accounting so the report is independent of the judge's eval
    // scale (Classic is integer chip units; a learned judge is tanh*out_scale).
    // EPS treats exact ties as neutral; mean delta still carries magnitude in
    // whatever units the judge produces.
    const double EPS = 1e-6;

    struct Acc {
        long   plies = 0, worse = 0, better = 0, gamesTouched = 0, gamesWorse = 0;
        double sumDelta = 0.0;
    } accW, accB;

    for (int g = 0; g < games; g++) {
        bool aWhite = (g % 2 == 0);
        int champColor = aWhite ? White : Black;
        int oppColor   = aWhite ? Black : White;
        Acc& acc = aWhite ? accW : accB;

        // Pass 1 (RNG-faithful): replay the both-random opener exactly as pairgen
        // would, recording the pre-move and post-random-move board for each champ ply.
        srand(gameSeed(A.id, B.id, g, runSeed));
        if (!reloadBoard(board)) { cout << "ERROR: cannot load board " << board << "\n"; return 1; }
        std::vector<BoardSnap> preSnap, randSnap;
        bool gameEnded = false;
        for (int h = 0; h < openPlies && !gameEnded; h++) {
            int side = (h % 2 == 0) ? White : Black;
            BoardSnap pre;
            if (side == champColor) { snapBoard(pre); }
            int v = (side == White) ? pureRandomMoveWhite() : pureRandomMoveBlack();
            if (side == champColor) {
                preSnap.push_back(pre);
                BoardSnap post; snapBoard(post); randSnap.push_back(post);
                // A game decided inside a 6-ply opener is impossible in Breakthrough
                // (no piece can cross the board that fast), but guard anyway.
                if (gameOutcome(v)) { preSnap.pop_back(); randSnap.pop_back(); gameEnded = true; }
            } else if (gameOutcome(v)) {
                gameEnded = true;
            }
        }

        // Pass 2 (counterfactual; RNG no longer matters): score each recorded ply.
        // Both the champion's own-move position and its forced-random position are
        // scored with the OPPONENT to move (it is the opponent's turn after either
        // champion move), so the eval's turn term is symmetric and cancels -- the
        // delta reflects position quality, not a tempo offset.
        bool touched = false, anyWorse = false;
        for (size_t k = 0; k < preSnap.size(); k++) {
            // Champion's own move (A picks), then score the resulting position
            // (opp to move) with the JUDGE agent's search.
            restoreBoardSnapshot(preSnap[k]);
            agentChooseMove(A.spec, champColor);   // board -> position after champ's best move
            agentChooseMove(J.spec, oppColor);     // judge scores it as the opponent's best reply
            double wcOwn = (oppColor == White) ? (double)g_downEvalWhite : (double)g_downEvalBlack;

            // Forced-random position, scored the same way (judge, opp to move).
            restoreBoardSnapshot(randSnap[k]);
            agentChooseMove(J.spec, oppColor);
            double wcRand = (oppColor == White) ? (double)g_downEvalWhite : (double)g_downEvalBlack;

            double champOwn  = (champColor == White) ? wcOwn  : -wcOwn;
            double champRand = (champColor == White) ? wcRand : -wcRand;
            double delta = champOwn - champRand;   // >0 => random opener hurt the champion

            acc.plies++;
            acc.sumDelta += delta;
            if (delta > EPS) { acc.worse++; anyWorse = true; }
            else if (delta < -EPS) acc.better++;
            touched = true;
        }
        if (touched) { acc.gamesTouched++; if (anyWorse) acc.gamesWorse++; }

        if ((g + 1) % 20 == 0)
            cout << "  scored " << (g + 1) << "/" << games << " games\n" << flush;
    }
    mlClearSlots();

    // Report.
    cout << "\nopener-bias: champion=" << idA << " vs " << idB
         << ", judge=" << jId << ", open-plies=" << openPlies << ", games=" << games << "\n";
    cout << "delta = champion-relative [own-move line value] - [forced-random line value]"
         << " (judge eval units);\n"
         << "positive => the random opener left the champion objectively worse off.\n\n";
    cout << "  color |   plies | mean delta | % hurt  | % helped | games hurt\n";
    cout << "  ------+---------+------------+---------+----------+------------\n";
    struct Row { const char* name; Acc* a; } rows[2] = { {"White", &accW}, {"Black", &accB} };
    Acc all;
    for (int i = 0; i < 2; i++) {
        Acc& a = *rows[i].a;
        double mean = a.plies ? a.sumDelta / a.plies : 0.0;
        double pw = a.plies ? 100.0 * a.worse  / a.plies : 0.0;
        double pb = a.plies ? 100.0 * a.better / a.plies : 0.0;
        char buf[256];
        snprintf(buf, sizeof(buf), "  %-5s | %7ld | %10.2f | %6.1f%% | %7.1f%% | %ld/%ld\n",
                 rows[i].name, a.plies, mean, pw, pb, a.gamesWorse, a.gamesTouched);
        cout << buf;
        all.plies += a.plies; all.worse += a.worse; all.better += a.better;
        all.gamesTouched += a.gamesTouched; all.gamesWorse += a.gamesWorse;
        all.sumDelta += a.sumDelta;
    }
    {
        double mean = all.plies ? all.sumDelta / all.plies : 0.0;
        double pw = all.plies ? 100.0 * all.worse  / all.plies : 0.0;
        double pb = all.plies ? 100.0 * all.better / all.plies : 0.0;
        char buf[256];
        snprintf(buf, sizeof(buf), "  %-5s | %7ld | %10.2f | %6.1f%% | %7.1f%% | %ld/%ld\n",
                 "both", all.plies, mean, pw, pb, all.gamesWorse, all.gamesTouched);
        cout << "  ------+---------+------------+---------+----------+------------\n" << buf;
    }
    return all.plies > 0 ? 0 : 1;
}

// Play from the current board state (no reload) to conclusion; unlike
// playOneGame, takes no match-row telemetry and does not touch the board on
// entry, so the caller controls exactly which position play resumes from.
static int playToConclusion(const RankAgent& wa, const RankAgent& ba, int startHalf) {
    int victor = None;
    for (int h = startHalf; h < 400; h++) {
        int side = (h % 2 == 0) ? White : Black;
        const RankAgent& ag = (side == White) ? wa : ba;
        victor = agentChooseMove(ag.spec, side);
        if (gameOutcome(victor)) break;
    }
    return victor;
}

int rankOpenerSwap(const string& idA, const string& idB, int games,
                   const string& board, int openPlies, unsigned runSeed) {
    if (games <= 0)     { cout << "ERROR: --games must be positive\n"; return 1; }
    if (openPlies <= 0) { cout << "ERROR: opener-swap needs --open-plies > 0\n"; return 1; }
    if (idA.empty() || idB.empty()) { cout << "ERROR: opener-swap needs --a <id> and --b <id>\n"; return 1; }

    RankAgent A, B;
    string err;
    if (!rankAgentFromId(idA, A, err)) { cout << "ERROR: --a: " << err << "\n"; return 1; }
    if (!rankAgentFromId(idB, B, err)) { cout << "ERROR: --b: " << err << "\n"; return 1; }
    std::vector<const RankAgent*> pairAgents;
    pairAgents.push_back(&A); pairAgents.push_back(&B);
    if (!loadModelSlots(pairAgents, err)) { cout << "ERROR: " << err << "\n"; return 1; }

    long bothWhite = 0, bothBlack = 0, bothA = 0, bothB = 0, inconclusive = 0;
    for (int g = 0; g < games; g++) {
        if (!reloadBoard(board)) { cout << "ERROR: cannot load board " << board << "\n"; return 1; }
        srand(gameSeed(A.id, B.id, g, runSeed));
        bool endedInOpener = false;
        for (int h = 0; h < openPlies && !endedInOpener; h++) {
            int side = (h % 2 == 0) ? White : Black;
            int v = (side == White) ? pureRandomMoveWhite() : pureRandomMoveBlack();
            if (gameOutcome(v)) endedInOpener = true;   // effectively impossible at small openPlies
        }
        if (endedInOpener) { inconclusive++; continue; }
        BoardSnap snap;
        snapBoard(snap);

        // Continuation 1: A = White, B = Black.
        int oc1 = gameOutcome(playToConclusion(A, B, openPlies));

        // Continuation 2: SAME snapshot, assignment swapped (B = White, A = Black).
        restoreBoardSnapshot(snap);
        int oc2 = gameOutcome(playToConclusion(B, A, openPlies));

        if (oc1 == 0 || oc2 == 0) { inconclusive++; continue; }   // a draw in either leg
        bool game1WhiteWon = (oc1 == 1);   // continuation 1: White == A
        bool game2WhiteWon = (oc2 == 1);   // continuation 2: White == B
        if (game1WhiteWon && game2WhiteWon)        bothWhite++;   // A-as-White and B-as-White both won
        else if (!game1WhiteWon && !game2WhiteWon) bothBlack++;   // B-as-Black and A-as-Black both won
        else if (game1WhiteWon && !game2WhiteWon)  bothA++;       // A won as White AND as Black
        else                                        bothB++;      // B won as White AND as Black

        if ((g + 1) % 20 == 0)
            cout << "  scored " << (g + 1) << "/" << games << " snapshots\n" << flush;
    }
    mlClearSlots();

    long classified = bothWhite + bothBlack + bothA + bothB;
    cout << "\nopener-swap: a=" << idA << " vs b=" << idB
         << ", open-plies=" << openPlies << ", snapshots=" << games << "\n";
    cout << "Each snapshot is played to conclusion twice from the SAME random-opener\n"
         << "position: once A=White/B=Black, once swapped. A result is classified by\n"
         << "who won BOTH continuations.\n\n";
    if (classified == 0) {
        cout << "no classified snapshots (" << inconclusive << " inconclusive/drawn)\n";
        return 1;
    }
    cout << "  outcome                        count   % of classified\n";
    cout << "  ------------------------------ ------- ----------------\n";
    cout << "  White won both (color effect)  " << bothWhite << "\t" << (100.0 * bothWhite / classified) << "%\n";
    cout << "  Black won both (color effect)  " << bothBlack << "\t" << (100.0 * bothBlack / classified) << "%\n";
    cout << "  " << idA << " won both (agent effect)\n    -> " << bothA << "\t" << (100.0 * bothA / classified) << "%\n";
    cout << "  " << idB << " won both (agent effect)\n    -> " << bothB << "\t" << (100.0 * bothB / classified) << "%\n";
    cout << "  inconclusive (draw in either leg): " << inconclusive << " of " << games << " snapshots\n";
    cout << "\ncolor effect total: " << (bothWhite + bothBlack) << "/" << classified
         << " (" << (100.0 * (bothWhite + bothBlack) / classified) << "%), agent effect total: "
         << (bothA + bothB) << "/" << classified << " (" << (100.0 * (bothA + bothB) / classified) << "%)\n";
    return 0;
}

// ============================================================
// CHECK
// ============================================================
int rankCheck(const string& rosterFile, const string& storeFile, int gamesPerPair,
              const string& board) {
    string err;
    if (!rankEvalCodecComplete(err)) { cout << "ERROR: " << err << "\n"; return 1; }

    // Model hashes first: they are what a user needs to paste into a learned
    // agent's id, and they help even when the roster fails to parse.
    for (int slot = 0; slot < ML_SLOTS; slot++) {
        string h = rankFileHash8(slotFile(slot));
        if (!h.empty())
            cout << "model hash: " << slotFile(slot) << " = " << h << " (slot " << slot << ")\n";
    }

    std::vector<RankAgent> roster;
    if (!rankLoadRosterFile(rosterFile, roster, err)) { cout << "ERROR: " << err << "\n"; return 1; }

    int nAct = 0;
    cout << "roster " << rosterFile << ": " << roster.size() << " agents\n";
    for (size_t i = 0; i < roster.size(); i++) {
        const char* st = roster[i].anchor ? "anchor" : (roster[i].active ? "on" : "off");
        std::ostringstream ln;
        ln << "  " << std::left << std::setw(7) << st << " " << roster[i].id;
        cout << ln.str() << "\n";
        if (roster[i].active) nAct++;
    }

    std::vector<RankMatchRow> store;
    int skipped = 0;
    rankLoadMatches(storeFile, board, store, skipped);
    if (skipped) cout << "WARNING: skipped " << skipped << " malformed line(s) in " << storeFile << "\n";
    std::vector<RankPendingGame> pending = rankSchedule(roster, store, gamesPerPair, 1);
    long long pairs = (long long)nAct * (nAct - 1) / 2;
    cout << nAct << " active agents, " << pairs << " pairs, " << store.size()
         << " stored games (board " << board << "), " << pending.size()
         << " pending at --games " << gamesPerPair << "\n";
    cout << "OK\n";
    return 0;
}
