// Training-compute meter, wall-clock rung ladder, and checkpoint provenance.
//
// Two separable things are covered here, both from
// plans/budget-parity-plan-1-steady-meridian.md Part 1:
//
//   P5  A checkpoint's `teacher=` line must record what that checkpoint
//       ACTUALLY cost, not what its command line requested. Provenance used to
//       be written once before the training loop from cfg.games, while
//       tools/tdleaf_study.ps1 passes the ladder's LAST rung as --games, so
//       every rung of a laddered run carried the same claim. slot169's header
//       says games=4000 and it trained on 1,500.
//
//   P4/P6  Regimes are compared at equal training WALL CLOCK, so a run needs
//       cumulative second marks it checkpoints at, a hard stop, a node meter
//       that makes the spend portable off this machine, and a resume that
//       continues the same cumulative ladder rather than restarting it.
#include "catch.hpp"
#include "helpers.h"
#include "train_budget.h"
#include "ml_model.h"
#include "ml_eval.h"
#include "ml_features.h"
#include "ml_tdleaf.h"
#include "board_io.h"
#include <vector>
#include <chrono>
#include <thread>

// ============================================================
// MARK PARSING
// ============================================================

TEST_CASE("train budget - mark list parses, sorts, dedupes, and rejects junk") {
    std::vector<double> marks; string err;
    REQUIRE(tbParseMarks("7200,14400,28800", marks, err) == true);
    REQUIRE(marks.size() == 3);
    REQUIRE(marks[0] == Approx(7200.0));
    REQUIRE(marks[2] == Approx(28800.0));

    REQUIRE(tbParseMarks(" 28800 , 7200,14400 , 7200 ", marks, err) == true);
    REQUIRE(marks.size() == 3);           // deduped
    REQUIRE(marks[0] == Approx(7200.0));  // sorted

    REQUIRE(tbParseMarks("7200,oops", marks, err) == false);
    REQUIRE(err.empty() == false);
    REQUIRE(tbParseMarks("0", marks, err) == false);    // a zero mark is meaningless
    REQUIRE(tbParseMarks("-5", marks, err) == false);
}

TEST_CASE("train budget - a wall-rung path names the nominal mark, not the actual elapsed time") {
    // A rung's FILE NAME has to be the same across seeds and regimes so a study
    // can find it; the true elapsed time lives in the file's provenance.
    REQUIRE(tbMarkPath("models/sweep/tdl_s1", 7200.0) == "models/sweep/tdl_s1_t7200.txt");
    REQUIRE(tbMarkPath("models/sweep/tdl_s1", 7203.49) == "models/sweep/tdl_s1_t7203.txt");
}

// ============================================================
// SPEND STAMP
// ============================================================

TEST_CASE("train budget - the spend stamp round-trips through provenance") {
    TrainBudget b = tbDefaults();
    b.priorUnits = 1500; b.priorSec = 7203.5; b.priorNodes = 41288301ULL;
    string teacher = "tdleaf(lambda=0.7,lr=0.01,l2=0,d6,nb200000,batch=1,open=4,explore=0,seed=1001"
                   + tbStamp(b, "games") + ") init:models/pst_value.txt";

    TrainBudget r = tbDefaults();
    REQUIRE(tbParsePrior(teacher, r, "games") == true);
    REQUIRE(r.priorUnits == 1500);
    REQUIRE(r.priorSec == Approx(7203.5).epsilon(1e-6));
    REQUIRE(r.priorNodes == 41288301ULL);
}

TEST_CASE("train budget - an unstamped provenance line reports that it has no spend") {
    // The distinction matters: a resume must refuse such a file rather than
    // silently continue from zero and mislabel the result.
    TrainBudget none = tbDefaults();
    REQUIRE(tbParsePrior("AlphaBeta(Classic, d2) params=[100,0,0]", none, "games") == false);
    REQUIRE(none.priorUnits == 0);
    REQUIRE(none.priorSec == Approx(0.0));
}

TEST_CASE("train budget - stamp lookup does not match inside a longer key") {
    TrainBudget r = tbDefaults();
    REQUIRE(tbParsePrior("regime(opengames=99,games=7,secs=1.5,nodes=8)", r, "games") == true);
    REQUIRE(r.priorUnits == 7);
    REQUIRE(r.priorNodes == 8ULL);
}

