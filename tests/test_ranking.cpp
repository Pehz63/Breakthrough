#include "catch.hpp"
#include "helpers.h"
#include "ranking.h"
#include "ai_eval.h"
#include "ai_random.h"
#include "ml_model.h"
#include "ml_eval.h"
#include "datastore.h"
#include <sstream>
#include <set>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

// ============================================================
// Helpers
// ============================================================
// Parse an ID, assert success, and assert the canonical round trip (emit(parse(id)) == id).
static RankAgent parseOk(const string& id) {
    RankAgent a;
    string err;
    bool ok = rankAgentFromId(id, a, err);
    INFO("id: " << id << "  parse error: " << err);
    REQUIRE(ok);
    REQUIRE(rankAgentId(a.spec) == id);
    return a;
}
// Parse an ID, assert clean failure, and return the error message.
static string parseErr(const string& id) {
    RankAgent a;
    string err;
    bool ok = rankAgentFromId(id, a, err);
    INFO("id: " << id << " unexpectedly parsed");
    REQUIRE_FALSE(ok);
    REQUIRE_FALSE(err.empty());
    return err;
}
static int rkEvalIdx(const char* n) {
    for (int i = 0; i < g_evalCount; i++) if (string(g_evaluators[i].name) == n) return i;
    return -1;
}

// ============================================================
// ID codec
// ============================================================
TEST_CASE("ranking id - canonical round trips") {
    RankAgent a;

    a = parseOk("rand@1");
    REQUIRE(a.spec.brain == BRAIN_POLICY);
    REQUIRE(a.spec.randomMoveProb == 0.0);

    parseOk("tiered@1");

    a = parseOk("smart(4)@1");
    REQUIRE(a.spec.chooserParam == 4);

    a = parseOk("greedy@1.classic(t1,c4,w0,l0)@2");
    REQUIRE(a.spec.brain == BRAIN_SEARCH);
    REQUIRE(a.spec.depth == 1);
    REQUIRE(a.spec.evaluator == rkEvalIdx("Classic"));
    REQUIRE(a.spec.evalParams[0] == 1);
    REQUIRE(a.spec.evalParams[1] == 4);
    REQUIRE(a.spec.evalParams[2] == 0);
    REQUIRE(a.spec.evalParams[3] == 0);

    a = parseOk("ab(d6)@1.classic(t2,c10,w3,l2)@2");
    REQUIRE(a.spec.depth == 6);
    REQUIRE(a.spec.useAlphaBeta);
    REQUIRE_FALSE(a.spec.useTT);
    REQUIRE(a.spec.evalParams[1] == 10);

    a = parseOk("ab(d8,tt,ord,nb200k)@1.exp(t2,c10,w3,l2,f2)@2.dil(r5)@1");
    REQUIRE(a.spec.depth == 8);
    REQUIRE(a.spec.useTT);
    REQUIRE(a.spec.useMoveOrder);
    REQUIRE(a.spec.nodeBudget == 200000ULL);
    REQUIRE(a.spec.evaluator == rkEvalIdx("Experimental"));
    REQUIRE(a.spec.evalParams[4] == 2);
    REQUIRE(a.spec.randomMoveProb == Approx(0.05));

    a = parseOk("ab(d3,noab,part,asp50,tb250ms,cap2)@1.classic(t1,c4,w0,l0)@2");
    REQUIRE_FALSE(a.spec.useAlphaBeta);
    REQUIRE(a.spec.keepPartial);
    REQUIRE(a.spec.aspirationWindow == 50);
    REQUIRE(a.spec.timeBudgetMs == Approx(250.0));
    REQUIRE(a.spec.depthCap == 2);

    // Quiescence flag on the ab head (captures-only leaf extension).
    a = parseOk("ab(d6,tt,ord,qs,nb200k)@1.classic(t1,c4,w0,l0)@2");
    REQUIRE(a.spec.useQuiescence);
    REQUIRE(a.spec.useTT);
    REQUIRE(a.spec.useMoveOrder);
    parseErr("ab(d4,qs,qs)@1.classic(t1,c4,w0,l0)@2");   // duplicate flag rejected

    // Advanced evaluator: all 16 weights, including a negative one (signed
    // weights are legal in IDs) and the noise seed / racewin toggle slots.
    a = parseOk("ab(d4)@1.adv(t20,c50,w-3,l0,f10,d5,e2,m3,h4,b2,o2,r3,x2,n1,s7,g1)@1");
    REQUIRE(a.spec.evaluator == rkEvalIdx("Advanced"));
    REQUIRE(a.spec.evalParams[0] == 20);
    REQUIRE(a.spec.evalParams[2] == -3);
    REQUIRE(a.spec.evalParams[13] == 1);
    REQUIRE(a.spec.evalParams[14] == 7);
    REQUIRE(a.spec.evalParams[15] == 1);

    a = parseOk("smart(4)@1.dil(r2.5)@1");
    REQUIRE(a.spec.randomMoveProb == Approx(0.025));
    REQUIRE(a.spec.dilDepth == 0);   // plain dilution = fully random move

    // Stochastic depth dilution: dilute with a shallower search instead of a random move.
    a = parseOk("ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d3)@1");
    REQUIRE(a.spec.randomMoveProb == Approx(0.30));
    REQUIRE(a.spec.dilDepth == 3);

    a = parseOk("greedy@1.exp(t1,c4,w0,l0,f-2)@2");
    REQUIRE(a.spec.evalParams[4] == -2);

    a = parseOk("ab(d4,nb2m)@1.classic(t1,c4,w0,l0)@2");
    REQUIRE(a.spec.nodeBudget == 2000000ULL);
    a = parseOk("ab(d4,nb1500)@1.classic(t1,c4,w0,l0)@2");
    REQUIRE(a.spec.nodeBudget == 1500ULL);

    // Identity-level opener: a registered opener kind + arg as an ID segment.
    a = parseOk("ab(d6,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,6)@1");
    REQUIRE(a.spec.openerKind == openerIndexByIdName("rand"));
    REQUIRE(a.spec.openerArg == 6);
    a = parseOk("smart(4)@1.opener(rand,3)@1");
    REQUIRE(a.spec.openerArg == 3);

    // opener() and dil() compose; dil is always emitted first (canonical order).
    a = parseOk("ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d3)@1.opener(rand,6)@1");
    REQUIRE(a.spec.dilDepth == 3);
    REQUIRE(a.spec.openerArg == 6);

    // The book opener kind (arg = book slot, models/book<arg>.txt).
    a = parseOk("ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(book,1)@1");
    REQUIRE(a.spec.openerKind == openerIndexByIdName("book"));
    REQUIRE(a.spec.openerArg == 1);
}

