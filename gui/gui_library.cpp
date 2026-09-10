// gui_library.cpp - see gui_library.h.
//
// Sections (grep "// ==="):
//   SMALL HELPERS
//   SOURCES        standings.tsv, CHAMPION.md, presets, favorites, history
//   NAMES          display names for ids
//   MODEL CATALOG  background scan of the model slot files
//   BOARDS + SETTINGS

#include "gui_library.h"
#include "globals.h"
#include "ranking.h"      // rankReportId, rankSlotFile, rankAgentFromId
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>

#if !defined(__EMSCRIPTEN__)
#include <thread>
#include <atomic>
#define LIB_THREADED 1
#else
#define LIB_THREADED 0
#endif

// ============================================================
// SMALL HELPERS
// ============================================================
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == sep) { out.push_back(cur); cur.clear(); }
        else cur += s[i];
    }
    out.push_back(cur);
    return out;
}

static bool fileExists(const std::string& p) {
    std::ifstream f(p.c_str());
    return f.good();
}

// The head is everything before the first top-level '.' (a '.' inside
// parentheses, e.g. in a weight list, does not count).
std::string libHeadOf(const std::string& id) {
    int depth = 0;
    for (size_t i = 0; i < id.size(); i++) {
        if (id[i] == '(') depth++;
        else if (id[i] == ')') depth--;
        else if (id[i] == '.' && depth == 0) return id.substr(0, i);
    }
    return id;
}

// ============================================================
// SOURCES
// ============================================================
static std::vector<LibEntry> s_standings, s_champions, s_presets, s_favorites;
static std::vector<std::string> s_history;
static std::string s_standingsNote;

static const char* HISTORY_FILE   = "gui_agent_history.txt";
static const char* FAVORITES_FILE = "gui_favorites.txt";
static const char* PRESETS_FILE   = "gui/presets.txt";
static const size_t HISTORY_MAX   = 30;

static void loadStandings() {
    s_standings.clear();
    s_standingsNote.clear();
    std::ifstream f("ranking/standings.tsv");
    if (!f.is_open()) {
        s_standingsNote = "ranking/standings.tsv not found (run rank.exe rate to generate it)";
        return;
    }
    std::string line;
    std::vector<std::string> cols;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> c = split(trim(line), '\t');
        if (cols.empty()) { cols = c; continue; }        // header row
        if (c.size() < cols.size()) continue;
        LibEntry e;
        for (size_t i = 0; i < cols.size(); i++) {
            const std::string& k = cols[i];
            if (k == "id") e.id = c[i];
            else if (k == "head") e.head = c[i];
            else if (k == "elo") { e.elo = std::atoi(c[i].c_str()); e.hasElo = true; }
            else if (k == "pm") e.pm = std::atoi(c[i].c_str());
            else if (k == "games") e.games = std::atoi(c[i].c_str());
            else if (k == "cpu_ms_move") e.cpuMs = std::atof(c[i].c_str());
        }
        if (e.id.empty()) continue;
        if (e.head.empty()) e.head = libHeadOf(e.id);
        s_standings.push_back(e);
    }
    s_standingsNote = std::to_string(s_standings.size()) +
        " active agents from ranking/standings.tsv. Compare Elo only within one head.";
}

// The Summary table of ranking/CHAMPION.md: one row per category, the category
// in bold in the first cell and the champion's id as the first backticked span
// in the second.
static void loadChampions() {
    s_champions.clear();
    std::ifstream f("ranking/CHAMPION.md");
    if (!f.is_open()) return;
    std::string line;
    bool inSummary = false;
    while (std::getline(f, line)) {
        if (line.compare(0, 3, "## ") == 0) {
            if (inSummary) break;
            inSummary = (line.find("Summary") != std::string::npos);
            continue;
        }
        if (!inSummary || line.empty() || line[0] != '|') continue;
        std::vector<std::string> cells = split(line, '|');
        if (cells.size() < 3) continue;
        std::string cat = cells[1];
        size_t b0 = cat.find("**"), b1 = (b0 == std::string::npos) ? b0 : cat.find("**", b0 + 2);
        if (b0 == std::string::npos || b1 == std::string::npos) continue;
        std::string id = cells[2];
        size_t t0 = id.find('`'), t1 = (t0 == std::string::npos) ? t0 : id.find('`', t0 + 1);
        if (t0 == std::string::npos || t1 == std::string::npos) continue;
        LibEntry e;
        e.label = cat.substr(b0 + 2, b1 - b0 - 2);
        e.id = id.substr(t0 + 1, t1 - t0 - 1);
        e.note = "champion, " + e.label + " (ranking/CHAMPION.md)";
        e.head = libHeadOf(e.id);
        s_champions.push_back(e);
    }
}

// gui/presets.txt: `role | name | id | description`, '#' comments.
static void loadPresets() {
    s_presets.clear();
    std::ifstream f(PRESETS_FILE);
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        std::vector<std::string> c = split(t, '|');
        if (c.size() < 3) continue;
        LibEntry e;
        e.note = trim(c[0]);                  // role
        e.label = trim(c[1]);
        e.id = trim(c[2]);
        if (c.size() > 3) e.note += ": " + trim(c[3]);
        e.head = libHeadOf(e.id);
        s_presets.push_back(e);
    }
}