TEST_CASE("train budget - CPU seconds charge busy work and not idle waiting") {
    // The knob check for the replication study's compute unit: waiting moves
    // the wall clock and not the CPU meter, spinning moves both.
    TrainBudget b = tbDefaults();
    tbBegin(b);
    double c0 = tbCpu(b), w0 = tbElapsed(b);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    double cIdle = tbCpu(b) - c0, wIdle = tbElapsed(b) - w0;
    INFO("idle: wall " << wIdle << " s, cpu " << cIdle << " s");
    REQUIRE(wIdle >= 0.25);
    REQUIRE(cIdle < 0.1);

    double c1 = tbCpu(b), w1 = tbElapsed(b);
    volatile double sink = 0.0;
    while (tbElapsed(b) - w1 < 0.3) sink = sink + 1.0;
    double cBusy = tbCpu(b) - c1;
    INFO("busy: cpu " << cBusy << " s");
    REQUIRE(cBusy >= 0.15);

    // The stamp carries it and a resume reads it back.
    string stamp = "regime(x=1" + tbStamp(b, "games") + ")";
    REQUIRE(stamp.find(",cpu=") != string::npos);
    TrainBudget r = tbDefaults();
    REQUIRE(tbParsePrior("regime(games=4,secs=9,cpu=7.25,nodes=11)", r, "games") == true);
    REQUIRE(r.priorCpu == Approx(7.25));
    TrainBudget old = tbDefaults();
    REQUIRE(tbParsePrior("regime(games=4,secs=9,nodes=11)", old, "games") == true);
    REQUIRE(old.priorCpu == Approx(0.0));   // pre-CPU stamps resume with a zero prior
}

// ============================================================
// LADDER MECHANICS
// ============================================================

TEST_CASE("train budget - a resumed ladder skips the rungs the prior spend already passed") {
    TrainBudget b = tbDefaults();
    b.wallCkptAt.push_back(10.0);
    b.wallCkptAt.push_back(20.0);
    b.wallCkptAt.push_back(30.0);
    b.priorSec = 22.0;             // resumed past two marks
    tbBegin(b);
    REQUIRE(b.nextMark == 2);      // 10 and 20 already written, 30 still pending
    double mark = 0.0;
    REQUIRE(tbTakeDueMark(b, mark) == false);   // 30s has not elapsed yet
}

TEST_CASE("train budget - wall marks come due in order and are consumed once each") {
    // One long unit can carry a run past several marks at once, and every rung
    // the ladder asked for must still be written, so the caller loops.
    TrainBudget b = tbDefaults();
    b.wallCkptAt.push_back(1.0);
    b.wallCkptAt.push_back(2.0);
    b.priorSec = 5.0;              // already past both
    b.nextMark = 0;                // deliberately not via tbBegin, to exercise the loop
    b.running = false;
    double mark = 0.0;
    REQUIRE(tbTakeDueMark(b, mark) == true);
    REQUIRE(mark == Approx(1.0));
    REQUIRE(tbTakeDueMark(b, mark) == true);
    REQUIRE(mark == Approx(2.0));
    REQUIRE(tbTakeDueMark(b, mark) == false);
}

TEST_CASE("train budget - no wall stop configured means the run is never stopped by one") {
    TrainBudget b = tbDefaults();
    b.priorSec = 1e9;
    REQUIRE(tbShouldStop(b) == false);
    b.wallStopSec = 100.0;
    REQUIRE(tbShouldStop(b) == true);
}

// ============================================================
// P5: THE PROVENANCE BUG ITSELF
// ============================================================

TEST_CASE("trainTDLeaf - every checkpoint records the games it ACTUALLY trained on") {
    TDLeafConfig c = tdLeafDefaults();
    c.outPath     = "build/test_tdl_prov";
    c.boardFile   = "boards\\board1.txt";
    c.games       = 6;             // the ladder's last rung, exactly as the study passes it
    c.depth       = 2;
    c.nodeBudget  = 2000;
    c.openPlies   = 1;
    c.seed        = 4711;
    c.reportEvery = 0;
    c.ckptAt.push_back(2);
    c.ckptAt.push_back(6);
    REQUIRE(trainTDLeaf(c) == 0);

    Model* rung2 = loadModel("build/test_tdl_prov_g2.txt");
    Model* rung6 = loadModel("build/test_tdl_prov_g6.txt");
    REQUIRE(rung2 != nullptr);
    REQUIRE(rung6 != nullptr);
    INFO("rung2 teacher: " << rung2->teacher);
    INFO("rung6 teacher: " << rung6->teacher);
    REQUIRE(rung2->teacher.find("games=2,") != string::npos);
    REQUIRE(rung6->teacher.find("games=6,") != string::npos);
    // Identical provenance on two different rungs is exactly what the pre-fix
    // code produced, so assert they differ as well as being individually right.
    REQUIRE(rung2->teacher != rung6->teacher);

    TrainBudget b2 = tbDefaults();
    REQUIRE(tbParsePrior(rung2->teacher, b2, "games") == true);
    REQUIRE(b2.priorUnits == 2);
    REQUIRE(b2.priorNodes > 0);    // a d2/nb2000 search visits nodes, so the meter ran
    delete rung2; delete rung6;
}

