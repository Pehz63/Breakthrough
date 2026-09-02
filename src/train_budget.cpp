#include "train_budget.h"
#include <chrono>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace {
using TBClock = std::chrono::steady_clock;

double tbNowSeconds() {
    return std::chrono::duration<double>(TBClock::now().time_since_epoch()).count();
}
}

TrainBudget tbDefaults() {
    return TrainBudget();
}

bool tbParseMarks(const std::string& csv, std::vector<double>& out, std::string& err) {
    std::vector<double> v;
    std::string cur;
    std::istringstream ss(csv);
    while (std::getline(ss, cur, ',')) {
        // Trim: a hand-written "7200, 14400" should not be a parse error.
        size_t a = cur.find_first_not_of(" \t");
        size_t b = cur.find_last_not_of(" \t");
        if (a == std::string::npos) continue;
        std::string tok = cur.substr(a, b - a + 1);
        char* end = nullptr;
        double x = std::strtod(tok.c_str(), &end);
        if (end == tok.c_str() || (end && *end != '\0')) {
            err = "not a number: '" + tok + "'";
            return false;
        }
        if (!(x > 0.0)) { err = "mark must be positive: '" + tok + "'"; return false; }
        v.push_back(x);
    }
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    out = v;
    return true;
}

void tbBegin(TrainBudget& b) {
    b.startClock = tbNowSeconds();
    b.baseNodes  = g_trainNodesTotal;
    b.units      = 0;
    b.running    = true;
    // A resumed run must not re-write rungs it already has: skip every mark
    // the prior spend already passed.
    b.nextMark = 0;
    while (b.nextMark < b.wallCkptAt.size() && b.wallCkptAt[b.nextMark] <= b.priorSec)
        b.nextMark++;
}

double tbElapsed(const TrainBudget& b) {
    if (!b.running) return b.priorSec;
    return b.priorSec + (tbNowSeconds() - b.startClock);
}

unsigned long long tbNodes(const TrainBudget& b) {
    if (!b.running) return b.priorNodes;
    return b.priorNodes + (g_trainNodesTotal - b.baseNodes);
}

long long tbUnits(const TrainBudget& b) {
    return b.priorUnits + b.units;
}

bool tbShouldStop(const TrainBudget& b) {
    if (b.wallStopSec <= 0.0) return false;
    return tbElapsed(b) >= b.wallStopSec;
}

bool tbTakeDueMark(TrainBudget& b, double& markOut) {
    if (b.nextMark >= b.wallCkptAt.size()) return false;
    if (tbElapsed(b) < b.wallCkptAt[b.nextMark]) return false;
    markOut = b.wallCkptAt[b.nextMark];
    b.nextMark++;
    return true;
}

std::string tbMarkPath(const std::string& outPath, double markSec) {
    // Integer seconds keep the name stable across seeds and regimes; the true
    // elapsed time lives in the file's own provenance line, not its name.
    long long s = (long long)llround(markSec);
    return outPath + "_t" + std::to_string(s) + ".txt";
}

std::string tbStamp(const TrainBudget& b, const char* unitName) {
    std::ostringstream o;
    o << "," << (unitName ? unitName : "units") << "=" << tbUnits(b)
      << ",secs=" << tbElapsed(b)
      << ",nodes=" << tbNodes(b);
    return o.str();
}

// Read "<key>=<number>" out of a provenance string. Matches only at a token
// boundary so "games=" never matches inside "opengames=".
static bool tbFindNumber(const std::string& s, const std::string& key, double& out) {
    std::string pat = key + "=";
    size_t from = 0;
    while (true) {
        size_t p = s.find(pat, from);
        if (p == std::string::npos) return false;
        bool atBoundary = (p == 0) || !(isalnum((unsigned char)s[p - 1]) || s[p - 1] == '_');
        if (atBoundary) {
            const char* start = s.c_str() + p + pat.size();
            char* end = nullptr;
            double v = std::strtod(start, &end);
            if (end != start) { out = v; return true; }
        }
        from = p + pat.size();
    }
}

bool tbParsePrior(const std::string& teacher, TrainBudget& b, const char* unitName) {
    bool any = false;
    double v = 0.0;
    if (tbFindNumber(teacher, unitName ? unitName : "units", v)) { b.priorUnits = (long long)v; any = true; }
    if (tbFindNumber(teacher, "secs", v))  { b.priorSec = v; any = true; }
    if (tbFindNumber(teacher, "nodes", v)) { b.priorNodes = (unsigned long long)v; any = true; }
    return any;
}