static void loadFavorites() {
    s_favorites.clear();
    std::ifstream f(FAVORITES_FILE);
    std::string line;
    while (std::getline(f, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        size_t tab = t.find('\t');
        if (tab == std::string::npos) continue;
        LibEntry e;
        e.label = trim(t.substr(0, tab));
        e.id = trim(t.substr(tab + 1));
        e.head = libHeadOf(e.id);
        s_favorites.push_back(e);
    }
}

static void saveFavorites() {
    std::ofstream f(FAVORITES_FILE, std::ios::trunc);
    f << "# Saved GUI agents: <name><TAB><canonical id>, one per line.\n";
    for (size_t i = 0; i < s_favorites.size(); i++)
        f << s_favorites[i].label << "\t" << s_favorites[i].id << "\n";
}

static void loadHistory() {
    s_history.clear();
    std::ifstream f(HISTORY_FILE);
    std::string line;
    while (std::getline(f, line)) {
        std::string t = trim(line);
        if (t.empty()) continue;
        s_history.push_back(t);
        if (s_history.size() >= HISTORY_MAX) break;
    }
}

static void saveHistory() {
    std::ofstream f(HISTORY_FILE, std::ios::trunc);
    for (size_t i = 0; i < s_history.size(); i++) f << s_history[i] << "\n";
}

static void attachElo(std::vector<LibEntry>& v) {
    for (size_t i = 0; i < v.size(); i++) {
        const LibEntry* s = libStandingsFor(v[i].id);
        if (s) { v[i].hasElo = true; v[i].elo = s->elo; v[i].pm = s->pm; v[i].games = s->games; v[i].cpuMs = s->cpuMs; }
    }
}

void libLoad() {
    loadStandings();
    loadChampions();
    loadPresets();
    loadFavorites();
    loadHistory();
    attachElo(s_champions);
    attachElo(s_presets);
    attachElo(s_favorites);
}

const std::vector<LibEntry>& libStandings() { return s_standings; }
const std::vector<LibEntry>& libChampions() { return s_champions; }
const std::vector<LibEntry>& libPresets()   { return s_presets; }
const std::vector<LibEntry>& libFavorites() { return s_favorites; }
const std::vector<std::string>& libHistory() { return s_history; }
std::string libStandingsNote() { return s_standingsNote; }

std::vector<std::string> libHeads() {
    std::map<std::string, int> count;
    for (size_t i = 0; i < s_standings.size(); i++) count[s_standings[i].head]++;
    std::vector<std::pair<int, std::string> > v;
    for (std::map<std::string, int>::iterator it = count.begin(); it != count.end(); ++it)
        v.push_back(std::make_pair(-it->second, it->first));
    std::sort(v.begin(), v.end());
    std::vector<std::string> out;
    for (size_t i = 0; i < v.size(); i++) out.push_back(v[i].second);
    return out;
}

const LibEntry* libPresetByRole(const std::string& role) {
    for (size_t i = 0; i < s_presets.size(); i++) {
        const std::string& n = s_presets[i].note;
        if (n.compare(0, role.size(), role) == 0 && (n.size() == role.size() || n[role.size()] == ':'))
            return &s_presets[i];
    }
    return nullptr;
}

const LibEntry* libStandingsFor(const std::string& id) {
    for (size_t i = 0; i < s_standings.size(); i++)
        if (s_standings[i].id == id) return &s_standings[i];
    return nullptr;
}

void libAddFavorite(const std::string& name, const std::string& id) {
    for (size_t i = 0; i < s_favorites.size(); i++)
        if (s_favorites[i].id == id) { s_favorites[i].label = name; saveFavorites(); return; }
    LibEntry e;
    e.label = name;
    e.id = id;
    e.head = libHeadOf(id);
    s_favorites.push_back(e);
    attachElo(s_favorites);
    saveFavorites();
}

void libRemoveFavorite(size_t index) {
    if (index >= s_favorites.size()) return;
    s_favorites.erase(s_favorites.begin() + index);
    saveFavorites();
}

void libRemember(const std::string& id) {
    for (size_t i = 0; i < s_history.size(); i++)
        if (s_history[i] == id) { s_history.erase(s_history.begin() + i); break; }
    s_history.insert(s_history.begin(), id);
    if (s_history.size() > HISTORY_MAX) s_history.resize(HISTORY_MAX);
    saveHistory();
}

// ============================================================
// NAMES
// ============================================================
// rankReportId is the project's human-facing rendering (drops the default
// nodes=200k and the assumed tt/ord, leads learned() with its regime, drops the
// hash). Display only: it never parses back into an agent.
std::string libShortName(const std::string& id) {
    if (id.empty()) return "";
    std::string s = rankReportId(id);
    return s.empty() ? id : s;
}

// ============================================================
// MODEL CATALOG
// ============================================================
static std::vector<LibModel> s_models;
#if LIB_THREADED
static std::thread       s_scanThread;
static std::atomic<bool> s_scanDone{ false };
static std::atomic<int>  s_scanProgress{ 0 };
#else
static bool s_scanDone = false;
static int  s_scanProgress = 0;
#endif
static bool s_scanStarted = false;

static bool readModelHeader(int slot, LibModel& m) {
    m.slot = slot;
    m.file = rankSlotFile(slot);
    if (m.file.empty()) return false;
    std::ifstream f(m.file.c_str(), std::ios::binary);
    if (!f.is_open()) return false;
    f.seekg(0, std::ios::end);
    m.bytes = (long long)f.tellg();
    f.seekg(0, std::ios::beg);
    std::string line;
    int lines = 0;
    std::string firstLine;
    while (std::getline(f, line) && lines < 40) {
        lines++;
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        if (lines == 1) firstLine = line;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "teacher" && m.teacher.empty()) m.teacher = v;
        else if (k == "type" && m.type.empty()) m.type = v;
        else if ((k == "feature_count" || k == "v_feature_count") && m.features.empty()) m.features = v;
    }
    if (firstLine.find("Breakthrough ML model") == std::string::npos) return false;
    if (m.type.empty()) m.type = "linear";
    return true;
}

