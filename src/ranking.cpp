// ============================================================
// Persistent agent Elo ranking (rank.exe) -- see ranking.h
// ============================================================
// Sections: SMALL UTILITIES, ID CODEC, ROSTER, MATCH STORE, SCHEDULER,
// BRADLEY-TERRY FIT, GAME RUNNER, RATE + REPORTS, HISTORY, GAUNTLET, EXTRACT,
// DETERMINISM PROBE, REFUTE, CHECK.
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
#include "ml_cluster.h"
#include "ml_features.h"   // generateMoves (refute: enumerating a node's untried moves)
#include "datastore.h"
#include "transposition.h"
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
// conventions; slots 3..(ML_SLOTS-ML_RESERVED_SLOTS-1) are a generic
// sweep/experiment convention so many independently-trained candidates can each
// get a permanent identity (a slot + file) and be rated together in one process,
// instead of one shared file being swapped serially between gauntlet calls. The
// top ML_RESERVED_SLOTS slots are permanently reserved scratch space (see
// ML_RESERVED_SLOTS, ml_eval.h) and resolve to a SEPARATE models/scratch/
// directory that no roster-tracked identity is ever written into, so ephemeral
// test/tooling writes there can never collide with a live agent's model file.
// Exported (not file-local) so callers building a scratch path -- the test
// suite included -- go through this one implementation rather than each
// re-deriving the naming convention themselves and risking divergence.
string rankSlotFile(int slot) {
    if (slot == 0) return "models/lin_value.txt";
    if (slot == 1) return "models/lin_policy.txt";
    if (slot == 2) return "models/pst_value.txt";   // sparse piece-square value model (feature v2, incremental)
    if (slot >= ML_SLOTS - ML_RESERVED_SLOTS && slot < ML_SLOTS)
        return "models/scratch/slot" + std::to_string(slot) + ".txt";
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

// Thousands-separated integer, for report.md columns large enough to be hard
// to read at a glance (games, nodes/move can run into six figures).
static string fmtInt(long long v) {
    bool neg = v < 0;
    string digits = std::to_string(neg ? -v : v);
    string out;
    int cnt = 0;
    for (int i = (int)digits.size() - 1; i >= 0; i--) {
        out += digits[i];
        if (++cnt % 3 == 0 && i != 0) out += ',';
    }
    std::reverse(out.begin(), out.end());
    return neg ? ("-" + out) : out;
}

// ============================================================
// LABELLED NUMBERS
// ============================================================
// Every number in an id carries a label saying WHAT IT IS, not merely what unit
// it is in: `deep6` rather than `d6` or `6ply`, because many different
// quantities here are measured in plies and "ply" alone would conflate a search
// depth with a dilution depth with an opening cutoff. `ply` is used only where
// the number genuinely IS a position on the game clock (the book cutoff).
// Labels precede their number, matching the grammar's existing shape.
//
// Legacy short labels still PARSE so the store's existing rows keep resolving to
// the agents the roster names; only the current label is ever EMITTED, and
// rankUpgradeIds rewrites stored ids on read.
//
// Longest label first: `deep6` must not be eaten by the legacy `d` alias.
static bool labelledNum(const string& tok, const char* const* labels, int nLabels,
                        string& tail) {
    for (int i = 0; i < nLabels; i++) {
        const size_t L = std::strlen(labels[i]);
        if (tok.size() > L && tok.compare(0, L, labels[i]) == 0) {
            tail = tok.substr(L);
            if (!tail.empty()) return true;
        }
    }
    return false;
}
// An UNDERSCORE separates a label from its number: `model_111`, not `model111`,
// because glyphs like l and 1 run together without a visual break. Longest
// spelling first so `deep_6` is never eaten by the legacy `d` alias.
// `=` separates a label from its VALUE; `_` only ever joins words inside a
// name (mu_shape, teacher_games). Keeping the two jobs on different characters
// is what makes mu_shape=129-512-8-1 readable. Longest spelling first so
// `deep=6` is never eaten by the legacy `d` alias.
static const char* const LBL_DEEP[]    = { "deep=", "deep_", "deep", "d" };
static const char* const LBL_MAXDEEP[] = { "maxdeep=", "maxdeep_", "maxdeep", "cap" };
static const char* const LBL_MARGIN[]  = { "margin=", "margin_", "margin", "asp" };
// Minimum unspent share of the node budget needed to start another deepening
// iteration. New field, so one spelling only (see AgentSpec::iterMinRemain).
static const char* const LBL_REM[]     = { "rem=" };
static const char* const LBL_NODES[]   = { "nodes=", "nodes_", "nodes", "nb" };
static const char* const LBL_TIME[]    = { "time=", "time_", "time", "tb" };
// Declarative calibration target: recorded in the id, never read by the search
// (see AgentSpec::calTargetMs). New field, so one spelling only.
static const char* const LBL_CAL[]     = { "cal=" };
static const char* const LBL_PROB[]    = { "prob=", "prob_", "prob", "r" };
static const char* const LBL_PIECES[]  = { "pieces=", "pieces_", "pieces" };
static const char* const LBL_PLY[]     = { "ply=", "ply_", "ply" };
static const char* const LBL_MODEL[]   = { "model=", "model_", "model", "s" };
static const char* const LBL_CONN[]    = { "conn=", "conn_", "conn", "con" };
static const char* const LBL_RISK[]    = { "risk=" };   // no legacy spelling: new field, one form only
static const char* const LBL_SIMS[]    = { "sims=" };   // no legacy spelling: new field, one form only
static const char* const LBL_CVISIT[]  = { "cvisit=" };  // no legacy spelling: new field, one form only
static const char* const LBL_CSCALE[]  = { "cscale=" };  // no legacy spelling: new field, one form only
static const char* const LBL_ROOTM[]   = { "m=" };        // no legacy spelling: new field, one form only
static const int LBLN_DEEP = 4, LBLN_MAXDEEP = 4, LBLN_MARGIN = 4, LBLN_NODES = 4;
static const int LBLN_TIME = 4, LBLN_PROB = 4, LBLN_PIECES = 3, LBLN_PLY = 3;
static const int LBLN_MODEL = 4, LBLN_CONN = 4, LBLN_RISK = 1, LBLN_SIMS = 1;
static const int LBLN_CAL = 1, LBLN_REM = 1;
static const int LBLN_CVISIT = 1, LBLN_CSCALE = 1, LBLN_ROOTM = 1;

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
    { "Greedy",     "greedy", 1 },
    { "AlphaBeta",  "ab",     3 },   // @2: TT cross-agent contamination fix (searcher-context key).
                                     // @3: time= budget enforcement (sticky expiry + 256-node sampling +
                                     // nextIterationFits). Migrated by tools/migrate_ab_v3.py, which kept the
                                     // 685,210 rows no time= head played and dropped the 324,570 it did.
                                     // Both bumps: see Docs/corrections.md
    { "GumbelMCTS", "gaz",    1 },
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

// Current code version of a registry module, by registry name. Lets code that
// needs to NAME a specific agent compose a canonical ID instead of hardcoding
// an "@N" that goes stale the next time that module's behavior changes.
static int rkExplorerVersion(const string& regName) {
    for (int i = 0; i < g_rkExplorerCount; i++)
        if (regName == g_rkExplorers[i].regName) return g_rkExplorers[i].version;
    return 1;
}
static int rkEvalVersion(const string& regName) {
    for (int i = 0; i < g_rkEvalCount; i++)
        if (regName == g_rkEvals[i].regName) return g_rkEvals[i].version;
    return 1;
}
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

// ---- Learned-model architecture descriptor (ID enrichment) ----
// A `learned()` ID used to read `learned(s111,78ef6974)`, which made every model in
// the project look alike: only a slot number and a content hash, no hint of whether
// the evaluator was a 130-parameter linear map or a 71k-parameter MLP. Compared with
// the heuristic IDs, which spell out every weight, that was unusable in reporting.
// The ID now carries the architecture, read out of the model file:
//
//   learned(s111,78ef6974,dist,mlp,129-512-8-1,sig129-64-1,con100)@1
//   learned(s76,ef183148,dist,lin,129-1,sig129-1,con100)@1
//   learned(s98,5801570e,value,lin,129-1,con100)@1
//
// Fields after the hash: recipe (`dist` = mu/sigma heads from the position-oracle
// pipeline, `value` = a plain outcome-trained head), mu head type (`mlp`/`lin`), mu
// layer shape, an optional `sig<shape>` for the dist sigma head (search never reads
// it, but it distinguishes training recipes), and `con<N>` = percent connectivity,
// currently always 100 and reserved for future sparsity. An optional trailing
// `risk=<tenths>` (LearnedValue's Risk weight, evalParams[1], an integer count
// of TENTHS of a sigma multiple: risk=5 means k=0.5) is the one field here
// that search DOES act on: a nonzero k scores mu + k*sigma instead of mu alone
// (DistModel slots only, see mlValueScoreRisk in src/ml_eval.cpp), and it is
// always the LAST token when present, after any architecture fields. Omitted
// at its default of 0, like conn=, so an unrisked agent's id is unchanged.
//
// Index of the ')' matching the '(' at `openParen`, or npos if unbalanced. Needed
// because a regime may carry its own parameters, so ids now nest one level.
static size_t matchParen(const string& s, size_t openParen) {
    if (openParen >= s.size() || s[openParen] != '(') return string::npos;
    int depth = 0;
    for (size_t i = openParen; i < s.size(); i++) {
        if (s[i] == '(') depth++;
        else if (s[i] == ')') { depth--; if (depth == 0) return i; }
    }
    return string::npos;
}

// Split a comma list on TOP-LEVEL commas only (see matchParen).
static void splitTopLevel(const string& s, std::vector<string>& out) {
    out.clear();
    string cur;
    int depth = 0;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '(') depth++;
        else if (c == ')') depth--;
        if (c == ',' && depth == 0) { out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    out.push_back(cur);
}

// IDENTITY IS STILL (slot, hash). The architecture fields are descriptive: they are
// derived from the same file the hash covers, so they cannot disagree with it. They
// are NOT re-validated against the file on parse, which matters because a slot may
// since have been overwritten by a later model (49 of the 137 historical learned
// identities in the store are in that state). Those keep their legacy short IDs
// forever, since their architecture is genuinely unrecoverable.
static string archDescForSlot(int slot) {
    static std::map<int, string> cache;
    std::map<int, string>::iterator c = cache.find(slot);
    if (c != cache.end()) return c->second;
    string out;
    std::ifstream f(rankSlotFile(slot).c_str());
    if (f.is_open()) {
        std::map<string, string> kv;
        string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#') continue;
            size_t eq = line.find('=');
            if (eq == string::npos || eq == 0 || line.size() > 400) continue;
            string k = line.substr(0, eq), v = line.substr(eq + 1);
            while (!v.empty() && (v[v.size()-1] == 13 || v[v.size()-1] == 10)) v.erase(v.size()-1);
            if (!kv.count(k)) kv[k] = v;
        }
        string type = kv.count("type") ? kv["type"] : "linear";
        string feat = kv.count("feature_count") ? kv["feature_count"] : "129";
        // Dash-separate a comma layer list; a linear head has no stored dims, so its
        // shape is <features>-1, which is exactly the parameter count the old ID hid.
        struct L { static string shape(const string& layers, const string& feat) {
            if (layers.empty()) return feat + "-1";
            string r = layers;
            for (size_t i = 0; i < r.size(); i++) if (r[i] == ',') r[i] = '-';
            return r;
        } };
        // TRAINING REGIME, read from the model file's own `teacher=` provenance
        // line. This field used to be the model TYPE ("dist" or "value"), which
        // could not distinguish agents that share an architecture but were
        // produced by completely different pipelines -- a TD-Leaf model and a
        // pool-replay-supervised model are both `value,lin,129-1` and are not
        // remotely the same thing. Naming the regime instead makes the id say how
        // the agent was made, which is what a roster needs spread across.
        //
        // Why it belongs in the ID and not only in the model file: the ID is
        // stamped into every append-only match-store row and outlives the file.
        // models/sweep/slot9.txt was overwritten on 2026-07-30 and its teacher=
        // line went with it, so that agent's regime is now unrecoverable forever;
        // had the ID carried it, 649k stored rows would still know.
        string teacher = kv.count("teacher") ? kv["teacher"] : "";
        struct R {
            // Pull "<key><digits>" out of a provenance string, e.g. tag(t,", d","")
            // reads the 2 out of "AlphaBeta(Classic, d2)".
            static string num(const string& t, const string& key) {
                size_t p = t.find(key);
                if (p == string::npos) return "";
                size_t b = p + key.size(), e = b;
                while (e < t.size() && t[e] >= '0' && t[e] <= '9') e++;
                return (e > b) ? t.substr(b, e - b) : string("");
            }
            static string of(const string& t, const string& type) {
            if (t.compare(0, 7, "tdleaf(") == 0)              return "tdleaf_self";
            if (t.compare(0, 11, "gumbelzero(") == 0)         return "gumbel_self";
            if (t.compare(0, 11, "labelstore:") == 0)         return "position_elo";
            if (t.compare(0, 7, "replay:") == 0)              return "pool_games";
            if (t.compare(0, 9, "ensemble(") == 0)            return "weight_merge";
            // Parameterised regimes. A regime whose recipe has a knob that changes
            // how the agent PLAYS carries that knob, because two agents sharing a
            // regime word can be as unalike as two sharing an architecture were
            // before the regime field existed. Parameters go in parens, comma
            // separated, exactly like dil(r30,d3) -- underscore stays reserved for
            // joining words of a name. A regime with no knobs stays bare, so no
            // existing id changes.
            if (t.compare(0, 10, "AlphaBeta(") == 0) {
                // "AlphaBeta(Classic, d2) params=[...] dil(0.3->0.05/30p)"
                string d = num(t, ", d");
                string s = "teacher_games";
                if (!d.empty()) s += "(deep=" + d;
                else            s += "(deep=?";
                size_t dl = t.find("dil(");
                if (dl != string::npos) {
                    // Record the STARTING dilution rate as a percent: it is what
                    // spreads the teacher's position distribution.
                    double start = atof(t.c_str() + dl + 4);
                    int pct = (int)(start * 100.0 + 0.5);
                    std::ostringstream o; o << ",prob=" << pct;
                    s += o.str();
                }
                return s + ")";
            }
            if (t.find("self-play-bootstrap") != string::npos) {
                // "self-play-bootstrap(alphabeta,d4,gen=3,model=...)": gen is how
                // many LEARNED models deep the chain is (gen 1 learned from a
                // hand-written heuristic), d is the search depth the parent used
                // to generate the games. Both are read from THIS file, never by
                // walking to the parent -- the hash covers this file only, and the
                // ancestors live in reusable slots that may since have been reused.
                string g = num(t, "gen="), d = num(t, ",d");
                string s = "model_games(";
                s += g.empty() ? "gen=?" : ("gen=" + g);
                if (!d.empty()) s += ",deep=" + d;
                return s + ")";
            }
            if (!t.empty())                                   return "unknown";
            // No teacher= line at all: provenance is genuinely lost. Fall back to
            // the model type so a dist head is still recognisable as such.
            return type == "dist" ? "position_elo" : "unknown";
        } };
        string regime = R::of(teacher, type);

        // Layer widths are labelled `shape_`, never left as a bare dashed list:
        // the dashes separate the widths from each other and say nothing about
        // what the list IS, and a dash is too useful a separator to burn on
        // implying one meaning.
        //
        // A `dist` model carries TWO networks over the same features, so each
        // shape names its head: `mu` predicts the White advantage in logits,
        // `sigma` predicts the volatility of that advantage (see ml_model.h --
        // A ~ N(mu, sigma^2), with log-sigma trained and clamped). Every other
        // learned model is a single network, so it just says `shape_`.
        if (type == "dist") {
            string mt = kv.count("mu_type") ? kv["mu_type"] : "linear";
            string st = kv.count("s_type")  ? kv["s_type"]  : "linear";
            out = regime + "," + string(mt == "mlp" ? "mlp" : "lin")
                + ",mu_shape="    + L::shape(kv.count("mu_layers") ? kv["mu_layers"] : "", feat)
                + ",sigma_shape=" + L::shape(kv.count("s_layers")  ? kv["s_layers"]  : "", feat);
            (void)st;
        } else if (type == "joint") {
            // Two heads over DIFFERENT feature layouts (board value vs. per-move
            // policy), unlike dist's two heads over the same layout, so each
            // shape needs its own feature count rather than sharing `feat`.
            string vFeat = kv.count("v_feature_count") ? kv["v_feature_count"] : feat;
            string pFeat = kv.count("p_feature_count") ? kv["p_feature_count"] : "9";
            out = regime + ",joint"
                + ",value_shape="  + L::shape(kv.count("v_layers") ? kv["v_layers"] : "", vFeat)
                + ",policy_shape=" + L::shape(kv.count("p_layers") ? kv["p_layers"] : "", pFeat);
        } else {
            out = regime + "," + string(type == "mlp" ? "mlp" : "lin")
                + ",shape=" + L::shape(kv.count("layers") ? kv["layers"] : "", feat);
        }
        // Connectivity is 100% for every model built so far and is reserved for
        // future sparsity, so the default says nothing and is omitted. A sparse
        // model would print conn=<pct>.
        // (no conn field emitted at the default)
    }
    cache[slot] = out;
    return out;
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

// Does `id` differ from its own canonical form ONLY inside the learned()
// parentheses, while still naming the same (slot, hash) identity?
//
// That is exactly the set of superseded learned() spellings, and there are three
// generations of them:
//   * LEGACY 2-arg `learned(s111,78ef6974)`, written before the ID carried any
//     architecture at all (2026-07-26).
//   * STALE rich, whose first descriptor field held the model TYPE ("value",
//     "dist") rather than the TRAINING REGIME ("pool_games", "tdleaf_self")
//     it holds now (2026-08-01).
//   * OLD LABELS, `s111` before `model=111` and `129-1` before `shape=129-1`
//     (2026-08-03).
// All three re-derive to the same canonical ID from the slot file, because the
// architecture fields are DESCRIPTIVE: identity is (slot, hash) alone. So
// accepting them merges no identities and loses no information, and it is what
// lets a hand-written roster line name a learned agent by slot and hash without
// having to spell out an architecture only the model file knows.
//
// Deliberately narrow. Anything differing OUTSIDE learned()'s parentheses -- a
// stale `@N`, a legacy weight spelling, a reordered segment -- is still rejected
// with the canonical form printed to paste, which is the whole value of the
// strict check.
static bool learnedSpellingOnly(const string& id, const string& canon) {
    const size_t oldP = id.find(".learned("), newP = canon.find(".learned(");
    if (oldP == string::npos || newP == string::npos) return false;
    // The regime field may carry its own parenthesised parameters
    // (`model_games(gen3,d4)`), so find the ')' that MATCHES learned's '(',
    // not the first one -- which would cut the descriptor in half.
    const size_t oldOpen = oldP + 8, newOpen = newP + 8;
    const size_t oldClose = matchParen(id, oldOpen), newClose = matchParen(canon, newOpen);
    if (oldClose == string::npos || newClose == string::npos) return false;
    if (id.substr(0, oldOpen + 1) != canon.substr(0, newOpen + 1)) return false;
    if (id.substr(oldClose) != canon.substr(newClose)) return false;
    std::vector<string> of, nf;
    splitTopLevel(id.substr(oldOpen + 1, oldClose - oldOpen - 1), of);
    splitTopLevel(canon.substr(newOpen + 1, newClose - newOpen - 1), nf);
    if (of.size() < 2 || nf.size() < 2) return false;
    string oSlot, nSlot;
    if (!labelledNum(of[0], LBL_MODEL, LBLN_MODEL, oSlot)) oSlot = of[0];
    if (!labelledNum(nf[0], LBL_MODEL, LBLN_MODEL, nSlot)) nSlot = nf[0];
    return oSlot == nSlot && of[1] == nf[1];
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
        if (idn == "smart") s = "smart(pieces=" + std::to_string(a.chooserParam) + ")";
        else                s = idn;
        s += "@" + std::to_string(row ? row->version : 1);
        if (idn == "policy")
            s += ".linpol(model=" + std::to_string(a.modelSlot) + ","
               + rankFileHash8(rankSlotFile(a.modelSlot)) + ")";
    } else {
        const char* ename = (a.explorer >= 0 && a.explorer < g_explorerCount)
                          ? g_explorers[a.explorer].name : "";
        const RankNameCodec* row = nullptr;
        for (int i = 0; i < g_rkExplorerCount; i++)
            if (string(g_rkExplorers[i].regName) == ename) row = &g_rkExplorers[i];
        string idn = row ? row->idName : "?";
        if (idn == "ab") {
            s = "ab(deep=" + std::to_string(a.depth);
            if (!a.useAlphaBeta)        s += ",noab";
            if (a.useTT)                s += ",tt";
            if (a.useMoveOrder)         s += ",ord";
            if (a.useQuiescence)        s += ",qs";
            if (a.keepPartial)          s += ",part";
            if (a.aspirationWindow > 0) s += ",margin=" + std::to_string(a.aspirationWindow);
            if (a.iterMinRemain > 0)    s += ",rem=" + std::to_string(a.iterMinRemain);
            if (a.retainBudget)         s += ",retain";
            if (a.nodeBudget)           s += ",nodes=" + fmtBudget(a.nodeBudget);
            if (a.timeBudgetMs > 0.0)   s += ",time=" + std::to_string((long long)a.timeBudgetMs) + "ms";
            if (a.calTargetMs > 0.0)    s += ",cal=" + std::to_string((long long)a.calTargetMs) + "ms";
            if (a.depthCap > 0)         s += ",maxdeep=" + std::to_string(a.depthCap);
            s += ")";
        } else if (idn == "gaz") {
            // Gumbel MCTS: a.depth carries the total simulation budget, the same
            // reuse ab() makes of it for search depth. cvisit/cscale/m are optional
            // search-shape knobs (src/ai_gumbel.cpp's g_gumbelCVisit/g_gumbelCScale/
            // g_gumbelRootM), only appended when non-default so a plain gaz(sims=N)@1
            // id stays unchanged. cscale is spelled in TENTHS (cscale=10 -> 1.0),
            // matching learned()'s risk=<tenths> convention.
            s = "gaz(sims=" + std::to_string(a.depth);
            if (a.gumbelCVisit != 50)       s += ",cvisit=" + std::to_string(a.gumbelCVisit);
            if (a.gumbelCScaleTenths != 10) s += ",cscale=" + std::to_string(a.gumbelCScaleTenths);
            if (a.gumbelRootM != 16)        s += ",m=" + std::to_string(a.gumbelRootM);
            s += ")";
        } else {
            s = idn;   // greedy (always 1-ply, no arguments)
        }
        s += "@" + std::to_string(row ? row->version : 1);
        const char* vname = (a.evaluator >= 0 && a.evaluator < g_evalCount)
                          ? g_evaluators[a.evaluator].name : "";
        const RankEvalCodec* ev = evalCodecByRegName(vname);
        if (ev && ev->letters[0] == '\0') {
            string arch = archDescForSlot(a.modelSlot);
            s += ".learned(model=" + std::to_string(a.modelSlot) + ","
               + rankFileHash8(rankSlotFile(a.modelSlot))
               + (arch.empty() ? "" : "," + arch);
            // Risk (evalParams[1]): tenths-of-sigma multiplier in mu + k*sigma
            // (risk=5 -> k=0.5), DistModel slots only. Always last, omitted at
            // its default of 0 like conn=, so a risk-less agent's id is
            // unchanged by this field existing.
            if (a.evalParams[1] != 0) s += ",risk=" + std::to_string(a.evalParams[1]);
            s += ")@" + std::to_string(ev->version);
        } else if (ev) {
            // Weights are named (`chip=4`, not `c4`) and only the ones that say
            // something about this agent are shown. Two omissions:
            //   - a weight that is ZERO when its DEFAULT is zero: an optional
            //     term that was simply never switched on. A non-default zero is
            //     always printed, so `racewin_0` (default 1, 0 = off) survives --
            //     this is why the rule is not plain "hide zeros", which would
            //     silently re-enable it.
            //   - the TURN weight, inert unless leaves sit at mixed ply parity,
            //     which happens only under `qs` or `part`. At fixed depth it adds
            //     the same constant to every leaf, shifting whole subtrees equally
            //     and reordering nothing (see src/CLAUDE.md, "Turn weight").
            // Matched by parameter NAME, not index, so reordering a registry
            // cannot silently elide the wrong weight.
            //
            // CHIP RESCALE: when chip is the only ACTIVE weight, it is written as
            // 100 rather than its stored value, matching the hill climber's
            // sum-100 convention. Scaling the sole active term is a monotonic
            // rescale of every leaf, so it cannot reorder a single move; agents
            // with any other term live keep their stored numbers, where scaling
            // one weight WOULD change the mix.
            //
            // Verified before adopting: this collapses no identities -- all 175
            // roster agents and all 381 stored ids stay distinct, and the rescale
            // introduces no collisions.
            const bool turnLive = a.useQuiescence || a.keepPartial;
            const EvalDef& def = g_evaluators[a.evaluator];
            int chipIdx = -1, activeCount = 0;
            for (int i = 0; i < def.paramCount; i++) {
                const bool isTurn = (std::strcmp(def.params[i].name, "Turn") == 0);
                if (a.evalParams[i] == 0 || (isTurn && !turnLive)) continue;
                activeCount++;
                if (std::strcmp(def.params[i].name, "Chip") == 0) chipIdx = i;
            }
            const bool chipOnly = (activeCount == 1 && chipIdx >= 0);
            string body;
            for (int i = 0; i < def.paramCount; i++) {
                if (a.evalParams[i] == 0 && def.params[i].def == 0) continue;
                if (!turnLive && std::strcmp(def.params[i].name, "Turn") == 0) continue;
                if (!body.empty()) body += ",";
                const int shown = (chipOnly && i == chipIdx) ? 100 : a.evalParams[i];
                body += string(def.params[i].key) + "=" + std::to_string(shown);
            }
            // Every weight suppressed: emit the bare evaluator name rather than
            // empty parens, which the segment splitter rejects.
            s += "." + string(ev->idName);
            if (!body.empty()) s += "(" + body + ")";
            s += "@" + std::to_string(ev->version);
        } else {
            s += ".?";
        }
    }
    if (a.randomMoveProb > 0.0) {
        s += ".dil(prob=" + fmtPct(a.randomMoveProb);
        if (a.dilDepth > 0) s += ",deep=" + std::to_string(a.dilDepth);  // stochastic depth dilution
        s += ")@" + std::to_string(RK_DIL_VERSION);
    }
    if (a.openerKind >= 0 && a.openerKind < g_openerCount) {
        s += ".opener(" + string(g_openers[a.openerKind].idName);
        if (g_openers[a.openerKind].hasArg)
            s += "," + string(g_openers[a.openerKind].argLabel) + "="
               + std::to_string(a.openerArg);
        // arg2 is optional and omitted when unset, so every id written before
        // the cap existed still round-trips to exactly the same string.
        if (g_openers[a.openerKind].hasArg2 && a.openerArg2 > 0)
            s += ",ply=" + std::to_string(a.openerArg2);
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
    // An argument may itself be parameterised -- `model_games(gen3,d4)` sitting in
    // a learned() field -- so split on TOP-LEVEL commas only. Nesting is not
    // blanket-legal: each call site validates its own arguments against the flags
    // it knows, so an unexpected `tt(x)` is still rejected, just one layer later.
    string cur;
    int depth = 0;
    for (size_t i = 0; i < inner.size(); i++) {
        char ch = inner[i];
        if (ch == '(') depth++;
        else if (ch == ')') {
            depth--;
            if (depth < 0) { err = "unbalanced ')' in '" + tok + "'"; return false; }
        }
        if (ch == ',' && depth == 0) {
            if (cur.empty()) { err = "empty argument in '" + tok + "'"; return false; }
            args.push_back(cur);
            cur.clear();
        } else cur += ch;
    }
    if (depth != 0) { err = "unbalanced '(' in '" + tok + "'"; return false; }
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

static bool parseAgentId(const string& id, RankAgent& out, string& err, bool lenient) {
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
    bool fRetain = false;
    bool haveAsp = false, haveCap = false, haveTb = false, haveNb = false, haveCal = false;
    bool haveRem = false;
    long long asp = 0, cap = 0, tbMs = 0, calMs = 0, remPct = 0;
    unsigned long long nb = 0;
    bool haveCVisit = false, haveCScale = false, haveRootM = false;
    long long gCVisit = 0, gCScale = 0, gRootM = 0;

    if (headWord == "rand" || headWord == "tiered" || headWord == "policy") {
        if (parens) { err = "'" + headWord + "' takes no arguments"; return false; }
        const char* reg = (headWord == "rand") ? "UniformRandom"
                        : (headWord == "tiered") ? "TieredRandom" : "LearnedPolicy";
        chooserIdx = chooserIndexByName(reg);
        if (chooserIdx < 0) { err = string("chooser '") + reg + "' not in registry"; return false; }
    } else if (headWord == "smart") {
        long long n;
        string sTail;
        if (!parens || args.size() != 1) {
            err = "smart needs one positive argument, e.g. smart(pieces4)";
            return false;
        }
        if (!labelledNum(args[0], LBL_PIECES, LBLN_PIECES, sTail)) sTail = args[0];   // legacy bare form
        if (!lenientInt(sTail, false, n) || n < 1) {
            err = "smart needs one positive argument, e.g. smart(pieces4)";
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
        string dTail;
        if (!labelledNum(args[0], LBL_DEEP, LBLN_DEEP, dTail)
            || !lenientInt(dTail, false, d) || d < 1 || d > 99) {
            err = "ab()'s first argument must be a depth like deep6 (got '" + args[0] + "')";
            return false;
        }
        depth = (int)d;
        for (size_t i = 1; i < args.size(); i++) {
            const string& f = args[i];
            long long n;
            string fTail;
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
            } else if (f == "retain") {
                if (fRetain) { err = "duplicate ab() flag 'retain'"; return false; }
                fRetain = true;
            } else if (labelledNum(f, LBL_MAXDEEP, LBLN_MAXDEEP, fTail)) {
                if (haveCap) { err = "duplicate ab() flag '" + f + "'"; return false; }
                if (!lenientInt(fTail, false, n) || n <= 0) {
                    err = "bad depth ceiling '" + f + "' (expected like maxdeep3)";
                    return false;
                }
                cap = n; haveCap = true;
            } else if (labelledNum(f, LBL_MARGIN, LBLN_MARGIN, fTail)) {
                if (haveAsp) { err = "duplicate ab() flag '" + f + "'"; return false; }
                if (!lenientInt(fTail, false, n) || n <= 0) {
                    err = "bad search margin '" + f + "' (expected like margin50)";
                    return false;
                }
                asp = n; haveAsp = true;
            } else if (labelledNum(f, LBL_REM, LBLN_REM, fTail)) {
                if (haveRem) { err = "duplicate ab() flag '" + f + "'"; return false; }
                if (!lenientInt(fTail, false, n) || n <= 0 || n > 99) {
                    err = "bad remaining-budget gate '" + f + "' (expected like rem=76, 1-99)";
                    return false;
                }
                remPct = n; haveRem = true;
            } else if (labelledNum(f, LBL_TIME, LBLN_TIME, fTail)
                       && fTail.size() > 2 && fTail.compare(fTail.size()-2, 2, "ms") == 0) {
                if (haveTb) { err = "duplicate ab() flag '" + f + "'"; return false; }
                if (!lenientInt(fTail.substr(0, fTail.size()-2), false, n) || n <= 0) {
                    err = "bad time budget '" + f + "' (expected like time250ms)";
                    return false;
                }
                tbMs = n; haveTb = true;
            } else if (labelledNum(f, LBL_CAL, LBLN_CAL, fTail)
                       && fTail.size() > 2 && fTail.compare(fTail.size()-2, 2, "ms") == 0) {
                if (haveCal) { err = "duplicate ab() flag '" + f + "'"; return false; }
                if (!lenientInt(fTail.substr(0, fTail.size()-2), false, n) || n <= 0) {
                    err = "bad calibration target '" + f + "' (expected like cal=250ms)";
                    return false;
                }
                calMs = n; haveCal = true;
            } else if (labelledNum(f, LBL_NODES, LBLN_NODES, fTail)) {
                if (haveNb) { err = "duplicate ab() flag '" + f + "'"; return false; }
                if (!lenientBudget(fTail, nb)) {
                    err = "bad node budget '" + f + "' (expected like nodes200k, nodes2m)";
                    return false;
                }
                haveNb = true;
            } else {
                err = "unknown ab() flag '" + f + "'";
                return false;
            }
        }
    } else if (headWord == "gaz") {
        if (!parens || args.empty()) {
            err = "gaz needs at least one argument, e.g. gaz(sims=50)";
            return false;
        }
        isSearch = true;
        explorerIdx = explorerIndexByName("GumbelMCTS");
        if (explorerIdx < 0) { err = "explorer 'GumbelMCTS' not in registry"; return false; }
        long long s;
        string sTail;
        if (!labelledNum(args[0], LBL_SIMS, LBLN_SIMS, sTail)
            || !lenientInt(sTail, false, s) || s < 1) {
            err = "gaz()'s first argument must be a simulation budget like sims=50 (got '" + args[0] + "')";
            return false;
        }
        depth = (int)s;   // total simulation budget, reusing the same field ab() uses for depth
        for (size_t i = 1; i < args.size(); i++) {
            const string& f = args[i];
            long long n;
            string fTail;
            if (labelledNum(f, LBL_CVISIT, LBLN_CVISIT, fTail)) {
                if (haveCVisit) { err = "duplicate gaz() flag '" + f + "'"; return false; }
                if (!lenientInt(fTail, false, n) || n < 0) {
                    err = "bad cvisit '" + f + "' (expected like cvisit=50)";
                    return false;
                }
                gCVisit = n; haveCVisit = true;
            } else if (labelledNum(f, LBL_CSCALE, LBLN_CSCALE, fTail)) {
                if (haveCScale) { err = "duplicate gaz() flag '" + f + "'"; return false; }
                if (!lenientInt(fTail, false, n) || n < 0) {
                    err = "bad cscale '" + f + "' (expected TENTHS, like cscale=10 for 1.0)";
                    return false;
                }
                gCScale = n; haveCScale = true;
            } else if (labelledNum(f, LBL_ROOTM, LBLN_ROOTM, fTail)) {
                if (haveRootM) { err = "duplicate gaz() flag '" + f + "'"; return false; }
                if (!lenientInt(fTail, false, n) || n < 1) {
                    err = "bad root breadth '" + f + "' (expected like m=16)";
                    return false;
                }
                gRootM = n; haveRootM = true;
            } else {
                err = "unknown gaz() flag '" + f + "'";
                return false;
            }
        }
    } else {
        err = "unknown head '" + headWord
            + "' (expected rand, tiered, smart(N), policy, greedy, ab(...), or gaz(...), each with @<version>)";
        return false;
    }

    // Remaining segments: evaluator / model / dilution, each at most once.
    // Evaluator and dil segments carry their module version; linpol does not
    // (its model-content hash is its identity).
    int evalIdx = -1, modelSlot = -1;
    std::vector<long long> weights;
    bool haveEval = false, haveModel = false, haveDil = false, haveOpener = false;
    string modelHash;
    long long riskVal = 0;   // learned()'s optional trailing risk=<k>, see below
    double dilProb = 0.0;
    int dilDepth = 0;
    int openerKindVal = -1, openerArgVal = 0, openerArg2Val = 0;

    for (size_t si = 1; si < segs.size(); si++) {
        if (!splitTok(segs[si], word, args, parens, atV, err)) return false;
        if (word == "opener") {
            if (haveOpener) { err = "duplicate opener() segment"; return false; }
            if (atV < 1) { err = "opener segment '" + segs[si] + "' needs a module version like @1"; return false; }
            // Upper bound is 3 (name, arg, ply cutoff) because openers declaring
            // hasArg2 accept a cutoff. The exact per-opener bound is enforced below
            // from that opener's own metadata. This was 2 until 2026-08-26, which
            // made hasArg2 unreachable: the `ply=` cap `book` has implemented since
            // 2026-08-03 could not be written in any id that parsed.
            if (!parens || args.empty() || args.size() > 3) {
                err = "opener() takes an opener name, an optional arg, and an optional "
                      "ply cutoff, e.g. opener(rand,moves=6)@1 or opener(book,book=2,ply=8)@1";
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
                const size_t maxArgs = g_openers[ok].hasArg2 ? 3 : 2;
                if (args.size() < 2 || args.size() > maxArgs) {
                    err = string("opener '") + args[0] + "' needs an arg, e.g. opener("
                        + args[0] + "," + g_openers[ok].argLabel + "6)@1";
                    if (g_openers[ok].hasArg2)
                        err += string(" (optionally a ply cutoff, e.g. opener(") + args[0] + ","
                             + g_openers[ok].argLabel + "6,ply8)@1)";
                    return false;
                }
                long long op;
                // Accept the opener's own label (moves4 / book6) and the legacy
                // bare number, so pre-label ids in the store still resolve.
                const string lab = g_openers[ok].argLabel;
                const string labE = lab + "=", labU = lab + "_";
                const char* argLabels[3] = { labE.c_str(), labU.c_str(), lab.c_str() };
                string opTail;
                if (!labelledNum(args[1], argLabels, 3, opTail)) opTail = args[1];
                if (!lenientInt(opTail, false, op) || op < 1) {
                    err = "bad opener() arg '" + args[1] + "' (expected "
                        + g_openers[ok].argLabel + "_<n>, e.g. "
                        + g_openers[ok].argLabel + "_6)";
                    return false;
                }
                openerArgVal = (int)op;
                if (args.size() == 3) {
                    long long op2;
                    string plyTail;
                    if (!labelledNum(args[2], LBL_PLY, LBLN_PLY, plyTail)) plyTail = args[2];
                    if (!lenientInt(plyTail, false, op2) || op2 < 1) {
                        err = "bad opener() cutoff '" + args[2] + "' (expected ply_<n>, e.g. ply_8)";
                        return false;
                    }
                    openerArg2Val = (int)op2;
                }
            } else if (args.size() != 1) {
                err = string("opener '") + args[0] + "' takes no arg (use opener(" + args[0] + ")@1)";
                return false;
            }
            openerKindVal = ok;
            haveOpener = true;
        } else if (word == "dil") {
            if (haveDil) { err = "duplicate dil() segment"; return false; }
            if (atV < 1) { err = "dil segment '" + segs[si] + "' needs a module version like @1"; return false; }
            if (!parens) { err = "dil needs an argument, e.g. dil(prob5)@1"; return false; }
            if (args.size() > 2) {
                err = "dil() takes at most prob<percent> and an optional deep<depth>, got '"
                    + segs[si] + "'";
                return false;
            }
            double pct;
            string pTail;
            if (!labelledNum(args[0], LBL_PROB, LBLN_PROB, pTail) || !lenientPct(pTail, pct)) {
                err = "bad dil() argument '" + args[0] + "' (expected prob<percent>, e.g. prob5 or prob2.5)";
                return false;
            }
            dilProb = pct / 100.0;
            // Optional second argument d<depth>: dilute with a shallower search instead of a
            // fully random move. Requires a search head and a depth strictly below the agent's.
            if (args.size() == 2) {
                long long dd;
                string ddTail;
                if (!labelledNum(args[1], LBL_DEEP, LBLN_DEEP, ddTail)
                    || !lenientInt(ddTail, false, dd) || dd < 1) {
                    err = "bad dil() argument '" + args[1] + "' (expected deep<depth>, e.g. deep3)";
                    return false;
                }
                if (!isSearch) {
                    err = "dil() depth dilution '" + args[1] + "' needs a search head (ab/greedy)";
                    return false;
                }
                if (dd >= depth) {
                    err = "dil() depth dilution must be shallower than the agent depth deep"
                        + std::to_string(depth) + " (got '" + args[1] + "')";
                    return false;
                }
                dilDepth = (int)dd;
            }
            haveDil = true;
        } else if (word == "learned" || word == "linpol") {
            // Optional trailing risk=<k> (LearnedValue's Risk weight, whole-sigma
            // multiplier in mu + k*sigma; DistModel slots only). Always the LAST
            // token when present, so it is stripped here before the existing
            // positional model=/hash/arch parsing below runs unmodified against
            // the remaining args. Omitted at its default of 0, like conn=.
            // linpol() is a policy chooser, not a value head, so it carries no
            // such weight -- only "learned" strips it.
            if (word == "learned" && parens && !args.empty()
                && args.back().compare(0, 4, "risk") == 0) {
                string rTail;
                if (!labelledNum(args.back(), LBL_RISK, LBLN_RISK, rTail)
                    || !lenientInt(rTail, true, riskVal) || riskVal < -50 || riskVal > 50) {
                    err = "bad learned() risk '" + args.back() + "' (expected risk=<-50..50>, tenths of sigma)";
                    return false;
                }
                args.pop_back();
            }
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
            // Two accepted arg forms. LEGACY is (s<slot>,<hash8>). RICH adds the
            // architecture descriptor: (s<slot>,<hash8>,<recipe>,<mu_type>,<mu_shape>
            // [,sig<shape>],con<N>). Identity is (slot, hash) in both, so a legacy ID
            // parses to exactly the same agent and rankAgentId re-emits it in rich
            // form, which is how the stores' 90k legacy rows canonicalise on read
            // without being rewritten. The trailing fields are descriptive and are
            // deliberately NOT re-validated against the model file: a slot may since
            // have been overwritten, and such an ID must still parse.
            long long sl;
            string slTail;
            bool argsOk = parens && (args.size() == 2 || (args.size() >= 5 && args.size() <= 7));
            if (!argsOk || !labelledNum(args[0], LBL_MODEL, LBLN_MODEL, slTail)
                || !lenientInt(slTail, false, sl) || sl < 0 || sl >= ML_SLOTS) {
                err = word + " needs (model<slot>,<hash8>) or (model<slot>,<hash8>,<recipe>,<mu_type>,"
                      "<mu_shape>[,sig<shape>],conn<N>) with slot in 0.."
                      + std::to_string(ML_SLOTS-1);
                return false;
            }
            if (args.size() > 2) {
                // The regime may carry parameters -- teacher_games(deep_2) -- so
                // validate the FAMILY name, not the whole token.
                const string recipeFull = args[2];
                const string recipe = recipeFull.substr(0, recipeFull.find('('));
                const string& mut    = args[3];
                // Training-regime token, derived from the model file's teacher=
                // line by archDescForSlot(). "value"/"dist" are the SUPERSEDED
                // model-type tokens, still accepted (never emitted) because the
                // ~49 identities whose slot files were overwritten can never be
                // re-derived and keep their old strings permanently.
                static const char* kRegimes[] = {
                    "tdleaf_self",    // TD-Leaf(lambda) on self-play games
                    "gumbel_self",    // Gumbel-Zero self-play (src/ml_gumbelzero.cpp)
                    "pool_games",     // outcomes from games replayed out of the ranked pool
                    "teacher_games",  // outcomes from a fixed heuristic teacher's self-play
                    "model_games",    // outcomes from a previously-trained model's self-play
                    "position_elo",   // per-position Elo labels (position-oracle pipeline)
                    "weight_merge",   // weight averaging and/or mirror symmetrisation
                    "unknown",        // provenance lost with the model file
                    "value", "dist"   // superseded model-type tokens, parse-only
                };
                bool regimeOk = false;
                for (size_t k = 0; k < sizeof(kRegimes)/sizeof(kRegimes[0]); k++)
                    if (recipe == kRegimes[k]) { regimeOk = true; break; }
                if (!regimeOk) {
                    err = "learned() regime '" + recipe + "' is not a known token";
                    return false;
                }
                if (mut != "mlp" && mut != "lin" && mut != "joint") {
                    err = "learned() mu type must be 'mlp', 'lin', or 'joint', got '" + mut + "'";
                    return false;
                }
                // Connectivity is OPTIONAL: it is 100% for every model built so
                // far, and a field that never varies says nothing, so the emitter
                // omits it at that default. When a sparse model does appear it
                // prints conn=<pct> and lands here as the final field. Older ids
                // carry the mandatory con<N> and still parse.
                const string& last = args[args.size()-1];
                if (last.compare(0, 3, "con") == 0) {
                    string cTail;
                    long long pct;
                    if (!labelledNum(last, LBL_CONN, LBLN_CONN, cTail)
                        || !lenientInt(cTail, false, pct) || pct < 1 || pct > 100) {
                        err = "bad learned() connectivity '" + last + "' (expected conn=<1..100>)";
                        return false;
                    }
                }
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
            const EvalDef& edef = g_evaluators[evalIdx];
            int pc = edef.paramCount;
            // Weights are a SUBSET, not the full list. The emitter drops a weight
            // that is zero when its default is zero, plus the inert turn weight,
            // so AN ABSENT WEIGHT MEANS ITS REGISTRY DEFAULT. A non-default zero
            // is written explicitly (racewin_0), which is what stops an omission
            // from silently re-enabling a term the id says is off. An evaluator
            // with nothing to say is written bare, without parens.
            if ((int)args.size() > pc) {
                err = word + " has more weights than the " + std::to_string(pc) + " it defines";
                return false;
            }
            weights.assign(pc, 0);
            for (int i = 0; i < pc; i++) weights[i] = edef.params[i].def;
            std::vector<bool> seen(pc, false);
            const size_t nw = parens ? args.size() : 0;
            for (size_t k = 0; k < nw; k++) {
                const string& a = args[k];
                // `chip=4` (current), `chip_4` (its predecessor), or legacy `c4`,
                // so stored rows written under any of the three keep parsing.
                int wi = -1;
                string valTxt;
                size_t us = a.find('=');
                if (us == string::npos) us = a.find('_');   // pre-'=' spelling
                if (us != string::npos) {
                    const string key = a.substr(0, us);
                    for (int i = 0; i < pc; i++)
                        if (key == edef.params[i].key) { wi = i; break; }
                    valTxt = a.substr(us + 1);
                } else if (a.size() >= 2) {
                    const char* pos = strchr(row->letters, a[0]);
                    if (pos) { wi = (int)(pos - row->letters); valTxt = a.substr(1); }
                }
                if (wi < 0 || wi >= pc) {
                    err = "bad weight '" + a + "' for " + word + " (expected <name>=<value>, e.g. "
                        + edef.params[1].key + "=4)";
                    return false;
                }
                if (seen[wi]) { err = "duplicate weight '" + string(edef.params[wi].key) + "'"; return false; }
                long long v;
                if (!lenientInt(valTxt, true, v) || v < -100000 || v > 100000) {
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
            err = "'policy' needs a linpol(model<slot>,<hash8>) segment";
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
        string mf = rankSlotFile(modelSlot);
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
        // weights stays empty for a learned() segment (it has no generic weight
        // loop, see above), so this is the only place LearnedValue's Risk param
        // is ever set from an id; guarded by evalIdx so it can never stomp
        // another evaluator's own evalParams[1] (e.g. Advanced's Chip).
        if (evalIdx == learnedValueIndex()) a.evalParams[1] = (int)riskVal;
        a.useAlphaBeta = !fNoab;
        a.useTT = fTT;
        a.useMoveOrder = fOrd;
        a.useQuiescence = fQs;
        a.keepPartial = fPart;
        a.aspirationWindow = (int)asp;
        a.iterMinRemain = (int)remPct;
        a.retainBudget = fRetain;
        a.nodeBudget = nb;
        a.timeBudgetMs = (double)tbMs;
        a.calTargetMs = (double)calMs;
        a.depthCap = (int)cap;
        a.gumbelCVisit = haveCVisit ? (int)gCVisit : 50;
        a.gumbelCScaleTenths = haveCScale ? (int)gCScale : 10;
        a.gumbelRootM = haveRootM ? (int)gRootM : 16;
    } else {
        a = agentMakePolicy("", chooserIdx, chooserParam, modelSlot >= 0 ? modelSlot : 0);
    }
    a.randomMoveProb = dilProb;
    a.dilDepth = dilDepth;
    a.openerKind = openerKindVal;
    a.openerArg = openerArgVal;
    a.openerArg2 = openerArg2Val;

    // Canonical form check: re-emitting must reproduce the input exactly. This
    // also rejects stale module versions, pointing at the current form.
    //
    // ONE ALIAS IS ACCEPTED: a superseded spelling of the learned() DESCRIPTOR,
    // which is re-derived from the slot file rather than being part of the
    // identity. See learnedSpellingOnly(). Anything else that fails to
    // round-trip is still an error.
    string canon = rankAgentId(a);
    if (canon != id && learnedSpellingOnly(id, canon)) {
        // Superseded learned() spelling: accept, the caller sees the canonical spec.
    } else if (canon != id && !lenient) {
        err = "id is not canonical; use: " + canon;
        return false;
    }
    std::strncpy(a.name, id.c_str(), sizeof(a.name) - 1);
    a.name[sizeof(a.name) - 1] = '\0';
    out.spec = a;
    // `canon`, not the original `id`: a legacy short-form learned() id was just
    // validated as equivalent to `canon` above, but storing the SHORT form here
    // silently broke every string-keyed lookup against it downstream. Concretely:
    // rankLoadMatches rewrites every STORED match row's w/b through
    // rankUpgradeId() (so old rows keep matching an enriched roster), but
    // a roster loaded via the short form never went through that expansion, so
    // rankSchedule's `have` map (keyed by the roster's ids) could never match a
    // stored row's (post-expansion) key -- pending games for that pair silently
    // never decreased no matter how many games were actually played, and a
    // resumed/re-run play phase would replay the whole schedule from scratch.
    // Caught 2026-07-30 isolating a 3-agent, 1-game repro: 24 pending stayed 24
    // after adding one real, correctly stored game for the pair in question.
    out.id = canon;
    out.active = false;
    out.anchor = false;
    return true;
}

// Public entry: STRICT. A roster line must be spelled canonically, so a stale
// id is rejected with the current form printed to paste.
bool rankAgentFromId(const string& id, RankAgent& out, string& err) {
    return parseAgentId(id, out, err, false);
}

// Rewrite a stored id into today's canonical spelling. Stored rows were written
// under older label conventions (`d6` before `deep=6`, `c4` before `chip=100`,
// `s111` before `model=111`), and the roster now carries only the current form,
// so without this every one of the store's rows would stop matching the agents
// the roster names and the fit would see 0 games each.
//
// The learned() segment is the trap. rankAgentId re-derives a model's descriptor
// AND hash from the slot file as it stands NOW, but ~49 historical identities
// point at slots that were later overwritten by a different model. Re-emitting
// those would silently swap in another model's hash and architecture, renaming
// one agent into another. So when the stored hash disagrees with the file, the
// original learned() segment is spliced back verbatim and only the surrounding
// segments are upgraded -- the same protection learnedSpellingOnly relies on.
string rankUpgradeId(const string& id) {
    static std::map<string, string> cache;
    std::map<string, string>::iterator c = cache.find(id);
    if (c != cache.end()) return c->second;

    string out = id;
    RankAgent a;
    string err;
    if (parseAgentId(id, a, err, true)) {
        string canon = rankAgentId(a.spec);
        // Splice the stored learned()/linpol() payload back when the slot file no
        // longer matches, so an overwritten slot cannot rewrite an old identity.
        size_t oldP = id.find(".learned(");
        if (oldP == string::npos) oldP = id.find(".linpol(");
        size_t newP = canon.find(".learned(");
        if (newP == string::npos) newP = canon.find(".linpol(");
        if (oldP != string::npos && newP != string::npos) {
            size_t oldOpen = id.find('(', oldP), newOpen = canon.find('(', newP);
            size_t oldClose = matchParen(id, oldOpen), newClose = matchParen(canon, newOpen);
            if (oldClose != string::npos && newClose != string::npos) {
                const string oldSeg = id.substr(oldOpen + 1, oldClose - oldOpen - 1);
                const string newSeg = canon.substr(newOpen + 1, newClose - newOpen - 1);
                std::vector<string> of, nf;
                splitTopLevel(oldSeg, of);
                splitTopLevel(newSeg, nf);
                if (of.size() >= 2 && nf.size() >= 2 && of[1] != nf[1])
                    canon = canon.substr(0, newOpen + 1) + oldSeg + canon.substr(newClose);
            }
        }
        // Preserve every segment's @N verbatim. rankAgentId emits each module's
        // CURRENT version, but a stored row may name a RETIRED identity pinned at
        // an older one -- `classic(...)@1` is a different player from `@2`, which
        // is the entire point of module versioning. Re-deriving them merged 49
        // retired identities into their live successors on the first attempt at
        // this migration, silently combining their game histories.
        std::vector<string> oldSegs, newSegs;
        string e1, e2;
        if (splitSegs(id, oldSegs, e1) && splitSegs(canon, newSegs, e2)
            && oldSegs.size() == newSegs.size()) {
            string rebuilt;
            for (size_t i = 0; i < newSegs.size(); i++) {
                string seg = newSegs[i];
                const size_t oldAt = oldSegs[i].rfind('@');
                const size_t newAt = seg.rfind('@');
                if (oldAt != string::npos) {
                    const string ver = oldSegs[i].substr(oldAt);
                    seg = (newAt == string::npos) ? seg + ver : seg.substr(0, newAt) + ver;
                } else if (newAt != string::npos) {
                    seg = seg.substr(0, newAt);   // the stored form carried none
                }
                if (i) rebuilt += ".";
                rebuilt += seg;
            }
            canon = rebuilt;
        }
        out = canon;
    }
    cache[id] = out;
    return out;
}

// Shorten an id for PRINTING ONLY, by dropping each segment's `@N` when N is
// already that module's current version.
//
// The version is load-bearing exactly when it is NOT current: `classic(...)@1`
// beside a live `classic(...)@2` is a different, retired player, and that is the
// one case this keeps visible. On a screen full of agents that all sit at the
// current version, repeating it on every row is noise that pushes the part a
// reader is comparing off the right edge.
//
// Console tables only. Every FILE keeps the full canonical id -- the match store
// is keyed by it, `standings.tsv`/`ratings.tsv` are read back by tooling, and an
// id pasted out of a doc has to parse. rankAgentId is unaffected, so nothing
// functional ever sees the short form.
string rankDisplayId(const string& id) {
    static std::map<string, string> cache;
    std::map<string, string>::iterator c = cache.find(id);
    if (c != cache.end()) return c->second;

    string out = id;
    RankAgent a;
    string err;
    std::vector<string> segs, canonSegs;
    if (parseAgentId(id, a, err, true)
        && splitSegs(id, segs, err)
        && splitSegs(rankAgentId(a.spec), canonSegs, err)
        && segs.size() == canonSegs.size()) {
        string rebuilt;
        for (size_t i = 0; i < segs.size(); i++) {
            string seg = segs[i];
            const size_t at = seg.rfind('@'), cAt = canonSegs[i].rfind('@');
            if (at != string::npos && cAt != string::npos
                && seg.substr(at) == canonSegs[i].substr(cAt))
                seg = seg.substr(0, at);
            if (i) rebuilt += ".";
            rebuilt += seg;
        }
        out = rebuilt;
    }
    cache[id] = out;
    return out;
}

string rankReportId(const string& id) {
    static std::map<string, string> cache;
    std::map<string, string>::iterator c = cache.find(id);
    if (c != cache.end()) return c->second;

    std::vector<string> segs;
    string err;
    string base = rankDisplayId(id);
    if (!splitSegs(base, segs, err)) { cache[id] = base; return base; }

    for (size_t i = 0; i < segs.size(); i++) {
        string& seg = segs[i];
        if (seg.compare(0, 3, "ab(") == 0) {
            size_t close = seg.find(')', 3);
            if (close != string::npos) {
                string inner = seg.substr(3, close - 3);       // "deep=6,tt,ord,nodes=200k"
                string tail = seg.substr(close + 1);           // "" or "@N"
                size_t dc = inner.find(',');
                string deepPart = (dc == string::npos) ? inner : inner.substr(0, dc);
                string flags = (dc == string::npos) ? string() : inner.substr(dc + 1);

                std::vector<string> tokens;
                if (!flags.empty()) {
                    size_t start = 0;
                    while (true) {
                        size_t comma = flags.find(',', start);
                        if (comma == string::npos) { tokens.push_back(flags.substr(start)); break; }
                        tokens.push_back(flags.substr(start, comma - start));
                        start = comma + 1;
                    }
                }
                // tt/ord are assumed on (169/217 active agents carry tt, nearly always
                // paired with ord) and dropped unconditionally when present. A handful
                // of active agents deviate (an explicit tt/ord ablation study, e.g.
                // ab(deep=6,ord,nodes=200k) with no tt) -- for those, print noTT/noOrd
                // rather than silently rendering identically to the standard config.
                // tt/ord are meaningless under noab (full minimax, no alpha-beta path),
                // so no marker is added there.
                bool hasTT = false, hasOrd = false, hasNoab = false;
                std::vector<string> kept;
                for (size_t k = 0; k < tokens.size(); k++) {
                    if (tokens[k] == "tt") { hasTT = true; continue; }
                    if (tokens[k] == "ord") { hasOrd = true; continue; }
                    if (tokens[k] == "nodes=200k") continue;
                    if (tokens[k] == "noab") hasNoab = true;
                    kept.push_back(tokens[k]);
                }
                std::vector<string> out;
                if (!hasNoab) {
                    if (!hasTT) out.push_back("noTT");
                    if (!hasOrd) out.push_back("noOrd");
                }
                for (size_t k = 0; k < kept.size(); k++) out.push_back(kept[k]);

                string rebuilt = deepPart;
                for (size_t k = 0; k < out.size(); k++) rebuilt += "," + out[k];
                seg = "AB(" + rebuilt + ")" + tail;
            }
        } else if (seg.compare(0, 8, "learned(") == 0) {
            size_t close = seg.find(')', 8);
            if (close != string::npos) {
                string inner = seg.substr(8, close - 8);
                string tail = seg.substr(close + 1);
                std::vector<string> fields;
                size_t start = 0;
                while (true) {
                    size_t comma = inner.find(',', start);
                    if (comma == string::npos) { fields.push_back(inner.substr(start)); break; }
                    fields.push_back(inner.substr(start, comma - start));
                    start = comma + 1;
                }
                // fields[0]=model=N, [1]=content hash, [2]=regime, [3..]=arch/shape/conn/risk
                if (fields.size() >= 3) {
                    string rebuilt = fields[2] + "," + fields[0];
                    for (size_t k = 3; k < fields.size(); k++) rebuilt += "," + fields[k];
                    seg = "learned(" + rebuilt + ")" + tail;
                }
            }
        }
    }
    string out;
    for (size_t i = 0; i < segs.size(); i++) { if (i) out += "."; out += segs[i]; }
    cache[id] = out;
    return out;
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

// "ranking/matches.jsonl" + 2 -> "ranking/matches.0002.jsonl". The index is
// zero-padded so a plain lexicographic listing is also chronological order.
string rankStoreShardPath(const string& storeFile, int index) {
    string stem = storeFile;
    const string ext = ".jsonl";
    if (stem.size() > ext.size() &&
        stem.compare(stem.size() - ext.size(), ext.size(), ext) == 0)
        stem = stem.substr(0, stem.size() - ext.size());
    std::ostringstream s;
    s << stem << "." << std::setw(4) << std::setfill('0') << index << ext;
    return s.str();
}

// Read one store file into `out`. Shared by the sealed shards and the tail so a
// row cannot be parsed differently depending on which of the two it lives in.
static void readMatchStream(std::istream& f, const string& board,
                            std::vector<RankMatchRow>& out, int& skipped) {
    string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size() - 1);
        if (trimWs(line).empty()) continue;
        RankMatchRow m;
        if (!rankParseMatchRow(line, m)) { skipped++; continue; }
        if (!board.empty() && m.board != board) continue;
        // Upgrade the WHOLE id, not just its learned() segment: stored rows
        // predate the current label spelling in every segment (head, dilution,
        // opener, evaluator weights), and the roster now carries only the new
        // form, so matching on the old text would orphan the entire history.
        m.w = rankUpgradeId(m.w);
        m.b = rankUpgradeId(m.b);
        out.push_back(m);
    }
}



// ============================================================
// REGIMES AND MATCHUPS
// ============================================================
string rankAgentRegime(const string& id) {
    // Heuristic evaluators name themselves.
    if (id.find(".classic(") != string::npos) return "classic";
    if (id.find(".exp(")     != string::npos) return "exp";
    if (id.find(".adv(")     != string::npos) return "adv";
    if (id.find(".linpol(")  != string::npos) return "linpol";

    // learned(slot,hash,<regime>,...) -- the regime is the third field. The
    // legacy two-field form carries no regime and is reported as such rather
    // than guessed at, because a model file that has since been overwritten
    // makes it genuinely unrecoverable.
    size_t p = id.find(".learned(");
    if (p != string::npos) {
        size_t open = p + 9;
        size_t close = matchParen(id, p + 8);
        if (close == string::npos) return "learned?";
        std::vector<string> f;
        splitTopLevel(id.substr(open, close - open), f);
        if (f.size() < 3) return "learned?";
        // Return the FAMILY, dropping any parameters: `teacher_games(d6)` groups
        // with `teacher_games(d2)`. Blocs are what balancing and the census are
        // about, and treating every parameterisation as its own regime would give
        // a lone configuration the same weight as a 50-agent family. The
        // parameters stay visible in the id itself.
        string r = f[2];
        size_t paren = r.find('(');
        return (paren == string::npos) ? r : r.substr(0, paren);
    }
    // No evaluator segment at all: a policy/random head.
    return "nonlearning";
}

void rankMatchupByRegime(const std::vector<RankMatchRow>& rows, const RankFit& fit,
                         std::vector<RankMatchupCell>& out) {
    out.clear();
    std::map<string, double> elo;
    for (size_t i = 0; i < fit.ids.size(); i++) elo[fit.ids[i]] = fit.elo[i];

    std::map<string, string> regimeOf;   // memoized: the parse is per distinct id
    std::map<std::pair<string,string>, RankMatchupCell> cells;

    for (size_t k = 0; k < rows.size(); k++) {
        const string& w = rows[k].w;
        const string& b = rows[k].b;
        if (w == b) continue;
        std::map<string,double>::const_iterator ew = elo.find(w), eb = elo.find(b);
        if (ew == elo.end() || eb == elo.end()) continue;   // unrated, no expectation

        if (!regimeOf.count(w)) regimeOf[w] = rankAgentRegime(w);
        if (!regimeOf.count(b)) regimeOf[b] = rankAgentRegime(b);
        const string& rw = regimeOf[w];
        const string& rb = regimeOf[b];

        const double sWhite = (rows[k].r == 'W') ? 1.0 : (rows[k].r == 'B') ? 0.0 : 0.5;
        // Logistic expectation from the fitted gap, the same scale the fit uses.
        const double pWhite = 1.0 / (1.0 + std::pow(10.0, (eb->second - ew->second) / 400.0));

        // Key on the lexicographically smaller regime so both colours land in
        // one cell and the White advantage cancels.
        const bool whiteIsA = (rw <= rb);
        std::pair<string,string> key = whiteIsA ? std::make_pair(rw, rb)
                                                : std::make_pair(rb, rw);
        RankMatchupCell& c = cells[key];
        c.a = key.first; c.b = key.second;
        c.games    += 1.0;
        c.score    += whiteIsA ? sWhite : 1.0 - sWhite;
        c.expected += whiteIsA ? pWhite : 1.0 - pWhite;
    }
    for (std::map<std::pair<string,string>, RankMatchupCell>::const_iterator it = cells.begin();
         it != cells.end(); ++it) out.push_back(it->second);
}

static string storeStem(const string& storeFile) {
    const string ext = ".jsonl";
    if (storeFile.size() > ext.size() &&
        storeFile.compare(storeFile.size() - ext.size(), ext.size(), ext) == 0)
        return storeFile.substr(0, storeFile.size() - ext.size());
    return storeFile;
}

static string storeDir(const string& storeFile) {
    size_t slash = storeFile.find_last_of("/\\");
    return (slash == string::npos) ? string("") : storeFile.substr(0, slash + 1);
}

string rankStoreIndexPath(const string& storeFile) {
    return storeStem(storeFile) + ".index.txt";
}

void rankStoreParts(const string& storeFile, std::vector<string>& out) {
    out.clear();
    std::ifstream idx(rankStoreIndexPath(storeFile).c_str());
    if (idx.is_open()) {
        const string dir = storeDir(storeFile);
        string line;
        while (std::getline(idx, line)) {
            if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size() - 1);
            line = trimWs(line);
            if (line.empty() || line[0] == '#') continue;
            out.push_back(dir + line);
        }
    } else {
        // No index: the contiguous sealed-shard chain, first missing index ends it.
        for (int n = 1; ; n++) {
            string p = rankStoreShardPath(storeFile, n);
            std::ifstream s(p.c_str());
            if (!s.is_open()) break;
            out.push_back(p);
        }
    }
    out.push_back(storeFile);   // the live tail is last, even if it does not exist yet
}

bool rankLoadMatches(const string& file, const string& board,
                     std::vector<RankMatchRow>& out, int& skipped) {
    out.clear();
    skipped = 0;
    std::vector<string> parts;
    rankStoreParts(file, parts);
    for (size_t i = 0; i < parts.size(); i++) {
        std::ifstream f(parts[i].c_str());
        // A missing part is not an error: retired-agent parts are deliberately
        // untracked, so a fresh clone has an index line with no file behind it,
        // and the tail does not exist until the first game is played.
        if (!f.is_open()) continue;
        readMatchStream(f, board, out, skipped);
    }
    return true;
}

// Count non-blank lines, the unit the seal verification compares on (blank lines
// carry no row, so preserving them is not part of the contract).
static long long countStoreLines(const string& path) {
    std::ifstream f(path.c_str());
    if (!f.is_open()) return 0;
    long long n = 0;
    string line;
    while (std::getline(f, line)) if (!trimWs(line).empty()) n++;
    return n;
}

// Rolls one bucket's rows across numbered part files, starting a new part rather
// than letting any single file pass maxBytes.
namespace {
class PartWriter {
public:
    PartWriter(const string& stem, const string& bucket, long long maxBytes, bool apply)
        : stem_(stem), bucket_(bucket), max_(maxBytes), apply_(apply), cur_(0) {}

    bool write(const string& line, RankStoreBucket& stat, string& err) {
        const long long need = (long long)line.size() + 1;
        stat.rows++;
        stat.bytes += need;
        if (!apply_) {   // measuring: still model where the part boundaries fall
            if (stat.parts.empty() || cur_ + need > max_) {
                stat.parts.push_back(partPath((int)stat.parts.size() + 1));
                cur_ = 0;
            }
            cur_ += need;
            return true;
        }
        if (!out_.is_open() || cur_ + need > max_) {
            if (out_.is_open()) out_.close();
            string p = partPath((int)stat.parts.size() + 1);
            out_.open(p.c_str(), std::ios::binary | std::ios::trunc);
            if (!out_.is_open()) { err = "cannot write " + p; return false; }
            stat.parts.push_back(p);
            cur_ = 0;
        }
        out_ << line << "\n";
        cur_ += need;
        return true;
    }
    void close() { if (out_.is_open()) out_.close(); }

private:
    string partPath(int n) const {
        std::ostringstream s;
        s << stem_ << "." << bucket_ << "." << std::setw(4) << std::setfill('0') << n << ".jsonl";
        return s.str();
    }
    string       stem_, bucket_;
    long long    max_;
    bool         apply_;
    long long    cur_;
    std::ofstream out_;
};
}  // namespace

// Partition the store by who played each game (see ranking.h for the buckets).
//
// Stored ids are canonicalized exactly as rankLoadMatches canonicalizes them
// before the roster is consulted. Skipping that would read every legacy-form
// `learned(sN,hash8)` row as non-rostered and file live agents' games under
// "retired" -- the same id-canonicalization gap that has already broken
// scheduler dedup twice.
int rankSplitStore(const string& storeFile, const string& rosterFile,
                   const string& groupMatch, long long maxBytes, bool apply,
                   RankSplitStats& out, string& err) {
    err.clear();
    out = RankSplitStats();
    if (maxBytes <= 0) { err = "maxBytes must be positive"; return -1; }

    std::vector<RankAgent> roster;
    if (!rankLoadRosterFile(rosterFile, roster, err)) return -1;
    std::set<string> live;
    for (size_t i = 0; i < roster.size(); i++) live.insert(roster[i].id);
    out.rosterAgents = (long long)live.size();

    const string stem   = storeStem(storeFile);
    const string tagged = groupMatch.empty() ? string("") : ("retired_" + groupMatch);

    out.buckets.resize(tagged.empty() ? 2 : 3);
    out.buckets[0].name = "roster";
    if (!tagged.empty()) out.buckets[1].name = tagged;
    out.buckets.back().name = "retired_other";

    std::vector<PartWriter*> w;
    for (size_t i = 0; i < out.buckets.size(); i++)
        w.push_back(new PartWriter(stem, out.buckets[i].name, maxBytes, apply));

    struct Cleanup {
        std::vector<PartWriter*>& v;
        explicit Cleanup(std::vector<PartWriter*>& r) : v(r) {}
        ~Cleanup() { for (size_t i = 0; i < v.size(); i++) { v[i]->close(); delete v[i]; } }
    } cleanup(w);
    (void)cleanup;

    std::vector<string> parts;
    rankStoreParts(storeFile, parts);

    long long scanned = 0;
    for (size_t i = 0; i < parts.size(); i++) {
        std::ifstream f(parts[i].c_str());
        if (!f.is_open()) continue;
        string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size() - 1);
            if (trimWs(line).empty()) continue;
            scanned++;
            RankMatchRow m;
            if (!rankParseMatchRow(line, m)) {
                // Unparseable rows ride with the roster part: this store is never
                // regenerated, so discarding something we merely failed to read
                // would be the one unrecoverable outcome.
                out.malformed++;
                if (!w[0]->write(line, out.buckets[0], err)) return -1;
                continue;
            }
            // rankUpgradeId, not a learned()-only rewrite: the roster carries
            // today's labels and the store carries whatever was current when each
            // row was written, so comparing the two spellings directly would file
            // every LIVE agent's games under "retired" and orphan them.
            const string a = rankUpgradeId(m.w);
            const string b = rankUpgradeId(m.b);
            const bool aLive = live.count(a) > 0;
            const bool bLive = live.count(b) > 0;
            size_t bucket;
            if (aLive && bLive) {
                bucket = 0;
            } else {
                if (!aLive) out.retired[a]++;
                if (!bLive) out.retired[b]++;
                const bool hit = !tagged.empty() &&
                                 ((!aLive && a.find(groupMatch) != string::npos) ||
                                  (!bLive && b.find(groupMatch) != string::npos));
                bucket = hit ? 1 : out.buckets.size() - 1;
            }
            if (!w[bucket]->write(line, out.buckets[bucket], err)) return -1;
        }
    }
    if (!apply) return 0;
    for (size_t i = 0; i < w.size(); i++) w[i]->close();

    // Verify the parts hold every scanned row BEFORE the originals are touched.
    long long written = 0;
    for (size_t i = 0; i < out.buckets.size(); i++)
        for (size_t p = 0; p < out.buckets[i].parts.size(); p++)
            written += countStoreLines(out.buckets[i].parts[p]);
    if (written != scanned) {
        for (size_t i = 0; i < out.buckets.size(); i++)
            for (size_t p = 0; p < out.buckets[i].parts.size(); p++)
                std::remove(out.buckets[i].parts[p].c_str());
        std::ostringstream e;
        e << "parts hold " << written << " rows but the store had " << scanned
          << "; store left untouched";
        err = e.str();
        return -1;
    }

    // Index first, so a crash before the old files go leaves a readable store.
    const string idxPath = rankStoreIndexPath(storeFile);
    {
        std::ofstream idx(idxPath.c_str(), std::ios::trunc);
        if (!idx.is_open()) { err = "cannot write " + idxPath; return -1; }
        idx << "# Parts of this match store, in load order. One filename per line,\n"
            << "# relative to this directory. To drop a group of games from the\n"
            << "# ratings, delete its line (and, when you mean it, its file).\n"
            << "# A listed part that is missing is skipped, so parts kept out of\n"
            << "# git simply do not contribute on a fresh clone.\n"
            << "# Written by rank.exe split. New games still append to the store\n"
            << "# file itself, which is always loaded last.\n";
        const string dir = storeDir(storeFile);
        for (size_t i = 0; i < out.buckets.size(); i++) {
            if (out.buckets[i].parts.empty()) continue;
            idx << "\n# " << out.buckets[i].name << ": " << out.buckets[i].rows << " rows\n";
            for (size_t p = 0; p < out.buckets[i].parts.size(); p++) {
                string rel = out.buckets[i].parts[p];
                if (!dir.empty() && rel.compare(0, dir.size(), dir) == 0) rel = rel.substr(dir.size());
                idx << rel << "\n";
            }
        }
    }

    // The rows now live in the parts, so the old chain and tail can go.
    for (size_t i = 0; i + 1 < parts.size(); i++) std::remove(parts[i].c_str());
    std::ofstream tail(storeFile.c_str(), std::ios::binary | std::ios::trunc);
    if (!tail.is_open()) { err = "cannot truncate the tail " + storeFile; return -1; }
    return 0;
}

int rankSealStore(const string& storeFile, long long maxBytes, string& err) {
    err.clear();
    if (maxBytes <= 0) { err = "maxBytes must be positive"; return -1; }

    std::ifstream probe(storeFile.c_str(), std::ios::binary | std::ios::ate);
    if (!probe.is_open()) { err = "no store at " + storeFile; return -1; }
    long long tailBytes = (long long)probe.tellg();
    probe.close();
    if (tailBytes <= maxBytes) return 0;   // nothing to do

    // Sealed shards are immutable, so new ones start after the existing chain.
    int firstNew = 1;
    while (true) {
        std::ifstream s(rankStoreShardPath(storeFile, firstNew).c_str());
        if (!s.is_open()) break;
        firstNew++;
    }

    const long long before = countStoreLines(storeFile);
    // Emit only whole shards; whatever is left over stays in the tail rather than
    // becoming a stunted shard that the next seal would have to work around.
    const int targetShards = (int)(tailBytes / maxBytes);

    std::ifstream in(storeFile.c_str());
    if (!in.is_open()) { err = "cannot read " + storeFile; return -1; }

    const string tmpTail = storeFile + ".sealtmp";
    std::vector<string> written;
    std::ofstream cur;
    std::ofstream tail;
    int made = 0;
    long long curBytes = 0;
    string line;

    while (std::getline(in, line)) {
        if (!line.empty() && line[line.size()-1] == '\r') line.erase(line.size() - 1);
        if (trimWs(line).empty()) continue;
        const long long need = (long long)line.size() + 1;
        if (made < targetShards) {
            if (!cur.is_open() || curBytes + need > maxBytes) {
                if (cur.is_open()) { cur.close(); made++; }
                if (made < targetShards) {
                    string p = rankStoreShardPath(storeFile, firstNew + made);
                    cur.open(p.c_str(), std::ios::binary | std::ios::trunc);
                    if (!cur.is_open()) { err = "cannot write " + p; return -1; }
                    written.push_back(p);
                    curBytes = 0;
                }
            }
            if (made < targetShards) { cur << line << "\n"; curBytes += need; continue; }
        }
        if (!tail.is_open()) {
            tail.open(tmpTail.c_str(), std::ios::binary | std::ios::trunc);
            if (!tail.is_open()) { err = "cannot write " + tmpTail; return -1; }
        }
        tail << line << "\n";
    }
    if (cur.is_open()) { cur.close(); made++; }
    if (!tail.is_open()) {   // every line landed in a shard; the tail becomes empty
        tail.open(tmpTail.c_str(), std::ios::binary | std::ios::trunc);
        if (!tail.is_open()) { err = "cannot write " + tmpTail; return -1; }
    }
    tail.close();
    in.close();

    // Verify BEFORE destroying the original: this store is never regenerated, so
    // a silent short write here would be unrecoverable.
    long long after = countStoreLines(tmpTail);
    for (size_t i = 0; i < written.size(); i++) after += countStoreLines(written[i]);
    if (after != before) {
        for (size_t i = 0; i < written.size(); i++) std::remove(written[i].c_str());
        std::remove(tmpTail.c_str());
        std::ostringstream e;
        e << "line count changed (" << before << " -> " << after << "), store left untouched";
        err = e.str();
        return -1;
    }

    // When a part index exists it is the authority on what gets loaded, so newly
    // sealed shards must be listed in it or their rows would silently stop being
    // read. Append before the tail is replaced.
    const string idxPath = rankStoreIndexPath(storeFile);
    {
        std::ifstream probeIdx(idxPath.c_str());
        if (probeIdx.is_open()) {
            probeIdx.close();
            std::ofstream idx(idxPath.c_str(), std::ios::app);
            if (!idx.is_open()) {
                for (size_t i = 0; i < written.size(); i++) std::remove(written[i].c_str());
                std::remove(tmpTail.c_str());
                err = "cannot append to " + idxPath + "; store left untouched";
                return -1;
            }
            const string dir = storeDir(storeFile);
            idx << "\n# sealed from the tail\n";
            for (size_t i = 0; i < written.size(); i++) {
                string rel = written[i];
                if (!dir.empty() && rel.compare(0, dir.size(), dir) == 0) rel = rel.substr(dir.size());
                idx << rel << "\n";
            }
        }
    }

    std::remove(storeFile.c_str());
    if (std::rename(tmpTail.c_str(), storeFile.c_str()) != 0) {
        err = "sealed shards written but could not replace the tail; recover from " + tmpTail;
        return -1;
    }
    return made;
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

// Paired-opening seed: both games of a colour-swapped couple get ONE seed, derived
// from the canonically ordered pair so it does not change when the colours swap.
// The random opener (`.opener(rand,K)`) draws its moves from rand(), and during the
// opener window neither brain is consulted, so an identical rand() stream produces
// an identical opening line in both games. The couple therefore plays the SAME
// opening position with the colours reversed, which cancels that opening's inherent
// bias: the pair is compared on how each side recovers from equal ground rather than
// on which of them drew the kinder random start. Standard variance reduction in
// engine testing. Inert for agents that consume no rand() (no opener, no dilution),
// so enabling it cannot change a deterministic fixed-start pool.
static unsigned coupleSeed(const string& a, const string& b, long long couple, unsigned runSeed) {
    const string& lo = (a < b) ? a : b;
    const string& hi = (a < b) ? b : a;
    std::ostringstream s;
    s << lo << "|" << hi << "|c" << couple << "|" << runSeed;
    string k = s.str();
    return (unsigned)(fnv1a64(k.data(), k.size(), 1469598103934665603ULL) & 0xffffffffULL);
}

bool rankAgentIsDeterministic(const AgentSpec& spec) {
    // A wall-clock budget makes an agent non-deterministic WITHOUT drawing from
    // rand(): the search stops wherever the deadline lands between two nodes, and
    // that point moves with machine load, so two runs of the same position can
    // return different moves. Measured 2026-09-01 on the fixed binary, `rank.exe
    // determinism --replicas 3 --only "time=150ms"`: 3 of 34 subject-colours did
    // not reproduce, and they were exactly the agents whose budget actually BINDS
    // (theory 59, Docs/theories.md). Without this line pairGameTarget pins two
    // such agents at 2 games as both floor and ceiling, on the reasoning that
    // further games would only store replays -- but those 2 games are SAMPLES of a
    // noisy process, not replays of one game, so the error bar is understated.
    // A `cal=` head is deliberately NOT covered: its depth is fixed and the search
    // reads no clock, which is the whole point of the calibrated form.
    if (spec.timeBudgetMs > 0.0) return false;
    if (spec.randomMoveProb > 0.0) return false;          // dilution draws from rand()
    if (spec.openerKind >= 0 && spec.openerKind < g_openerCount &&
        std::strcmp(g_openers[spec.openerKind].idName, "rand") == 0) return false;
    if (spec.brain == BRAIN_POLICY) {
        // The random family draws; LearnedPolicy is an argmax and does not.
        if (spec.chooser >= 0 && spec.chooser < g_chooserCount &&
            std::strcmp(g_choosers[spec.chooser].name, "LearnedPolicy") != 0) return false;
    } else {
        // GumbelMCTS draws a Gumbel variate per legal move on every root
        // search (gumbelTopK, src/ai_gumbel.cpp), so it is never deterministic
        // -- unlike AlphaBeta/Greedy, which are pure functions of the board.
        if (spec.explorer >= 0 && spec.explorer < g_explorerCount &&
            std::strcmp(g_explorers[spec.explorer].name, "GumbelMCTS") == 0) return false;
    }
    return true;
}

// Games worth playing for one pair. A deterministic pair replays one game per
// colour, so 2 is BOTH the floor and the ceiling: fewer leaves a colour
// unmeasured, more just stores copies and understates the error bar.
static int pairGameTarget(const RankAgent& a, const RankAgent& b, int gamesPerPair) {
    if (rankAgentIsDeterministic(a.spec) && rankAgentIsDeterministic(b.spec)) return 2;
    return gamesPerPair;
}

std::vector<RankPendingGame> rankSchedule(const std::vector<RankAgent>& roster,
                                          const std::vector<RankMatchRow>& store,
                                          int gamesPerPair, unsigned runSeed,
                                          bool pairedOpenings,
                                          const std::set<std::string>* cohort) {
    std::vector<RankPendingGame> out;
    std::vector<string> ids;
    std::map<string, const RankAgent*> specOf;
    for (size_t i = 0; i < roster.size(); i++)
        if (roster[i].active) { ids.push_back(roster[i].id); specOf[roster[i].id] = &roster[i]; }
    std::sort(ids.begin(), ids.end());

    std::map<std::pair<string,string>, long long> have;   // (white, black) -> games played
    for (size_t i = 0; i < store.size(); i++)
        have[std::make_pair(store[i].w, store[i].b)]++;

    for (size_t i = 0; i < ids.size(); i++)
        for (size_t j = i + 1; j < ids.size(); j++) {
            const string& a = ids[i];   // lexicographically smaller: White in ceil(G/2)
            const string& b = ids[j];
            // Cohort mode: only pairs TOUCHING a cohort agent are scheduled, so
            // rating a new cohort never triggers a full roster-vs-roster refill.
            // (Measured 2026-07-29: the store holds 27,265 pairs at a median of 6
            // games, so an unfiltered --games 32 pass would schedule ~702,000
            // games that have nothing to do with the cohort.)
            if (cohort && !cohort->count(a) && !cohort->count(b)) continue;
            long long haveAW = 0, haveBW = 0;
            std::map<std::pair<string,string>, long long>::iterator it;
            it = have.find(std::make_pair(a, b));
            if (it != have.end()) haveAW = it->second;
            it = have.find(std::make_pair(b, a));
            if (it != have.end()) haveBW = it->second;
            const int target = pairGameTarget(*specOf[a], *specOf[b], gamesPerPair);
            long long pendAW = (target + 1) / 2 - haveAW;
            long long pendBW = target / 2 - haveBW;
            if (pendAW < 0) pendAW = 0;
            if (pendBW < 0) pendBW = 0;
            long long ordinal = haveAW + haveBW;
            while (pendAW > 0 || pendBW > 0) {
                RankPendingGame g;
                if (pendAW >= pendBW) { g.w = a; g.b = b; pendAW--; }
                else                  { g.w = b; g.b = a; pendBW--; }
                // Emission order alternates a-White / b-White, so ordinals 2k and
                // 2k+1 are one colour-swapped couple and share a couple index.
                g.seed = pairedOpenings ? coupleSeed(a, b, ordinal / 2, runSeed)
                                        : gameSeed(g.w, g.b, ordinal, runSeed);
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

void rankFitBT(const std::vector<RankMatchRow>& rows, const string& anchorId, RankFit& out,
               bool regimeBalanced) {
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
    // Optional regime balancing, applied to the REAL counts before the prior so
    // the prior stays a genuine half-game regularizer per pair.
    //
    // Problem it solves: pooled Elo depends on the pool's composition whenever
    // matchups are non-transitive, and this roster is 30.6% classic + 30.0%
    // pool_games (2026-08-01), so "strong" and "good against those two" are
    // nearly the same measurement. Weighting each pair by
    // 1/(agents in A's regime * agents in B's regime) makes every regime BLOC
    // contribute equally however many agents happen to wear it, then the whole
    // set is rescaled to the original total so error bars stay on the same
    // footing. Note the SEs are then effective-sample SEs, not game counts.
    if (regimeBalanced) {
        std::vector<string> regimeOf(n);
        std::map<string,int> pop;
        for (std::map<string,int>::const_iterator it = idx.begin(); it != idx.end(); ++it) {
            regimeOf[it->second] = rankAgentRegime(it->first);
            pop[regimeOf[it->second]]++;
        }
        double rawTotal = 0.0, wTotal = 0.0;
        for (PairMap::iterator it = agg.begin(); it != agg.end(); ++it) rawTotal += it->second.first;
        for (PairMap::iterator it = agg.begin(); it != agg.end(); ++it) {
            double w = 1.0 / ((double)pop[regimeOf[it->first.first]] *
                              (double)pop[regimeOf[it->first.second]]);
            it->second.first  *= w;
            it->second.second *= w;
            wTotal += it->second.first;
        }
        if (wTotal > 0.0) {
            const double s = rawTotal / wTotal;
            for (PairMap::iterator it = agg.begin(); it != agg.end(); ++it) {
                it->second.first  *= s;
                it->second.second *= s;
            }
        }
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

// Bradley-Terry MM fit with a SUBSET of agents' ratings HELD FIXED.
//
// Purpose: rate a cohort of new agents on the EXISTING roster's Elo scale
// without letting them perturb it. A normal refit re-solves every rating at
// once, so adding a cohort shifts the whole table and makes the previous fit's
// numbers non-comparable (Docs/benchmarking.md, "Elo scale drift across fits").
// Pinning the roster keeps every existing number valid for the duration of a
// study, so a cohort's Elo can be read against a stable reference.
//
// Better than N independent gauntlets (rankFitSingle) because cohort-vs-cohort
// games are used too: the fit resolves the cohort's INTERNAL ordering, which is
// usually the quantity a study actually cares about, and which a per-candidate
// gauntlet cannot see at all.
//
// This is a SCREENING instrument, not a certification one. A pinned fit cannot
// dethrone anything: the champions' ratings are inputs to it. Certification
// remains the full unpinned refit (ranking/CHAMPION.md rule 1), which is the
// deliberate last step after the cohort is chosen.
//
// `pinned` maps agent id -> fixed Elo. Ids absent from it are free. Free agents
// with no game path to any pinned agent are unidentified on the pinned scale and
// are flagged provisional (centered on their own component mean of 1000).
void rankFitBTPinned(const std::vector<RankMatchRow>& rows,
                     const std::map<std::string,double>& pinned,
                     RankFit& out) {
    out.ids.clear(); out.elo.clear(); out.se.clear();
    out.provisional.clear(); out.pinned.clear();
    out.anchored = false;

    std::map<string,int> idx;
    for (size_t k = 0; k < rows.size(); k++) {
        if (rows[k].w == rows[k].b) continue;
        idx[rows[k].w] = 0;
        idx[rows[k].b] = 0;
    }
    int n = 0;
    for (std::map<string,int>::iterator it = idx.begin(); it != idx.end(); ++it) it->second = n++;
    if (n == 0) return;

    std::vector<char> isPin(n, 0);
    std::vector<double> pinElo(n, 0.0);
    int nPinned = 0;
    for (std::map<string,int>::iterator it = idx.begin(); it != idx.end(); ++it) {
        std::map<std::string,double>::const_iterator p = pinned.find(it->first);
        if (p != pinned.end()) { isPin[it->second] = 1; pinElo[it->second] = p->second; nPinned++; }
    }
    out.anchored = (nPinned > 0);

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
    for (PairMap::iterator it = agg.begin(); it != agg.end(); ++it) {
        it->second.first += 0.5;
        it->second.second += 0.25;
    }

    std::vector<double> W(n, 0.0);
    for (PairMap::iterator it = agg.begin(); it != agg.end(); ++it) {
        W[it->first.first]  += it->second.second;
        W[it->first.second] += it->second.first - it->second.second;
    }

    // Pinned agents enter at their fixed strength and never move. No geometric
    // -mean renormalization: the pins ARE the scale (renormalizing would drag
    // them, which is exactly what pinning exists to prevent).
    std::vector<double> g(n, 1.0), gn(n, 0.0), denom(n, 0.0);
    for (int i = 0; i < n; i++) if (isPin[i]) g[i] = std::exp(pinElo[i] / ELO_PER_NAT);
    for (int pass = 0; pass < 5000; pass++) {
        std::fill(denom.begin(), denom.end(), 0.0);
        for (PairMap::iterator it = agg.begin(); it != agg.end(); ++it) {
            int i = it->first.first, j = it->first.second;
            double d = it->second.first / (g[i] + g[j]);
            denom[i] += d;
            denom[j] += d;
        }
        double maxd = 0.0;
        for (int i = 0; i < n; i++) {
            if (isPin[i]) { gn[i] = g[i]; continue; }
            gn[i] = (denom[i] > 0.0) ? W[i] / denom[i] : g[i];
            double d = std::fabs(std::log(gn[i]) - std::log(g[i]));
            if (d > maxd) maxd = d;
        }
        g = gn;
        if (maxd < 1e-9) break;
    }

    std::vector<int> parent(n);
    for (int i = 0; i < n; i++) parent[i] = i;
    struct UF2 {
        static int find(std::vector<int>& p, int x) {
            while (p[x] != x) { p[x] = p[p[x]]; x = p[x]; }
            return x;
        }
    };
    for (PairMap::iterator it = agg.begin(); it != agg.end(); ++it) {
        int a = UF2::find(parent, it->first.first), b = UF2::find(parent, it->first.second);
        if (a != b) parent[a] = b;
    }
    // Components containing at least one pinned agent are on the pinned scale.
    std::map<int,char> compHasPin;
    for (int i = 0; i < n; i++) if (isPin[i]) compHasPin[UF2::find(parent, i)] = 1;

    std::vector<double> elo(n);
    for (int i = 0; i < n; i++) elo[i] = ELO_PER_NAT * std::log(g[i]);

    // Unidentified components get the usual mean-1000 centering.
    std::map<int,int> compCount;
    std::map<int,double> compSum, compShift;
    for (int i = 0; i < n; i++) {
        int r = UF2::find(parent, i);
        compCount[r]++; compSum[r] += elo[i];
    }
    for (std::map<int,int>::iterator it = compCount.begin(); it != compCount.end(); ++it) {
        int r = it->first;
        compShift[r] = compHasPin.count(r) ? 0.0 : (compSum[r] / it->second - 1000.0);
    }

    std::vector<double> info(n, 0.0);
    for (PairMap::iterator it = agg.begin(); it != agg.end(); ++it) {
        int i = it->first.first, j = it->first.second;
        double p = g[i] / (g[i] + g[j]);
        double c = it->second.first * p * (1.0 - p);
        info[i] += c;
        info[j] += c;
    }

    out.ids.resize(n); out.elo.resize(n); out.se.resize(n);
    out.provisional.resize(n); out.pinned.resize(n);
    for (std::map<string,int>::iterator it = idx.begin(); it != idx.end(); ++it) {
        int i = it->second;
        int r = UF2::find(parent, i);
        out.ids[i] = it->first;
        out.elo[i] = elo[i] - compShift[r];
        // A pinned rating is an input, not an estimate, so it carries no error bar.
        out.se[i]  = isPin[i] ? 0.0 : ((info[i] > 0.0) ? ELO_PER_NAT / std::sqrt(info[i]) : 0.0);
        out.provisional[i] = (char)(compHasPin.count(r) ? 0 : 1);
        out.pinned[i] = isPin[i];
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
// `trace`, when non-null, receives one position hash per half-move played (the
// position AFTER that half-move, keyed for the side then to move). Two games
// with identical traces followed identical move sequences, so it is the exact
// trajectory fingerprint the determinism probe compares. Cheap enough to leave
// opt-in rather than always-on: one positionKey per ply against a search that
// costs a 200k-node budget.
static bool playOneGame(const RankAgent& wa, const RankAgent& ba, const string& board,
                        RankMatchRow& m, std::vector<unsigned long long>* trace = nullptr) {
    if (!reloadBoard(board)) return false;
    if (trace) trace->clear();
    // Fresh TT per game. Without this a tt-flagged agent's play depends on every
    // game the worker process happened to run first, so the same scheduled game
    // gives different results under a different shard split or resume point --
    // the exact defect the replay and label paths already clear for. It also
    // makes "deterministic" mean what it says: measured 2026-08-03, the boost run
    // returned 0.706 distinct trajectories per stored row for pairs that draw no
    // randomness at all, and that residual variation was cross-game TT state, not
    // game diversity. The retain purse is per game for the same reason.
    ttClear();
    retainResetCarry();
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
            playedByOpener = g_openers[ag.spec.openerKind].fn(side, h / 2, h, ag.spec.openerArg, ag.spec.openerArg2, victor);
        if (!playedByOpener)
            victor = agentChooseMove(ag.spec, side);
        // An opener may have narrowed the root move list for this ply only (cbook).
        // Clear it unconditionally so a restriction can never leak into a later ply,
        // the opponent's move, or a ply past the opener's own cap.
        g_useRootFilter = false;
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
        if (trace) trace->push_back(positionKey(side == White ? Black : White, false).hash);
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
        string f = rankSlotFile(*it);
        if (f.empty() || !mlLoadSlot(*it, f)) {
            err = "cannot load model " + f + " into slot " + std::to_string(*it);
            return false;
        }
    }
    return true;
}

// Single-spec convenience wrapper around loadModelSlots, for a caller (the GUI)
// that has one AgentSpec at a time rather than a roster to schedule games over.
bool rankLoadAgentModels(const AgentSpec& spec, string& err) {
    RankAgent tmp; tmp.spec = spec;
    std::vector<const RankAgent*> v; v.push_back(&tmp);
    return loadModelSlots(v, err);
}

// ============================================================
// PLAY
// ============================================================
// Read a plain list of agent ids (one per line, '#' comments, blanks ignored).
// Used by --cohort to name the agents a play pass should schedule games for.
//
// Canonicalized on read for the SAME reason rankLoadMatches canonicalizes every
// stored row's w/b and rankAgentFromId now returns the canonical form for a
// legacy learned() id: rankSchedule's cohort filter (`cohort->count(a)`) compares
// this set against the roster's `ids`, which are always canonical post-fix. A
// cohort file written in the legacy short form (as BuildRoster-style tooling
// does) would otherwise never match anything here, silently filtering out every
// pair and reporting "0 pending" -- not because the schedule is satisfied, but
// because the cohort set and the roster ids never agreed on a string to compare.
// Caught 2026-07-30 immediately after fixing the roster side of this exact gap.
static bool loadIdList(const string& path, std::set<std::string>& out, string& err) {
    std::ifstream f(path.c_str());
    if (!f) { err = "cannot open " + path; return false; }
    string line;
    while (std::getline(f, line)) {
        size_t a = line.find_first_not_of(" \t\r\n");
        if (a == string::npos) continue;
        if (line[a] == '#') continue;
        size_t b = line.find_last_not_of(" \t\r\n");
        out.insert(rankUpgradeId(line.substr(a, b - a + 1)));
    }
    return true;
}

// Read frozen ratings from a ratings.tsv / standings.tsv produced by an earlier
// fit: any agent found here is PINNED at that Elo. Both files carry '#' comment
// banners and a header row; the id is the LAST tab-separated column and the Elo
// is named by the header, so this reads either layout without being told which.
static bool loadPinnedRatings(const string& path, std::map<std::string,double>& out, string& err) {
    std::ifstream f(path.c_str());
    if (!f) { err = "cannot open " + path; return false; }
    string line;
    int eloCol = -1, idCol = -1;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::vector<string> col;
        size_t start = 0;
        while (true) {
            size_t t = line.find('\t', start);
            col.push_back(line.substr(start, (t == string::npos ? line.size() : t) - start));
            if (t == string::npos) break;
            start = t + 1;
        }
        if (eloCol < 0) {                       // header row: locate the columns by name
            for (size_t i = 0; i < col.size(); i++) {
                if (col[i] == "elo") eloCol = (int)i;
                if (col[i] == "id")  idCol  = (int)i;
            }
            if (eloCol < 0 || idCol < 0) { err = path + ": no 'elo'/'id' header columns"; return false; }
            continue;
        }
        if ((int)col.size() <= eloCol || (int)col.size() <= idCol) continue;
        if (col[idCol].empty()) continue;
        out[col[idCol]] = atof(col[eloCol].c_str());
    }
    if (out.empty()) { err = path + ": no ratings rows parsed"; return false; }
    return true;
}

int rankPlay(const string& rosterFile, const string& storeFile, const string& outFile,
             int gamesPerPair, int shard, int ofK, unsigned runSeed, const string& board,
             bool pairedOpenings, const string& cohortFile, bool ladder) {
    std::vector<RankAgent> roster;
    string err;
    if (!rankLoadRosterFile(rosterFile, roster, err)) { cout << "ERROR: " << err << "\n"; return 1; }
    std::set<std::string> cohort;
    if (!cohortFile.empty()) {
        if (!loadIdList(cohortFile, cohort, err)) { cout << "ERROR: " << err << "\n"; return 1; }
        cout << "cohort mode: " << cohort.size() << " agent(s) from " << cohortFile
             << " -- scheduling only pairs that touch one of them\n";
    }
    if (ofK < 1) ofK = 1;
    if (shard < 0 || shard >= ofK) { cout << "ERROR: --shard must be in [0, --of)\n"; return 1; }

    std::vector<RankMatchRow> store;
    int skipped = 0;
    rankLoadMatches(storeFile, board, store, skipped);
    if (skipped) cout << "WARNING: skipped " << skipped << " malformed line(s) in " << storeFile << "\n";

    const std::set<std::string>* cohortPtr = cohort.empty() ? nullptr : &cohort;

    // --- The rung ladder ----------------------------------------------------
    // `--games N` is a TARGET, not an increment: rankSchedule counts the games
    // already in the store for each pair and returns only the deficit. So
    // playing rungs 2, 4, 8, ... N in sequence plays exactly the same set of
    // games as one pass at N, for the price of a few extra scheduling passes.
    // What it buys is an early read. Rung 1 touches EVERY pair, so a broken
    // agent, a mis-specified roster or an unexpected cost shows up minutes in
    // rather than at the end, and the rung's measured rate projects the rest.
    //
    // Sharding disables the ladder. Each shard writes its own output file and
    // cannot see its siblings' rung-1 games, so its rung-2 schedule would
    // re-issue games another shard already played. tools/run_rank.ps1 drives
    // the rungs for sharded runs instead, merging between them so that every
    // worker starts each rung seeing the whole store.
    std::vector<int> rungs;
    if (ladder && ofK == 1 && gamesPerPair > 2) {
        for (int g = 2; g < gamesPerPair; g *= 2) rungs.push_back(g);
    }
    rungs.push_back(gamesPerPair);

    // Cumulative pending counts, measured against the store as it stands now.
    // Scheduling is deficit-based, so cum[r] is the total this run will have
    // played once rung r finishes, and cum[r] - cum[r-1] is that rung's own
    // cost. Both are exact, which is the point: no multiplier is involved.
    std::vector<size_t> cum(rungs.size(), 0);
    for (size_t r = 0; r < rungs.size(); r++)
        cum[r] = rankSchedule(roster, store, rungs[r], runSeed, pairedOpenings, cohortPtr).size();

    int nActive = 0;
    for (size_t i = 0; i < roster.size(); i++) if (roster[i].active) nActive++;

    string pre = (ofK > 1) ? ("[s" + std::to_string(shard) + "] ") : string("");
    cout << pre << "rank: " << nActive << " active agents, " << cum.back()
         << " pending games (target " << gamesPerPair << "/pair)";
    if (pairedOpenings) cout << ", paired openings";
    if (ofK > 1) cout << ", shard " << shard << "/" << ofK;
    cout << "\n" << flush;
    if (cum.back() == 0) {
        cout << pre << "nothing to play: every active pair is at target\n";
        return 0;
    }
    if (rungs.size() > 1) {
        cout << pre << "ladder: ";
        for (size_t r = 0; r < rungs.size(); r++)
            cout << (r ? ", " : "") << rungs[r] << "/pair -> " << cum[r] << " cumulative";
        cout << "\n" << pre
             << "  rung 1 is a full pass over every pair, so it is the most expensive"
                " single rung and its rate projects the rest.\n" << flush;
    } else if (ladder && ofK > 1) {
        cout << pre << "ladder off: sharded run, the rungs are the wrapper's job\n" << flush;
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
    std::chrono::steady_clock::time_point tRun = std::chrono::steady_clock::now();

    for (size_t r = 0; r < rungs.size(); r++) {
        // Re-schedule against the store INCLUDING this run's earlier rungs, so
        // each rung issues only its own increment.
        std::vector<RankPendingGame> pending =
            rankSchedule(roster, store, rungs[r], runSeed, pairedOpenings, cohortPtr);
        std::chrono::steady_clock::time_point tRung = std::chrono::steady_clock::now();
        long long rungPlayed = 0;

        if (rungs.size() > 1)
            cout << pre << "== rung " << (r + 1) << "/" << rungs.size() << ", --games "
                 << rungs[r] << ": " << pending.size() << " game(s)\n" << flush;

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
            store.push_back(m);          // the next rung must see it
            played++; rungPlayed++;

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
               << rankDisplayId(gm.w) << " (W) vs " << rankDisplayId(gm.b)
               << " : " << m.r << " in " << m.plies
               << " plies, " << fmtN((m.wms + m.bms) / 1000.0, 1) << "s | pair "
               << t.w << "-" << t.l;
            cout << ln.str() << "\n" << flush;
        }

        if (rungs.size() > 1) {
            double rs = std::chrono::duration<double>(
                            std::chrono::steady_clock::now() - tRung).count();
            double rate = (rs > 0.0) ? (double)rungPlayed / rs : 0.0;
            cout << pre << "== rung " << (r + 1) << "/" << rungs.size() << " done: "
                 << rungPlayed << " game(s) in " << fmtN(rs, 1) << "s ("
                 << fmtN(rate, 2) << " games/s), " << played << " cumulative\n";
            // Exact remaining count, then a projection at the rate measured so
            // far. The rate is the part to doubt: later rungs skew toward the
            // stochastic pairs, whose games need not be the same length as the
            // deterministic ones that dominate rung 1.
            if (r + 1 < rungs.size()) {
                size_t remain = cum.back() - cum[r];
                cout << pre << "   " << remain << " game(s) left in rungs";
                for (size_t q = r + 1; q < rungs.size(); q++) cout << " " << rungs[q];
                double all = std::chrono::duration<double>(
                                 std::chrono::steady_clock::now() - tRun).count();
                double overall = (all > 0.0) ? (double)played / all : 0.0;
                if (overall > 0.0)
                    cout << ", about " << fmtN((double)remain / overall / 60.0, 1)
                         << " min at " << fmtN(overall, 2) << " games/s so far";
                cout << "\n" << flush;
            }
        }
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
    nodS = fmtInt((long long)(nod + 0.5)) + (serial ? "" : "*");
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
           << "  " << std::setw(5) << effCol(fit.elo[i], cpu) << "  " << rankDisplayId(id);
        if (fit.provisional[i]) ln << " ~provisional";
        if (st == "off") ln << " (off)";
        if (st == "gone") ln << " (retired)";
        cout << ln.str() << "\n";
    }
    cout << "\n";
}

// Rating outputs are named after the match store, so a second pool can be rated
// without clobbering the first. "ranking/matches.jsonl" -> ranking/ratings.tsv (the
// historical names, unchanged); "ranking/matches_open.jsonl" -> ranking/ratings_open.tsv,
// standings_open.tsv, games_open.tsv, report_open.md.
static string g_outSuffix;

static void setOutSuffixFromStore(const string& storeFile) {
    g_outSuffix.clear();
    size_t slash = storeFile.find_last_of("/\\");
    string base = (slash == string::npos) ? storeFile : storeFile.substr(slash + 1);
    size_t dot = base.find('.');
    if (dot != string::npos) base = base.substr(0, dot);
    if (base.size() > 7 && base.compare(0, 7, "matches") == 0) g_outSuffix = base.substr(7);
}

static string outPath(const char* stem, const char* ext) {
    return string("ranking/") + stem + g_outSuffix + ext;
}

static void writeRatingsTsv(const RankFit& fit, const std::vector<int>& order,
                            const std::map<string, AgentAgg>& agg,
                            const std::map<string, string>& state) {
    std::ofstream f(outPath("ratings", ".tsv").c_str());
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

// CURRENT STANDINGS: the comparison-safe view of the same fit.
//
// ratings.tsv holds EVERY agent ever rated, including retired ones ("gone":
// superseded @N code versions frozen at whatever game count they had when they
// left the roster). Quoting a retired row as current strength, or comparing
// agents across different search heads, are the two ways a standings claim goes
// wrong -- an agent is search + evaluator, so a row is only comparable to rows
// with the SAME head. This file removes both traps: active agents only, and a
// head column to group by, so "same search, different evaluator" is a filter
// rather than a judgement call.
// Drop the turn weight from an evaluator segment when the search cannot act on
// it. `evalLeaf` adds +t/-t purely by side to move, so at a fixed depth every
// leaf shares one ply parity and receives the same constant -- which shifts all
// subtree values equally and reorders nothing. Turn only becomes live at mixed
// leaf parity: quiescence (`qs`) or a retained cut iteration (`part`). Printing
// the inert value invites false comparisons (a "chip/turn ratio" means nothing
// when t does not affect play), so the standings show an effective evaluator
// with t elided. The canonical ID is NEVER rewritten -- it is the permanent
// match-store key -- so this is presentation only.
static string effectiveEvaluator(const string& head, const string& ev) {
    if (head.find(",qs") != string::npos || head.find(",part") != string::npos)
        return ev;                       // turn is live; show it
    size_t op = ev.find("(t");
    if (op == string::npos) return ev;   // no turn weight (e.g. learned(...))
    size_t i = op + 2;
    if (i < ev.size() && ev[i] == '-') i++;
    size_t d0 = i;
    while (i < ev.size() && ev[i] >= '0' && ev[i] <= '9') i++;
    if (i == d0) return ev;              // "(t" not followed by a number
    if (i < ev.size() && ev[i] == ',') i++;   // swallow the separator too
    else if (i < ev.size() && ev[i] == ')') return ev;  // t is the only weight; keep it
    return ev.substr(0, op + 1) + ev.substr(i);
}

static void writeStandingsTsv(const RankFit& fit, const std::vector<int>& order,
                              const std::map<string, AgentAgg>& agg,
                              const std::map<string, string>& state) {
    std::ofstream f(outPath("standings", ".tsv").c_str());
    if (!f.is_open()) return;
    f << "# Current standings: active roster only, from the same fit as ratings.tsv.\n"
      << "# Compare Elo only WITHIN one head and WITHIN this file (never across fits).\n"
      << "# Retired ('gone') agents are excluded by design; see ratings.tsv for the full history.\n"
      << "# eff_evaluator elides the turn weight t when the search cannot act on it (no qs/part):\n"
      << "#   t shifts every leaf by one constant at fixed depth, so it reorders nothing there.\n"
      << "#   Compare cores on eff_evaluator; 'evaluator' and 'id' keep the exact canonical form.\n";
    f << "head\telo\tpm\tgames\tcpu_ms_move\tactive\teff_evaluator\tevaluator\tid\n";
    // Group by search head (the ID up to its first '.'), heads ordered by their
    // strongest member, agents within a head by Elo -- the order a reader wants.
    std::vector<string> heads;
    std::map<string, double> headBest;
    for (size_t r = 0; r < order.size(); r++) {
        int i = order[r];
        const string& id = fit.ids[i];
        string st = stateFor(state, id);
        if (st != "on" && st != "anchor") continue;
        string head = id.substr(0, id.find('.'));
        if (!headBest.count(head)) { heads.push_back(head); headBest[head] = fit.elo[i]; }
        else if (fit.elo[i] > headBest[head]) headBest[head] = fit.elo[i];
    }
    for (size_t h = 0; h + 1 < heads.size(); h++)
        for (size_t k = 0; k + 1 < heads.size() - h; k++)
            if (headBest[heads[k]] < headBest[heads[k+1]]) std::swap(heads[k], heads[k+1]);
    for (size_t h = 0; h < heads.size(); h++) {
        for (size_t r = 0; r < order.size(); r++) {
            int i = order[r];
            const string& id = fit.ids[i];
            string st = stateFor(state, id);
            if (st != "on" && st != "anchor") continue;
            size_t dot = id.find('.');
            string head = id.substr(0, dot);
            if (head != heads[h]) continue;
            string ev = (dot == string::npos) ? string("-") : id.substr(dot + 1);
            const AgentAgg& a = aggFor(agg, id);
            double cpu = cpuMsPerMove(a);
            f << head << "\t" << roundElo(fit.elo[i]) << "\t" << roundElo(fit.se[i]) << "\t"
              << a.games << "\t" << (cpu >= 0.0 ? fmtN(cpu, 3) : string("")) << "\t"
              << st << "\t" << effectiveEvaluator(head, ev) << "\t" << ev << "\t" << id << "\n";
        }
    }
}

// Machine-readable per-game export (one row per stored game, empty = unrecorded).
static void writeGamesTsv(const std::vector<RankMatchRow>& rows) {
    std::ofstream f(outPath("games", ".tsv").c_str());
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

// Classifies a CANONICAL id into ranking/CHAMPION.md's 3-division x 2-track
// category scheme, for report.md's division/track columns. Mechanical fact
// about the id only -- does NOT encode the reference-class eligibility
// exclusion (the d8/nb2m oracle), which is a separate title-holding judgment
// documented in CHAMPION.md, not a property of the id itself.
void rankCategoryOf(const string& id, string& division, string& track) {
    division = "-"; track = "-";
    size_t dot = id.find('.');
    string head = (dot == string::npos) ? id : id.substr(0, dot);
    // `cal=<N>ms` marks the wall-clock track's CALIBRATED form: the agent carries a
    // per-core cap (a node budget, or a fixed depth) that was chosen so it spends
    // about that much wall clock. Nothing in the search reads the clock, so it
    // reproduces where a live `time=` head does not.
    //
    // This is tested BEFORE `nodes=` and the order is load-bearing. A calibrated
    // wall-clock agent carries BOTH labels: `nodes=` says how it is capped and
    // `cal=` says which track it belongs to. Only `cal=` separates it from a
    // node-track agent using the same capping mechanism, so testing `nodes=` first
    // would file every calibrated time-track agent under "node" and silently merge
    // the two categories that exist to be compared.
    if (head.find("cal=") != string::npos || head.find("time=") != string::npos) track = "time";
    else if (head.find("nodes=") != string::npos) track = "node";

    bool hasOpener8 = id.find(".opener(rand,moves=8)@") != string::npos;
    bool hasDil20 = id.find(".dil(prob=20)@") != string::npos;
    bool hasAnyOpener = id.find(".opener(") != string::npos;
    bool hasAnyDil = id.find(".dil(") != string::npos;

    if (hasOpener8 && !hasAnyDil) division = "opener8";
    else if (hasDil20 && !hasAnyOpener) division = "dil20";
    else if (!hasAnyOpener && !hasAnyDil) division = "openless";
    // else: a non-titled opener/dilution combination -- left as "-".
}

// One precomputed active-agent row, shared by report.md's grouped table, flat
// table, and exceptions section so each stat is computed once, not per-view.
struct ActiveRow {
    int idx;
    string id, division, track;
    double elo; bool provisional;
    double se;
    long long games;
    bool hasMargin; double margin;
    double avgPlies;
    string edge;              // "+N%"/"-N%"/"0%"/"-"
    double cpu;               // ms/move, -1 if unavailable
    string effStr;
    string wallStr, nodStr;   // pre-formatted wall/mv, nodes/mv (may carry '*')
};

static ActiveRow buildActiveRow(int i, const RankFit& fit, const std::map<string, AgentAgg>& agg) {
    ActiveRow r;
    r.idx = i;
    r.id = fit.ids[i];
    rankCategoryOf(r.id, r.division, r.track);
    r.elo = fit.elo[i];
    r.provisional = fit.provisional[i];
    r.se = fit.se[i];
    const AgentAgg& a = aggFor(agg, r.id);
    r.games = a.games;
    r.hasMargin = a.marginGames > 0;
    r.margin = r.hasMargin ? a.marginSum / a.marginGames : 0.0;
    r.avgPlies = (a.games > 0) ? (double)a.pliesSum / a.games : 0.0;
    long long tw = a.winsW + a.lossesW, tb = a.winsB + a.lossesB;
    r.edge = "-";
    if (tw > 0 && tb > 0) {
        double wRate = 100.0 * a.winsW / tw, bRate = 100.0 * a.winsB / tb;
        long long rr = roundElo(wRate - bRate);
        r.edge = (rr > 0 ? "+" : "") + std::to_string(rr) + "%";
    }
    r.cpu = cpuMsPerMove(a);
    r.effStr = effCol(r.elo, r.cpu);
    timingCols(a, r.wallStr, r.nodStr);
    return r;
}

static double medianOf(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    return (n % 2 == 1) ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

static void meanStd(const std::vector<double>& v, double& mean, double& sd) {
    mean = 0.0; sd = 0.0;
    if (v.empty()) return;
    for (size_t i = 0; i < v.size(); i++) mean += v[i];
    mean /= (double)v.size();
    if (v.size() < 2) return;
    double ss = 0.0;
    for (size_t i = 0; i < v.size(); i++) { double d = v[i] - mean; ss += d * d; }
    sd = std::sqrt(ss / (double)(v.size() - 1));
}

static void writeReportMd(const RankFit& fit, const std::vector<int>& order,
                          const std::map<string, AgentAgg>& agg,
                          const std::map<string, string>& state,
                          const std::vector<RankAgent>& roster,
                          const std::map<std::pair<string,string>, PairAgg>& pairs,
                          const string& board, const string& storeFile,
                          size_t nRows, const string& anchorId) {
    std::ofstream f(outPath("report", ".md").c_str());
    if (!f.is_open()) return;

    std::map<string, double> eloBy;
    std::map<string, int> fitIdx;
    for (size_t i = 0; i < fit.ids.size(); i++) { eloBy[fit.ids[i]] = fit.elo[i]; fitIdx[fit.ids[i]] = (int)i; }

    f << "# Agent ranking report\n\n";
    f << "Generated " << nowUtc() << ". Board `" << board << "`. "
      << nRows << " games from `" << storeFile << "`, " << fit.ids.size() << " rated agents.\n\n";
    // This report lists EVERY rated agent, retired ones included. Two ways a claim
    // taken from it goes wrong (see Docs/benchmarking.md, "Elo comparison hygiene").
    f << "> **Reading this table:** it lists every agent ever rated, including RETIRED ones "
      << "(marked `(retired)`; superseded `@N` identities frozen at old game counts -- their Elo "
      << "is not current strength). It also spans different SEARCH HEADS, and an agent is "
      << "search + evaluator, so `ab(d6,tt,ord,nb200k)` and `ab(d6,ord,nb200k)` are different "
      << "agents whose Elos are not interchangeable. For a current-standings comparison read "
      << "`ranking/standings.tsv` (active only, grouped by head) instead, fix ONE head, and "
      << "compare only within this one fit. Full rules: `Docs/benchmarking.md`.\n\n";
    f << "Fit: Bradley-Terry MM refit over the full store, prior 0.5 virtual games per played pair, "
      << "anchor `" << anchorId << "` = Elo 0. `+/-` is one standard error. "
      << "`white win%`/`black win%` are this agent's own win rate when playing that color "
      << "(wins / (wins+losses) played as that color); a wide gap between the two is a first-move/"
      << "color-advantage signal, not a strength signal -- compare `white win%` to `black win%` on "
      << "the SAME agent, never across agents. `white edge` = `white win%` - `black win%`, signed "
      << "(`-` if either color has no games yet); positive means this agent wins more often as White "
      << "than as Black. `division`/`track` classify the id into "
      << "`ranking/CHAMPION.md`'s 3-division (openless/opener8/dil20) x 2-track (node/time) category "
      << "scheme (`-` = a non-titled opener/dilution combination, e.g. a book opener); this is a "
      << "mechanical fact about the id, not a title-eligibility check (the d8/nb2m reference-class "
      << "oracle still shows `openless`/`node` here even though CHAMPION.md excludes it from holding "
      << "that title). "
      << "`ms/move` is per-move process CPU time in milliseconds, not a CPU-core count "
      << "(contention-safe, valid in parallel runs). "
      << "`eff (Elo/2x cpu)` = Elo / log2(1 + cpu_us/move), the Elo bought per doubling of per-move "
      << "compute -- a rough dollars-per-Elo-point measure across agents of different cost, not a "
      << "standalone quality score. "
      << "`wall/mv` prefers serial games; `*` marks a fallback that includes contended parallel moves. "
      << "`nodes/mv` is the average search-node count per move, the direct way to check whether a "
      << "`time=`-budgeted agent's actual node use looks like the `nodes=`-budgeted track's, or has "
      << "overshot its wall-clock budget instead (see `ranking/CHAMPION.md`'s time-budget defect note). "
      << "`margin (end pieces)` is the average end-of-game piece lead (own minus opponent) -- positive "
      << "means this agent usually finishes with more pieces than its opponent, whether or not it won. "
      << "`~` marks agents whose games do not connect to the anchor (rated relative to their own mean of 1000). "
      << "`id` here is a human-readable simplification (`rankReportId`): `nodes=200k` (the default "
      << "budget) and `tt`/`ord` are dropped unconditionally (assumed on for nearly the whole roster), "
      << "but a `noTT`/`noOrd` marker is printed for the minority of agents that deviate rather than "
      << "silently rendering identically to the standard config; a `learned(...)` core leads with its "
      << "training regime and drops the content hash. This is NOT the canonical id -- quote "
      << "`ranking/standings.tsv`'s `id` column instead when citing an agent anywhere else.\n\n";
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
            string division, track;
            rankCategoryOf(id, division, track);
            long long tw = a.winsW + a.lossesW, tb = a.winsB + a.lossesB;
            double wRate = (tw > 0) ? 100.0 * a.winsW / tw : -1.0;
            double bRate = (tb > 0) ? 100.0 * a.winsB / tb : -1.0;
            string wPct = (tw > 0) ? fmtN(wRate, 0) + "%" : string("-");
            string bPct = (tb > 0) ? fmtN(bRate, 0) + "%" : string("-");
            string edge = "-";
            if (tw > 0 && tb > 0) {
                long long r = roundElo(wRate - bRate);   // integer round avoids a "-0%" display
                edge = (r > 0 ? "+" : "") + std::to_string(r) + "%";
            }
            f << "| " << rank << " | " << roundElo(fit.elo[i]) << (fit.provisional[i] ? "~" : "")
              << " | " << pm << " | " << fmtInt(a.games) << " | " << division << " | " << track
              << " | " << wPct << " | " << bPct << " | " << edge
              << " | " << fmtN(a.games > 0 ? (double)a.pliesSum / a.games : 0.0, 0)
              << " | " << (a.marginGames > 0 ? fmtN(a.marginSum / a.marginGames, 1) : string("-"))
              << " | " << (cpu >= 0.0 ? fmtN(cpu, 2) : string("-"))
              << " | " << effCol(fit.elo[i], cpu)
              << " | " << ms << " | " << nod << " | " << st << " | `" << rankReportId(id) << "` |\n";
        }
        static void head(std::ofstream& f) {
            f << "| rank | Elo | +/- | games | division | track | white win% | black win% | white edge "
              << "| avg plies | margin (end pieces) | ms/move | eff (Elo/2x cpu) | wall/mv | nodes/mv "
              << "| state | id |\n";
            f << "|---:|---:|---:|---:|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|\n";
        }
    };

    // Active agents: precompute one ActiveRow per agent, then render three
    // views from it (grouped, flat, exceptions) rather than recomputing per
    // view. `order` (hence activeOrder) is already Elo-descending.
    std::vector<ActiveRow> activeRows;
    activeRows.reserve(activeOrder.size());
    for (size_t r = 0; r < activeOrder.size(); r++)
        activeRows.push_back(buildActiveRow(activeOrder[r], fit, agg));

    static const char* kBuckets[6][2] = {
        {"openless", "node"}, {"openless", "time"},
        {"opener8", "node"},  {"opener8", "time"},
        {"dil20", "node"},    {"dil20", "time"},
    };

    f << "## Ratings by category (active agents)\n\n";
    f << "The primary \"who's winning\" view: grouped into `ranking/CHAMPION.md`'s 6 titled "
      << "categories (Elo-sorted within each group), so this never mixes agents across "
      << "incomparable heads/loadouts the way one flat Elo sort would. A `+N%`/`-N%` `edge` is "
      << "`white win%` - `black win%` for that agent; SE, game count, margin, and avg plies are "
      << "not shown per row -- they only matter as exceptions, see below. Compute cost per row: "
      << "`ms/move` OR `nodes/mv`, whichever this group's `track` actually budgets on.\n\n";
    for (int b = 0; b < 6; b++) {
        string wantDiv = kBuckets[b][0], wantTrk = kBuckets[b][1];
        std::vector<const ActiveRow*> grp;
        for (size_t r = 0; r < activeRows.size(); r++)
            if (activeRows[r].division == wantDiv && activeRows[r].track == wantTrk)
                grp.push_back(&activeRows[r]);
        if (grp.empty()) continue;
        f << "### " << wantDiv << " x " << wantTrk << "\n\n";
        f << "| rank | Elo | edge | " << (wantTrk == string("time") ? "ms/move" : "nodes/mv") << " | eff | id |\n";
        f << "|---:|---:|---:|---:|---:|---|\n";
        for (size_t k = 0; k < grp.size(); k++) {
            const ActiveRow& r = *grp[k];
            f << "| " << (k + 1) << " | " << roundElo(r.elo) << (r.provisional ? "~" : "")
              << " | " << r.edge << " | " << (wantTrk == string("time") ? r.wallStr : r.nodStr)
              << " | " << r.effStr << " | `" << rankReportId(r.id) << "` |\n";
        }
        f << "\n";
    }
    {
        std::vector<const ActiveRow*> other;
        for (size_t r = 0; r < activeRows.size(); r++)
            if (activeRows[r].division == "-" || activeRows[r].track == "-") other.push_back(&activeRows[r]);
        if (!other.empty()) {
            f << "### other (book openers, non-titled opener/dilution values, non-standard budgets)\n\n";
            f << "| rank | Elo | edge | ms/move | nodes/mv | eff | id |\n";
            f << "|---:|---:|---:|---:|---:|---:|---|\n";
            for (size_t k = 0; k < other.size(); k++) {
                const ActiveRow& r = *other[k];
                f << "| " << (k + 1) << " | " << roundElo(r.elo) << (r.provisional ? "~" : "")
                  << " | " << r.edge << " | " << (r.cpu >= 0.0 ? fmtN(r.cpu, 2) : string("-"))
                  << " | " << r.nodStr << " | " << r.effStr << " | `" << rankReportId(r.id) << "` |\n";
            }
            f << "\n";
        }
    }

    f << "## All active agents (flat)\n\n";
    f << "Quick global scan across every category at once, by raw Elo -- NOT a valid ranking "
      << "across the `division`/`track` boundary (an agent here can outrank a real contender "
      << "purely by playing a cheaper or reference-class configuration); use the grouped tables "
      << "above for any \"who's winning\" claim.\n\n";
    f << "| rank | Elo | division | track | eff | id |\n";
    f << "|---:|---:|---|---|---:|---|\n";
    for (size_t r = 0; r < activeRows.size(); r++) {
        const ActiveRow& row = activeRows[r];
        f << "| " << (r + 1) << " | " << roundElo(row.elo) << (row.provisional ? "~" : "")
          << " | " << row.division << " | " << row.track << " | " << row.effStr
          << " | `" << rankReportId(row.id) << "` |\n";
    }
    f << "\n";

    // Notable exceptions: per-group median/mean+stddev on SE, games, margin,
    // avg plies, flagging only agents that deviate -- the "did anything look
    // off" half of this report's job. Skipped for a group under 4 members,
    // where a median/stddev is not meaningful.
    f << "## Notable exceptions (active agents)\n\n";
    f << "Flagged only when an agent deviates from its OWN division x track group -- most "
      << "agents appear here zero times. `high SE` = needs more games before its rank is "
      << "trustworthy; `few games` = thin data even if SE looks fine; `margin outlier`/"
      << "`plies outlier` = end-game piece lead or game length far from its peers, worth a look. "
      << "(Every active agent's `wall/mv`/`nodes/mv` carries the `*` fallback marker -- this "
      << "roster is always played via sharded parallel workers, never serially, so that is a "
      << "property of the whole store, not a per-agent exception, and is not flagged here.)\n\n";
    bool anyFlag = false;
    for (int b = -1; b < 6; b++) {
        string wantDiv = (b >= 0) ? kBuckets[b][0] : "", wantTrk = (b >= 0) ? kBuckets[b][1] : "";
        std::vector<const ActiveRow*> grp;
        for (size_t r = 0; r < activeRows.size(); r++) {
            bool inOther = (activeRows[r].division == "-" || activeRows[r].track == "-");
            if (b < 0) { if (inOther) grp.push_back(&activeRows[r]); }
            else if (!inOther && activeRows[r].division == wantDiv && activeRows[r].track == wantTrk)
                grp.push_back(&activeRows[r]);
        }
        if ((int)grp.size() < 4) continue;
        std::vector<double> ses, gamesD, margins, pliesV;
        for (size_t k = 0; k < grp.size(); k++) {
            ses.push_back(grp[k]->se);
            gamesD.push_back((double)grp[k]->games);
            if (grp[k]->hasMargin) margins.push_back(grp[k]->margin);
            pliesV.push_back(grp[k]->avgPlies);
        }
        double medSE = medianOf(ses), medGames = medianOf(gamesD);
        double mMargin, sdMargin, mPlies, sdPlies;
        meanStd(margins, mMargin, sdMargin);
        meanStd(pliesV, mPlies, sdPlies);
        string label = (b >= 0) ? (wantDiv + " x " + wantTrk) : "other";
        for (size_t k = 0; k < grp.size(); k++) {
            const ActiveRow& r = *grp[k];
            std::vector<string> flags;
            if (medSE > 0.0 && r.se > 1.5 * medSE)
                flags.push_back("high SE (" + fmtN(r.se, 0) + " vs group median " + fmtN(medSE, 0) + ")");
            if (medGames > 0.0 && (double)r.games < 0.5 * medGames)
                flags.push_back("few games (" + fmtInt(r.games) + " vs group median " + fmtInt((long long)medGames) + ")");
            if (r.hasMargin && sdMargin > 0.0 && std::fabs(r.margin - mMargin) > 2.0 * sdMargin)
                flags.push_back("margin outlier (" + fmtN(r.margin, 1) + " vs group mean " + fmtN(mMargin, 1) + ")");
            if (sdPlies > 0.0 && std::fabs(r.avgPlies - mPlies) > 2.0 * sdPlies)
                flags.push_back("plies outlier (" + fmtN(r.avgPlies, 0) + " vs group mean " + fmtN(mPlies, 0) + ")");
            if (flags.empty()) continue;
            anyFlag = true;
            f << "- `" << rankReportId(r.id) << "` (" << label << ", Elo " << roundElo(r.elo) << "): ";
            for (size_t fi = 0; fi < flags.size(); fi++) f << (fi ? "; " : "") << flags[fi];
            f << "\n";
        }
    }
    if (!anyFlag) f << "None.\n";
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
            f << "| ms/move | Elo | eff | frontier | id |\n";
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
                  << rankReportId(fit.ids[i]) << "` |\n";
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
        for (size_t i = 0; i < unrated.size(); i++) f << "- `" << rankReportId(unrated[i]) << "`\n";
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
            f << "| " << (r + 1) << " | `" << rankReportId(rid) << "` |";
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
        f << "### " << (r + 1) << ". `" << rankReportId(id) << "` (Elo " << roundElo(fit.elo[i]) << " " << pm << ")\n\n";

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
            f << "| `" << rankReportId(o.opp) << "` | " << (long long)o.pa.n << " | " << w << "-" << l
              << " | " << fmtN(sc, 2) << " | " << expS << " | " << dltS
              << " | " << fmtN((double)o.pa.pliesSum / o.pa.n, 0) << " |\n";
        }
        f << "\n";
    }
}

int rankMatchup(const string& rosterFile, const string& storeFile, const string& board,
                int minGames) {
    std::vector<RankAgent> roster;
    string err;
    if (!rankLoadRosterFile(rosterFile, roster, err)) { cout << "ERROR: " << err << "\n"; return 1; }
    string anchorId;
    for (size_t i = 0; i < roster.size(); i++) if (roster[i].anchor) anchorId = roster[i].id;

    std::vector<RankMatchRow> rows;
    int skipped = 0;
    rankLoadMatches(storeFile, board, rows, skipped);
    if (rows.empty()) { cout << "No games in " << storeFile << " for board " << board << ".\n"; return 1; }

    RankFit fit;
    rankFitBT(rows, anchorId, fit);
    std::vector<RankMatchupCell> cells;
    rankMatchupByRegime(rows, fit, cells);

    // Roster census: a residual only skews the pooled table in proportion to how
    // much of the pool wears that regime, so the two belong in one report.
    std::map<string,int> census;
    for (size_t i = 0; i < roster.size(); i++)
        if (roster[i].active) census[rankAgentRegime(roster[i].id)]++;
    int activeTotal = 0;
    for (std::map<string,int>::const_iterator it = census.begin(); it != census.end(); ++it)
        activeTotal += it->second;

    cout << "Regime matchups over " << rows.size() << " games, " << fit.ids.size()
         << " rated agents (board " << board << ")\n\n";
    cout << "Active roster by regime (" << activeTotal << " agents):\n";
    for (std::map<string,int>::const_iterator it = census.begin(); it != census.end(); ++it) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "  %-16s %4d  %5.1f%%",
                      it->first.c_str(), it->second,
                      activeTotal ? 100.0 * it->second / activeTotal : 0.0);
        cout << buf << "\n";
    }

    cout << "\nA vs B, both colours combined. 'actual' and 'Elo-exp' are A's score rate;\n"
         << "'resid' is actual minus expected, i.e. how far the one-number model is off.\n"
         << "A positive residual means A does BETTER against B than its rating predicts.\n\n";
    char hdr[200];
    std::snprintf(hdr, sizeof(hdr), "  %-16s %-16s %9s %8s %8s %8s",
                  "A", "B", "games", "actual", "Elo-exp", "resid");
    cout << hdr << "\n";

    // Largest absolute residual first: the worst model failures are the point.
    std::vector<std::pair<double, size_t> > order;
    for (size_t i = 0; i < cells.size(); i++) {
        if (cells[i].games < minGames) continue;
        double resid = cells[i].score / cells[i].games - cells[i].expected / cells[i].games;
        order.push_back(std::make_pair(-std::fabs(resid), i));
    }
    std::sort(order.begin(), order.end());
    for (size_t k = 0; k < order.size(); k++) {
        const RankMatchupCell& c = cells[order[k].second];
        double act = c.score / c.games, exp_ = c.expected / c.games;
        char buf[200];
        std::snprintf(buf, sizeof(buf), "  %-16s %-16s %9.0f %7.1f%% %7.1f%% %+7.1f",
                      c.a.c_str(), c.b.c_str(), c.games, 100.0 * act, 100.0 * exp_,
                      100.0 * (act - exp_));
        cout << buf << "\n";
    }
    if (order.empty()) cout << "  (no regime pair reached --min-games " << minGames << ")\n";
    cout << "\nA large residual is a TRANSITIVITY violation: Bradley-Terry gives each agent\n"
         << "one strength, so it cannot represent 'A is strong but happens to lose to B'.\n"
         << "Where residuals are large, pooled Elo depends on the pool's regime mix, and\n"
         << "adding or removing a bloc will move ratings that its games never touched.\n";
    return 0;
}

int rankRate(const string& rosterFile, const string& storeFile, const string& board,
             const string& pinFile, bool regimeBalanced) {
    setOutSuffixFromStore(storeFile);
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
    if (!pinFile.empty()) {
        // Screening fit: hold every agent listed in pinFile at its existing Elo
        // and solve only for the rest. Keeps the reference scale fixed so an
        // earlier fit's numbers stay comparable for the whole of a study.
        std::map<std::string,double> pinned;
        if (!loadPinnedRatings(pinFile, pinned, err)) { cout << "ERROR: " << err << "\n"; return 1; }
        // A pinned fit is a screening artifact and must never overwrite the
        // canonical ratings/standings the project quotes from, so it gets its own
        // "_pinned" output family (same mechanism the second store uses).
        g_outSuffix += "_pinned";
        cout << "PINNED FIT: " << pinned.size() << " frozen rating(s) from " << pinFile << ".\n"
             << "  Screening only -- pinned agents cannot move, so this fit can never\n"
             << "  dethrone a champion. Certify with a plain 'rate' (no --pin).\n"
             << "  Writing ranking/*" << g_outSuffix << ".* (canonical files untouched).\n";
        rankFitBTPinned(rows, pinned, fit);
    } else {
        if (regimeBalanced) {
            // Like --pin, this is a different question from the canonical fit, so
            // it gets its own output family rather than overwriting the table
            // every doc quotes. Promoting it to canonical is a deliberate act.
            g_outSuffix += "_balanced";
            cout << "REGIME-BALANCED FIT: every regime bloc weighted equally regardless of\n"
                 << "  how many agents wear it, so the rating answers 'strong against the\n"
                 << "  space of strategies' rather than 'strong against this pool'.\n"
                 << "  Writing ranking/*" << g_outSuffix << ".* (canonical files untouched).\n";
        }
        rankFitBT(rows, anchorId, fit, regimeBalanced);
        if (!fit.anchored)
            cout << "WARNING: anchor " << anchorId
                 << " has no games; ratings centered on mean 1000 instead of anchor = 0\n";
    }

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
    writeStandingsTsv(fit, order, agg, state);
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
    cout << "wrote " << outPath("ratings", ".tsv") << ", " << outPath("standings", ".tsv")
         << " (active only, by head), " << outPath("games", ".tsv") << " and "
         << outPath("report", ".md") << "\n";
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
               << rankDisplayId(m.w) << " (W) vs " << rankDisplayId(m.b)
               << " : " << m.r << " in " << m.plies
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
            playedByOpener = g_openers[moverSpec.openerKind].fn(side, h / 2, h, moverSpec.openerArg, moverSpec.openerArg2, victor);
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
        g_useRootFilter = false;   // one-ply root narrowing (cbook), see playOneGame
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
                playedByOpener = g_openers[moverSpec.openerKind].fn(side, h / 2, h, moverSpec.openerArg, moverSpec.openerArg2, victor);
            if (!playedByOpener) victor = agentChooseMove(moverSpec, side);
            g_useRootFilter = false;   // one-ply root narrowing (cbook), see playOneGame
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
// CBOOKDUMP -- replay winning games, record clustering points
// ============================================================
// The expensive half of cluster-book mining, and the only half that depends on
// the game set. The store keeps summary rows with no move list, so recovering
// what was played means re-running both agents' searches from the start board,
// exactly as bookgen does. Clustering is cheap and depends only on the recorded
// triples, so it lives in cbookfit and every (clusters, keep, seed, mirror)
// combination reuses one dump instead of forcing another replay pass. Same split
// as `rank.exe extract` -> `train.exe --from-data`.
//
// Three things this does that bookgen does not, each of them load-bearing:
//
//  - DEDUPLICATES rows before replaying. The store holds about 0.41 distinct
//    games per row (measured 2026-08-26 over the loaded parts), because a
//    deterministic pair replays one line per colour however many rows it has. An
//    exact-hash book does not care, since a duplicate re-sets the same entry, but
//    a frequency-weighted cluster book would count that single line once per row
//    in both the centroids and the move counts.
//  - SKIPS DRIFTED REPLAYS, following rankExtract's determinism guard rather than
//    bookgen's "keep it if A still won". Cross-game search state makes replay
//    imperfect (theory 19b: models/book2.txt's own header records 12 of 32
//    replays drifting on a pair with no stochastic element), and mining a game
//    that did not happen while attributing it to the stored winner would put a
//    fault in the instrument rather than in the result.
//  - GATES ON WINNER STRENGTH. SMARTSTART mined professional games. This store is
//    dominated by weak play (of the wins whose winner is in ranking/ratings.tsv,
//    2.2% came from agents at 1000+ Elo and 33% from below 600), so --min-elo is
//    the closest available analogue. Ids from rankLoadMatches are already run
//    through rankUpgradeId, so they join to the ratings file directly.
//
// Output is JSONL: one meta row carrying the start board's own vector, then one
// row per recorded position with the RAW (uncanonicalised) difference vector and
// the RAW move. Canonicalisation is cbookfit's business, so a single dump can
// serve the canon, augment, and off mirror modes.
// The dump's own writer, for the two free-text meta fields. Agent ids carry
// parentheses and commas but never quotes or backslashes, so this is a guard
// against a future id grammar rather than a live need.
static string cbookJsonEscape(const string& s) {
    string o;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '"' || s[i] == '\\') o += '\\';
        o += s[i];
    }
    return o;
}

// The opener segment is always emitted last (see the id-building code above:
// evaluator segment(s), then an optional .dil(...), then an optional
// .opener(...)@N, with nothing appended after it), so "core" scope strips
// everything from ".opener(" onward rather than parsing the segment itself.
static string rankIdWithoutOpener(const string& id) {
    size_t p = id.find(".opener(");
    return (p == string::npos) ? id : id.substr(0, p);
}

int rankClusterBookDump(const string& storeFile, const string& board, const string& idA,
                        const string& coreId, const string& regime, double minElo,
                        const string& ratingsFile, int maxPlies, int sampleN,
                        unsigned seed, const string& outFile) {
    if (outFile.empty()) { cout << "ERROR: cbookdump needs --out (data/cbook_<scope>.jsonl)\n"; return 1; }
    const int scopeCount = (!idA.empty() ? 1 : 0) + (!coreId.empty() ? 1 : 0) + (!regime.empty() ? 1 : 0);
    if (scopeCount > 1) {
        cout << "ERROR: cbookdump takes at most one of --a, --core, and --regime (none = universal scope)\n";
        return 1;
    }
    if (maxPlies < 1) maxPlies = 1;
    const string coreKey = rankIdWithoutOpener(coreId);
    const string scope = !idA.empty() ? ("agent:" + idA)
                       : !coreId.empty() ? ("core:" + coreKey)
                       : !regime.empty() ? ("regime:" + regime) : "universal";

    std::map<string, double> elo;
    if (minElo > 0.0) {
        if (!readRatingsTsv(ratingsFile, elo)) {
            cout << "ERROR: --min-elo needs a readable ratings file (--ratings, default ranking/ratings.tsv). "
                 << "Run `rank.exe rate` first: the rating outputs are gitignored.\n";
            return 1;
        }
    }

    std::vector<RankMatchRow> rows;
    int skipped = 0;
    rankLoadMatches(storeFile, board, rows, skipped);
    if (rows.empty()) {
        cout << "ERROR: no matches for board " << board << " in " << storeFile << "\n";
        return 1;
    }

    // Scope filter, strength gate, and row deduplication in one pass. The dedup
    // key is the full game identity: same pair, same colours, same seed, same
    // result, same length is the same trajectory.
    std::set<string> seenGame;
    std::vector<const RankMatchRow*> games;
    int offScope = 0, belowGate = 0, unrated = 0, dupRows = 0;
    for (size_t i = 0; i < rows.size(); i++) {
        const RankMatchRow& row = rows[i];
        if (row.r != 'W' && row.r != 'B') continue;
        const string& winner = (row.r == 'W') ? row.w : row.b;
        if (!idA.empty()) {
            if (winner != idA) { offScope++; continue; }
        } else if (!coreId.empty()) {
            if (rankIdWithoutOpener(winner) != coreKey) { offScope++; continue; }
        } else if (!regime.empty()) {
            if (rankAgentRegime(winner) != regime) { offScope++; continue; }
        }
        if (minElo > 0.0) {
            std::map<string, double>::const_iterator e = elo.find(winner);
            if (e == elo.end()) { unrated++; continue; }
            if (e->second < minElo) { belowGate++; continue; }
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "|%u|%c|%d", row.seed, row.r, row.plies);
        string key = row.w + "|" + row.b + buf;
        if (!seenGame.insert(key).second) { dupRows++; continue; }
        games.push_back(&row);
    }
    if (games.empty()) {
        cout << "ERROR: no winning games matched scope " << scope << " for board " << board << "\n";
        return 1;
    }

    // Deterministic shuffle, same convention as rankExtract: a bigger --sample is
    // a prefix extension of a smaller one rather than an unrelated draw.
    std::vector<int> order(games.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
    srand(seed);
    for (size_t i = order.size(); i > 1; i--) {
        size_t j = (size_t)(((double)rand() / ((double)RAND_MAX + 1.0)) * i);
        if (j >= i) j = i - 1;
        std::swap(order[i-1], order[j]);
    }
    const int target = (sampleN > 0 && sampleN < (int)order.size()) ? sampleN : (int)order.size();

    cout << "cbookdump: scope " << scope << ", " << rows.size() << " store rows -> "
         << games.size() << " distinct winning games (" << offScope << " off scope, "
         << belowGate << " below the Elo gate, " << unrated << " winner unrated, "
         << dupRows << " duplicate rows collapsed), replaying " << target
         << ", recording the winner's own first " << maxPlies << " half-moves\n";

    if (!reloadBoard(board)) { cout << "ERROR: cannot load board " << board << "\n"; return 1; }
    MlcVec startVec;
    mlcBoardVector(startVec);

    ensureDir("data");
    std::ofstream out(outFile.c_str(), std::ios::trunc);
    if (!out.is_open()) { cout << "ERROR: cannot write " << outFile << "\n"; return 1; }
    {
        char hex[40];
        snprintf(hex, sizeof(hex), "%016llx%016llx",
                 (unsigned long long)startVec.b[1], (unsigned long long)startVec.b[0]);
        out << "{\"t\":\"meta\",\"scope\":\"" << cbookJsonEscape(scope) << "\",\"board\":\""
            << cbookJsonEscape(board) << "\",\"start\":\"" << hex << "\",\"maxPlies\":" << maxPlies
            << ",\"minElo\":" << minElo << ",\"sample\":" << target << ",\"seed\":" << seed << "}\n";
    }

    int replayed = 0, drifted = 0, idSkipped = 0, diffFail = 0;
    long long positions = 0;
    for (int k = 0; k < target; k++) {
        const RankMatchRow& row = *games[order[k]];
        RankAgent wa, ba;
        string err;
        if (!rankAgentFromId(row.w, wa, err) || !rankAgentFromId(row.b, ba, err)) { idSkipped++; continue; }
        std::vector<const RankAgent*> pair;
        pair.push_back(&wa); pair.push_back(&ba);
        if (!loadModelSlots(pair, err)) { idSkipped++; continue; }

        const bool winnerIsWhite = (row.r == 'W');
        if (!reloadBoard(board)) { cout << "ERROR: cannot load board " << board << "\n"; return 1; }
        srand(row.seed);
        std::vector<string> rec;
        int victor = None;
        for (int h = 0; h < 400; h++) {
            int side = (h % 2 == 0) ? White : Black;
            const bool record = ((side == White) == winnerIsWhite) && h < maxPlies;
            MlcVec cur, diff;
            BoardSnap before;
            if (record) { mlcBoardVector(cur); mlcDiff(startVec, cur, diff); snapBoard(before); }
            const AgentSpec& moverSpec = (side == White) ? wa.spec : ba.spec;
            bool playedByOpener = false;
            if (moverSpec.openerKind >= 0 && moverSpec.openerKind < g_openerCount)
                playedByOpener = g_openers[moverSpec.openerKind].fn(side, h / 2, h, moverSpec.openerArg, moverSpec.openerArg2, victor);
            if (!playedByOpener) victor = agentChooseMove(moverSpec, side);
            g_useRootFilter = false;   // one-ply root narrowing (cbook), see playOneGame
            if (record) {
                int sx, sy, dx;
                if (diffMoveFromSnap(before, side, sx, sy, dx)) {
                    char hex[40];
                    snprintf(hex, sizeof(hex), "%016llx%016llx",
                             (unsigned long long)diff.b[1], (unsigned long long)diff.b[0]);
                    std::ostringstream ls;
                    ls << "{\"h\":" << h << ",\"v\":\"" << hex << "\",\"sx\":" << sx
                       << ",\"sy\":" << sy << ",\"dx\":" << dx << "}";
                    rec.push_back(ls.str());
                } else {
                    diffFail++;
                }
            }
            if (gameOutcome(victor)) break;
        }
        const int oc = gameOutcome(victor);
        const char r = (oc == 1) ? 'W' : (oc == 2) ? 'B' : 'D';
        // Determinism guard: a replay that did not reproduce the stored result is
        // a different game, so its positions are not this winner's line.
        if (r != row.r) { drifted++; continue; }
        for (size_t i = 0; i < rec.size(); i++) out << rec[i] << "\n";
        positions += (long long)rec.size();
        replayed++;
        if (replayed % 200 == 0)
            cout << "  replayed " << replayed << "/" << target << " games, " << positions << " positions\n";
    }
    out.close();
    mlClearSlots();
    cout << "Kept " << replayed << " of " << target << " replays (" << drifted
         << " drifted from the stored result, " << idSkipped << " unparseable/stale ids, "
         << diffFail << " move-diff failures), " << positions << " positions -> "
         << outFile << "\n";
    return positions > 0 ? 0 : 1;
}

// ============================================================
// CBOOKFIT -- cluster a dump into cbook files
// ============================================================
// The cheap half. Reads a cbookdump, clusters each half-move bucket independently
// with spherical k-means (src/ml_cluster.cpp), and writes one models/cbook<N>.txt
// per requested (clusters, keep) pair, numbered from --out-slot.
//
// Buckets are independent, which is why the ply window is a runtime cap rather
// than a mining axis: a dump taken to half-move 32 contains a half-move 6 bucket
// identical to the one a dump taken to half-move 8 would contain, so the `cbook`
// opener's own `ply=` argument selects the window from one file. Only the
// winner's own plies are recorded, so even buckets come from White winners and
// odd buckets from Black winners, and each holds roughly half the dump.
//
// --keep truncates each cluster's move list to its top M by count, and it is the
// parameter that actually matters. There are 22 legal moves at half-move 0, so a
// cluster holding hundreds of positions covers essentially every legal move and
// an untruncated whitelist filters nothing. --keep 0 writes the full list, which
// is the no-op control arm.
struct CbookPoint {
    MlcVec v;
    int sx, sy, dx;
};

// Count-descending, with the move triple as a tie-break so a book file is
// reproducible from the same dump rather than depending on map iteration order.
static bool cbookMoveMoreCommon(const MlcMove& a, const MlcMove& b) {
    if (a.count != b.count) return a.count > b.count;
    if (a.sx != b.sx) return a.sx < b.sx;
    if (a.sy != b.sy) return a.sy < b.sy;
    return a.dx < b.dx;
}

static bool cbookParseHexVec(const string& s, MlcVec& v) {
    if (s.size() != 32) return false;
    mlcClear(v);
    for (int half = 0; half < 2; half++) {
        unsigned long long acc = 0ULL;
        for (int i = 0; i < 16; i++) {
            char c = s[half * 16 + i];
            int d;
            if      (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return false;
            acc = (acc << 4) | (unsigned long long)d;
        }
        v.b[half == 0 ? 1 : 0] = acc;
    }
    return true;
}

int rankClusterBookFit(const string& dumpFile, const std::vector<int>& clusterList,
                       const std::vector<int>& keepList, const string& mirrorMode,
                       unsigned seed, int minPerCluster, int outSlot) {
    if (dumpFile.empty()) { cout << "ERROR: cbookfit needs --in (a cbookdump file)\n"; return 1; }
    if (clusterList.empty() || keepList.empty()) { cout << "ERROR: cbookfit needs --clusters and --keep\n"; return 1; }
    if (mirrorMode != "canon" && mirrorMode != "augment" && mirrorMode != "off") {
        cout << "ERROR: --mirror must be canon, augment, or off\n";
        return 1;
    }
    if (outSlot < 1) { cout << "ERROR: --out-slot must be >= 1\n"; return 1; }
    if (minPerCluster < 1) minPerCluster = MLC_MIN_PER_CLUSTER;

    std::ifstream in(dumpFile.c_str());
    if (!in.is_open()) { cout << "ERROR: cannot open " << dumpFile << "\n"; return 1; }

    MlcVec startVec;
    mlcClear(startVec);
    bool sawMeta = false;
    string metaScope, metaBoard;
    std::map<int, std::vector<CbookPoint> > byPly;
    long long nPoints = 0, badRows = 0;
    string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line.find("\"t\":\"meta\"") != string::npos) {
            string sv;
            if (!jsonStr(line, "start", sv) || !cbookParseHexVec(sv, startVec)) {
                cout << "ERROR: bad or missing start vector in dump meta\n";
                return 1;
            }
            jsonStr(line, "scope", metaScope);
            jsonStr(line, "board", metaBoard);
            sawMeta = true;
            continue;
        }
        CbookPoint p;
        string hv;
        if (!jsonStr(line, "v", hv) || !cbookParseHexVec(hv, p.v)) { badRows++; continue; }
        double h = 0, sx = 0, sy = 0, dx = 0;
        if (!jsonNum(line, "h", h) || !jsonNum(line, "sx", sx)
            || !jsonNum(line, "sy", sy) || !jsonNum(line, "dx", dx)) { badRows++; continue; }
        p.sx = (int)sx; p.sy = (int)sy; p.dx = (int)dx;
        byPly[(int)h].push_back(p);
        nPoints++;
    }
    in.close();
    if (!sawMeta) { cout << "ERROR: " << dumpFile << " has no meta row\n"; return 1; }
    if (byPly.empty()) { cout << "ERROR: " << dumpFile << " has no position rows\n"; return 1; }
    cout << "cbookfit: " << nPoints << " positions across " << byPly.size()
         << " half-move buckets from " << dumpFile
         << (badRows ? (" (" + std::to_string(badRows) + " unparseable rows skipped)") : "") << "\n";

    // Apply the mirror mode once, since it does not depend on K or keep.
    //   canon   -> replace each point with the smaller of itself and its mirror,
    //              flipping the move's columns to match. Halves the space exactly.
    //   augment -> keep the point AND add its mirror as a second point.
    //   off     -> leave the raw points alone.
    for (std::map<int, std::vector<CbookPoint> >::iterator it = byPly.begin(); it != byPly.end(); ++it) {
        std::vector<CbookPoint>& pts = it->second;
        if (mirrorMode == "canon") {
            for (size_t i = 0; i < pts.size(); i++)
                if (mlcCanonical(pts[i].v)) {
                    pts[i].sx = SIZE - 1 - pts[i].sx;
                    pts[i].dx = SIZE - 1 - pts[i].dx;
                }
        } else if (mirrorMode == "augment") {
            const size_t n0 = pts.size();
            pts.reserve(n0 * 2);
            for (size_t i = 0; i < n0; i++) {
                CbookPoint m = pts[i];
                mlcMirror(pts[i].v, m.v);
                m.sx = SIZE - 1 - m.sx;
                m.dx = SIZE - 1 - m.dx;
                pts.push_back(m);
            }
        }
    }

    // One clustering per (bucket, K, seed): the move cutoff only truncates the
    // resulting lists, so every keep value reuses the same fit.
    int slot = outSlot;
    for (size_t ki = 0; ki < clusterList.size(); ki++) {
        const int K = clusterList[ki];
        std::map<int, MlcBucket> fitted;
        for (std::map<int, std::vector<CbookPoint> >::const_iterator it = byPly.begin(); it != byPly.end(); ++it) {
            const std::vector<CbookPoint>& pts = it->second;
            std::vector<MlcVec> vecs(pts.size());
            for (size_t i = 0; i < pts.size(); i++) vecs[i] = pts[i].v;
            std::vector<float> centroids;
            std::vector<int> assign;
            const int kEff = mlcSphericalKMeans(vecs, K, minPerCluster, seed, 100, centroids, assign);
            MlcBucket b;
            b.ply = it->first;
            b.points = (int)pts.size();
            b.clusters.resize(kEff > 0 ? kEff : 0);
            for (int c = 0; c < kEff; c++) {
                b.clusters[c].centroid.assign(centroids.begin() + (size_t)c * MLC_DIM,
                                              centroids.begin() + (size_t)(c + 1) * MLC_DIM);
                b.clusters[c].size = 0;
                b.clusters[c].meanCos = 0.0;
            }
            // Move counts and the mean intra-cluster cosine, in one pass.
            std::vector<std::map<string, MlcMove> > tally(kEff > 0 ? kEff : 0);
            for (size_t i = 0; i < pts.size(); i++) {
                const int c = assign[i];
                if (c < 0 || c >= kEff) continue;
                b.clusters[c].size++;
                b.clusters[c].meanCos += mlcCosine(pts[i].v, &b.clusters[c].centroid[0]);
                char key[32];
                snprintf(key, sizeof(key), "%d,%d,%d", pts[i].sx, pts[i].sy, pts[i].dx);
                std::map<string, MlcMove>::iterator mit = tally[c].find(key);
                if (mit == tally[c].end()) {
                    MlcMove mv;
                    mv.sx = pts[i].sx; mv.sy = pts[i].sy; mv.dx = pts[i].dx; mv.count = 1;
                    tally[c][key] = mv;
                } else {
                    mit->second.count++;
                }
            }
            for (int c = 0; c < kEff; c++) {
                if (b.clusters[c].size > 0) b.clusters[c].meanCos /= (double)b.clusters[c].size;
                for (std::map<string, MlcMove>::const_iterator mit = tally[c].begin(); mit != tally[c].end(); ++mit)
                    b.clusters[c].moves.push_back(mit->second);
                std::sort(b.clusters[c].moves.begin(), b.clusters[c].moves.end(), cbookMoveMoreCommon);
            }
            fitted[b.ply] = b;
        }

        for (size_t kk = 0; kk < keepList.size(); kk++) {
            const int keep = keepList[kk];
            MlcBook book;
            book.start = startVec;
            for (std::map<int, MlcBucket>::const_iterator it = fitted.begin(); it != fitted.end(); ++it) {
                MlcBucket b = it->second;
                if (keep > 0)
                    for (size_t c = 0; c < b.clusters.size(); c++)
                        if ((int)b.clusters[c].moves.size() > keep) b.clusters[c].moves.resize(keep);
                book.buckets.push_back(b);
            }
            std::ostringstream path;
            path << "models/cbook" << slot << ".txt";
            std::vector<string> hdr;
            {
                std::ostringstream h1, h2, h3;
                h1 << "dump " << dumpFile << ", scope " << metaScope << ", board " << metaBoard;
                h2 << "clusters " << K << " (requested), keep " << keep
                   << ", mirror " << mirrorMode << ", seed " << seed
                   << ", min-per-cluster " << minPerCluster;
                h3 << "positions " << nPoints << ", buckets " << book.buckets.size();
                hdr.push_back(h1.str());
                hdr.push_back(h2.str());
                hdr.push_back(h3.str());
            }
            string err;
            ensureDir("models");
            if (!mlcSaveBook(path.str(), book, hdr, err)) { cout << "ERROR: " << err << "\n"; return 1; }
            // Per-bucket diagnostics: without these a null strength result cannot be
            // told apart from a clustering that quietly degenerated.
            long long clusters = 0;
            double sumCos = 0.0;
            int cosN = 0, minSize = INT_MAX, maxSize = 0;
            for (size_t bi = 0; bi < book.buckets.size(); bi++)
                for (size_t c = 0; c < book.buckets[bi].clusters.size(); c++) {
                    clusters++;
                    sumCos += book.buckets[bi].clusters[c].meanCos;
                    cosN++;
                    const int s = book.buckets[bi].clusters[c].size;
                    if (s < minSize) minSize = s;
                    if (s > maxSize) maxSize = s;
                }
            cout << "  -> " << path.str() << ": clusters=" << K << " keep=" << keep
                 << " mirror=" << mirrorMode << " | " << book.buckets.size() << " buckets, "
                 << clusters << " clusters, size " << (minSize == INT_MAX ? 0 : minSize)
                 << ".." << maxSize << ", mean intra-cluster cosine "
                 << (cosN ? sumCos / cosN : 0.0) << "\n";
            slot++;
        }
    }
    cout << "Wrote " << (slot - outSlot) << " book(s), slots " << outSlot << ".." << (slot - 1) << "\n";
    return 0;
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

        // Fresh TT per game, the same contract playOneGame and the extract replay
        // path hold. Without it a tt-flagged agent's play here depends on every
        // earlier game in the process, so a deterministic pair does not replay
        // and `--games N` yields N different games rather than N copies of one.
        ttClear();
        retainResetCarry();
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
static int playToConclusion(const RankAgent& wa, const RankAgent& ba, int startHalf,
                            int* pliesOut = nullptr) {
    int victor = None;
    int played = 0;
    for (int h = startHalf; h < 400; h++) {
        int side = (h % 2 == 0) ? White : Black;
        const RankAgent& ag = (side == White) ? wa : ba;
        victor = agentChooseMove(ag.spec, side);
        played++;
        if (gameOutcome(victor)) break;
    }
    if (pliesOut) *pliesOut = played;
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
// MOVE AGREEMENT BETWEEN TWO BUDGET RULES ON ONE CORE (`agree`)
// ============================================================
// The question: if a node-budget agent is replaced by a fixed-depth agent
// calibrated to the same wall clock, how often does the substitute actually
// pick a different move? Neither `determinism` nor `pairgen` can answer it.
// Both compare whole games, so after the FIRST divergence the two agents are
// standing in different positions and their later moves are no longer
// comparable. `agree` keeps them in lockstep: snapshot the position, poll
// both agents from it, then follow only the driver.
//
// Two confounds are handled explicitly.
//  1. TRANSPOSITION-TABLE LEAKAGE. The TT searcher context keys on evaluator,
//     eval params, quiescence and root side, NOT on the budget, so two agents
//     differing only in `nodes=` vs `deep=` share one table. Polling the
//     second agent right after the first searched the same root would let it
//     read the first agent's stored result back out, measuring cache reuse
//     rather than agreement. Both searches therefore run against a wiped TT.
//     The cost is that this is cold-TT play: in a rostered game each agent's
//     table carries across its own moves, and here it does not.
//  2. FORCED PLIES. A position with <= 1 legal move agrees trivially. Those
//     plies are counted separately and excluded from the headline rate.
struct AgreeStat {
    long   polls, same, forced;
    double drvDepth, othDepth, drvMs, othMs, drvNodes, othNodes;
    long   othDeeper, othShallower, othSameDepth;
    long   diffDeeper, diffShallower, diffSameDepth;   // disagreements, split by depth relation
    AgreeStat()
      : polls(0), same(0), forced(0), drvDepth(0), othDepth(0), drvMs(0), othMs(0),
        drvNodes(0), othNodes(0), othDeeper(0), othShallower(0), othSameDepth(0),
        diffDeeper(0), diffShallower(0), diffSameDepth(0) {}
};

static double agreeNowMs() {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

// Wilson score interval for a binomial proportion at 95%.
static void agreeWilson(long k, long n, double& lo, double& hi) {
    if (n <= 0) { lo = 0.0; hi = 0.0; return; }
    const double z = 1.959964;
    double p = (double)k / (double)n;
    double d = 1.0 + z*z/(double)n;
    double c = p + z*z/(2.0*(double)n);
    double s = z * std::sqrt(p*(1.0-p)/(double)n + z*z/(4.0*(double)n*(double)n));
    lo = (c - s) / d; hi = (c + s) / d;
    if (lo < 0.0) lo = 0.0; if (hi > 1.0) hi = 1.0;
}

// The polled agent's `retain` purses (nodes and milliseconds), parked between plies
// so they do not share the driver's. Reset per game in agreeRunDirection, alongside
// retainResetCarry().
static unsigned long long s_agreeOthCarry[2] = { 0, 0 };
static double s_agreeOthTimeCarry[2] = { 0.0, 0.0 };

// One driver half-move. Runs `drv`, records where it left the board, rewinds,
// runs `oth` from the identical position, compares, then leaves the board on
// the DRIVER's move so the game follows the driver. Returns the driver's
// victor code.
static int agreeStep(const RankAgent& drv, const RankAgent& oth, int side, AgreeStat& st,
                     std::ofstream* tsv, int dir, int game, int ply) {
    Move legal[ML_MAX_MOVES];
    int nLegal = generateMoves(side, legal);

    BoardSnap before; snapBoard(before);

    ttClear();
    double t0 = agreeNowMs();
    int victor = agentChooseMove(drv.spec, side);
    double drvMs    = agreeNowMs() - t0;
    double drvDepth = g_lastEffDepth;
    double drvNodes = (double)g_lastNodes;
    BoardSnap after; snapBoard(after);

    // The driver and the polled agent each need their own `retain` purse: they run
    // on the same side at the same ply, so one global purse would let the probe
    // spend the driver's saved nodes and vice versa. The driver's purse is the live
    // g_nodeCarry (the game follows the driver), and the polled agent's is parked
    // in s_agreeOthCarry between plies. The wall-clock purse is parked the same
    // way in s_agreeOthTimeCarry. Inert for agents without `retain`.
    unsigned long long drvCarry[2] = { g_nodeCarry[0], g_nodeCarry[1] };
    double drvTimeCarry[2] = { g_timeCarry[0], g_timeCarry[1] };
    g_nodeCarry[0] = s_agreeOthCarry[0]; g_nodeCarry[1] = s_agreeOthCarry[1];
    g_timeCarry[0] = s_agreeOthTimeCarry[0]; g_timeCarry[1] = s_agreeOthTimeCarry[1];

    restoreBoardSnapshot(before);
    ttClear();
    t0 = agreeNowMs();
    agentChooseMove(oth.spec, side);
    s_agreeOthCarry[0] = g_nodeCarry[0]; s_agreeOthCarry[1] = g_nodeCarry[1];
    s_agreeOthTimeCarry[0] = g_timeCarry[0]; s_agreeOthTimeCarry[1] = g_timeCarry[1];
    g_nodeCarry[0] = drvCarry[0]; g_nodeCarry[1] = drvCarry[1];
    g_timeCarry[0] = drvTimeCarry[0]; g_timeCarry[1] = drvTimeCarry[1];
    double othMs    = agreeNowMs() - t0;
    double othDepth = g_lastEffDepth;
    double othNodes = (double)g_lastNodes;
    bool agreed = (std::memcmp(after.sq, board, sizeof(after.sq)) == 0);

    restoreBoardSnapshot(after);   // follow the driver

    // Per-ply dump. `drv_cd`/`oth_cd` are the COMPLETED depth, floor of the
    // effective depth, and they are the decision-relevant number: without `part`
    // a budget-cut iteration is discarded (ai_minimax.cpp, `adopt`), so an agent
    // reporting effective depth 6.3 plays its completed depth-6 move and the 30%
    // of root moves it looked at under depth 7 changed nothing.
    if (tsv) {
        (*tsv) << dir << "\t" << game << "\t" << ply << "\t"
               << (side == White ? "W" : "B") << "\t" << nLegal << "\t"
               << drvDepth << "\t" << (int)drvDepth << "\t" << (unsigned long long)drvNodes << "\t"
               << othDepth << "\t" << (int)othDepth << "\t" << (unsigned long long)othNodes << "\t"
               << (agreed ? 1 : 0) << "\n";
    }
    if (nLegal <= 1) { st.forced++; return victor; }
    st.polls++;
    if (agreed) st.same++;
    st.drvMs    += drvMs;    st.othMs    += othMs;
    st.drvDepth += drvDepth; st.othDepth += othDepth;
    st.drvNodes += drvNodes; st.othNodes += othNodes;
    if (othDepth > drvDepth + 1e-9)      { st.othDeeper++;    if (!agreed) st.diffDeeper++; }
    else if (othDepth < drvDepth - 1e-9) { st.othShallower++; if (!agreed) st.diffShallower++; }
    else                                 { st.othSameDepth++; if (!agreed) st.diffSameDepth++; }
    return victor;
}

// Play `games` self-play games driven entirely by `drv` (both colours), polling
// `oth` at every ply. Random opening plies are mandatory: both agents are
// deterministic, so without them every game would be the same trajectory.
static void agreeRunDirection(const RankAgent& drv, const RankAgent& oth,
                              int games, const string& boardFile, int openPlies,
                              unsigned runSeed, AgreeStat& st, long& gamesUsed,
                              std::ofstream* tsv, int dir) {
    for (int g = 0; g < games; g++) {
        if (!reloadBoard(boardFile)) { cout << "ERROR: cannot load board " << boardFile << "\n"; return; }
        srand(gameSeed(drv.id, oth.id, g, runSeed));
        bool endedInOpener = false;
        for (int h = 0; h < openPlies && !endedInOpener; h++) {
            int side = (h % 2 == 0) ? White : Black;
            int v = (side == White) ? pureRandomMoveWhite() : pureRandomMoveBlack();
            if (gameOutcome(v)) endedInOpener = true;
        }
        if (endedInOpener) continue;
        gamesUsed++;
        retainResetCarry();
        s_agreeOthCarry[0] = s_agreeOthCarry[1] = 0;
        s_agreeOthTimeCarry[0] = s_agreeOthTimeCarry[1] = 0.0;
        for (int h = openPlies; h < 400; h++) {
            int side = (h % 2 == 0) ? White : Black;
            int v = agreeStep(drv, oth, side, st, tsv, dir, g, h);
            if (gameOutcome(v)) break;
        }
        double lo, hi; agreeWilson(st.same, st.polls, lo, hi);
        cout << "  game " << (g + 1) << "/" << games << ": polls=" << st.polls
             << " agree=" << st.same << " ("
             << (st.polls ? 100.0 * st.same / st.polls : 0.0) << "%, 95% CI "
             << 100.0 * lo << "-" << 100.0 * hi << ")\n" << flush;
    }
}

static void agreeReport(const string& drvId, const string& othId,
                        const AgreeStat& st, long gamesUsed) {
    cout << "\n  driver: " << drvId << "\n";
    cout << "  polled: " << othId << "\n";
    if (st.polls == 0) { cout << "  no non-forced polls\n"; return; }
    double lo, hi; agreeWilson(st.same, st.polls, lo, hi);
    cout << "  games played           " << gamesUsed << "\n";
    cout << "  forced plies (skipped) " << st.forced << "\n";
    cout << "  polls (non-forced)     " << st.polls << "\n";
    cout << "  same move              " << st.same << "\n";
    cout << "  AGREEMENT              " << (100.0 * st.same / st.polls)
         << "%   95% CI " << (100.0 * lo) << "-" << (100.0 * hi) << "%\n";
    cout << "  mean eff depth         driver " << (st.drvDepth / st.polls)
         << "   polled " << (st.othDepth / st.polls) << "\n";
    cout << "  mean nodes/move        driver " << (st.drvNodes / st.polls)
         << "   polled " << (st.othNodes / st.polls) << "\n";
    cout << "  mean ms/move (cold TT) driver " << (st.drvMs / st.polls)
         << "   polled " << (st.othMs / st.polls) << "\n";
    cout << "  polled searched deeper " << st.othDeeper << " plies ("
         << st.diffDeeper << " disagreed)\n";
    cout << "  polled searched equal  " << st.othSameDepth << " plies ("
         << st.diffSameDepth << " disagreed)\n";
    cout << "  polled searched shallower " << st.othShallower << " plies ("
         << st.diffShallower << " disagreed)\n";
}

int rankMoveAgree(const string& idA, const string& idB, int games,
                  const string& boardFile, int openPlies, unsigned runSeed,
                  double* pooledAgreementOut, const string& outTsv) {
    if (pooledAgreementOut) *pooledAgreementOut = -1.0;
    if (games <= 0)     { cout << "ERROR: --games must be positive\n"; return 1; }
    if (openPlies <= 0) { cout << "ERROR: agree needs --open-plies > 0 (both agents are deterministic)\n"; return 1; }
    if (idA.empty() || idB.empty()) { cout << "ERROR: agree needs --a <id> and --b <id>\n"; return 1; }

    RankAgent A, B; string err;
    if (!rankAgentFromId(idA, A, err)) { cout << "ERROR: --a: " << err << "\n"; return 1; }
    if (!rankAgentFromId(idB, B, err)) { cout << "ERROR: --b: " << err << "\n"; return 1; }
    std::vector<const RankAgent*> ags; ags.push_back(&A); ags.push_back(&B);
    if (!loadModelSlots(ags, err)) { cout << "ERROR: " << err << "\n"; return 1; }

    cout << "move agreement, " << games << " games/direction, open-plies=" << openPlies
         << ", board=" << boardFile << ", seed=" << runSeed << "\n";
    cout << "Each direction is self-play by the DRIVER on both colours; the other\n"
         << "agent is polled from the identical position at every ply and its move\n"
         << "is discarded. The TT is wiped before BOTH searches (they share a\n"
         << "searcher context, so otherwise the second would read the first's\n"
         << "entries). Forced plies (<= 1 legal move) are excluded.\n";

    std::ofstream tsv;
    if (!outTsv.empty()) {
        size_t sl = outTsv.find_last_of("/\\");
        if (sl != string::npos) ensureDir(outTsv.substr(0, sl));
        tsv.open(outTsv.c_str());
        if (!tsv) { cout << "ERROR: cannot write " << outTsv << "\n"; return 1; }
        tsv << "dir\tgame\tply\tside\tlegal\tdrv_eff\tdrv_cd\tdrv_nodes\toth_eff\toth_cd\toth_nodes\tagreed\n";
    }
    std::ofstream* tsvp = outTsv.empty() ? 0 : &tsv;

    cout << "\n--- direction 1: A drives, B polled ---\n";
    AgreeStat s1; long used1 = 0;
    agreeRunDirection(A, B, games, boardFile, openPlies, runSeed, s1, used1, tsvp, 1);

    cout << "\n--- direction 2: B drives, A polled ---\n";
    AgreeStat s2; long used2 = 0;
    agreeRunDirection(B, A, games, boardFile, openPlies, runSeed + 7919u, s2, used2, tsvp, 2);
    if (tsvp) { tsv.close(); cout << "\nper-ply rows -> " << outTsv << "\n"; }

    mlClearSlots();

    cout << "\n============ RESULTS ============\n";
    cout << "\n[direction 1]";
    agreeReport(idA, idB, s1, used1);
    cout << "\n[direction 2]";
    agreeReport(idB, idA, s2, used2);

    long tp = s1.polls + s2.polls, ts = s1.same + s2.same;
    if (tp > 0 && pooledAgreementOut) *pooledAgreementOut = (double)ts / (double)tp;
    if (tp > 0) {
        double lo, hi; agreeWilson(ts, tp, lo, hi);
        cout << "\n[pooled] " << ts << "/" << tp << " = " << (100.0 * ts / tp)
             << "% agreement, 95% CI " << (100.0 * lo) << "-" << (100.0 * hi) << "%\n";
    }
    return tp > 0 ? 0 : 1;
}


static const int NP_MAX = 16;

// ============================================================
// NODE PROFILE (nodeprofile)
// ============================================================
// How many nodes has iterative deepening spent by the time it FINISHES each
// depth? g_lastEffDepth cannot answer that: it reports one number for the whole
// search. The per-iteration profile is what says how much of a budget the last
// (usually discarded) iteration is consuming, which is the quantity a
// "do not start an iteration that will not fit" rule has to predict.
//
// One agent self-plays both colours from a seeded random opener and every
// non-forced ply emits its whole ladder. Positions where the search stopped for
// a reason other than the budget (a mate score found, or a nearWinCheck
// short-circuit) are still emitted, tagged by `kind`, so the caller can exclude
// the collapsed-tree endgames rather than guessing a node threshold.
static void nodeProfileGame(const RankAgent& ag, int games, const string& boardFile,
                            int openPlies, unsigned runSeed, std::ofstream& tsv,
                            long& plies, long& used) {
    for (int g = 0; g < games; g++) {
        if (!reloadBoard(boardFile)) { cout << "ERROR: cannot load board " << boardFile << "\n"; return; }
        srand(gameSeed(ag.id, ag.id, g, runSeed));
        bool endedInOpener = false;
        for (int h = 0; h < openPlies && !endedInOpener; h++) {
            int side = (h % 2 == 0) ? White : Black;
            int v = (side == White) ? pureRandomMoveWhite() : pureRandomMoveBlack();
            if (gameOutcome(v)) endedInOpener = true;
        }
        if (endedInOpener) continue;
        used++;
        retainResetCarry();   // per game, unlike the per-ply ttClear below
        for (int h = openPlies; h < 400; h++) {
            int side = (h % 2 == 0) ? White : Black;
            Move legal[ML_MAX_MOVES];
            int nLegal = generateMoves(side, legal);
            ttClear();
            int victor = agentChooseMove(ag.spec, side);
            if (nLegal > 1) {
                plies++;
                tsv << g << "\t" << h << "\t" << (side == White ? "W" : "B")
                    << "\t" << nLegal << "\t" << g_lastEffDepth
                    << "\t" << (int)g_lastEffDepth << "\t" << g_lastBudgetKind
                    << "\t" << g_lastNodes << "\t" << g_lastPartAdopt;
                for (int d = 1; d <= NP_MAX; d++) tsv << "\t" << g_nodesAtDepth[d];
                tsv << "\n";
            }
            if (gameOutcome(victor)) break;
        }
        cout << "  game " << (g + 1) << "/" << games << ": plies=" << plies << "\n" << flush;
    }
}

int rankNodeProfile(const string& id, int games, const string& boardFile,
                    int openPlies, unsigned runSeed, const string& outTsv) {
    if (games <= 0)     { cout << "ERROR: --games must be positive\n"; return 1; }
    if (openPlies <= 0) { cout << "ERROR: nodeprofile needs --open-plies > 0 (the agent is deterministic)\n"; return 1; }
    if (id.empty())     { cout << "ERROR: nodeprofile needs --id <id>\n"; return 1; }
    if (outTsv.empty()) { cout << "ERROR: nodeprofile needs --out <tsv>\n"; return 1; }

    RankAgent A; string err;
    if (!rankAgentFromId(id, A, err)) { cout << "ERROR: --id: " << err << "\n"; return 1; }
    std::vector<const RankAgent*> ags; ags.push_back(&A);
    if (!loadModelSlots(ags, err)) { cout << "ERROR: " << err << "\n"; return 1; }

    cout << "node profile, " << games << " games, open-plies=" << openPlies
         << ", board=" << boardFile << ", seed=" << runSeed << "\n";
    cout << "  agent: " << id << "\n";
    cout << "n<d> is the CUMULATIVE node count when depth d finished, 0 if it never\n"
         << "finished. kind is the BudgetKind that ended the search: 1=depth (includes\n"
         << "a mate score found early), 2=nodes, 3=time. Forced plies are excluded.\n";

    size_t sl = outTsv.find_last_of("/\\");
    if (sl != string::npos) ensureDir(outTsv.substr(0, sl));
    std::ofstream tsv(outTsv.c_str());
    if (!tsv) { cout << "ERROR: cannot write " << outTsv << "\n"; return 1; }
    tsv << "game" << "\t" << "ply" << "\t" << "side" << "\t" << "legal" << "\t" << "eff"
        << "\t" << "cd" << "\t" << "kind" << "\t" << "nodes" << "\t" << "adopt";
    for (int d = 1; d <= NP_MAX; d++) tsv << "\t" << "n" << d;
    tsv << "\n";

    long plies = 0, used = 0;
    nodeProfileGame(A, games, boardFile, openPlies, runSeed, tsv, plies, used);
    tsv.close();
    mlClearSlots();
    cout << "\ngames played " << used << ", non-forced plies " << plies
         << " -> " << outTsv << "\n";
    return 0;
}

// ============================================================
// POSITION-ORACLE LABEL PIPELINE (posgen / label / labelfit)
// ============================================================
// See ranking.h for the pipeline overview. All three subcommands share the
// probitPoint math from ml_model.h, so the per-position label fit and the
// DistModel trainer can never diverge.

static string hashHex16(unsigned long long h) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%016llx", h);
    return string(buf);
}

// Ply band for the pool stratification: {6-10, 11-16, 17-24, 25-34, 35+}.
static int posgenPlyBand(int ply) {
    if (ply <= 10) return 0;
    if (ply <= 16) return 1;
    if (ply <= 24) return 2;
    if (ply <= 34) return 3;
    return 4;
}

int rankPosGen(const string& storeFile, const string& board,
               const string& outTrain, const string& outEval,
               int targetTrain, int targetEval, int perGameCap,
               int minPly, int maxPly, unsigned seed) {
    if (targetTrain <= 0 && targetEval <= 0) { cout << "ERROR: nothing to do (targets are 0)\n"; return 1; }
    std::vector<RankMatchRow> rows;
    int storeSkipped = 0;
    rankLoadMatches(storeFile, board, rows, storeSkipped);
    if (rows.empty()) { cout << "ERROR: no matches for board " << board << " in " << storeFile << "\n"; return 1; }
    cout << "Loaded " << rows.size() << " match rows (" << storeSkipped << " store-parse skipped) for board " << board << "\n";

    // Deterministic shuffle, same convention as rankExtract.
    std::vector<int> order(rows.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = (int)i;
    srand(seed);
    for (size_t i = order.size(); i > 1; i--) {
        size_t j = (size_t)(((double)rand() / ((double)RAND_MAX + 1.0)) * i);
        if (j >= i) j = i - 1;
        std::swap(order[i-1], order[j]);
    }

    // Pre-seed the dedup set from existing pool files so a second pass merges.
    std::set<string> seenEnc;
    int preexisting = 0;
    for (int t = 0; t < 2; t++) {
        const string& f = (t == 0) ? outTrain : outEval;
        std::ifstream in(f.c_str());
        string line, enc;
        while (in.is_open() && std::getline(in, line))
            if (jsonStr(line, "enc", enc) && seenEnc.insert(enc).second) preexisting++;
    }

    struct PoolRow { int tier; string enc; unsigned long long hash; int ply, stm, md, seen; };
    std::vector<PoolRow> kept;
    std::map<string, size_t> keptIdx;                    // enc -> kept index (seen bumps)
    // Quotas: per tier x ply band x |md| bucket.
    int bandQuota[2], bucketCap[2];
    bandQuota[0] = (targetTrain + 4) / 5; bucketCap[0] = (bandQuota[0] + 2) / 3;
    bandQuota[1] = (targetEval  + 4) / 5; bucketCap[1] = (bandQuota[1] + 2) / 3;
    int bandCount[2][5] = {{0}};
    int buckCount[2][5][4] = {{{0}}};
    int tierCount[2] = { 0, 0 };
    int target[2] = { targetTrain, targetEval };

    int replayed = 0, idSkipped = 0, mismatchSkipped = 0, nearWinSkipped = 0, dupSeen = 0;
    for (size_t k = 0; k < order.size(); k++) {
        if (tierCount[0] >= target[0] && tierCount[1] >= target[1]) break;
        const RankMatchRow& row = rows[order[k]];
        RankAgent wa, ba;
        string err;
        if (!rankAgentFromId(row.w, wa, err) || !rankAgentFromId(row.b, ba, err)) { idSkipped++; continue; }
        std::vector<const RankAgent*> pair;
        pair.push_back(&wa); pair.push_back(&ba);
        if (!loadModelSlots(pair, err)) { idSkipped++; continue; }

        // A fresh TT per replay makes each game's replay independent of every
        // other game this process has run (cross-game TT pollution is the known
        // source of order-dependent determinism mismatches).
        ttClear();
        retainResetCarry();
        srand(row.seed);
        std::vector<int> capSide;
        std::vector<std::vector<float> > capFeat;
        std::vector<BoardSnap> snaps;
        int victor = playOneGameCapture(wa, ba, board, 2, capSide, capFeat, nullptr, 0, &snaps);
        int oc = gameOutcome(victor);
        char r = (oc == 1) ? 'W' : (oc == 2) ? 'B' : 'D';
        if (r != row.r) { mismatchSkipped++; continue; }   // determinism drift guard
        replayed++;

        int fromThisGame = 0;
        for (size_t h = 0; h < snaps.size() && fromThisGame < perGameCap; h++) {
            int ply = (int)h;
            if (ply < minPly || ply > maxPly) continue;
            restoreBoardSnapshot(snaps[h]);
            if (nearWinCheck(capSide[h]) != 0) { nearWinSkipped++; continue; }
            PosKey key = positionKey(capSide[h], false);
            std::map<string, size_t>::iterator it = keptIdx.find(key.enc);
            if (it != keptIdx.end()) { kept[it->second].seen++; dupSeen++; continue; }
            if (seenEnc.count(key.enc)) { dupSeen++; continue; }   // from a previous pass

            int tier = (key.hash % 17 == 0) ? 1 : 0;
            int band = posgenPlyBand(ply);
            int md = g_chipDiff;
            int buck = (md < 0 ? -md : md); if (buck > 3) buck = 3;
            if (tierCount[tier] >= target[tier]) continue;
            if (bandCount[tier][band] >= bandQuota[tier]) continue;
            if (buckCount[tier][band][buck] >= bucketCap[tier]) continue;

            PoolRow pr;
            pr.tier = tier; pr.enc = key.enc; pr.hash = key.hash;
            pr.ply = ply; pr.stm = capSide[h]; pr.md = md; pr.seen = 1;
            keptIdx[pr.enc] = kept.size();
            kept.push_back(pr);
            seenEnc.insert(pr.enc);
            tierCount[tier]++; bandCount[tier][band]++; buckCount[tier][band][buck]++;
            fromThisGame++;
        }
        if (replayed % 500 == 0)
            cout << "  replayed " << replayed << " games: " << tierCount[0] << "/" << target[0]
                 << " train, " << tierCount[1] << "/" << target[1] << " eval positions\n" << flush;
    }

    ensureDir("data");
    ensureDir("data/labels");
    std::ofstream fTrain(outTrain.c_str(), std::ios::app);
    std::ofstream fEval(outEval.c_str(), std::ios::app);
    if (!fTrain.is_open() || !fEval.is_open()) { cout << "ERROR: cannot write pool files\n"; return 1; }
    for (size_t i = 0; i < kept.size(); i++) {
        const PoolRow& p = kept[i];
        std::ostringstream ln;
        ln << "{\"enc\":\"" << p.enc << "\",\"h\":\"" << hashHex16(p.hash) << "\""
           << ",\"ply\":" << p.ply << ",\"stm\":\"" << (p.stm == White ? "W" : "B") << "\""
           << ",\"md\":" << p.md << ",\"seen\":" << p.seen << "}";
        ((p.tier == 0) ? fTrain : fEval) << ln.str() << "\n";
    }
    cout << "posgen: replayed " << replayed << " games (" << idSkipped << " unparseable/stale ids, "
         << mismatchSkipped << " determinism mismatches), kept " << tierCount[0] << " train + "
         << tierCount[1] << " eval positions (" << preexisting << " preexisting, " << dupSeen
         << " duplicate encounters, " << nearWinSkipped << " near-win skips) -> "
         << outTrain << " / " << outEval << "\n";
    mlClearSlots();
    return (tierCount[0] > 0 || tierCount[1] > 0) ? 0 : 1;
}

// ---- Ladder spec ----
bool rankLoadLadder(std::istream& in, std::vector<LabelRung>& rungs,
                    std::vector<LabelPairing>& pairs, string& err) {
    rungs.clear(); pairs.clear();
    string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        size_t hashPos = line.find('#');
        if (hashPos != string::npos) line = line.substr(0, hashPos);
        std::istringstream ss(line);
        string kind;
        if (!(ss >> kind)) continue;                     // blank / comment-only
        if (kind == "rung") {
            int idx; string id;
            if (!(ss >> idx >> id)) { err = "line " + std::to_string(lineNo) + ": rung needs '<index> <id>'"; return false; }
            if (idx != (int)rungs.size()) { err = "line " + std::to_string(lineNo) + ": rung indices must be sequential from 0 (got " + std::to_string(idx) + ", expected " + std::to_string(rungs.size()) + ")"; return false; }
            LabelRung r; r.id = id;
            rungs.push_back(r);
        } else if (kind == "pair") {
            LabelPairing p;
            p.modK = 0; p.modR = 0;
            if (!(ss >> p.wi >> p.bi >> p.games)) { err = "line " + std::to_string(lineNo) + ": pair needs '<wi> <bi> <games>'"; return false; }
            string mod;
            if (ss >> mod) {
                if (mod != "mod" || !(ss >> p.modK >> p.modR)) { err = "line " + std::to_string(lineNo) + ": trailing clause must be 'mod <k> <r>'"; return false; }
                if (p.modK <= 0 || p.modR < 0 || p.modR >= p.modK) { err = "line " + std::to_string(lineNo) + ": need mod k > 0 and 0 <= r < k"; return false; }
            }
            if (p.games <= 0) { err = "line " + std::to_string(lineNo) + ": pair games must be positive"; return false; }
            pairs.push_back(p);
        } else {
            err = "line " + std::to_string(lineNo) + ": unknown directive '" + kind + "' (want rung or pair)";
            return false;
        }
    }
    if (rungs.empty()) { err = "ladder has no rungs"; return false; }
    if (pairs.empty()) { err = "ladder has no pairings"; return false; }
    for (size_t i = 0; i < pairs.size(); i++) {
        if (pairs[i].wi < 0 || pairs[i].wi >= (int)rungs.size()
            || pairs[i].bi < 0 || pairs[i].bi >= (int)rungs.size()) {
            err = "pair " + std::to_string(i) + " references a rung index outside 0.." + std::to_string(rungs.size() - 1);
            return false;
        }
    }
    return true;
}

// A rung produces VARIED playouts from a fixed position only if it consumes
// rand() during play: a random-family chooser, or dilution (the per-move
// dilution roll draws from rand(), so different game seeds diverge -- with
// dil(rP,dN) the "diluted" move is itself a shallower search, so a
// depth-diluted strong agent stays near full strength while being genuinely
// stochastic). Advanced-eval noise does NOT count: both noise forms are
// deterministic per NoiseSeed (they reorder choices without ever drawing from
// rand()), so a jitter-vs-jitter pairing replays one identical game per
// position regardless of the game seed.
static bool labelRungStochastic(const RankAgent& a) {
    const AgentSpec& s = a.spec;
    if (s.brain == BRAIN_POLICY)
        return string(g_choosers[s.chooser].name) != "LearnedPolicy";
    if (s.randomMoveProb > 0.0) return true;             // dilution rolls rand() per move
    return false;
}

int rankLabel(const string& poolFile, const string& ladderFile,
              const string& outFile, unsigned runSeed, int shard, int ofK,
              bool resume, const string& doneFile, int maxPositions) {
    if (ofK <= 0 || shard < 0 || shard >= ofK) { cout << "ERROR: need 0 <= shard < of\n"; return 1; }

    // Ladder: parse, resolve every rung, validate.
    std::vector<LabelRung> rungs;
    std::vector<LabelPairing> pairs;
    string err;
    {
        std::ifstream lf(ladderFile.c_str());
        if (!lf.is_open()) { cout << "ERROR: cannot open ladder " << ladderFile << "\n"; return 1; }
        if (!rankLoadLadder(lf, rungs, pairs, err)) { cout << "ERROR: ladder: " << err << "\n"; return 1; }
    }
    std::vector<RankAgent> agents(rungs.size());
    std::vector<const RankAgent*> ptrs;
    for (size_t i = 0; i < rungs.size(); i++) {
        if (!rankAgentFromId(rungs[i].id, agents[i], err)) {
            cout << "ERROR: rung " << i << ": " << err << "\n"; return 1;
        }
        if (rungs[i].id.find(".opener(") != string::npos) {
            cout << "ERROR: rung " << i << " has an identity opener; opener ply counters assume games start at ply 0, which is wrong from a mid-game position\n";
            return 1;
        }
        ptrs.push_back(&agents[i]);
    }
    if (!loadModelSlots(ptrs, err)) { cout << "ERROR: " << err << "\n"; return 1; }
    for (size_t i = 0; i < pairs.size(); i++) {
        if (!labelRungStochastic(agents[pairs[i].wi]) && !labelRungStochastic(agents[pairs[i].bi])) {
            cout << "ERROR: pairing " << i << " (" << rungs[pairs[i].wi].id << " vs "
                 << rungs[pairs[i].bi].id << ") is deterministic on both sides; it would replay one game "
                 << "(give at least one side dilution or a random chooser)\n";
            return 1;
        }
    }

    // Pool.
    struct PoolPos { string enc, hex; unsigned long long hash; };
    std::vector<PoolPos> pool;
    {
        std::ifstream pf(poolFile.c_str());
        if (!pf.is_open()) { cout << "ERROR: cannot open pool " << poolFile << "\n"; return 1; }
        string line;
        while (std::getline(pf, line)) {
            PoolPos p;
            if (!jsonStr(line, "enc", p.enc) || !jsonStr(line, "h", p.hex)) continue;
            p.hash = strtoull(p.hex.c_str(), nullptr, 16);
            pool.push_back(p);
        }
    }
    if (pool.empty()) { cout << "ERROR: pool " << poolFile << " has no usable rows\n"; return 1; }

    // Resume: meta consistency + existing-row counts per (h, wi, bi).
    std::map<string, int> have;
    if (resume) {
        for (int t = 0; t < 2; t++) {
            const string& f = (t == 0) ? outFile : doneFile;
            if (f.empty()) continue;
            string metaPath = f + ".meta.json";
            std::ifstream mf(metaPath.c_str());
            if (mf.is_open()) {
                std::ostringstream all; all << mf.rdbuf();
                string meta = all.str();
                size_t at = meta.find("\"ladder\":[");
                if (at != string::npos) {
                    // Compare the frozen id list against the current ladder.
                    size_t pos = at + 10;
                    size_t idx = 0;
                    while (pos < meta.size() && meta[pos] != ']') {
                        if (meta[pos] == '"') {
                            size_t e = meta.find('"', pos + 1);
                            if (e == string::npos) break;
                            string id = meta.substr(pos + 1, e - pos - 1);
                            if (idx >= rungs.size() || rungs[idx].id != id) {
                                cout << "ERROR: " << metaPath << " ladder mismatch at rung " << idx
                                     << " (store: " << id << ", current: "
                                     << (idx < rungs.size() ? rungs[idx].id : string("<missing>"))
                                     << "); refusing to mix designs\n";
                                return 1;
                            }
                            idx++;
                            pos = e + 1;
                        } else pos++;
                    }
                }
            }
            std::ifstream rf(f.c_str());
            string line, hex;
            double wi, bi;
            while (rf.is_open() && std::getline(rf, line)) {
                if (!jsonStr(line, "h", hex) || !jsonNum(line, "wi", wi) || !jsonNum(line, "bi", bi)) continue;
                have[hex + "|" + std::to_string((int)wi) + "|" + std::to_string((int)bi)]++;
            }
        }
    }

    ensureDir("data");
    ensureDir("data/labels");
    std::ofstream out(outFile.c_str(), resume ? std::ios::app : std::ios::trunc);
    if (!out.is_open()) { cout << "ERROR: cannot write " << outFile << "\n"; return 1; }

    long long rowsWritten = 0, draws = 0, decodeSkipped = 0, decidedSkipped = 0;
    int positionsTouched = 0, positionsSeen = 0;
    std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now();
    bool capped = false;
    for (size_t i = 0; i < pool.size() && !capped; i++) {
        if ((int)(i % (size_t)ofK) != shard) continue;
        const PoolPos& pp = pool[i];
        positionsSeen++;

        // What does this position still need?
        long long needed = 0;
        for (size_t pi = 0; pi < pairs.size(); pi++) {
            const LabelPairing& pr = pairs[pi];
            if (pr.modK > 0 && (int)(pp.hash % (unsigned long long)pr.modK) != pr.modR) continue;
            std::map<string, int>::iterator it = have.find(pp.hex + "|" + std::to_string(pr.wi) + "|" + std::to_string(pr.bi));
            int got = (it == have.end()) ? 0 : it->second;
            if (got < pr.games) needed += pr.games - got;
        }
        if (needed == 0) continue;
        if (maxPositions > 0 && positionsTouched >= maxPositions) { capped = true; break; }

        int stm = White;
        if (!decodePositionEnc(pp.enc, stm)) { decodeSkipped++; continue; }
        if (nearWinCheck(stm) != 0) { decidedSkipped++; continue; }
        positionsTouched++;

        for (size_t pi = 0; pi < pairs.size(); pi++) {
            const LabelPairing& pr = pairs[pi];
            if (pr.modK > 0 && (int)(pp.hash % (unsigned long long)pr.modK) != pr.modR) continue;
            std::map<string, int>::iterator it = have.find(pp.hex + "|" + std::to_string(pr.wi) + "|" + std::to_string(pr.bi));
            int got = (it == have.end()) ? 0 : it->second;
            const RankAgent& wa = agents[pr.wi];
            const RankAgent& ba = agents[pr.bi];
            for (int g = got; g < pr.games; g++) {
                int stm2 = White;
                if (!decodePositionEnc(pp.enc, stm2)) break;
                // Fresh TT per game: without this, a tt-flagged rung's play
                // would depend on every game played earlier in the process,
                // breaking shard-split and resume reproducibility.
                ttClear();
                retainResetCarry();
                unsigned seed = gameSeed(wa.id, ba.id, (long long)pi * 1000 + g,
                                         runSeed ^ (unsigned)(pp.hash & 0xffffffffULL));
                srand(seed);
                int plies = 0;
                int victor = playToConclusion(wa, ba, (stm2 == White) ? 0 : 1, &plies);
                int oc = gameOutcome(victor);
                if (oc == 0) { draws++; continue; }
                out << "{\"h\":\"" << pp.hex << "\",\"wi\":" << pr.wi << ",\"bi\":" << pr.bi
                    << ",\"g\":" << g << ",\"seed\":" << seed << ",\"y\":" << (oc == 1 ? 1 : 0)
                    << ",\"p\":" << plies << "}\n";
                rowsWritten++;
            }
        }
        if (positionsTouched % 25 == 0) {
            double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            double perHour = (secs > 0.5) ? positionsTouched * 3600.0 / secs : 0.0;
            cout << "  labeled " << positionsTouched << " positions (" << rowsWritten
                 << " rows, " << (long long)perHour << " pos/hour)\n" << flush;
        }
    }
    out.close();

    // Meta sidecar: the frozen ladder is the authoritative rung-index mapping.
    {
        std::ofstream meta((outFile + ".meta.json").c_str(), std::ios::trunc);
        if (meta.is_open()) {
            meta << "{\"pool\":\"" << poolFile << "\",\"ladder\":[";
            for (size_t i = 0; i < rungs.size(); i++) { if (i) meta << ","; meta << "\"" << rungs[i].id << "\""; }
            meta << "],\"pairings\":[";
            for (size_t i = 0; i < pairs.size(); i++) {
                if (i) meta << ",";
                meta << "{\"wi\":" << pairs[i].wi << ",\"bi\":" << pairs[i].bi
                     << ",\"games\":" << pairs[i].games << ",\"mod_k\":" << pairs[i].modK
                     << ",\"mod_r\":" << pairs[i].modR << "}";
            }
            meta << "],\"seed\":" << runSeed << ",\"shard\":" << shard << ",\"of\":" << ofK
                 << ",\"positions_touched\":" << positionsTouched
                 << ",\"rows_written\":" << rowsWritten << ",\"draws\":" << draws
                 << ",\"decode_skipped\":" << decodeSkipped
                 << ",\"decided_skipped\":" << decidedSkipped
                 << ",\"capped\":" << (capped ? 1 : 0)
                 << ",\"ts\":\"" << nowUtc() << "\"}\n";
        }
    }
    cout << "label: " << positionsTouched << " positions touched of " << positionsSeen
         << " in shard " << shard << "/" << ofK << ", " << rowsWritten << " rows appended, "
         << draws << " draws dropped, " << decodeSkipped << " undecodable + "
         << decidedSkipped << " decided positions skipped"
         << (capped ? " (stopped at --max-positions)" : "") << " -> " << outFile << "\n";
    mlClearSlots();
    return 0;
}

// ---- Per-position 2-parameter probit MLE ----
int rankFitMuSigma(const std::vector<double>& d, const std::vector<double>& v,
                   const std::vector<double>& y, double& mu, double& s,
                   double& seMu, double& seS) {
    size_t n = y.size();
    mu = 0.0; s = 0.0; seMu = 99.0; seS = 99.0;
    if (n == 0) return 0;

    struct Eval {
        const std::vector<double>* d; const std::vector<double>* v; const std::vector<double>* y;
        double operator()(double m, double sv, double& gM, double& gS) const {
            double L = 0.0; gM = 0.0; gS = 0.0;
            ProbitGrad g;
            for (size_t i = 0; i < y->size(); i++) {
                L += probitPoint(m, sv, (*d)[i], (*v)[i], (*y)[i], g);
                gM += g.gMu; gS += g.gS;
            }
            return L;
        }
    };
    Eval eval; eval.d = &d; eval.v = &v; eval.y = &y;

    double gM, gS;
    double L = eval(mu, s, gM, gS);
    double lr = 0.5 / (double)n;
    int iters = 0;
    for (; iters < 500; iters++) {
        double nm = mu - lr * gM, ns = s - lr * gS;
        if (nm > 10.0) nm = 10.0;
        if (nm < -10.0) nm = -10.0;
        if (ns > PROBIT_S_MAX) ns = PROBIT_S_MAX;
        if (ns < PROBIT_S_MIN) ns = PROBIT_S_MIN;
        double ngM, ngS;
        double nL = eval(nm, ns, ngM, ngS);
        if (nL <= L) {
            bool conv = (std::fabs(nm - mu) < 1e-8 && std::fabs(ns - s) < 1e-8);
            mu = nm; s = ns; L = nL; gM = ngM; gS = ngS;
            lr *= 1.2;
            if (conv) break;
        } else {
            lr *= 0.5;
            if (lr < 1e-14) break;
        }
    }
    // Observed Fisher information via central differences of the analytic
    // gradients; SEs from the inverted 2x2 (99.0 sentinels if degenerate).
    const double eps = 1e-4;
    double gMp, gSp, gMm, gSm;
    eval(mu + eps, s, gMp, gSp); eval(mu - eps, s, gMm, gSm);
    double H00 = (gMp - gMm) / (2 * eps);
    double H10 = (gSp - gSm) / (2 * eps);
    eval(mu, s + eps, gMp, gSp); eval(mu, s - eps, gMm, gSm);
    double H01 = (gMp - gMm) / (2 * eps);
    double H11 = (gSp - gSm) / (2 * eps);
    double off = 0.5 * (H01 + H10);
    double det = H00 * H11 - off * off;
    if (det > 1e-12 && H00 > 0.0 && H11 > 0.0) {
        seMu = std::sqrt(H11 / det);
        seS  = std::sqrt(H00 / det);
    }
    return iters;
}

// Ratings reader variant that also returns the pm (standard error) column.
static bool readRatingsEloPm(const string& path, std::map<string, std::pair<double, double> >& out) {
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
        if (cols.size() < 4) continue;
        try {
            out[cols[cols.size()-1]] = std::make_pair(std::stod(cols[1]), std::stod(cols[2]));
        } catch (...) {}
    }
    return !out.empty();
}

int rankLabelFit(const string& storeFile, const string& poolFile,
                 const string& ratingsFile, const string& outFile,
                 int minRows, bool useRatingSe) {
    // The frozen ladder id list from the store's meta sidecar.
    std::vector<string> ladder;
    {
        std::ifstream mf((storeFile + ".meta.json").c_str());
        if (!mf.is_open()) { cout << "ERROR: cannot open " << storeFile << ".meta.json (the rung-id mapping)\n"; return 1; }
        std::ostringstream all; all << mf.rdbuf();
        string meta = all.str();
        size_t at = meta.find("\"ladder\":[");
        if (at == string::npos) { cout << "ERROR: meta has no ladder array\n"; return 1; }
        size_t pos = at + 10;
        while (pos < meta.size() && meta[pos] != ']') {
            if (meta[pos] == '"') {
                size_t e = meta.find('"', pos + 1);
                if (e == string::npos) break;
                ladder.push_back(meta.substr(pos + 1, e - pos - 1));
                pos = e + 1;
            } else pos++;
        }
    }
    if (ladder.empty()) { cout << "ERROR: empty ladder in meta\n"; return 1; }

    std::map<string, std::pair<double, double> > ratings;
    if (!readRatingsEloPm(ratingsFile, ratings)) { cout << "ERROR: cannot read ratings " << ratingsFile << "\n"; return 1; }
    std::vector<double> rungElo(ladder.size()), rungPm(ladder.size());
    for (size_t i = 0; i < ladder.size(); i++) {
        std::map<string, std::pair<double, double> >::iterator it = ratings.find(ladder[i]);
        if (it == ratings.end()) { cout << "ERROR: rung " << i << " (" << ladder[i] << ") is not in " << ratingsFile << "\n"; return 1; }
        rungElo[i] = it->second.first;
        rungPm[i]  = it->second.second;
    }

    // Pool order + enc lookup.
    std::vector<string> poolOrder;                       // hex, in file order
    std::map<string, string> encByHex;
    {
        std::ifstream pf(poolFile.c_str());
        if (!pf.is_open()) { cout << "ERROR: cannot open pool " << poolFile << "\n"; return 1; }
        string line, enc, hex;
        while (std::getline(pf, line)) {
            if (!jsonStr(line, "enc", enc) || !jsonStr(line, "h", hex)) continue;
            if (encByHex.insert(std::make_pair(hex, enc)).second) poolOrder.push_back(hex);
        }
    }

    // Raw rows grouped per position; d/v cached per (wi, bi).
    struct Obs { std::vector<double> d, v, y; std::vector<int> pairing; };
    std::map<string, Obs> byPos;
    std::map<std::pair<int, int>, std::pair<double, double> > dvByPair;
    struct PairQc { long long games; double wins; double pSum; };
    std::map<std::pair<int, int>, PairQc> qc;
    long long rawRows = 0, rawSkipped = 0;
    {
        std::ifstream rf(storeFile.c_str());
        if (!rf.is_open()) { cout << "ERROR: cannot open store " << storeFile << "\n"; return 1; }
        string line, hex;
        double wi, bi, yv;
        while (std::getline(rf, line)) {
            if (!jsonStr(line, "h", hex) || !jsonNum(line, "wi", wi)
                || !jsonNum(line, "bi", bi) || !jsonNum(line, "y", yv)) { rawSkipped++; continue; }
            int w = (int)wi, b = (int)bi;
            if (w < 0 || w >= (int)ladder.size() || b < 0 || b >= (int)ladder.size()) { rawSkipped++; continue; }
            std::pair<int, int> key(w, b);
            std::map<std::pair<int, int>, std::pair<double, double> >::iterator dv = dvByPair.find(key);
            if (dv == dvByPair.end()) {
                double dd = (rungElo[w] - rungElo[b]) / ELO_PER_LOGIT;
                double vv = useRatingSe
                    ? (rungPm[w] * rungPm[w] + rungPm[b] * rungPm[b]) / (ELO_PER_LOGIT * ELO_PER_LOGIT)
                    : 0.0;
                dv = dvByPair.insert(std::make_pair(key, std::make_pair(dd, vv))).first;
            }
            Obs& o = byPos[hex];
            o.d.push_back(dv->second.first);
            o.v.push_back(dv->second.second);
            o.y.push_back(yv);
            o.pairing.push_back(w * 10000 + b);
            rawRows++;
        }
    }
    if (byPos.empty()) { cout << "ERROR: no usable raw rows in " << storeFile << "\n"; return 1; }

    ensureDir("data");
    ensureDir("data/labels");
    std::ofstream out(outFile.c_str(), std::ios::trunc);
    if (!out.is_open()) { cout << "ERROR: cannot write " << outFile << "\n"; return 1; }

    long long fitted = 0, thin = 0, allwin = 0, allloss = 0, clamped = 0, tooFew = 0, noEnc = 0;
    std::vector<double> allD, allV, allY;
    for (size_t k = 0; k < poolOrder.size(); k++) {
        const string& hex = poolOrder[k];
        std::map<string, Obs>::iterator it = byPos.find(hex);
        if (it == byPos.end()) continue;
        Obs& o = it->second;
        int n = (int)o.y.size();
        if (n < minRows) { tooFew++; continue; }

        double mu, s, seMu, seS;
        rankFitMuSigma(o.d, o.v, o.y, mu, s, seMu, seS);

        double wsum = 0.0;
        ProbitGrad g;
        double nll = 0.0;
        for (int i = 0; i < n; i++) {
            nll += probitPoint(mu, s, o.d[i], o.v[i], o.y[i], g);
            wsum += o.y[i];
            std::pair<int, int> key(o.pairing[i] / 10000, o.pairing[i] % 10000);
            PairQc& q = qc[key];
            q.games++; q.wins += o.y[i]; q.pSum += g.p;
            allD.push_back(o.d[i]); allV.push_back(o.v[i]); allY.push_back(o.y[i]);
        }
        nll /= n;

        string flags = "ok";
        if (wsum >= n - 1e-9)      { flags = "allwin";  allwin++; }
        else if (wsum <= 1e-9)     { flags = "allloss"; allloss++; }
        else if (s >= PROBIT_S_MAX - 1e-6 || s <= PROBIT_S_MIN + 1e-6) { flags = "clamped"; clamped++; }
        else if (n < 32)           { flags = "thin";    thin++; }

        out << "{\"enc\":\"" << encByHex[hex] << "\",\"h\":\"" << hex << "\""
            << ",\"mu_elo\":" << (mu * ELO_PER_LOGIT)
            << ",\"sd_elo\":" << (exp(s) * ELO_PER_LOGIT)
            << ",\"se_mu\":" << (seMu * ELO_PER_LOGIT)
            << ",\"se_sd\":" << (seS < 90.0 ? exp(s) * seS * ELO_PER_LOGIT : 9999.0)
            << ",\"n\":" << n << ",\"nll\":" << nll
            << ",\"flags\":\"" << flags << "\"}\n";
        fitted++;
    }
    // Positions in the store but missing from the pool file (should be none).
    for (std::map<string, Obs>::iterator it = byPos.begin(); it != byPos.end(); ++it)
        if (!encByHex.count(it->first)) noEnc++;
    out.close();

    // Pooled intercept-only baseline: one (mu, s) over every row.
    double bMu, bS, bSeMu, bSeS, baseNll = 0.0;
    if (!allY.empty()) {
        rankFitMuSigma(allD, allV, allY, bMu, bS, bSeMu, bSeS);
        ProbitGrad g;
        for (size_t i = 0; i < allY.size(); i++)
            baseNll += probitPoint(bMu, bS, allD[i], allV[i], allY[i], g);
        baseNll /= (double)allY.size();
    }

    cout << "labelfit: " << fitted << " positions fitted from " << rawRows << " raw rows ("
         << rawSkipped << " malformed rows, " << tooFew << " positions under --min-rows, "
         << noEnc << " store positions missing from the pool)\n";
    cout << "  flags: " << allwin << " allwin, " << allloss << " allloss, " << clamped
         << " clamped, " << thin << " thin\n";
    cout << "  pooled intercept-only baseline: mean NLL " << baseNll << " over "
         << allY.size() << " rows\n";
    cout << "  per-pairing QC (games, empirical White win rate, mean fitted p):\n";
    for (std::map<std::pair<int, int>, PairQc>::iterator it = qc.begin(); it != qc.end(); ++it) {
        char buf[160];
        snprintf(buf, sizeof(buf), "    w=%d b=%d  %8lld games  emp %.3f  fit %.3f\n",
                 it->first.first, it->first.second, it->second.games,
                 it->second.wins / it->second.games, it->second.pSum / it->second.games);
        cout << buf;
    }
    cout << "  labels -> " << outFile << "\n";
    return fitted > 0 ? 0 : 1;
}

// ============================================================
// DETERMINISM PROBE
// ============================================================
// Answers one question: does a deterministic agent, replayed against a fixed
// deterministic opponent, produce the SAME game every time?
//
// rankAgentIsDeterministic() answers whether an agent draws from rand(). That is
// not the same property. An agent can consume no randomness and still play a
// different game on a second run, because search state that outlives one move
// (the transposition table) or a budget measured against the wall clock makes
// its choice depend on something other than the position. This probe measures
// the property that actually matters to any work assuming replayability: mining
// lines against a fixed opponent, counting distinct games behind an error bar,
// or reproducing a stored result.
//
// Design. Replicas are played in an INTERLEAVED order (outer loop = replica,
// inner loop = agent), so replica 2 of an agent is preceded by an entirely
// different sequence of games than replica 1 was. A pass that looped agent-major
// would play each agent's replicas back to back and would not disturb whatever
// process state it is trying to detect. Cross-PROCESS reproducibility is a
// separate question: run the command twice and compare the two TSVs.
//
// Each game's trajectory is fingerprinted by playOneGame's per-ply position-hash
// trace, so "same game" means the same move sequence, not a proxy for it.
int rankDeterminism(const string& rosterFile, const string& probeId, int replicas,
                    const string& board, const string& outFile, const string& only,
                    int shard, int ofK, bool includeStochastic) {
    if (replicas < 2) { cout << "ERROR: --replicas must be >= 2\n"; return 1; }
    if (ofK < 1) ofK = 1;
    if (shard < 0 || shard >= ofK) { cout << "ERROR: --shard must be in [0,--of)\n"; return 1; }

    string err;
    std::vector<RankAgent> roster;
    if (!rankLoadRosterFile(rosterFile, roster, err)) { cout << "ERROR: " << err << "\n"; return 1; }

    RankAgent probe;
    // Composed from the registries, not hardcoded, for the reason given on
    // rankRefute's default oracle: a module version bump re-canonicalises every
    // ID wearing that module, and a baked-in "@N" then refuses to start.
    string pid = probeId;
    if (pid.empty()) {
        std::ostringstream d;
        d << "ab(deep=2)@" << rkExplorerVersion("AlphaBeta")
          << ".classic(chip=100)@" << rkEvalVersion("Classic");
        pid = d.str();
    }
    if (!rankAgentFromId(pid, probe, err)) { cout << "ERROR: bad --probe id: " << err << "\n"; return 1; }
    if (!rankAgentIsDeterministic(probe.spec)) {
        cout << "ERROR: --probe " << probe.id << " is not deterministic, so it would supply the\n"
             << "       variation this probe is trying to attribute to the agent under test\n";
        return 1;
    }

    // Subjects: active, deterministic (unless --include-stochastic), matching
    // --only, in this shard.
    //
    // --include-stochastic exists to be the POSITIVE CONTROL. A probe that only
    // ever reports "reproducible" is indistinguishable from a probe that cannot
    // detect anything, so before trusting a clean sweep, point it at an agent
    // known to draw from rand() (a dil(...) or opener(rand,...) roster line) and
    // confirm it reports > 1 distinct move sequence. Not for ordinary use: a
    // stochastic subject is EXPECTED to fail and says nothing about state.
    std::vector<const RankAgent*> subj;
    int nActive = 0, nStochastic = 0, nDeterministic = 0;
    for (size_t i = 0; i < roster.size(); i++) {
        if (!roster[i].active) continue;
        nActive++;
        bool det = rankAgentIsDeterministic(roster[i].spec);
        if (det) nDeterministic++; else nStochastic++;
        if (!det && !includeStochastic) continue;
        if (!only.empty() && roster[i].id.find(only) == string::npos) continue;
        subj.push_back(&roster[i]);
    }
    std::vector<const RankAgent*> mine;
    for (size_t i = 0; i < subj.size(); i++)
        if ((int)(i % (size_t)ofK) == shard) mine.push_back(subj[i]);
    if (mine.empty()) { cout << "ERROR: no deterministic subjects selected\n"; return 1; }

    std::vector<const RankAgent*> toLoad(mine);
    toLoad.push_back(&probe);
    if (!loadModelSlots(toLoad, err)) { cout << "ERROR: " << err << "\n"; return 1; }

    cout << "determinism probe: " << mine.size() << " subjects selected"
         << " (roster: " << nActive << " active = " << nDeterministic << " deterministic + "
         << nStochastic << " stochastic";
    if (includeStochastic) cout << "; --include-stochastic ON, stochastic subjects are the POSITIVE CONTROL";
    if (!only.empty()) cout << "; --only \"" << only << "\"";
    if (ofK > 1) cout << "; shard " << shard << " of " << ofK;
    cout << "), probe = " << probe.id << ", " << replicas << " replicas x 2 colours"
         << ", " << (mine.size() * 2 * (size_t)replicas) << " games\n" << flush;

    // traces[subject][colour][replica]; colour 0 = subject as White.
    std::vector<std::vector<std::vector<std::vector<unsigned long long> > > > traces(
        mine.size(), std::vector<std::vector<std::vector<unsigned long long> > >(2));
    std::vector<std::vector<std::vector<RankMatchRow> > > rowsOut(
        mine.size(), std::vector<std::vector<RankMatchRow> >(2));

    for (int rep = 0; rep < replicas; rep++) {
        for (size_t i = 0; i < mine.size(); i++) {
            for (int col = 0; col < 2; col++) {
                const RankAgent& wa = (col == 0) ? *mine[i] : probe;
                const RankAgent& ba = (col == 0) ? probe : *mine[i];
                // A DIFFERENT seed per replica, on purpose. A deterministic agent
                // draws from rand() nowhere, so varying the seed cannot change its
                // game -- if it does, the agent is not what rankAgentIsDeterministic
                // claims, which is itself worth catching. Pinning one seed instead
                // would silently disarm --include-stochastic: a dilution agent
                // replays exactly when handed the same seed, so the positive control
                // would report "reproducible" and prove nothing. (Measured: it did,
                // 56/56, before this line varied.)
                srand(12345u + (unsigned)rep);
                RankMatchRow m;
                std::vector<unsigned long long> tr;
                if (!playOneGame(wa, ba, board, m, &tr)) {
                    cout << "ERROR: cannot load board " << board << "\n";
                    mlClearSlots();
                    return 1;
                }
                traces[i][col].push_back(tr);
                rowsOut[i][col].push_back(m);
            }
        }
        cout << "  replica " << (rep + 1) << "/" << replicas << " done\n" << flush;
    }

    ensureDir("ranking");
    std::ofstream out(outFile.c_str(), std::ios::trunc);
    if (out.is_open()) {
        out << "# rank.exe determinism -- one row per (subject, colour)\n";
        out << "# probe=" << probe.id << "\treplicas=" << replicas << "\tboard=" << board << "\n";
        out << "# distinct = distinct move sequences over the replicas (1 = reproducible)\n";
        out << "# first_div_ply = first half-move where two replicas differ (-1 = none)\n";
        out << "# det = rankAgentIsDeterministic (0 = draws from rand(), expected to fail)\n";
        out << "# traceset = hash over the SORTED DISTINCT per-ply position traces. Two runs of\n";
        out << "#   this command agree on a row's trajectories exactly when its traceset matches,\n";
        out << "#   so diffing two TSVs is an exact cross-process test, not a plies/nodes proxy.\n";
        out << "colour\tdet\ttimed\treplicas\tdistinct\tfirst_div_ply\ttraceset\tplies\tresult\tnodes_self\tid\n";
    }

    int repro = 0, total = 0, timedRepro = 0, timedTotal = 0;
    std::vector<const RankAgent*> failures;
    std::vector<int> failColour, failDiv, failDistinct;
    for (size_t i = 0; i < mine.size(); i++) {
        bool timed = (mine[i]->spec.timeBudgetMs > 0.0);
        bool det = rankAgentIsDeterministic(mine[i]->spec);
        for (int col = 0; col < 2; col++) {
            const std::vector<std::vector<unsigned long long> >& T = traces[i][col];
            std::set<std::vector<unsigned long long> > uniq(T.begin(), T.end());
            int distinct = (int)uniq.size();
            // std::set orders the traces, so this hash is independent of the order
            // the replicas happened to be played in.
            unsigned long long tsh = 1469598103934665603ULL;
            for (std::set<std::vector<unsigned long long> >::const_iterator u = uniq.begin();
                 u != uniq.end(); ++u)
                tsh = fnv1a64((const char*)(u->empty() ? nullptr : &(*u)[0]),
                              u->size() * sizeof(unsigned long long), tsh);
            int div = -1;
            for (size_t r = 1; r < T.size() && div < 0; r++) {
                size_t n = T[0].size() < T[r].size() ? T[0].size() : T[r].size();
                for (size_t k = 0; k < n; k++)
                    if (T[0][k] != T[r][k]) { div = (int)k; break; }
                if (div < 0 && T[0].size() != T[r].size()) div = (int)n;
            }
            total++;
            if (timed) timedTotal++;
            if (distinct == 1) { repro++; if (timed) timedRepro++; }
            else { failures.push_back(mine[i]); failColour.push_back(col);
                   failDiv.push_back(div); failDistinct.push_back(distinct); }
            const RankMatchRow& m0 = rowsOut[i][col][0];
            double selfNodes = (col == 0) ? m0.wnod : m0.bnod;
            if (out.is_open())
                out << (col == 0 ? "W" : "B") << "\t" << (det ? 1 : 0) << "\t" << (timed ? 1 : 0) << "\t" << replicas
                    << "\t" << distinct << "\t" << div << "\t" << std::hex << tsh << std::dec
                    << "\t" << m0.plies << "\t" << m0.r
                    << "\t" << (long long)selfNodes << "\t" << mine[i]->id << "\n";
        }
    }
    out.close();

    cout << "\nreproducible (1 distinct move sequence over " << replicas << " replicas): "
         << repro << "/" << total << " subject-colours\n";
    if (timedTotal > 0)
        cout << "  of which wall-clock-budgeted (time=): " << timedRepro << "/" << timedTotal
             << " reproducible, node-budgeted: " << (repro - timedRepro) << "/"
             << (total - timedTotal) << "\n";
    for (size_t f = 0; f < failures.size() && f < 12; f++)
        cout << "  NONREPRO  " << (failColour[f] == 0 ? "W" : "B") << "  " << failDistinct[f]
             << " distinct, first divergence at half-move " << failDiv[f] << "  "
             << failures[f]->id << "\n";
    if (failures.size() > 12)
        cout << "  ... and " << (failures.size() - 12) << " more (see " << outFile << ")\n";
    cout << "  -> " << outFile << "\n";

    mlClearSlots();
    return failures.empty() ? 0 : 2;
}

// ============================================================
// REFUTE
// ============================================================
// Mine ONE position-keyed book that beats every deterministic agent on the
// roster, both colours.
//
// Why this is a search over our own moves rather than a minimax: with both sides
// deterministic, a (book, opponent, colour) triple produces exactly ONE game.
// Beating N opponents is winning 2N specific games, not 2N distributions, so
// finding each line is depth-first search over OUR moves with backtracking, with
// the opponent treated as a fixed reply function of the game.
//
// A fixed function of the PATH, though, not of the position. A `tt`-flagged
// opponent carries its transposition table across the plies of a game
// (playOneGame clears it once, per game, not per ply). So an alternative move
// cannot be tried by un-playing the last one and searching again: the opponent's
// reply would be computed against a table that the real game would never have
// held. Every trial here replays the game from the start. That is the dominant
// cost, and the reason stage 1 below is greedy rather than exhaustive.
//
// It is also ONE book rather than 2N books. `openerBook` keys on
// positionKey(sideToMove) alone, with no ply and no path, so one entry serves
// every line that reaches that position, and two lines needing DIFFERENT moves
// from one position cannot both be expressed. That merge conflict is avoided by
// construction rather than repaired afterwards: the book is shared from the
// first game onward and every move our side plays is committed to it
// immediately, so a later target inherits an earlier one's choice instead of
// deriving its own. Deriving its own would not agree anyway -- the oracle's TT
// state at the same position differs by which opponent led it there.
//
// Stages:
//   1. Sequential mining over the shared book. Play each target once with the
//      current book, falling back to the oracle where the book is silent, and
//      commit every move the oracle picks. A win claims ownership of every
//      position on its line.
//   2. Backtracking repair for the targets that lost. Walk the losing line from
//      the deepest of OUR moves backwards. At a position no WON target owns,
//      re-search with the already-tried moves filtered out of the root, and
//      replay from the start. A position a won target owns is a shared prefix
//      and is left alone, which is how a real conflict surfaces instead of
//      quietly breaking a solved line.
//   3. Prune to the entries the winning lines actually use, write the book, and
//      verify by replaying every target through the real openerBook path with a
//      rostered wearer agent.

struct RefMove { int sx, sy, dx; };

static bool refSameMove(const RefMove& a, const RefMove& b) {
    return a.sx == b.sx && a.sy == b.sy && a.dx == b.dx;
}

// Same 16-digit form the book file and the report use, so a key printed by the
// collision warning can be grepped straight out of models/book<N>.txt.
static string refHexKey(unsigned long long k) {
    std::ostringstream o;
    o << std::hex << std::setw(16) << std::setfill('0') << k;
    return o.str();
}

static string refMoveStr(const RefMove& m) {
    std::ostringstream o;
    o << m.sx << "," << m.sy << "->" << m.dx;
    return o.str();
}

static string refPlural(int n, const char* noun) {
    std::ostringstream o;
    o << n << " " << noun << (n == 1 ? "" : "s");
    return o.str();
}

// Bounds first, for the same reason openerBook checks them: tryMoveQuick* skips
// bounds checks by design, so a bad entry must never index off the board.
static bool refMoveLegal(int side, const RefMove& e) {
    if (e.sx < 0 || e.sx >= SIZE || e.sy < 0 || e.sy >= SIZE
        || e.dx < 0 || e.dx >= SIZE || e.dx < e.sx - 1 || e.dx > e.sx + 1) return false;
    return (side == White) ? tryMoveQuickWhite(e.sx, e.sy, e.dx)
                           : tryMoveQuickBlack(e.sx, e.sy, e.dx);
}

// Narrow the search's root to the legal moves that are NOT in `exclude`, so the
// next search returns the oracle's best remaining candidate. This is the same
// one-shot root whitelist the cbook opener uses (globals.h), and the caller
// clears it right after the move is chosen.
static bool refSetRootFilter(int side, const std::vector<RefMove>& exclude, int& nLeft) {
    Move mv[ML_MAX_MOVES];
    int n = generateMoves(side, mv);
    int c = 0;
    for (int i = 0; i < n && c < ROOT_FILTER_MAX; i++) {
        bool skip = false;
        for (size_t e = 0; e < exclude.size(); e++)
            if (exclude[e].sx == mv[i].sx && exclude[e].sy == mv[i].sy
                && exclude[e].dx == mv[i].dx) { skip = true; break; }
        if (skip) continue;
        g_rootMoveWhitelist[c][0] = mv[i].sx;
        g_rootMoveWhitelist[c][1] = mv[i].sy;
        g_rootMoveWhitelist[c][2] = mv[i].dx;
        c++;
    }
    g_rootMoveWhitelistCount = c;
    nLeft = c;
    if (c == 0) { g_useRootFilter = false; return false; }
    g_useRootFilter = true;
    return true;
}

struct RefGame {
    int outcome;                                  // 1 = we won, 2 = we lost, 0 = draw/ply cap
    int plies;
    std::vector<unsigned long long> ourKeys;      // position hash at each of OUR turns
    std::vector<RefMove> ourMoves;                // what we played there
    bool probeExhausted;                          // the probed ply had no untried legal move
    bool probeIgnored;                            // the search returned an EXCLUDED move anyway
    int  oobFirst;                                // first of OUR plies the book did not serve (-1 = none)
    int  oobCount;                                // how many of OUR plies the book did not serve
    unsigned long long oobKey;                    // position hash at oobFirst (0 if none)
    int  overwritePly;                            // first of OUR plies where the book served a
                                                  // move DIFFERENT from the one the mine recorded
                                                  // at the same position (-1 = never)
    unsigned long long overwriteKey;              // position hash there (0 if none)
};

// One full game, our side driven by the book with the oracle as fallback.
//
// probeOurPly >= 0 suppresses the book at that one of OUR moves (counted in our
// own moves, not half-moves) and re-searches with `exclude` filtered out of the
// root, which is how stage 2 asks for the oracle's next-best move at a node.
// Everything before it replays identically, which is the point: the opponent
// reaches the probed ply with exactly the table it would really have.
static bool refPlayGame(const std::map<unsigned long long, RefMove>& book,
                        const RankAgent& oppAg, const AgentSpec& oracle,
                        int ourColour, const string& boardFile,
                        int probeOurPly, const std::vector<RefMove>* exclude,
                        RefGame& g,
                        const std::vector<unsigned long long>* minedKeys = 0,
                        const std::vector<RefMove>* minedMoves = 0) {
    if (!reloadBoard(boardFile)) return false;
    ttClear();
    retainResetCarry();
    g.outcome = 0; g.plies = 0;
    g.ourKeys.clear(); g.ourMoves.clear();
    g.probeExhausted = false; g.probeIgnored = false;
    g.oobFirst = -1; g.oobCount = 0; g.oobKey = 0;
    g.overwritePly = -1; g.overwriteKey = 0;

    const int ourSide = (ourColour == 0) ? White : Black;
    int victor = None;
    int ourPly = 0;
    for (int h = 0; h < 400; h++) {
        int side = (h % 2 == 0) ? White : Black;
        g_lastNodes = 0;
        if (side == ourSide) {
            unsigned long long key = positionKey(side, false).hash;
            RefMove chosen; chosen.sx = chosen.sy = chosen.dx = -1;
            bool haveBook = false;
            if (ourPly != probeOurPly) {
                std::map<unsigned long long, RefMove>::const_iterator it = book.find(key);
                if (it != book.end() && refMoveLegal(side, it->second)) {
                    chosen = it->second; haveBook = true;
                }
            }
            if (haveBook) {
                victor = (side == White) ? playMoveWhite(chosen.sx, chosen.sy, chosen.dx)
                                         : playMoveBlack(chosen.sx, chosen.sy, chosen.dx);
            } else {
                BoardSnap before;
                for (int y = 0; y < SIZE; y++)
                    for (int x = 0; x < SIZE; x++) before.sq[x][y] = board[x][y];
                if (ourPly == probeOurPly && exclude && !exclude->empty()) {
                    int nLeft = 0;
                    if (!refSetRootFilter(side, *exclude, nLeft)) {
                        g.probeExhausted = true;
                        return true;                       // no untried legal move here
                    }
                }
                victor = agentChooseMove(oracle, side);
                g_useRootFilter = false;
                if (!diffMoveFromSnap(before, side, chosen.sx, chosen.sy, chosen.dx))
                    return false;                          // move recovery failed: a bug, not a result
                if (ourPly != probeOurPly) {
                    // The book was silent (or held an entry illegal here) and the
                    // oracle covered for it. On a line the book is supposed to own
                    // outright, that is the defect, so record where it first happened.
                    if (g.oobFirst < 0) { g.oobFirst = ourPly; g.oobKey = key; }
                    g.oobCount++;
                }
                if (ourPly == probeOurPly && exclude)
                    for (size_t e = 0; e < exclude->size(); e++)
                        if (refSameMove((*exclude)[e], chosen)) g.probeIgnored = true;
            }
            // An overwrite is visible here and nowhere else: the replay stands on the
            // SAME position the mine recorded for this line, and the book hands back a
            // DIFFERENT move. That is a later winner having overwritten this line's
            // entry, observed rather than inferred from where the line later fell out
            // of book.
            if (minedKeys && minedMoves && g.overwritePly < 0
                && ourPly < (int)minedKeys->size() && (*minedKeys)[ourPly] == key
                && !refSameMove(chosen, (*minedMoves)[ourPly])) {
                g.overwritePly = ourPly; g.overwriteKey = key;
            }
            g.ourKeys.push_back(key);
            g.ourMoves.push_back(chosen);
            ourPly++;
        } else {
            // The opponent plays exactly as playOneGame would: its own opener
            // first (with its own move count), then its brain.
            bool playedByOpener = false;
            if (oppAg.spec.openerKind >= 0 && oppAg.spec.openerKind < g_openerCount)
                playedByOpener = g_openers[oppAg.spec.openerKind].fn(
                    side, h / 2, h, oppAg.spec.openerArg, oppAg.spec.openerArg2, victor);
            if (!playedByOpener) victor = agentChooseMove(oppAg.spec, side);
            g_useRootFilter = false;
        }
        g.plies = h + 1;
        if (gameOutcome(victor)) break;
    }
    int oc = gameOutcome(victor);
    g.outcome = (oc == 0) ? 0
              : (((oc == 1) == (ourSide == White)) ? 1 : 2);
    return true;
}

struct RefTarget {
    const RankAgent* opp;
    int  colour;          // 0 = the book plays White, 1 = the book plays Black
    int  status;          // 0 = unsolved, 1 = won, 2 = conceded
    int  plies;
    int  ourMoveCount;
    int  triedNodes;      // stage-2 positions examined
    int  triedMoves;      // stage-2 alternative moves searched
    bool blockedShared;   // stage 2 ran out of positions it was allowed to change
    bool auditWon;        // stage-3 audit: won replaying against the WRITTEN book
    int  oobFirst;        // stage-3 audit: first of OUR moves the book did not serve (-1 = none)
    int  oobCount;        // stage-3 audit: how many of OUR moves the book did not serve
    unsigned long long oobKey;  // stage-3 audit: position hash at oobFirst (0 if none). Printed so
                          // a line that left the book can be matched against the position a stage-3
                          // collision overwrote, instead of the match being guessed from ply numbers.
    int  rejected;        // stage-2 wins refused because committing the line would have
                          // overwritten a position an already-won line depends on
    int  overwritePly;    // stage-3 audit: first of OUR plies where the written book served a
                          // move different from the one the mine recorded at that same position
    unsigned long long overwriteKey;  // position hash there (0 if none)
    std::vector<unsigned long long> keys;
    std::vector<RefMove> moves;
};

int rankRefute(const string& rosterFile, const string& oracleId, const string& wearerId,
               int slot, const string& board, const string& only, const string& colours,
               int maxTries, long long maxGames, bool skipTimed, bool force,
               bool verifyOnly, const string& outFile) {
    if (slot <= 0) { cout << "ERROR: refute needs --slot <N> (writes models/book<N>.txt)\n"; return 1; }
    if (maxTries < 1) maxTries = 1;

    string err;
    std::vector<RankAgent> roster;
    if (!rankLoadRosterFile(rosterFile, roster, err)) { cout << "ERROR: " << err << "\n"; return 1; }

    // Composed from the registries rather than written out, so a module version
    // bump (which re-canonicalises every ID wearing that module) cannot leave a
    // stale "@N" baked into this default. It did: the "ab" bump to @2 on
    // 2026-08-28 broke a hardcoded "@1" here, and the failure mode is that the
    // command refuses to start at all until someone edits this line.
    string oid = oracleId;
    if (oid.empty()) {
        std::ostringstream d;
        d << "ab(deep=8,tt,ord,nodes=2m)@" << rkExplorerVersion("AlphaBeta")
          << ".classic(chip=100)@"        << rkEvalVersion("Classic");
        oid = d.str();
    }
    RankAgent oracle;
    if (!rankAgentFromId(oid, oracle, err)) { cout << "ERROR: bad --oracle id: " << err << "\n"; return 1; }
    if (!rankAgentIsDeterministic(oracle.spec)) {
        cout << "ERROR: --oracle " << oracle.id << " is not deterministic, so a mined line\n"
             << "       would not be reproducible even against a deterministic target\n";
        return 1;
    }
    if (oracle.spec.brain != BRAIN_SEARCH) {
        cout << "ERROR: --oracle must be a SEARCH brain. Stage 2 asks it for its next-best\n"
             << "       move by narrowing the search root, which only the search explorers honour\n";
        return 1;
    }

    string wid = wearerId.empty() ? oracle.id : wearerId;
    RankAgent wearer;
    if (!rankAgentFromId(wid, wearer, err)) { cout << "ERROR: bad --wearer id: " << err << "\n"; return 1; }
    int bookOpener = openerIndexByIdName("book");
    if (bookOpener < 0) { cout << "ERROR: no 'book' opener in the registry\n"; return 1; }
    wearer.spec.openerKind = bookOpener;
    wearer.spec.openerArg  = slot;
    wearer.spec.openerArg2 = 0;
    wearer.id = rankAgentId(wearer.spec);

    std::ostringstream bfn;
    bfn << "models/book" << slot << ".txt";
    if (!verifyOnly) {
        std::ifstream probe(bfn.str().c_str());
        if (probe.is_open() && !force) {
            cout << "ERROR: " << bfn.str() << " already exists. Books are immutable under their\n"
                 << "       slot number (the opener ID does not hash the file, so editing one\n"
                 << "       silently changes an existing agent's identity-play mapping). Pick a\n"
                 << "       free --slot, or pass --force if this slot is genuinely unused.\n";
            return 1;
        }
    }

    bool wantW = (colours != "b"), wantB = (colours != "w");
    std::vector<RefTarget> targets;
    int nActive = 0, nDet = 0, nTimedSkipped = 0;
    for (size_t i = 0; i < roster.size(); i++) {
        if (!roster[i].active) continue;
        nActive++;
        if (!rankAgentIsDeterministic(roster[i].spec)) continue;
        nDet++;
        if (!only.empty() && roster[i].id.find(only) == string::npos) continue;
        if (skipTimed && roster[i].spec.timeBudgetMs > 0.0) { nTimedSkipped++; continue; }
        for (int c = 0; c < 2; c++) {
            if (c == 0 && !wantW) continue;
            if (c == 1 && !wantB) continue;
            RefTarget t;
            t.opp = &roster[i]; t.colour = c; t.status = 0;
            t.plies = 0; t.ourMoveCount = 0; t.triedNodes = 0; t.triedMoves = 0;
            t.blockedShared = false; t.auditWon = false; t.oobFirst = -1; t.oobCount = 0;
            t.oobKey = 0; t.rejected = 0;
            t.overwritePly = -1; t.overwriteKey = 0;
            targets.push_back(t);
        }
    }
    if (targets.empty()) { cout << "ERROR: no deterministic targets selected\n"; return 1; }

    std::vector<const RankAgent*> toLoad;
    for (size_t i = 0; i < targets.size(); i++) toLoad.push_back(targets[i].opp);
    toLoad.push_back(&oracle);
    toLoad.push_back(&wearer);
    if (!loadModelSlots(toLoad, err)) { cout << "ERROR: " << err << "\n"; return 1; }

    cout << "refute: " << targets.size() << " targets (" << nActive << " active roster = "
         << nDet << " deterministic";
    if (skipTimed) cout << ", " << nTimedSkipped << " time=-budgeted skipped";
    cout << ")\n"
         << "  oracle  " << oracle.id << "\n"
         << "  wearer  " << wearer.id << "\n"
         << "  book    " << bfn.str() << ", max " << maxTries << " candidate moves per node\n" << flush;

    std::map<unsigned long long, RefMove> book;
    std::map<unsigned long long, int> owners;      // positions a WON line depends on
    long long gamesPlayed = 0;
    bool budgetHit = false;
    int stage1Won = 0;

  if (verifyOnly) {
    // Audit an already-written book instead of mining a new one. Stage 3 below
    // runs the same coverage audit and verification either way.
    std::ifstream bf(bfn.str().c_str());
    if (!bf.is_open()) { cout << "ERROR: cannot read " << bfn.str() << "\n"; mlClearSlots(); return 1; }
    string line;
    while (std::getline(bf, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ls(line);
        string hex; RefMove e;
        if (!(ls >> hex >> e.sx >> e.sy >> e.dx)) continue;
        book[strtoull(hex.c_str(), nullptr, 16)] = e;
    }
    cout << "\nverify-only: loaded " << book.size() << " entries from " << bfn.str() << "\n" << flush;
  } else {

    // ---- Stage 1: sequential mining over the shared book ----
    cout << "\nstage 1: mining " << targets.size() << " lines over one shared book\n" << flush;
    for (size_t i = 0; i < targets.size(); i++) {
        RefTarget& t = targets[i];
        srand(20260827u);
        RefGame g;
        if (!refPlayGame(book, *t.opp, oracle.spec, t.colour, board, -1, nullptr, g)) {
            cout << "ERROR: replay failed for " << t.opp->id << "\n"; mlClearSlots(); return 1;
        }
        gamesPlayed++;
        // Commit every move our side just made, so the next target inherits this
        // line's choices instead of deriving its own from a different TT history.
        for (size_t k = 0; k < g.ourKeys.size(); k++)
            if (book.find(g.ourKeys[k]) == book.end()) book[g.ourKeys[k]] = g.ourMoves[k];
        t.keys = g.ourKeys; t.moves = g.ourMoves;
        t.plies = g.plies; t.ourMoveCount = (int)g.ourMoves.size();
        if (g.outcome == 1) {
            t.status = 1;
            for (size_t k = 0; k < g.ourKeys.size(); k++) owners[g.ourKeys[k]]++;
        }
        if ((i + 1) % 20 == 0 || i + 1 == targets.size()) {
            int won = 0;
            for (size_t j = 0; j <= i; j++) if (targets[j].status == 1) won++;
            cout << "  " << (i + 1) << "/" << targets.size() << " mined, " << won
                 << " won, book " << book.size() << " entries, " << gamesPlayed << " games\n" << flush;
        }
    }

    for (size_t i = 0; i < targets.size(); i++) if (targets[i].status == 1) stage1Won++;
    cout << "stage 1 done: " << stage1Won << "/" << targets.size() << " won by the oracle line alone\n" << flush;

    // ---- Stage 2: backtracking repair for the lines that lost ----
    cout << "\nstage 2: repairing " << (targets.size() - stage1Won) << " unsolved lines\n" << flush;
    for (size_t i = 0; i < targets.size(); i++) {
        RefTarget& t = targets[i];
        if (t.status == 1) continue;
        if (maxGames > 0 && gamesPlayed >= maxGames) { budgetHit = true; break; }

        bool solved = false;
        bool sawFreeNode = false;
        // Deepest of our own moves first: a change there disturbs the least, and
        // a deep position is the most likely to be private to this one line.
        for (int k = (int)t.keys.size() - 1; k >= 0 && !solved; k--) {
            if (maxGames > 0 && gamesPlayed >= maxGames) { budgetHit = true; break; }
            std::map<unsigned long long, int>::const_iterator ow = owners.find(t.keys[k]);
            if (ow != owners.end() && ow->second > 0) continue;   // shared prefix, not ours to change
            sawFreeNode = true;
            t.triedNodes++;
            std::vector<RefMove> tried;
            tried.push_back(t.moves[k]);
            for (int attempt = 1; attempt < maxTries; attempt++) {
                if (maxGames > 0 && gamesPlayed >= maxGames) { budgetHit = true; break; }
                srand(20260827u);
                RefGame g;
                if (!refPlayGame(book, *t.opp, oracle.spec, t.colour, board, k, &tried, g)) {
                    cout << "ERROR: probe replay failed for " << t.opp->id << "\n"; mlClearSlots(); return 1;
                }
                gamesPlayed++;
                if (g.probeExhausted) break;                  // no untried legal move at this node
                if (g.probeIgnored) {
                    cout << "  WARNING: root filter ignored by " << oracle.id
                         << " -- cannot enumerate alternatives, stopping stage 2\n";
                    k = -1; break;
                }
                t.triedMoves++;
                tried.push_back(g.ourMoves[k]);
                if (g.outcome == 1) {
                    // A win is not enough to commit. The branch check above clears only
                    // the ONE position this probe changes; everything after it is a new
                    // continuation, and it can walk back through a position an already-won
                    // line depends on and want a different move there. Writing that would
                    // silently break the earlier line, which is precisely the collision
                    // stage 3 reports. Check the whole path, not just the branch point.
                    size_t badQ = 0; bool conflict = false;
                    for (size_t q = 0; q < g.ourKeys.size(); q++) {
                        std::map<unsigned long long, int>::const_iterator qo = owners.find(g.ourKeys[q]);
                        if (qo == owners.end() || qo->second <= 0) continue;
                        std::map<unsigned long long, RefMove>::const_iterator qb = book.find(g.ourKeys[q]);
                        if (qb != book.end() && !refSameMove(qb->second, g.ourMoves[q])) {
                            badQ = q; conflict = true; break;
                        }
                    }
                    if (conflict) {
                        t.rejected++;
                        cout << "    reject: won, but our ply " << badQ << " would overwrite "
                             << refHexKey(g.ourKeys[badQ]) << ", owned by "
                             << owners[g.ourKeys[badQ]] << " won line(s)\n" << flush;
                        continue;                          // fall through to the next candidate move
                    }
                    for (size_t q = 0; q < g.ourKeys.size(); q++) book[g.ourKeys[q]] = g.ourMoves[q];
                    for (size_t q = 0; q < g.ourKeys.size(); q++) owners[g.ourKeys[q]]++;
                    t.keys = g.ourKeys; t.moves = g.ourMoves;
                    t.plies = g.plies; t.ourMoveCount = (int)g.ourMoves.size();
                    t.status = 1; solved = true;
                    break;
                }
            }
        }
        if (!solved) {
            t.status = 2;
            t.blockedShared = !sawFreeNode;
        }
        int done = 0, won = 0;
        for (size_t j = 0; j <= i; j++) { if (targets[j].status != 0) done++; if (targets[j].status == 1) won++; }
        cout << "  " << (t.status == 1 ? "SOLVED  " : "CONCEDED") << "  "
             << (t.colour == 0 ? "W" : "B") << "  " << t.triedNodes << " nodes, "
             << t.triedMoves << " moves tried"
             << (t.rejected > 0 ? ", " + refPlural(t.rejected, "win") + " rejected as unmergeable" : "")
             << (t.blockedShared ? ", every node shared" : "")
             << "  " << t.opp->id << "\n" << flush;
    }
    if (budgetHit) cout << "  --max-games budget reached, remaining lines left unsolved\n";
  }

    // ---- Stage 3: prune, write, verify ----
    int minedWon = 0, collisions = 0;
    for (size_t i = 0; i < targets.size(); i++) if (targets[i].status == 1) minedWon++;

    // Keep only the entries a winning line actually walks. Stage 2 leaves the
    // discarded suffix of every failed attempt behind, and an unreachable entry
    // is one more position at which a future line can collide for no benefit.
    std::map<unsigned long long, RefMove> pruned;
    if (verifyOnly) {
        pruned = book;
    } else {
        for (size_t i = 0; i < targets.size(); i++) {
            if (targets[i].status != 1) continue;
            for (size_t k = 0; k < targets[i].keys.size(); k++) {
                std::map<unsigned long long, RefMove>::iterator ex = pruned.find(targets[i].keys[k]);
                if (ex != pruned.end() && !refSameMove(ex->second, targets[i].moves[k])) {
                    collisions++;
                    // Name it. A count alone leaves "which lines did this break?" to be
                    // guessed from ply numbers; the key can be matched exactly against
                    // the oob_key of every line that left the book.
                    cout << "  collision at " << refHexKey(targets[i].keys[k])
                         << ": " << refMoveStr(ex->second) << " overwritten with "
                         << refMoveStr(targets[i].moves[k]) << " at our ply " << k
                         << " by " << (targets[i].colour == 0 ? "W" : "B") << " "
                         << targets[i].opp->id << "\n" << flush;
                }
                pruned[targets[i].keys[k]] = targets[i].moves[k];
            }
        }
        cout << "\nstage 3: " << pruned.size() << " entries kept of " << book.size()
             << " mined (" << (book.size() - pruned.size()) << " unreachable pruned)\n" << flush;
        if (collisions > 0)
            cout << "  WARNING: " << collisions << " positions where two WINNING lines recorded\n"
                 << "  different moves. A position-keyed book holds one move per position, so the\n"
                 << "  later line's move overwrote the earlier one's and that earlier line is now\n"
                 << "  broken. This is the merge conflict the ownership check is supposed to\n"
                 << "  prevent, so its appearance means the check has a hole.\n";
    }

    ensureDir("models");
    if (!verifyOnly) {
        std::ofstream bf(bfn.str().c_str(), std::ios::trunc);
        if (!bf.is_open()) { cout << "ERROR: cannot write " << bfn.str() << "\n"; mlClearSlots(); return 1; }
        bf << "# rank.exe refute -- refutation book, one entry per position where the\n";
        bf << "# book-wearer moves. Lines carry the full continuation to the win, so the\n";
        bf << "# wearer's own brain is never consulted on a covered line.\n";
        bf << "# oracle=" << oracle.id << "\n";
        bf << "# board=" << board << "\troster=" << rosterFile << "\n";
        bf << "# targets=" << targets.size() << "\tsolved=" << minedWon << "\n";
        bf << "# wear it as: " << wearer.id << "\n";
        for (std::map<unsigned long long, RefMove>::const_iterator it = pruned.begin();
             it != pruned.end(); ++it)
            bf << std::hex << std::setw(16) << std::setfill('0') << it->first << std::dec
               << " " << it->second.sx << " " << it->second.sy << " " << it->second.dx << "\n";
    }
    if (!verifyOnly) cout << "  -> " << bfn.str() << "\n" << flush;

    // Coverage audit, run on every invocation and not only under --verify-only.
    // Mining wins are not evidence the BOOK wins: while a line is being mined our
    // side searches at every unbooked ply, and on playback it does not, which
    // changes what a `tt` opponent replies. A line that leaves the book is then
    // being carried by the wearer's brain, and the record says nothing about
    // memorization. This is the number that made a silent 238-0 into an honest one.
    cout << "\naudit: replaying " << targets.size() << " games against the written book\n" << flush;
    int aWon = 0, aLeft = 0;
    for (size_t i = 0; i < targets.size(); i++) {
        RefTarget& t = targets[i];
        srand(20260827u);
        RefGame g;
        // Only a WON line has a committed path worth comparing against. A conceded
        // target's keys and moves are its last FAILED attempt, which was never written
        // to the book, so every difference there is expected and means nothing. Passing
        // one in reports an overwrite on a line that was never in the book to begin
        // with, which it did: all 4 "overwritten" rows on the first run carrying this
        // check were conceded lines.
        bool cmpMined = !verifyOnly && t.status == 1;
        if (!refPlayGame(pruned, *t.opp, oracle.spec, t.colour, board, -1, nullptr, g,
                         cmpMined ? &t.keys : 0, cmpMined ? &t.moves : 0)) {
            cout << "ERROR: audit replay failed for " << t.opp->id << "\n"; mlClearSlots(); return 1;
        }
        t.oobFirst = g.oobFirst; t.oobCount = g.oobCount; t.oobKey = g.oobKey;
        t.overwritePly = g.overwritePly; t.overwriteKey = g.overwriteKey;
        t.auditWon = (g.outcome == 1);
        if (verifyOnly) {
            t.status = t.auditWon ? 1 : 2;
            t.plies = g.plies; t.ourMoveCount = (int)g.ourMoves.size();
        }
        if (t.auditWon) aWon++;
        if (g.oobFirst >= 0) aLeft++;
        if ((i + 1) % 40 == 0 || i + 1 == targets.size())
            cout << "  " << (i + 1) << "/" << targets.size() << " audited, " << aWon
                 << " won, " << aLeft << " left the book\n" << flush;
    }
    if (verifyOnly) { minedWon = aWon; }

    // Verification replays through playOneGame and the real openerBook, not
    // through the miner's own loop. A book that only wins inside the tool that
    // mined it has proved nothing about the agent that will wear it.
    cout << "\nverify: replaying " << targets.size() << " games with the wearer\n" << flush;
    int vWon = 0, vLost = 0, vDrew = 0, vMismatch = 0;
    std::vector<char> vres(targets.size(), '?');
    std::vector<int> vplies(targets.size(), 0);
    for (size_t i = 0; i < targets.size(); i++) {
        const RefTarget& t = targets[i];
        const RankAgent& wa = (t.colour == 0) ? wearer : *t.opp;
        const RankAgent& ba = (t.colour == 0) ? *t.opp : wearer;
        srand(20260827u);
        RankMatchRow m;
        if (!playOneGame(wa, ba, board, m)) {
            cout << "ERROR: cannot load board " << board << "\n"; mlClearSlots(); return 1;
        }
        int oc = (m.r == 'W') ? 1 : (m.r == 'B') ? 2 : 0;
        int ours = (oc == 0) ? 0 : (((oc == 1) == (t.colour == 0)) ? 1 : 2);
        vres[i] = (ours == 1) ? 'W' : (ours == 2) ? 'L' : 'D';
        vplies[i] = m.plies;
        if (ours == 1) vWon++; else if (ours == 2) vLost++; else vDrew++;
        if (t.auditWon != (ours == 1)) vMismatch++;
        if ((i + 1) % 40 == 0 || i + 1 == targets.size())
            cout << "  " << (i + 1) << "/" << targets.size() << " verified, " << vWon << " won\n" << flush;
    }

    ensureDir("ranking");
    string rep = outFile.empty() ? ("ranking/refute_book" + std::to_string(slot) + ".tsv") : outFile;
    std::ofstream out(rep.c_str(), std::ios::trunc);
    if (out.is_open()) {
        out << "# rank.exe refute -- one row per (target, colour)\n";
        out << "# oracle=" << oracle.id << "\twearer=" << wearer.id << "\tboard=" << board << "\n";
        out << "# mined: 1 = the miner found a win, 0 = conceded\n";
        out << "# verify: W/L/D for the BOOK's side, replayed through openerBook by the wearer\n";
        out << "# blocked_shared: every position on the losing line was owned by an already-won\n";
        out << "#   line, so no move could be changed without breaking one. That is the merge\n";
        out << "#   conflict a single position-keyed book cannot express.\n";
        out << "# audit: 1 = the WRITTEN book won this line replayed on its own. Distinct from\n";
        out << "#   `mined`, which only says the line was found while our side was still\n";
        out << "#   searching at unbooked plies.\n";
        out << "# oob_first: the first of OUR moves the book did not serve, so the fallback brain\n";
        out << "#   had to choose (-1 = the book covered the whole line). A line with oob_first\n";
        out << "#   >= 0 was carried by the brain, not by the book, whatever its result says.\n";
        out << "# oob_key: the position hash at oob_first, in the same 16-digit form the\n";
        out << "#   book file uses (0 = the book covered the whole line). Match it against\n";
        out << "#   the keys the stage-3 collision warning prints, to see whether a line left\n";
        out << "#   the book at a position a later winner overwrote, instead of inferring it\n";
        out << "#   from ply numbers.\n";
        out << "# ovr_ply / ovr_key: the first of OUR plies where the written book served a\n";
        out << "#   move different from the one the mine recorded at that same position, and\n";
        out << "#   the position hash there (-1 / 0 = never). A nonzero ovr_key is a measured\n";
        out << "#   overwrite of this line by a later winner, not an inference from oob_first.\n";
        out << "#   Blank under --verify-only, which has no mined path to compare against,\n";
        out << "#   and on a conceded line, whose stored path is a failed attempt the book\n";
        out << "#   never received.\n";
        out << "# rejected: stage-2 wins refused because committing the line would have\n";
        out << "#   overwritten a position an already-won line depends on.\n";
        out << "# v_plies: half-moves in the openerBook verification game. A line that matches\n";
        out << "#   `plies` played the same length game the miner recorded.\n";
        out << "colour\tmined\taudit\tverify\ttimed\tplies\tv_plies\tour_moves\toob_first\toob_count\toob_key\tovr_ply\tovr_key"
               "\ttried_nodes\ttried_moves\trejected\tblocked_shared\topponent\n";
        for (size_t i = 0; i < targets.size(); i++) {
            const RefTarget& t = targets[i];
            out << (t.colour == 0 ? "W" : "B") << "\t" << (t.status == 1 ? 1 : 0)
                << "\t" << (t.auditWon ? 1 : 0) << "\t" << vres[i]
                << "\t" << (t.opp->spec.timeBudgetMs > 0.0 ? 1 : 0) << "\t" << t.plies
                << "\t" << vplies[i] << "\t" << t.ourMoveCount
                << "\t" << t.oobFirst << "\t" << t.oobCount
                << "\t" << refHexKey(t.oobKey)
                << "\t" << t.overwritePly << "\t" << refHexKey(t.overwriteKey)
                << "\t" << t.triedNodes << "\t" << t.triedMoves
                << "\t" << t.rejected
                << "\t" << (t.blockedShared ? 1 : 0) << "\t" << t.opp->id << "\n";
        }
    }
    out.close();

    int blocked = 0, timedTargets = 0, timedWon = 0, rejectedWins = 0, rejectedLines = 0;
    for (size_t i = 0; i < targets.size(); i++) {
        if (targets[i].rejected > 0) { rejectedWins += targets[i].rejected; rejectedLines++; }
        if (targets[i].blockedShared) blocked++;
        if (targets[i].opp->spec.timeBudgetMs > 0.0) {
            timedTargets++;
            if (vres[i] == 'W') timedWon++;
        }
    }
    if (!verifyOnly)
        cout << "\nmined:  " << minedWon << "/" << targets.size() << " lines won ("
             << stage1Won << " by the oracle line alone, " << (minedWon - stage1Won) << " by repair)\n";
    cout << (verifyOnly ? "\n" : "") << "audit:  " << aWon << "/" << targets.size()
         << " lines won by the written book on its own, " << aLeft << " left the book\n";
    if (aLeft > 0)
        cout << "  Those " << aLeft << " lines were carried by the wearer's brain, not by the\n"
             << "  book, so the verified record below is NOT a memorization result for them.\n";
    if (!verifyOnly) {
        int ovr = 0, ovrLeft = 0;
        for (size_t i = 0; i < targets.size(); i++) {
            if (targets[i].overwritePly < 0) continue;
            ovr++;
            if (targets[i].oobFirst >= 0) ovrLeft++;
        }
        if (ovr > 0)
            cout << "  " << ovr << " line(s) were overwritten: the book served a move other than\n"
                 << "  the one mined for them, at a position they were replaying correctly. "
                 << ovrLeft << " of\n  those also left the book. See ovr_ply/ovr_key in the report.\n";
        else if (aLeft > 0)
            cout << "  None of them was overwritten, so leaving the book has a cause other than\n"
                 << "  one winning line clobbering another's entry.\n";
    }
    cout << "verify: " << vWon << "-" << vLost << "-" << vDrew << " (W-L-D for the book's side) over "
         << targets.size() << " games, " << pruned.size() << " book entries\n";
    if (timedTargets > 0)
        cout << "  of which wall-clock-budgeted (time=) targets: " << timedWon << "/" << timedTargets << " won\n";
    if (!verifyOnly && rejectedWins > 0)
        cout << "  " << rejectedWins << " won repair(s) across " << rejectedLines
             << " line(s) refused because committing them would have overwritten a\n"
             << "  position an already-won line depends on. Each fell through to the next\n"
             << "  candidate move, so a line still conceded is the cost of the check.\n";
    if (!verifyOnly && collisions > 0)
        cout << "  " << collisions << " position(s) where two winning lines wanted different\n"
             << "  moves. The write-side ownership check is supposed to make this zero, so a\n"
             << "  nonzero count is a live bug, and stage 3 above prints the keys.\n";
    if (blocked > 0)
        cout << "  " << blocked << " conceded with every node on the losing line owned by an\n"
             << "  already-won line: a single position-keyed book cannot express those\n";
    if (vMismatch > 0)
        cout << "  WARNING: " << vMismatch << " rows where the "
             << (verifyOnly ? "audit" : "mined") << " verdict and the verified replay\n"
             << "  disagree. The miner's loop and openerBook are not playing the same game, so\n"
             << "  neither number should be quoted until that is explained. Re-run with\n"
             << "  --verify-only to see whether the book stops covering those lines (oob_first)\n"
             << "  or whether the two loops diverge on a fully covered one.\n";
    cout << "  " << gamesPlayed << " mining games + " << targets.size() << " verification games\n";
    cout << "  -> " << rep << "\n";

    mlClearSlots();
    // Clean only when the book wins every line, on its own, through openerBook.
    return (vWon == (int)targets.size() && aLeft == 0 && vMismatch == 0) ? 0 : 2;
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
        string h = rankFileHash8(rankSlotFile(slot));
        if (!h.empty())
            cout << "model hash: " << rankSlotFile(slot) << " = " << h << " (slot " << slot << ")\n";
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