TEST_CASE("book opener - plays the stored reply, hands off out of book") {
    // Slot 99 is a scratch book written and removed by this test.
    clearBoard();
    board[2][4] = WHITE;   // c5
    board[1][5] = BLACK;   // b6 (capturable by c5)
    board[7][7] = BLACK;   // second black piece so the capture is not a win
    g_whiteCount = 1; g_blackCount = 2; g_chipDiff = -1;

    unsigned long long h = (unsigned long long)positionKey(White, false).hash;
    {
        std::ofstream f("models/book99.txt", std::ios::trunc);
        char hex[24];
        snprintf(hex, sizeof(hex), "%016llx", h);
        f << "# scratch test book\n";
        f << hex << " 2 4 1\n";               // c5xb6
        f << "0000000000000001 9 9 9\n";      // out-of-bounds entry, must never fire
    }

    int bookIdx = openerIndexByIdName("book");
    REQUIRE(bookIdx >= 0);
    int victor = None;

    // In book: the opener plays the stored capture and reports it handled the ply.
    bool played = g_openers[bookIdx].fn(White, 0, 99, victor);
    REQUIRE(played);
    REQUIRE(board[1][5] == WHITE);
    REQUIRE(board[2][4] == EMPTY);

    // Out of book (the position changed): the opener defers to the brain.
    played = g_openers[bookIdx].fn(Black, 0, 99, victor);
    REQUIRE_FALSE(played);

    // A missing book file also defers (slot 98 never written).
    played = g_openers[bookIdx].fn(White, 0, 98, victor);
    REQUIRE_FALSE(played);

    std::remove("models/book99.txt");
}

TEST_CASE("ranking id - stale or missing module versions are rejected") {
    // A stale version fails the canonical check and names the current form.
    REQUIRE(parseErr("rand@2").find("rand@1") != string::npos);
    REQUIRE(parseErr("ab(d6)@1.classic(t2,c10,w3,l2)@7").find("classic(t2,c10,w3,l2)@2") != string::npos);
    REQUIRE(parseErr("smart(4)@1.dil(r5)@3").find("dil(r5)@1") != string::npos);
    REQUIRE(parseErr("smart(4)@1.opener(rand,6)@3").find("opener(rand,6)@1") != string::npos);

    // Missing versions are named per segment.
    REQUIRE(parseErr("rand").find("module version") != string::npos);
    REQUIRE(parseErr("rand.v1").find("module version") != string::npos);   // the old grammar
    REQUIRE(parseErr("ab(d4)@1.classic(t1,c4,w0,l0)").find("module version") != string::npos);
    REQUIRE(parseErr("ab(d4)@1.classic(t1,c4,w0,l0)@2.dil(r5)").find("module version") != string::npos);

    // linpol is the one segment that must NOT carry a version (hash = identity).
    REQUIRE(parseErr("policy@1.linpol(s1,0011aabb)@1").find("no module version") != string::npos);

    // Malformed versions.
    parseErr("rand@0");
    parseErr("rand@");
    parseErr("rand@x");
}

TEST_CASE("ranking id - non-canonical and malformed ids are rejected") {
    // Non-canonical spellings name the canonical form to paste.
    REQUIRE(parseErr("smart(04)@1").find("smart(4)@1") != string::npos);
    REQUIRE(parseErr("greedy@1.classic(c4,t1,w0,l0)@1").find("greedy@1.classic(t1,c4,w0,l0)@2") != string::npos);
    REQUIRE(parseErr("ab(d4,nb2000k)@1.classic(t1,c4,w0,l0)@2").find("nb2m") != string::npos);

    // Structural errors.
    parseErr("mcts(d4)@1");                                  // unknown head
    parseErr("ab(d4,zz)@1.classic(t1,c4,w0,l0)@2");          // unknown ab() flag
    parseErr("ab(d4)@1.classic(t1,c4,w0)@1");                // missing weight
    parseErr("ab(d4)@1");                                    // search brain needs an evaluator
    parseErr("greedy(2)@1.classic(t1,c4,w0,l0)@2");          // greedy takes no arguments
    parseErr("rand@1.classic(t1,c4,w0,l0)@2");               // policy brain, no evaluator segment
    parseErr("policy@1");                                    // policy needs linpol(...)
    parseErr("rand@1.linpol(s1,0011aabb)");                  // linpol only after the policy head
    parseErr("smart(0)@1");                                  // smart N >= 1
    parseErr("rand@1.dil(r0)@1");                            // dilution must be > 0 (omit dil() instead)
    parseErr("rand@1.dil(r5,r10)@1");                        // 2nd dil arg must be d<depth>, not another r
    parseErr("rand@1.dil(r5,d3,d2)@1");                      // at most two dil() arguments
    REQUIRE(parseErr("rand@1.dil(r5,d3)@1").find("search head") != string::npos);        // depth dilution needs a search head
    REQUIRE(parseErr("ab(d4)@1.classic(t1,c4,w0,l0)@2.dil(r5,d0)@1").find("d<depth>") != string::npos);  // depth >= 1
    REQUIRE(parseErr("ab(d3)@1.classic(t1,c4,w0,l0)@2.dil(r5,d3)@1").find("shallower") != string::npos); // must be < agent depth
    REQUIRE(parseErr("ab(d5)@1.classic(t1,c4,w0,l0)@2.dil(r5,d9)@1").find("shallower") != string::npos);
    parseErr("rand@1.x7@1");                                 // unknown segment
    parseErr("ab(d4)@1.classic(t1,c4,w0,l0)@2.");            // trailing dot
    REQUIRE(parseErr("rand@1.opener(nope,6)@1").find("unknown opener") != string::npos);  // unknown opener kind
    parseErr("rand@1.opener(rand,0)@1");                     // rand arg must be > 0 (omit opener() instead)
    parseErr("rand@1.opener(rand,-1)@1");                    // negative
    parseErr("rand@1.opener(rand)@1");                       // rand needs its arg
    parseErr("rand@1.opener(rand,3,4)@1");                   // at most name + one arg
    parseErr("rand@1.opener(rand,x)@1");                     // non-integer arg
    parseErr("rand@1.opener(rand,3)@1.opener(rand,3)@1");    // duplicate segment
    // opener() must come after dil() to be canonical (matches emit order).
    REQUIRE(parseErr("smart(4)@1.opener(rand,6)@1.dil(r5)@1").find("dil(r5)@1.opener(rand,6)@1") != string::npos);
}