// ============================================================
// P4/P6: WALL-CLOCK STOP, RUNGS, RESUME
// ============================================================

TEST_CASE("trainTDLeaf - a wall-clock stop ends the run and writes its rungs") {
    TDLeafConfig c = tdLeafDefaults();
    c.outPath     = "build/test_tdl_wall";
    c.boardFile   = "boards\\board1.txt";
    c.games       = 0;             // wall clock alone governs the length
    c.depth       = 2;
    c.nodeBudget  = 2000;
    c.openPlies   = 1;
    c.seed        = 4712;
    c.reportEvery = 0;
    c.wallCkptAt.push_back(0.35);
    c.wallStopSec = 0.7;
    REQUIRE(trainTDLeaf(c) == 0);

    Model* rung = loadModel("build/test_tdl_wall_t0.txt");   // 0.35 s rounds to _t0
    REQUIRE(rung != nullptr);
    TrainBudget b = tbDefaults();
    REQUIRE(tbParsePrior(rung->teacher, b, "games") == true);
    REQUIRE(b.priorUnits > 0);     // it played real games before the mark
    REQUIRE(b.priorSec >= 0.35);   // the rung was written at or after its mark
    delete rung;

    Model* fin = loadModel("build/test_tdl_wall.txt");
    REQUIRE(fin != nullptr);
    TrainBudget f = tbDefaults();
    REQUIRE(tbParsePrior(fin->teacher, f, "games") == true);
    REQUIRE(f.priorSec >= 0.7);    // the stop held
    REQUIRE(f.priorSec < 60.0);    // and it did not run away
    delete fin;
}

TEST_CASE("trainTDLeaf - resume continues the same cumulative ladder") {
    TDLeafConfig c = tdLeafDefaults();
    c.outPath     = "build/test_tdl_res";
    c.boardFile   = "boards\\board1.txt";
    c.games       = 3;
    c.depth       = 2;
    c.nodeBudget  = 2000;
    c.openPlies   = 1;
    c.seed        = 4713;
    c.reportEvery = 0;
    REQUIRE(trainTDLeaf(c) == 0);

    TDLeafConfig r = c;
    r.outPath     = "build/test_tdl_res2";
    r.resumeFrom  = "build/test_tdl_res.txt";
    r.games       = 0;
    r.wallStopSec = 0.4;
    REQUIRE(trainTDLeaf(r) == 0);

    Model* m = loadModel("build/test_tdl_res2.txt");
    REQUIRE(m != nullptr);
    INFO("resumed teacher: " << m->teacher);
    TrainBudget b = tbDefaults();
    REQUIRE(tbParsePrior(m->teacher, b, "games") == true);
    REQUIRE(b.priorUnits > 3);     // cumulative, not restarted at zero
    REQUIRE(m->teacher.find("resume:") != string::npos);
    delete m;
}

TEST_CASE("trainTDLeaf - resume refuses a checkpoint with no spend stamp") {
    // A model saved by anything other than a stamped run cannot say how much
    // compute it represents, so continuing its ladder would invent a number.
    LinearModel* lm = new LinearModel(HEAD_VALUE, 2, MLV2_FEATURES, 900.0f);
    lm->teacher = "AlphaBeta(Classic, d2) params=[100,0,0]";
    REQUIRE(lm->save("build/test_tdl_nostamp.txt") == true);
    delete lm;

    TDLeafConfig r = tdLeafDefaults();
    r.outPath     = "build/test_tdl_nostamp_out";
    r.boardFile   = "boards\\board1.txt";
    r.resumeFrom  = "build/test_tdl_nostamp.txt";
    r.games       = 1;
    r.depth       = 2;
    r.reportEvery = 0;
    REQUIRE(trainTDLeaf(r) != 0);  // refused, not silently restarted
}
