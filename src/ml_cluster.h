#pragma once
#include "globals.h"
#include <vector>
#include <string>

// ============================================================
// Cluster book: fuzzy nearest-cluster opening guidance
// ============================================================
// The exact-hash opening book (ai_random.cpp's `book` opener) stops helping the
// instant the opponent leaves the mined line, because it needs a positionKey hash
// to match exactly. This module is the machinery for a book that matches
// APPROXIMATELY instead: mined positions are clustered per half-move, and at play
// time the live position is matched to its nearest cluster and inherits that
// cluster's historically-played moves. Guidance then degrades as the position
// drifts rather than disappearing at the first deviation.
//
// The method is Steinmetz & Gini's SMARTSTART, which clustered professional Go
// positions to guide Monte Carlo search. Two things had to change to bring it to
// an 8x8 board and an alpha-beta search.
//
// 1. WHAT IS CLUSTERED. SMARTSTART clustered raw board vectors, which works in Go
//    because a move-20 position has ~20 stones on 361 points, so two different
//    positions overlap barely at all and cosine similarity separates them. A
//    Breakthrough position at half-move 20 still holds close to 32 pieces on 64
//    squares and at most 2 to 3 of the 128 piece-square entries change per
//    half-move, so any two same-ply positions agree on nearly every entry and
//    cosine similarity sits near 1 whatever they actually are. So we cluster the
//    DIFFERENCE from the start position instead (MlcVec, a 128-bit XOR against the
//    start board's own vector). That is lossless given the start board, and it is
//    sparse in the regime the algorithm was designed for: about 16 to 24 bits set
//    at half-move 8 to 12.
//
// 2. HOW THE MOVES ARE USED. SMARTSTART filtered an MCTS playout's move choices.
//    The alpha-beta analogue is a ROOT-move filter: restrict the root move list to
//    the matched cluster's moves and let the normal node/time budget go deeper on
//    what is left (globals.h's g_useRootFilter). The search still evaluates every
//    surviving candidate, so unlike the exact-hash book nothing is played on trust.
//
// Everything here is pure and offline-testable. The clustering itself only ever
// runs in `rank.exe cbookfit`, the opener only loads and matches. Nothing in this
// module touches rand(), so a cbook agent stays deterministic and its ranking
// replays reproduce.

// Positions are the 128 piece-square entries of the v2 feature layout
// (ml_features.h: 0-63 White plane, 64-127 Black plane). Entry 128, side to move,
// is dropped: half-move parity already determines it within a ply bucket.
#define MLC_DIM 128

// Floor on cluster occupancy. A cluster with fewer supporting positions than this
// has move counts that are noise, so mlcSphericalKMeans reduces K rather than
// producing them. See mlcEffectiveK.
#define MLC_MIN_PER_CLUSTER 32

// A 128-bit 0/1 position vector. Used for both a raw board and a difference from
// the start board, since XOR of two 0/1 vectors is another 0/1 vector.
struct MlcVec {
    unsigned long long b[2];
};

void mlcClear(MlcVec& v);
bool mlcGet(const MlcVec& v, int i);
void mlcSet(MlcVec& v, int i);
int  mlcPopcount(const MlcVec& v);
bool mlcEqual(const MlcVec& a, const MlcVec& b);
// Ordering used to pick a canonical representative. Arbitrary but stable.
bool mlcLess(const MlcVec& a, const MlcVec& b);

// Fill `out` from the LIVE board, via mlExtractValueFeaturesV2 so the index layout
// can never drift from the model features.
void mlcBoardVector(MlcVec& out);
// out = cur XOR start.
void mlcDiff(const MlcVec& start, const MlcVec& cur, MlcVec& out);

// Left-right reflection of one feature index, the same mapping ml_train.cpp's
// ensemble symmetrisation uses: x -> SIZE-1-x within each colour plane.
int  mlcMirrorIndex(int i);
void mlcMirror(const MlcVec& in, MlcVec& out);
// Replace v with the smaller of itself and its mirror. Returns true if the mirror
// was taken, which is the signal to reflect the recorded/returned move columns.
// The rules and the standard start are left-right symmetric, so this halves the
// space exactly. Note that COLOUR-swap symmetry is not available: flipping the
// board vertically and swapping colours produces a side-to-move and move-count
// combination that no half-move of a real game reaches.
bool mlcCanonical(MlcVec& v);

// ---- Clustering ----

// The largest cluster count `n` points can support: min(k, n / minPerCluster),
// floored at 1 when there is at least one point.
int mlcEffectiveK(int k, int n, int minPerCluster);

// Spherical k-means over 0/1 points: each point is treated as unit-normalised
// (1/sqrt(popcount) times its bit vector), assignment is by cosine similarity
// (equivalently a dot product against a unit centroid), and each centroid is the
// re-normalised mean of its members. Seeded k-means++ initialisation, farthest-
// point reseeding of any cluster that empties, and its own LCG so it never touches
// the global rand() state that ranking replays depend on.
//
// Returns K_eff, the cluster count actually produced. centroids is filled with
// K_eff * MLC_DIM unit-norm floats and assign with one cluster index per point.
int mlcSphericalKMeans(const std::vector<MlcVec>& pts, int k, int minPerCluster,
                       unsigned seed, int maxIters,
                       std::vector<float>& centroids, std::vector<int>& assign);

// Cosine similarity of a 0/1 point to a unit-norm centroid. 0 for an empty point.
double mlcCosine(const MlcVec& v, const float* centroid);

// ---- Book structures and file IO ----

struct MlcMove {
    int sx, sy, dx;   // source column, source row, destination column
    int count;        // how many mined winning positions in this cluster played it
};

struct MlcCluster {
    std::vector<float>   centroid;   // MLC_DIM, unit norm
    int                  size;       // mined positions assigned here
    double               meanCos;    // mean cosine of members to this centroid
    std::vector<MlcMove> moves;      // count-descending, already truncated by --keep
};

struct MlcBucket {
    int ply;                            // half-move this bucket covers
    int points;                         // mined positions in the bucket
    std::vector<MlcCluster> clusters;
};

struct MlcBook {
    MlcVec start;                       // the start board this book's diffs are against
    std::vector<MlcBucket> buckets;     // ascending by ply
    std::string headerText;             // the '#' comment block, for reporting
    const MlcBucket* bucketFor(int ply) const;
};

// Index of the cluster whose centroid is most similar to `v`, or -1 when the
// bucket has no clusters.
int mlcNearest(const MlcBucket& bucket, const MlcVec& v);

bool mlcSaveBook(const std::string& path, const MlcBook& book,
                 const std::vector<std::string>& headerLines, std::string& err);
bool mlcLoadBook(const std::string& path, MlcBook& out, std::string& err);