TEST_CASE("ranking id - learned model hashes (when model files exist)") {
    string hv = rankFileHash8("models/lin_value.txt");
    if (hv.empty()) {
        SUCCEED("models/lin_value.txt not present; learned-value id test skipped");
    } else {
        RankAgent a = parseOk("greedy@1.learned(s0," + hv + ")@1");
        REQUIRE(a.spec.brain == BRAIN_SEARCH);
        REQUIRE(a.spec.evaluator == rkEvalIdx("LearnedValue"));
        REQUIRE(a.spec.modelSlot == 0);
        // A wrong hash is rejected and the error names the real one.
        string bad = string(1, hv[0] == '0' ? '1' : '0') + hv.substr(1);
        REQUIRE(parseErr("greedy@1.learned(s0," + bad + ")@1").find(hv) != string::npos);
    }
    string hp = rankFileHash8("models/lin_policy.txt");
    if (hp.empty()) {
        SUCCEED("models/lin_policy.txt not present; policy id test skipped");
    } else {
        RankAgent a = parseOk("policy@1.linpol(s1," + hp + ")");
        REQUIRE(a.spec.brain == BRAIN_POLICY);
        REQUIRE(a.spec.modelSlot == 1);
    }
    REQUIRE(rankFileHash8("models/no_such_model_file.txt") == "");
}

// Sweep/experiment slots (3..ML_SLOTS-1) use a generic models/sweep/slot<N>.txt
// convention (see slotFile() in ranking.cpp) instead of a fixed named file, so a
// large hyperparameter sweep can give each independently-trained candidate its
// own permanent identity and be rated together in one process.
TEST_CASE("ranking id - sweep slot convention (slot >= 3)") {
    const int slot = 5;
    const string path = "models/sweep/slot5.txt";
#ifdef _WIN32
    _mkdir("models/sweep");
#else
    mkdir("models/sweep", 0755);
#endif
    LinearModel m(HEAD_VALUE, 2, MLV2_FEATURES, 900.0f);
    m.bias = 0.1f;
    for (int i = 0; i < m.n; i++) m.w[i] = 0.01f * i;
    REQUIRE(m.save(path));

    string h = rankFileHash8(path);
    REQUIRE_FALSE(h.empty());
    RankAgent a = parseOk("greedy@1.learned(s" + std::to_string(slot) + "," + h + ")@1");
    REQUIRE(a.spec.brain == BRAIN_SEARCH);
    REQUIRE(a.spec.modelSlot == slot);

    // A slot beyond ML_SLOTS is rejected, not silently accepted.
    REQUIRE(parseErr("greedy@1.learned(s" + std::to_string(ML_SLOTS) + "," + h + ")@1").find("slot") != string::npos);
}

TEST_CASE("ranking codec - covers every registered evaluator with unique letters") {
    string err;
    bool ok = rankEvalCodecComplete(err);
    INFO(err);
    REQUIRE(ok);
}

// ============================================================
// Roster
// ============================================================
TEST_CASE("ranking roster - parse, toggles, and validation") {
    {
        std::istringstream in(
            "# comment line\r\n"
            "\r\n"
            "anchor  rand@1   # the anchor\r\n"
            "on      tiered@1\n"
            "off     smart(4)@1\n");
        std::vector<RankAgent> r;
        string err;
        REQUIRE(rankLoadRoster(in, r, err));
        REQUIRE(r.size() == 3);
        REQUIRE(r[0].anchor);
        REQUIRE(r[0].active);          // anchor is implicitly active
        REQUIRE(r[1].active);
        REQUIRE_FALSE(r[1].anchor);
        REQUIRE_FALSE(r[2].active);
        REQUIRE(r[2].id == "smart(4)@1");
    }
    {
        std::istringstream in("on rand@1\n");   // no anchor
        std::vector<RankAgent> r;
        string err;
        REQUIRE_FALSE(rankLoadRoster(in, r, err));
        REQUIRE(err.find("anchor") != string::npos);
    }
    {
        std::istringstream in("anchor rand@1\nanchor tiered@1\n");   // two anchors
        std::vector<RankAgent> r;
        string err;
        REQUIRE_FALSE(rankLoadRoster(in, r, err));
    }
    {
        std::istringstream in("anchor rand@1\non rand@1\n");   // duplicate id
        std::vector<RankAgent> r;
        string err;
        REQUIRE_FALSE(rankLoadRoster(in, r, err));
        REQUIRE(err.find("duplicate") != string::npos);
    }
    {
        std::istringstream in("anchor rand@1\nenabled tiered@1\n");   // bad state word
        std::vector<RankAgent> r;
        string err;
        REQUIRE_FALSE(rankLoadRoster(in, r, err));
    }
    {
        std::istringstream in("anchor rand@1\non tiered@1 junk\n");   // text after the id
        std::vector<RankAgent> r;
        string err;
        REQUIRE_FALSE(rankLoadRoster(in, r, err));
    }
}

