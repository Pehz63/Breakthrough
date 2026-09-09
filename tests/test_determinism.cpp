#include "catch.hpp"
#include "helpers.h"
#include "globals.h"
#include "ranking.h"
#include "datastore.h"
#include "transposition.h"
#include "agents.h"
#include <string>
#include <vector>

using std::string;
using std::vector;

// ============================================================
// Does a deterministic agent reproduce its replies when the OTHER side
// stops searching?
// ============================================================
// rankSchedule's pairGameTarget caps a pair of two deterministic agents at 2
// games, floor and ceiling, on the premise that further rows would replay one
// game per colour. That premise is the claim under test here: a reply must be a
// function of the position, not of what the opponent did to reach it.
//
// The asymmetry does not arise in ordinary ranked play, where both sides search
// every ply. It arises whenever one side is NOT searching, which is every
// book-wearing agent while in book and every `.opener(...)` agent during its
// opener. The state that could carry across is the transposition table, which
// is ONE process-wide array shared by both sides with an always-replace store,
// so a searching opponent evicts this agent's own entries.
//
// Measured answer, 2026-09-09: it does not happen for a node-budgeted or
// fixed-depth agent. `setTTContext` salts the key with the root side, evaluator
// and eval params, so a foreign entry is a miss rather than a false hit, and a
// miss costs time but not correctness. The root move loop is unordered
// (orderMoves runs only at interior nodes), so table-driven ordering cannot
// reorder the root candidates a tie is broken among. A `time=` agent is a
// different matter and is deliberately NOT asserted here: its depth depends on
// the wall clock, so it is genuinely non-deterministic, which is why
// rankAgentIsDeterministic returns false for it and the 2-game cap never
// applies to it.

namespace {

struct PlyRec {
    unsigned long long key;
    char after[SIZE][SIZE];
};

// Local copy of ranking.cpp's file-static helper: 1 = White won, 2 = Black won,
// 0 = play continues.
int gameOutcome(int victor) {
    if (victor >= WhiteWin) return 1;
    if (victor <= BlackWin) return 2;
    return 0;
}

RankAgent agentOf(const string& id) {
    RankAgent a;
    string err;
    bool ok = rankAgentFromId(id, a, err);
    INFO("id: " << id << "  parse error: " << err);
    REQUIRE(ok);
    return a;
}

void snapshot(char out[SIZE][SIZE]) {
    for (int x = 0; x < SIZE; x++)
        for (int y = 0; y < SIZE; y++) out[x][y] = board[x][y];
}

// Recover the move a side just played by diffing two board snapshots. Enough
// for this test: a Breakthrough move vacates exactly one square of `who` and
// occupies exactly one other.
bool recoverMove(const char before[SIZE][SIZE], const char after[SIZE][SIZE],
                 char who, int& fx, int& fy, int& tx, int& ty) {
    fx = fy = tx = ty = -1;
    for (int x = 0; x < SIZE; x++)
        for (int y = 0; y < SIZE; y++) {
            if (before[x][y] == who && after[x][y] != who) { fx = x; fy = y; }
            if (before[x][y] != who && after[x][y] == who) { tx = x; ty = y; }
        }
    return fx >= 0 && tx >= 0;
}

// Play White vs Black the way ranking.cpp's playOneGame does: fresh table and
// fresh retain purse per game, then alternate agentChooseMove.
void playSearched(const RankAgent& wa, const RankAgent& ba, const string& boardFile,
                  vector<PlyRec>& out, int maxPlies) {
    REQUIRE(reloadBoard(boardFile));
    ttClear();
    retainResetCarry();
    out.clear();
    int victor = None;
    for (int h = 0; h < maxPlies; h++) {
        int side = (h % 2 == 0) ? White : Black;
        victor = agentChooseMove((side == White) ? wa.spec : ba.spec, side);
        PlyRec r;
        r.key = positionKey(side == White ? Black : White, false).hash;
        snapshot(r.after);
        out.push_back(r);
        if (gameOutcome(victor)) break;
    }
}

// Replay the reference game with White NOT searching for its first
// `silentUntil` plies (negative = never searches): White's recorded move is
// applied directly, which is what a side in book or in a scripted opener does.
// Returns the ply at which the position first deviated from `ref`, or -1.
int replaySilentWhite(const RankAgent& wa, const RankAgent& ba, const string& boardFile,
                      const vector<PlyRec>& ref, int silentUntil) {
    REQUIRE(reloadBoard(boardFile));
    ttClear();
    retainResetCarry();
    int victor = None;
    char before[SIZE][SIZE];
    for (int h = 0; h < (int)ref.size(); h++) {
        int side = (h % 2 == 0) ? White : Black;
        snapshot(before);
        bool silent = (side == White) && (silentUntil < 0 || h < silentUntil);
        if (silent) {
            int fx, fy, tx, ty;
            if (!recoverMove(before, ref[h].after, WHITE, fx, fy, tx, ty)) return h;
            victor = playMoveWhite(fx, fy, tx);
        } else {
            victor = agentChooseMove((side == White) ? wa.spec : ba.spec, side);
        }
        if (positionKey(side == White ? Black : White, false).hash != ref[h].key) return h;
        if (gameOutcome(victor)) break;
    }
    return -1;
}

// Replay with both sides searching exactly as in the reference, but on a table
// another game already dirtied. This is the direct test of whether table state
// can move a reply at all, independent of who searched.
int replayDirtyTT(const RankAgent& wa, const RankAgent& ba, const string& boardFile,
                  const vector<PlyRec>& ref, const string& dirtyBoard) {
    REQUIRE(reloadBoard(dirtyBoard));
    ttClear();
    retainResetCarry();
    int v = None;
    for (int h = 0; h < 200; h++) {
        int side = (h % 2 == 0) ? White : Black;
        v = agentChooseMove((side == White) ? wa.spec : ba.spec, side);
        if (gameOutcome(v)) break;
    }
    REQUIRE(reloadBoard(boardFile));
    retainResetCarry();                    // deliberately NO ttClear()
    int victor = None;
    for (int h = 0; h < (int)ref.size(); h++) {
        int side = (h % 2 == 0) ? White : Black;
        victor = agentChooseMove((side == White) ? wa.spec : ba.spec, side);
        if (positionKey(side == White ? Black : White, false).hash != ref[h].key) return h;
        if (gameOutcome(victor)) break;
    }
    return -1;
}

const char* kBoard = "boards/board1.txt";
const char* kDirty = "boards/board4.txt";

// Both sides carry tt, which is what every rostered category agent does and
// what this test needs: the table is only written by a side searching WITH tt,
// so a non-tt White would leave it untouched and the test would have no teeth.
const string kHead   = "ab(deep=6,tt,ord,nodes=200k)@3.";
const string kClassic = "classic(chip=100)@2";

} // namespace