// A value slot's best standings row, so the catalog can show which models have
// actually been rated and how well their best agent did.
static void attachModelElo() {
    std::map<int, size_t> bySlot;
    for (size_t i = 0; i < s_models.size(); i++) bySlot[s_models[i].slot] = i;
    for (size_t i = 0; i < s_standings.size(); i++) {
        const std::string& id = s_standings[i].id;
        size_t p = id.find("(model=");
        if (p == std::string::npos) continue;
        int slot = std::atoi(id.c_str() + p + 7);
        std::map<int, size_t>::iterator it = bySlot.find(slot);
        if (it == bySlot.end()) continue;
        LibModel& m = s_models[it->second];
        if (m.bestElo < s_standings[i].elo) { m.bestElo = s_standings[i].elo; m.bestId = id; }
    }
}

static void scanModels() {
    std::vector<LibModel> out;
    for (int slot = 0; slot < 4096; slot++) {
        LibModel m;
        if (readModelHeader(slot, m)) out.push_back(m);
        s_scanProgress = slot + 1;
    }
    s_models.swap(out);
}

void libStartModelScan() {
    if (s_scanStarted) return;
    s_scanStarted = true;
#if LIB_THREADED
    s_scanThread = std::thread([] {
        scanModels();
        s_scanDone = true;
    });
    s_scanThread.detach();
#else
    scanModels();
    s_scanDone = true;
    attachModelElo();
#endif
}

bool libModelScanDone() {
    static bool attached = false;
    bool done = s_scanDone;
    if (done && !attached) { attached = true; attachModelElo(); }
    return done;
}
int  libModelScanProgress() { return s_scanProgress; }
const std::vector<LibModel>& libModels() { return s_models; }

// ============================================================
// BOARDS + SETTINGS
// ============================================================
std::vector<std::string> libBoardFiles() {
    std::vector<std::string> out;
    for (int i = 1; i <= 20; i++) {
        std::string p = "boards/board" + std::to_string(i) + ".txt";
        if (fileExists(p)) out.push_back(p);
    }
    for (int i = 1; i <= 60; i++) {
        std::string p = "boards/puzzle" + std::to_string(i) + ".txt";
        if (fileExists(p)) out.push_back(p);
    }
    return out;
}

static const char* SETTINGS_FILE = "gui_settings.txt";
static std::map<std::string, std::string> s_settings;
static bool s_settingsDirty = false;

void libLoadSettings() {
    s_settings.clear();
    std::ifstream f(SETTINGS_FILE);
    std::string line;
    while (std::getline(f, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        size_t eq = t.find('=');
        if (eq == std::string::npos) continue;
        s_settings[trim(t.substr(0, eq))] = trim(t.substr(eq + 1));
    }
    s_settingsDirty = false;
}

void libSaveSettings() {
    if (!s_settingsDirty) return;
    std::ofstream f(SETTINGS_FILE, std::ios::trunc);
    if (!f.is_open()) return;
    f << "# Breakthrough GUI settings (written by the GUI; safe to delete).\n";
    for (std::map<std::string, std::string>::iterator it = s_settings.begin(); it != s_settings.end(); ++it)
        f << it->first << "=" << it->second << "\n";
    s_settingsDirty = false;
}

std::string libSetting(const std::string& key, const std::string& def) {
    std::map<std::string, std::string>::iterator it = s_settings.find(key);
    return it == s_settings.end() ? def : it->second;
}

int libSettingInt(const std::string& key, int def) {
    std::map<std::string, std::string>::iterator it = s_settings.find(key);
    if (it == s_settings.end() || it->second.empty()) return def;
    return std::atoi(it->second.c_str());
}

void libSetSetting(const std::string& key, const std::string& value) {
    std::map<std::string, std::string>::iterator it = s_settings.find(key);
    if (it != s_settings.end() && it->second == value) return;
    s_settings[key] = value;
    s_settingsDirty = true;
}

void libSetSettingInt(const std::string& key, int value) { libSetSetting(key, std::to_string(value)); }