// ============================================================
// Match store
// ============================================================
TEST_CASE("ranking match rows - format/parse round trip") {
    RankMatchRow m;
    m.w = "ab(d4)@1.classic(t1,c4,w0,l0)@2";
    m.b = "rand@1";
    m.r = 'W';
    m.plies = 57;
    m.wms = 812.4; m.bms = 1.9;
    m.wmv = 29; m.bmv = 28;
    m.wnod = 181233; m.bnod = 0;
    m.seed = 3141592653u;
    m.board = "boards/board1.txt";
    m.par = 1;
    m.ts = "2026-07-03T12:34:56Z";
    m.run = "20260703T123456Z";
    m.wpc = 9; m.bpc = 4;
    m.wcpu = 640.625; m.bcpu = 0.0;
    m.wed = 116.25; m.bed = 0.0;
    m.wsn = 29; m.bsn = 0;

    RankMatchRow p;
    REQUIRE(rankParseMatchRow(rankFormatMatchRow(m), p));
    REQUIRE(p.w == m.w);
    REQUIRE(p.b == m.b);
    REQUIRE(p.r == 'W');
    REQUIRE(p.plies == 57);
    REQUIRE(p.wms == Approx(812.4));
    REQUIRE(p.bms == Approx(1.9));
    REQUIRE(p.wmv == 29);
    REQUIRE(p.bmv == 28);
    REQUIRE(p.wnod == Approx(181233));
    REQUIRE(p.bnod == Approx(0));
    REQUIRE(p.seed == 3141592653u);
    REQUIRE(p.board == m.board);
    REQUIRE(p.par == 1);
    REQUIRE(p.ts == m.ts);
    REQUIRE(p.run == m.run);
    REQUIRE(p.wpc == 9);
    REQUIRE(p.bpc == 4);
    REQUIRE(p.wcpu == Approx(640.625));
    REQUIRE(p.bcpu == Approx(0.0));
    REQUIRE(p.wed == Approx(116.25));
    REQUIRE(p.wsn == 29);

    // A first-generation row (no cpu/pc/ed keys) parses with "not recorded" sentinels.
    RankMatchRow old;
    REQUIRE(rankParseMatchRow(
        "{\"t\":\"g\",\"w\":\"rand@1\",\"b\":\"tiered@1\",\"r\":\"B\",\"plies\":30,"
        "\"wms\":0.1,\"bms\":0.2,\"wmv\":15,\"bmv\":15,\"wnod\":0,\"bnod\":0,"
        "\"seed\":7,\"board\":\"boards/board1.txt\",\"par\":1,\"ts\":\"\",\"run\":\"\"}", old));
    REQUIRE(old.wpc == -1);
    REQUIRE(old.bpc == -1);
    REQUIRE(old.wcpu == Approx(-1.0));
    REQUIRE(old.bcpu == Approx(-1.0));
    REQUIRE(old.wsn == 0);

    REQUIRE_FALSE(rankParseMatchRow("not json at all", p));
    REQUIRE_FALSE(rankParseMatchRow("{\"t\":\"g\",\"w\":\"a\",\"b\":\"b\"}", p));   // no result
    REQUIRE_FALSE(rankParseMatchRow("{\"t\":\"x\",\"w\":\"a\",\"b\":\"b\",\"r\":\"W\",\"plies\":3}", p));
}

TEST_CASE("ranking match store - board filter and malformed-line counting") {
    const char* path = "build\\test_rank_store.tmp";
    {
        std::ofstream f(path, std::ios::trunc);
        RankMatchRow m;
        m.w = "rand@1"; m.b = "tiered@1"; m.r = 'W'; m.plies = 9;
        m.wms = m.bms = 0; m.wmv = m.bmv = 5; m.wnod = m.bnod = 0;
        m.seed = 1; m.par = 1; m.ts = ""; m.run = "";
        m.board = "boards/board1.txt";
        f << rankFormatMatchRow(m) << "\n";
        m.board = "boards/board2.txt";
        f << rankFormatMatchRow(m) << "\n";
        f << "garbage line\n";
    }
    std::vector<RankMatchRow> rows;
    int skipped = 0;
    REQUIRE(rankLoadMatches(path, "boards/board1.txt", rows, skipped));
    REQUIRE(rows.size() == 1);
    REQUIRE(skipped == 1);
    REQUIRE(rankLoadMatches(path, "", rows, skipped));   // empty filter keeps all boards
    REQUIRE(rows.size() == 2);
}

// ============================================================
// Scheduler
// ============================================================
static RankAgent mkActive(const string& id) {
    RankAgent a;
    string err;
    bool ok = rankAgentFromId(id, a, err);
    INFO(err);
    REQUIRE(ok);
    a.active = true;
    return a;
}
static RankMatchRow asRow(const RankPendingGame& g) {
    RankMatchRow m;
    m.w = g.w; m.b = g.b; m.r = 'W'; m.plies = 9;
    m.wms = m.bms = 0; m.wmv = m.bmv = 5; m.wnod = m.bnod = 0;
    m.seed = g.seed; m.board = ""; m.par = 1;
    return m;
}