TEST_CASE("determinism - a tt agent's reply does not depend on whether the opponent searched") {
    RankAgent wa = agentOf(kHead + kClassic);
    RankAgent ba = agentOf(kHead + kClassic);

    vector<PlyRec> ref;
    playSearched(wa, ba, kBoard, ref, 120);
    REQUIRE(ref.size() > 10);

    SECTION("the harness reproduces its own reference") {
        // Without this the other sections could pass by measuring nothing.
        vector<PlyRec> again;
        playSearched(wa, ba, kBoard, again, 120);
        REQUIRE(again.size() == ref.size());
        for (size_t i = 0; i < ref.size(); i++) {
            INFO("ply " << i << " differs between two identical runs");
            REQUIRE(again[i].key == ref[i].key);
        }
    }

    SECTION("White never searches") {
        int div = replaySilentWhite(wa, ba, kBoard, ref, -1);
        INFO("first divergent ply of " << ref.size() << " (-1 = none): " << div);
        CHECK(div == -1);
    }

    SECTION("White is silent for its first 8 plies, the .opener(rand,moves=8) case") {
        int div = replaySilentWhite(wa, ba, kBoard, ref, 8);
        INFO("first divergent ply of " << ref.size() << " (-1 = none): " << div);
        CHECK(div == -1);
    }

    SECTION("the table already holds another game's entries") {
        int div = replayDirtyTT(wa, ba, kBoard, ref, kDirty);
        INFO("first divergent ply of " << ref.size() << " (-1 = none): " << div);
        CHECK(div == -1);
    }
}

TEST_CASE("determinism - the comparison can detect a divergence") {
    // The positive control for every CHECK above. A stochastic opponent MUST
    // deviate, so if this reports -1 the comparison is broken and the passes
    // above mean nothing.
    RankAgent wa = agentOf(kHead + kClassic);
    RankAgent ba = agentOf(kHead + kClassic + ".dil(prob=20)@1");

    vector<PlyRec> ref;
    playSearched(wa, ba, kBoard, ref, 120);
    REQUIRE(ref.size() > 10);

    int div = replaySilentWhite(wa, ba, kBoard, ref, -1);
    INFO("a 20% dilution failed to change any reply over " << ref.size() << " plies");
    CHECK(div >= 0);
}
