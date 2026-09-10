// gui_library.h - the GUI's agent library: where agent ids come from.
//
// Every source here is a plain text file read from the working directory, so the
// library needs no engine state and is safe on the UI thread:
//
//   ranking/standings.tsv   the active roster's ratings (rank.exe rate writes it;
//                           gitignored, regenerate with `rank.exe rate`)
//   ranking/CHAMPION.md     the declared category champions (its Summary table)
//   gui/presets.txt         curated agents, including the web build's difficulties
//   gui_favorites.txt       named agents the user saved (local, gitignored)
//   gui_agent_history.txt   recently applied ids (local, gitignored)
//   gui_settings.txt        the GUI's own persisted settings (local, gitignored)
//
// Model files are scanned on a background thread (native) because there are
// thousands of sweep slots.
#pragma once
#include <string>
#include <vector>
#include <map>

struct LibEntry {
    std::string id;          // canonical agent id
    std::string label;       // favorite name, preset name, or champion category
    std::string note;        // free text (preset description, champion category)
    std::string head;        // the id's leading search-head segment
    bool   hasElo = false;
    int    elo = 0, pm = 0, games = 0;
    double cpuMs = 0.0;
};

struct LibModel {
    int         slot = -1;
    std::string file;
    std::string type;        // linear / dist / mlp / joint / residual ...
    std::string teacher;     // the file's teacher= line, raw
    std::string features;    // feature count, when declared
    long long   bytes = 0;
    int         bestElo = -1;      // best standings Elo of an agent using this slot
    std::string bestId;
};

// Loads (or reloads) every small source file. Cheap: call at startup and when
// the library window opens.
void libLoad();

const std::vector<LibEntry>& libStandings();     // standings.tsv order (grouped by head, Elo desc)
const std::vector<LibEntry>& libChampions();     // CHAMPION.md Summary table
const std::vector<LibEntry>& libPresets();       // gui/presets.txt
const std::vector<LibEntry>& libFavorites();
const std::vector<std::string>& libHistory();
std::vector<std::string> libHeads();             // distinct standings heads, most agents first
std::string libStandingsNote();                  // provenance line for the standings tab

// A preset by its role (easy / medium / hard / watch_white / watch_black), or null.
const LibEntry* libPresetByRole(const std::string& role);
// Standings row for an exact id, or null.
const LibEntry* libStandingsFor(const std::string& id);

void libAddFavorite(const std::string& name, const std::string& id);
void libRemoveFavorite(size_t index);
void libRemember(const std::string& id);         // push to the front of the history

// Human-readable one-line name for an id (display only, never parsed back).
std::string libShortName(const std::string& id);
std::string libHeadOf(const std::string& id);

// ---- Model catalog (background scan) ----
void libStartModelScan();
bool libModelScanDone();
int  libModelScanProgress();                     // slots probed so far
const std::vector<LibModel>& libModels();        // valid once libModelScanDone()

// ---- Board files ----
std::vector<std::string> libBoardFiles();        // boards/*.txt that exist

// ---- Settings (key=value) ----
std::string libSetting(const std::string& key, const std::string& def);
int         libSettingInt(const std::string& key, int def);
void        libSetSetting(const std::string& key, const std::string& value);
void        libSetSettingInt(const std::string& key, int value);
void        libLoadSettings();
void        libSaveSettings();                   // no-op when nothing changed