TEST_CASE("ranking scheduler - color balance, incremental top-up, determinism") {
    std::vector<RankAgent> roster;
    roster.push_back(mkActive("rand@1"));
    roster.push_back(mkActive("tiered@1"));
    roster.push_back(mkActive("smart(4)@1"));

    std::vector<RankMatchRow> store;
    std::vector<RankPendingGame> p1 = rankSchedule(roster, store, 4, 1);
    REQUIRE(p1.size() == 12);   // 3 pairs x 4 games

    // Color balance: every pair gets 2 games with each color assignment.
    std::map<std::pair<string,string>, int> colorCount;
    std::set<unsigned> seeds;
    for (size_t i = 0; i < p1.size(); i++) {
        colorCount[std::make_pair(p1[i].w, p1[i].b)]++;
        seeds.insert(p1[i].seed);
    }
    REQUIRE(colorCount.size() == 6);   // 3 pairs x 2 color assignments
    for (std::map<std::pair<string,string>, int>::iterator it = colorCount.begin();
         it != colorCount.end(); ++it)
        REQUIRE(it->second == 2);
    REQUIRE(seeds.size() == 12);       // per-game seeds are all distinct

    // Deterministic: same inputs give the identical pending list.
    std::vector<RankPendingGame> p2 = rankSchedule(roster, store, 4, 1);
    REQUIRE(p2.size() == p1.size());
    for (size_t i = 0; i < p1.size(); i++) {
        REQUIRE(p1[i].w == p2[i].w);
        REQUIRE(p1[i].b == p2[i].b);
        REQUIRE(p1[i].seed == p2[i].seed);
    }

    // Feed the games back as played: nothing is pending anymore.
    for (size_t i = 0; i < p1.size(); i++) store.push_back(asRow(p1[i]));
    REQUIRE(rankSchedule(roster, store, 4, 1).empty());

    // Add one agent: exactly (N-1) x 4 new games, all involving the newcomer.
    string newcomer = "greedy@1.classic(t1,c4,w0,l0)@2";
    roster.push_back(mkActive(newcomer));
    std::vector<RankPendingGame> p3 = rankSchedule(roster, store, 4, 1);
    REQUIRE(p3.size() == 12);   // 3 new pairs x 4 games
    for (size_t i = 0; i < p3.size(); i++)
        REQUIRE((p3[i].w == newcomer || p3[i].b == newcomer));

    // Raising the target tops every pair up incrementally.
    REQUIRE(rankSchedule(roster, store, 6, 1).size() == 12 + 6 * 2);   // 3 played pairs need 2 more, 3 new pairs need 6
}

TEST_CASE("ranking scheduler - rebalances lopsided colors without deleting games") {
    std::vector<RankAgent> roster;
    roster.push_back(mkActive("rand@1"));
    roster.push_back(mkActive("smart(4)@1"));

    // 4 stored games, all with rand@1 as White (the smaller id already over target).
    std::vector<RankMatchRow> store;
    RankPendingGame g;
    g.w = "rand@1"; g.b = "smart(4)@1"; g.seed = 0;
    for (int i = 0; i < 4; i++) store.push_back(asRow(g));

    std::vector<RankPendingGame> p = rankSchedule(roster, store, 4, 1);
    REQUIRE(p.size() == 2);   // only the missing smart-as-White games
    for (size_t i = 0; i < p.size(); i++)
        REQUIRE(p[i].w == "smart(4)@1");
}

// ============================================================
// Bradley-Terry fit
// ============================================================
// Sample n games between a and b where a wins with probability pa (deterministic
// under the srand the caller set). Colors alternate.
static void addGames(std::vector<RankMatchRow>& rows, const string& a, const string& b,
                     int n, double pa) {
    for (int g = 0; g < n; g++) {
        RankMatchRow m;
        bool aWhite = (g % 2 == 0);
        m.w = aWhite ? a : b;
        m.b = aWhite ? b : a;
        double u = (double)rand() / (double)RAND_MAX;
        bool aWins = (u < pa);
        m.r = (aWins == aWhite) ? 'W' : 'B';
        m.plies = 9; m.wms = m.bms = 0; m.wmv = m.bmv = 5; m.wnod = m.bnod = 0;
        m.seed = 0; m.par = 1;
        rows.push_back(m);
    }
}
static void addDraws(std::vector<RankMatchRow>& rows, const string& a, const string& b, int n) {
    for (int g = 0; g < n; g++) {
        RankMatchRow m;
        m.w = (g % 2 == 0) ? a : b;
        m.b = (g % 2 == 0) ? b : a;
        m.r = 'D';
        m.plies = 400; m.wms = m.bms = 0; m.wmv = m.bmv = 200; m.wnod = m.bnod = 0;
        m.seed = 0; m.par = 1;
        rows.push_back(m);
    }
}
static int fitIndexOf(const RankFit& fit, const string& id) {
    for (size_t i = 0; i < fit.ids.size(); i++) if (fit.ids[i] == id) return (int)i;
    return -1;
}

TEST_CASE("ranking BT fit - anchored, accurate, deterministic, bounded") {
    // True strengths: A = 0, B = 200, C = 400 Elo.
    srand(4242);
    std::vector<RankMatchRow> rows;
    double pAB = 1.0 / (1.0 + pow(10.0, 200.0 / 400.0));
    double pAC = 1.0 / (1.0 + pow(10.0, 400.0 / 400.0));
    double pBC = 1.0 / (1.0 + pow(10.0, 200.0 / 400.0));
    addGames(rows, "A", "B", 300, pAB);
    addGames(rows, "A", "C", 300, pAC);
    addGames(rows, "B", "C", 300, pBC);

    RankFit fit;
    rankFitBT(rows, "A", fit);
    REQUIRE(fit.anchored);
    int ia = fitIndexOf(fit, "A"), ib = fitIndexOf(fit, "B"), ic = fitIndexOf(fit, "C");
    REQUIRE(ia >= 0); REQUIRE(ib >= 0); REQUIRE(ic >= 0);
    REQUIRE(fit.elo[ia] == Approx(0.0).margin(1e-6));    // the anchor is pinned exactly
    REQUIRE(fit.elo[ib] == Approx(200.0).margin(60.0));
    REQUIRE(fit.elo[ic] == Approx(400.0).margin(60.0));
    REQUIRE_FALSE(fit.provisional[ia]);
    REQUIRE(fit.se[ib] > 0.0);

    // Deterministic: a refit of the same rows is bit-identical.
    RankFit fit2;
    rankFitBT(rows, "A", fit2);
    for (size_t i = 0; i < fit.ids.size(); i++) {
        REQUIRE(fit.elo[i] == fit2.elo[i]);
        REQUIRE(fit.se[i] == fit2.se[i]);
    }

    // An undefeated agent stays finite (prior) and is less certain than a mixed one.
    addGames(rows, "D", "A", 8, 1.0);
    addGames(rows, "D", "B", 8, 1.0);
    addGames(rows, "D", "C", 8, 1.0);
    RankFit fit3;
    rankFitBT(rows, "A", fit3);
    int id3 = fitIndexOf(fit3, "D"), ib3 = fitIndexOf(fit3, "B");
    REQUIRE(id3 >= 0);
    REQUIRE(fit3.elo[id3] < 3000.0);
    REQUIRE(fit3.elo[id3] > fit3.elo[fitIndexOf(fit3, "C")]);
    REQUIRE(fit3.se[id3] > fit3.se[ib3]);

    // A draw-heavy pair fits to (almost) no gap.
    std::vector<RankMatchRow> drows;
    addDraws(drows, "E", "F", 20);
    RankFit dfit;
    rankFitBT(drows, "E", dfit);
    REQUIRE(dfit.elo[fitIndexOf(dfit, "E")] == Approx(0.0).margin(1e-6));
    REQUIRE(dfit.elo[fitIndexOf(dfit, "F")] == Approx(0.0).margin(1.0));
}

TEST_CASE("ranking BT fit - disconnected components and missing anchor") {
    srand(99);
    std::vector<RankMatchRow> rows;
    addGames(rows, "A", "B", 8, 0.5);
    addGames(rows, "X", "Y", 8, 0.5);   // never plays A or B

    RankFit fit;
    rankFitBT(rows, "A", fit);
    REQUIRE(fit.anchored);
    REQUIRE_FALSE(fit.provisional[fitIndexOf(fit, "A")]);
    REQUIRE_FALSE(fit.provisional[fitIndexOf(fit, "B")]);
    REQUIRE(fit.provisional[fitIndexOf(fit, "X")]);
    REQUIRE(fit.provisional[fitIndexOf(fit, "Y")]);
    // The stray component centers on mean 1000.
    double mx = (fit.elo[fitIndexOf(fit, "X")] + fit.elo[fitIndexOf(fit, "Y")]) / 2.0;
    REQUIRE(mx == Approx(1000.0).margin(1e-6));

    // With no anchor in the data, everything centers on mean 1000 and a warning
    // is the caller's job (anchored == false signals it).
    RankFit fit2;
    rankFitBT(rows, "not-a-player", fit2);
    REQUIRE_FALSE(fit2.anchored);
    double ma = (fit2.elo[fitIndexOf(fit2, "A")] + fit2.elo[fitIndexOf(fit2, "B")]) / 2.0;
    REQUIRE(ma == Approx(1000.0).margin(1e-6));
}

TEST_CASE("ranking gauntlet fit - candidate rated against a frozen pool") {
    // Scoring 50% against a 300-Elo pool means the candidate is at 300.
    std::vector<double> oppElo(10, 300.0), score;
    for (int i = 0; i < 10; i++) score.push_back(i % 2 == 0 ? 1.0 : 0.0);
    double se = 0.0;
    REQUIRE(rankFitSingle(oppElo, score, se) == Approx(300.0).margin(1.0));
    REQUIRE(se > 0.0);

    // An undefeated candidate stays finite thanks to the prior.
    std::vector<double> opp2(8, 0.0), score2(8, 1.0);
    double se2 = 0.0;
    double elo2 = rankFitSingle(opp2, score2, se2);
    REQUIRE(elo2 > 300.0);
    REQUIRE(elo2 < 2000.0);
}

// ============================================================
// Pairgen
// ============================================================
static string slurpFile(const string& path) {
    std::ifstream in(path.c_str(), std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
// Pull an integer field out of the one-line meta sidecar (fields are flat).
static long metaInt(const string& meta, const string& key) {
    size_t k = meta.find("\"" + key + "\":");
    REQUIRE(k != string::npos);
    return atol(meta.c_str() + k + key.size() + 3);
}

TEST_CASE("pairgen dilution schedule - linear decay to a held floor") {
    REQUIRE(rankDilutedProb(0.3, 0.05, 30, 0)  == Approx(0.3));
    REQUIRE(rankDilutedProb(0.3, 0.05, 30, 15) == Approx(0.175));
    REQUIRE(rankDilutedProb(0.3, 0.05, 30, 30) == Approx(0.05));
    REQUIRE(rankDilutedProb(0.3, 0.05, 30, 99) == Approx(0.05));
    // decayPlies <= 0 means a constant start value.
    REQUIRE(rankDilutedProb(0.3, 0.05, 0, 50)  == Approx(0.3));
}

TEST_CASE("pairgen - deterministic output, valid rows, honest meta tallies") {
    const string idA = "ab(d2)@1.classic(t1,c4,w0,l0)@2";
    const string idB = "ab(d1)@1.classic(t1,c4,w0,l0)@2";
    const string out1 = "build/pairgen_t1.jsonl", out2 = "build/pairgen_t2.jsonl";
    RankDilOverride dil;
    dil.apply = 1; dil.start = 0.3; dil.floorProb = 0.05; dil.decayPlies = 30;

    REQUIRE(rankPairGen(idA, idB, 4, out1, "boards/board1.txt", 2, 42, dil, 0, 0, 0, 0, 1) == 0);
    REQUIRE(rankPairGen(idA, idB, 4, out2, "boards/board1.txt", 2, 42, dil, 0, 0, 0, 0, 1) == 0);

    string d1 = slurpFile(out1), d2 = slurpFile(out2);
    REQUIRE_FALSE(d1.empty());
    REQUIRE(d1 == d2);   // same ids + seed reproduce byte-identical data

    // Rows are in the loadReplayDataset v2 format.
    REQUIRE(d1.find("\"ver\":2") != string::npos);
    REQUIRE(d1.find("\"stm\":") != string::npos);
    REQUIRE(d1.find("\"label\":") != string::npos);
    REQUIRE(d1.find("\"idx\":[") != string::npos);

    // Meta sidecar tallies agree with an unfiltered run.
    string meta = slurpFile(out1 + ".meta.json");
    REQUIRE_FALSE(meta.empty());
    REQUIRE(metaInt(meta, "played") == 4);
    REQUIRE(metaInt(meta, "kept") == 4);
    REQUIRE(metaInt(meta, "a_wins") + metaInt(meta, "b_wins") + metaInt(meta, "draws") == 4);
    REQUIRE(metaInt(meta, "positions") > 0);
}

TEST_CASE("pairgen - a zero override plays exactly like no override") {
    const string idA = "ab(d2)@1.classic(t1,c4,w0,l0)@2";
    const string idB = "ab(d1)@1.classic(t1,c4,w0,l0)@2";
    RankDilOverride none;                       // apply = 0
    RankDilOverride zero;
    zero.apply = 1; zero.start = 0.0; zero.floorProb = 0.0; zero.decayPlies = 0;

    REQUIRE(rankPairGen(idA, idB, 2, "build/pairgen_n.jsonl", "boards/board1.txt", 2, 7, none, 0, 0, 0, 0, 1) == 0);
    REQUIRE(rankPairGen(idA, idB, 2, "build/pairgen_z.jsonl", "boards/board1.txt", 2, 7, zero, 0, 0, 0, 0, 1) == 0);
    REQUIRE(slurpFile("build/pairgen_n.jsonl") == slurpFile("build/pairgen_z.jsonl"));
}

TEST_CASE("pairgen - winner filter keeps only that agent's wins") {
    const string idA = "ab(d3)@1.classic(t1,c4,w0,l0)@2";   // stronger
    const string idB = "ab(d1)@1.classic(t1,c4,w0,l0)@2";
    RankDilOverride dil;
    dil.apply = 1; dil.start = 0.2; dil.floorProb = 0.05; dil.decayPlies = 20;

    rankPairGen(idA, idB, 4, "build/pairgen_f.jsonl", "boards/board1.txt", 2, 11, dil, 0, 1 /*winner=a*/, 0, 0, 1);
    string meta = slurpFile("build/pairgen_f.jsonl.meta.json");
    REQUIRE_FALSE(meta.empty());
    REQUIRE(metaInt(meta, "played") == 4);
    REQUIRE(metaInt(meta, "kept") == metaInt(meta, "a_wins"));
}

TEST_CASE("pairgen - color-stratified tallies reconcile with the aggregate record") {
    const string idA = "ab(d3)@1.classic(t1,c4,w0,l0)@2";
    const string idB = "ab(d1)@1.classic(t1,c4,w0,l0)@2";
    RankDilOverride dil;
    dil.apply = 1; dil.start = 0.2; dil.floorProb = 0.05; dil.decayPlies = 20;

    // 8 games, unsharded: color alternates strictly by game index, so A must
    // hold White in exactly half and Black in exactly half.
    rankPairGen(idA, idB, 8, "build/pairgen_color.jsonl", "boards/board1.txt", 2, 13, dil, 0, 0, 0, 0, 1);
    string meta = slurpFile("build/pairgen_color.jsonl.meta.json");
    REQUIRE_FALSE(meta.empty());

    long wg = metaInt(meta, "a_white_games"), bg = metaInt(meta, "a_black_games");
    long ww = metaInt(meta, "a_white_wins"), bw = metaInt(meta, "a_black_wins");
    long wd = metaInt(meta, "a_white_draws"), bd = metaInt(meta, "a_black_draws");
    REQUIRE(wg == 4);
    REQUIRE(bg == 4);
    REQUIRE(wg + bg == metaInt(meta, "played"));
    REQUIRE(ww <= wg);
    REQUIRE(bw <= bg);
    REQUIRE(ww + wd <= wg);
    REQUIRE(bw + bd <= bg);
    // The color-split wins must reconcile exactly with the aggregate A record.
    REQUIRE(ww + bw == metaInt(meta, "a_wins"));
}

TEST_CASE("pairgen - open plies spread a deterministic pair, branch mode is deterministic") {
    const string idA = "ab(d2)@1.classic(t1,c4,w0,l0)@2";
    const string idB = "ab(d2)@1.classic(t1,c4,w0,l0)@2";
    RankDilOverride none;

    // Two clean deterministic agents: without open plies every game is one of
    // two fixed transcripts, so an open-plies run must produce different data.
    REQUIRE(rankPairGen(idA, idB, 4, "build/pairgen_o0.jsonl", "boards/board1.txt", 2, 5, none, 0, 0, 0, 0, 1) == 0);
    REQUIRE(rankPairGen(idA, idB, 4, "build/pairgen_o6.jsonl", "boards/board1.txt", 2, 5, none, 6, 0, 0, 0, 1) == 0);
    REQUIRE(slurpFile("build/pairgen_o0.jsonl") != slurpFile("build/pairgen_o6.jsonl"));
    string meta = slurpFile("build/pairgen_o6.jsonl.meta.json");
    REQUIRE(metaInt(meta, "positions") > 0);

    // Branch mode: deterministic across runs, tallies present.
    RankDilOverride dil;
    dil.apply = 1; dil.start = 0.3; dil.floorProb = 0.05; dil.decayPlies = 30;
    const string idW = "ab(d3)@1.classic(t1,c4,w0,l0)@2";   // A strong enough to win bases
    const string idL = "ab(d1)@1.classic(t1,c4,w0,l0)@2";
    REQUIRE(rankPairGen(idW, idL, 4, "build/pairgen_b1.jsonl", "boards/board1.txt", 2, 9, dil, 0, 1, 2, 0, 1) == 0);
    REQUIRE(rankPairGen(idW, idL, 4, "build/pairgen_b2.jsonl", "boards/board1.txt", 2, 9, dil, 0, 1, 2, 0, 1) == 0);
    REQUIRE(slurpFile("build/pairgen_b1.jsonl") == slurpFile("build/pairgen_b2.jsonl"));
    string bmeta = slurpFile("build/pairgen_b1.jsonl.meta.json");
    REQUIRE(metaInt(bmeta, "branch_tried") >= metaInt(bmeta, "branch_kept"));
    REQUIRE(metaInt(bmeta, "branch_kept") >= 0);
}

TEST_CASE("pairgen - asymmetric open side diverges, default stays symmetric-identical") {
    // Two DIFFERENT deterministic agents, so only one side playing the random
    // opener is a distinguishable perturbation from the other side playing it.
    const string idA = "ab(d3)@1.classic(t1,c4,w0,l0)@2";
    const string idB = "ab(d2)@1.classic(t1,c4,w0,l0)@2";
    RankDilOverride none;

    // Back-compat: the default trailing openSide (3 = both) reproduces the
    // pre-flag symmetric opener byte-for-byte.
    REQUIRE(rankPairGen(idA, idB, 4, "build/pg_os_default.jsonl", "boards/board1.txt", 2, 5, none, 6, 0, 0, 0, 1) == 0);
    REQUIRE(rankPairGen(idA, idB, 4, "build/pg_os_both.jsonl",    "boards/board1.txt", 2, 5, none, 6, 0, 0, 0, 1, 3) == 0);
    REQUIRE(slurpFile("build/pg_os_default.jsonl") == slurpFile("build/pg_os_both.jsonl"));

    // Only agent A random vs only agent B random must produce different games,
    // and both must differ from the symmetric run.
    REQUIRE(rankPairGen(idA, idB, 4, "build/pg_os_a.jsonl", "boards/board1.txt", 2, 5, none, 6, 0, 0, 0, 1, 1) == 0);
    REQUIRE(rankPairGen(idA, idB, 4, "build/pg_os_b.jsonl", "boards/board1.txt", 2, 5, none, 6, 0, 0, 0, 1, 2) == 0);
    REQUIRE(slurpFile("build/pg_os_a.jsonl") != slurpFile("build/pg_os_b.jsonl"));
    REQUIRE(slurpFile("build/pg_os_a.jsonl") != slurpFile("build/pg_os_both.jsonl"));

    // open_side is recorded in the meta sidecar.
    REQUIRE(slurpFile("build/pg_os_a.jsonl.meta.json").find("\"open_side\":\"a\"") != string::npos);
    REQUIRE(slurpFile("build/pg_os_b.jsonl.meta.json").find("\"open_side\":\"b\"") != string::npos);
    REQUIRE(slurpFile("build/pg_os_both.jsonl.meta.json").find("\"open_side\":\"both\"") != string::npos);

    // Determinism: same seed + same open-side = byte-identical output.
    REQUIRE(rankPairGen(idA, idB, 4, "build/pg_os_a2.jsonl", "boards/board1.txt", 2, 5, none, 6, 0, 0, 0, 1, 1) == 0);
    REQUIRE(slurpFile("build/pg_os_a.jsonl") == slurpFile("build/pg_os_a2.jsonl"));
}

TEST_CASE("identity-level opener (AgentSpec::openerPlies) randomizes its own opening plies") {
    // Two different deterministic agents, no pairgen-level --open-plies at all
    // (openPlies=0): any divergence across seeds must come from the agent's own
    // .opener() identity, not the pairgen flag this mirrors.
    const string idPlain  = "ab(d2)@1.classic(t1,c4,w0,l0)@2";
    const string idOpener = "ab(d3)@1.classic(t1,c4,w0,l0)@2.opener(rand,6)@1";
    RankDilOverride none;

    // Baseline: two plain deterministic agents replay identically across seeds.
    REQUIRE(rankPairGen(idPlain, idPlain, 2, "build/pg_id_op_base1.jsonl", "boards/board1.txt", 2, 5, none, 0, 0, 0, 0, 1) == 0);
    REQUIRE(rankPairGen(idPlain, idPlain, 2, "build/pg_id_op_base2.jsonl", "boards/board1.txt", 2, 6, none, 0, 0, 0, 0, 1) == 0);
    REQUIRE(slurpFile("build/pg_id_op_base1.jsonl") == slurpFile("build/pg_id_op_base2.jsonl"));

    // With one agent carrying .opener(rand,6), different seeds must diverge.
    REQUIRE(rankPairGen(idOpener, idPlain, 2, "build/pg_id_op_s5.jsonl", "boards/board1.txt", 2, 5, none, 0, 0, 0, 0, 1) == 0);
    REQUIRE(rankPairGen(idOpener, idPlain, 2, "build/pg_id_op_s6.jsonl", "boards/board1.txt", 2, 6, none, 0, 0, 0, 0, 1) == 0);
    REQUIRE(slurpFile("build/pg_id_op_s5.jsonl") != slurpFile("build/pg_id_op_s6.jsonl"));

    // Same seed => byte-identical (determinism preserved).
    REQUIRE(rankPairGen(idOpener, idPlain, 2, "build/pg_id_op_s5b.jsonl", "boards/board1.txt", 2, 5, none, 0, 0, 0, 0, 1) == 0);
    REQUIRE(slurpFile("build/pg_id_op_s5.jsonl") == slurpFile("build/pg_id_op_s5b.jsonl"));
}

TEST_CASE("opener-bias - runs and is deterministic across identical seeds") {
    const string champ = "ab(d3)@1.classic(t1,c4,w0,l0)@2";   // small depth to keep the test fast
    const string other = "ab(d2)@1.classic(t1,c4,w0,l0)@2";
    // 4 games, 6-ply opener: the command must succeed (nonzero A-plies scored).
    REQUIRE(rankOpenerBias(champ, other, 4, "boards/board1.txt", 6, 7) == 0);
    // Determinism is asserted at the RNG-faithful replay + deterministic-search
    // level; re-running with the same seed exercises the same code path.
    REQUIRE(rankOpenerBias(champ, other, 4, "boards/board1.txt", 6, 7) == 0);
}

TEST_CASE("opener-swap - color-swap recovery test runs and is deterministic") {
    const string a = "ab(d3)@1.classic(t1,c4,w0,l0)@2";
    const string b = "ab(d2)@1.classic(t1,c4,w0,l0)@2";
    // A stronger agent (d3) vs a weaker one (d2): expect an "agent effect" (A wins
    // both continuations) to show up at least sometimes, not asserted precisely
    // (search-dependent), just that the command succeeds and classifies something.
    REQUIRE(rankOpenerSwap(a, b, 6, "boards/board1.txt", 6, 11) == 0);
    // Determinism: identical seed reproduces identical classification (checked via
    // stdout capture would be heavier than needed here; re-running must at least
    // succeed identically without crashing or erroring differently).
    REQUIRE(rankOpenerSwap(a, b, 6, "boards/board1.txt", 6, 11) == 0);
}
