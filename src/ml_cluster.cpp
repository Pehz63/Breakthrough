#include "ml_cluster.h"
#include "ml_features.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>

// ============================================================
// BIT VECTOR
// ============================================================
void mlcClear(MlcVec& v) { v.b[0] = 0ULL; v.b[1] = 0ULL; }

bool mlcGet(const MlcVec& v, int i) {
    if (i < 0 || i >= MLC_DIM) return false;
    return (v.b[i >> 6] >> (i & 63)) & 1ULL;
}

void mlcSet(MlcVec& v, int i) {
    if (i < 0 || i >= MLC_DIM) return;
    v.b[i >> 6] |= (1ULL << (i & 63));
}

// Portable popcount: MSVC's __popcnt64 needs a target switch and the intrinsic
// header, and this is never on a hot path (mining and one call per opener ply).
static int popcount64(unsigned long long x) {
    int n = 0;
    while (x) { x &= x - 1ULL; n++; }
    return n;
}

int mlcPopcount(const MlcVec& v) { return popcount64(v.b[0]) + popcount64(v.b[1]); }

bool mlcEqual(const MlcVec& a, const MlcVec& b) { return a.b[0] == b.b[0] && a.b[1] == b.b[1]; }

bool mlcLess(const MlcVec& a, const MlcVec& b) {
    if (a.b[1] != b.b[1]) return a.b[1] < b.b[1];
    return a.b[0] < b.b[0];
}

void mlcBoardVector(MlcVec& out) {
    float f[MLV2_FEATURES];
    // turnColor only sets f[MLV2_STM], which we drop, so either value works here.
    mlExtractValueFeaturesV2(White, f);
    mlcClear(out);
    for (int i = 0; i < MLC_DIM; i++)
        if (f[i] != 0.0f) mlcSet(out, i);
}

void mlcDiff(const MlcVec& start, const MlcVec& cur, MlcVec& out) {
    out.b[0] = start.b[0] ^ cur.b[0];
    out.b[1] = start.b[1] ^ cur.b[1];
}

int mlcMirrorIndex(int i) {
    int plane = (i >= SIZE * SIZE) ? SIZE * SIZE : 0;
    int sq = i - plane;
    int x = sq % SIZE, y = sq / SIZE;
    return plane + (SIZE - 1 - x) + SIZE * y;
}

void mlcMirror(const MlcVec& in, MlcVec& out) {
    mlcClear(out);
    for (int i = 0; i < MLC_DIM; i++)
        if (mlcGet(in, i)) mlcSet(out, mlcMirrorIndex(i));
}

bool mlcCanonical(MlcVec& v) {
    MlcVec m;
    mlcMirror(v, m);
    if (mlcLess(m, v)) { v = m; return true; }
    return false;
}

double mlcCosine(const MlcVec& v, const float* centroid) {
    int pc = mlcPopcount(v);
    if (pc == 0) return 0.0;
    double dot = 0.0;
    for (int i = 0; i < MLC_DIM; i++)
        if (mlcGet(v, i)) dot += centroid[i];
    return dot / std::sqrt((double)pc);
}

// ============================================================
// SPHERICAL K-MEANS
// ============================================================
int mlcEffectiveK(int k, int n, int minPerCluster) {
    if (n <= 0) return 0;
    if (k < 1) k = 1;
    if (minPerCluster < 1) minPerCluster = 1;
    int cap = n / minPerCluster;
    if (cap < 1) cap = 1;
    return (k < cap) ? k : cap;
}

// Local LCG. Deliberately not rand(): mining runs inside processes whose game
// replays reseed the global generator per game (ranking.cpp), and clustering must
// not perturb that stream.
static unsigned lcgNext(unsigned& s) {
    s = s * 1664525u + 1013904223u;
    return s;
}
static double lcgUnit(unsigned& s) {
    return (double)(lcgNext(s) >> 8) / (double)(1u << 24);
}

// Unnormalised dot of a 0/1 point against a centroid. Points differ only by the
// positive constant 1/sqrt(popcount), so this ranks centroids identically to the
// true cosine and skips a square root per candidate during assignment.
static double rawDot(const MlcVec& v, const float* c) {
    double d = 0.0;
    for (int i = 0; i < MLC_DIM; i++)
        if (mlcGet(v, i)) d += c[i];
    return d;
}

static void setCentroidFromPoint(const MlcVec& v, float* c) {
    for (int i = 0; i < MLC_DIM; i++) c[i] = 0.0f;
    int pc = mlcPopcount(v);
    if (pc == 0) return;
    float w = (float)(1.0 / std::sqrt((double)pc));
    for (int i = 0; i < MLC_DIM; i++)
        if (mlcGet(v, i)) c[i] = w;
}

static bool normalizeCentroid(float* c) {
    double n = 0.0;
    for (int i = 0; i < MLC_DIM; i++) n += (double)c[i] * (double)c[i];
    if (n <= 0.0) return false;
    float inv = (float)(1.0 / std::sqrt(n));
    for (int i = 0; i < MLC_DIM; i++) c[i] *= inv;
    return true;
}

int mlcSphericalKMeans(const std::vector<MlcVec>& pts, int k, int minPerCluster,
                       unsigned seed, int maxIters,
                       std::vector<float>& centroids, std::vector<int>& assign) {
    const int n = (int)pts.size();
    centroids.clear();
    assign.assign(n < 0 ? 0 : n, 0);
    if (n == 0) return 0;

    const int kEff = mlcEffectiveK(k, n, minPerCluster);
    centroids.assign((size_t)kEff * MLC_DIM, 0.0f);
    if (maxIters < 1) maxIters = 1;
    unsigned rng = seed ? seed : 1u;

    // ---- k-means++ init over cosine distance (1 - cos), with the empty-vector
    // case handled: a zero point has cosine 0 to everything, which is a valid
    // distance rather than a special case. The h=0 bucket is exactly this: one
    // distinct position whose difference from the start is the zero vector.
    {
        int first = (int)(lcgUnit(rng) * n);
        if (first >= n) first = n - 1;
        setCentroidFromPoint(pts[first], &centroids[0]);
        std::vector<double> best(n, 0.0);
        for (int i = 0; i < n; i++) best[i] = 1.0 - mlcCosine(pts[i], &centroids[0]);
        for (int c = 1; c < kEff; c++) {
            double total = 0.0;
            for (int i = 0; i < n; i++) total += best[i] * best[i];
            int pick = -1;
            if (total <= 0.0) {
                // Every remaining point coincides with a chosen centroid. Fall back
                // to a uniform draw so the loop still terminates with kEff centroids.
                pick = (int)(lcgUnit(rng) * n);
                if (pick >= n) pick = n - 1;
            } else {
                double target = lcgUnit(rng) * total, acc = 0.0;
                for (int i = 0; i < n; i++) {
                    acc += best[i] * best[i];
                    if (acc >= target) { pick = i; break; }
                }
                if (pick < 0) pick = n - 1;
            }
            float* cc = &centroids[(size_t)c * MLC_DIM];
            setCentroidFromPoint(pts[pick], cc);
            for (int i = 0; i < n; i++) {
                double d = 1.0 - mlcCosine(pts[i], cc);
                if (d < best[i]) best[i] = d;
            }
        }
    }

    // ---- Lloyd iterations
    std::vector<double> acc((size_t)kEff * MLC_DIM, 0.0);
    std::vector<int> counts(kEff, 0);
    for (int iter = 0; iter < maxIters; iter++) {
        bool moved = false;
        for (int i = 0; i < n; i++) {
            int bestC = 0;
            double bestD = -1e300;
            for (int c = 0; c < kEff; c++) {
                double d = rawDot(pts[i], &centroids[(size_t)c * MLC_DIM]);
                if (d > bestD) { bestD = d; bestC = c; }
            }
            if (assign[i] != bestC) { assign[i] = bestC; moved = true; }
        }
        if (!moved && iter > 0) break;

        std::fill(acc.begin(), acc.end(), 0.0);
        std::fill(counts.begin(), counts.end(), 0);
        for (int i = 0; i < n; i++) {
            int c = assign[i];
            counts[c]++;
            int pc = mlcPopcount(pts[i]);
            if (pc == 0) continue;
            double w = 1.0 / std::sqrt((double)pc);
            double* a = &acc[(size_t)c * MLC_DIM];
            for (int j = 0; j < MLC_DIM; j++)
                if (mlcGet(pts[i], j)) a[j] += w;
        }
        for (int c = 0; c < kEff; c++) {
            float* cc = &centroids[(size_t)c * MLC_DIM];
            for (int j = 0; j < MLC_DIM; j++) cc[j] = (float)acc[(size_t)c * MLC_DIM + j];
            if (counts[c] > 0 && normalizeCentroid(cc)) continue;
            // Empty cluster (or a cluster of only zero-vectors, whose mean has no
            // direction). Reseed from the point currently farthest from every
            // centroid, the standard repair, so K_eff stays honest.
            int worst = -1;
            double worstSim = 1e300;
            for (int i = 0; i < n; i++) {
                double s = -1e300;
                for (int c2 = 0; c2 < kEff; c2++) {
                    double d = rawDot(pts[i], &centroids[(size_t)c2 * MLC_DIM]);
                    if (d > s) s = d;
                }
                if (s < worstSim) { worstSim = s; worst = i; }
            }
            if (worst >= 0) setCentroidFromPoint(pts[worst], cc);
        }
    }
    return kEff;
}

// ============================================================
// BOOK STRUCTURES
// ============================================================
const MlcBucket* MlcBook::bucketFor(int ply) const {
    for (size_t i = 0; i < buckets.size(); i++)
        if (buckets[i].ply == ply) return &buckets[i];
    return 0;
}

int mlcNearest(const MlcBucket& bucket, const MlcVec& v) {
    if (bucket.clusters.empty()) return -1;
    int best = -1;
    double bestD = -1e300;
    for (size_t c = 0; c < bucket.clusters.size(); c++) {
        const std::vector<float>& cen = bucket.clusters[c].centroid;
        if ((int)cen.size() < MLC_DIM) continue;
        double d = rawDot(v, &cen[0]);
        if (d > bestD) { bestD = d; best = (int)c; }
    }
    // Every candidate ties at 0 when the position matches the start exactly (the
    // h=0 bucket, or a bucket whose centroids are all orthogonal to this vector).
    // Falling back to cluster 0 is deliberate: an arbitrary but deterministic pick
    // beats returning "no match", since the caller intersects with legality anyway.
    if (best < 0) best = 0;
    return best;
}

// ============================================================
// FILE IO
// ============================================================
// Format (models/cbook<N>.txt), one book per file:
//
//   # cbook v1 ... (free-form comment block, preserved as headerText)
//   start <32 hex chars>          the start board's own 128-bit vector
//   ply <h> points <N> clusters <K_eff>
//   c <size> <meanCos> <nnz> <idx>:<val> ...      centroid, sparse, unit norm
//   m <sx> <sy> <dx> <count>                      one per kept move
//   c ...
//   ply <h+1> ...
//
// Centroids are stored sparse because early buckets are nearly all zeros: a
// half-move-4 difference vector sets about 8 of 128 entries, so a dense line would
// be mostly "0".
static void writeVecHex(std::ostream& o, const MlcVec& v) {
    char buf[40];
    snprintf(buf, sizeof(buf), "%016llx%016llx",
             (unsigned long long)v.b[1], (unsigned long long)v.b[0]);
    o << buf;
}

static bool readVecHex(const std::string& s, MlcVec& v) {
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

bool mlcSaveBook(const std::string& path, const MlcBook& book,
                 const std::vector<std::string>& headerLines, std::string& err) {
    std::ofstream out(path.c_str(), std::ios::trunc);
    if (!out.is_open()) { err = "cannot write " + path; return false; }
    out << "# cbook v1 (rank.exe cbookfit)\n";
    for (size_t i = 0; i < headerLines.size(); i++) out << "# " << headerLines[i] << "\n";
    out << "start ";
    writeVecHex(out, book.start);
    out << "\n";
    char num[64];
    for (size_t bi = 0; bi < book.buckets.size(); bi++) {
        const MlcBucket& b = book.buckets[bi];
        out << "ply " << b.ply << " points " << b.points
            << " clusters " << b.clusters.size() << "\n";
        for (size_t ci = 0; ci < b.clusters.size(); ci++) {
            const MlcCluster& c = b.clusters[ci];
            int nnz = 0;
            for (int j = 0; j < MLC_DIM && j < (int)c.centroid.size(); j++)
                if (c.centroid[j] != 0.0f) nnz++;
            snprintf(num, sizeof(num), "%.6f", c.meanCos);
            out << "c " << c.size << " " << num << " " << nnz;
            for (int j = 0; j < MLC_DIM && j < (int)c.centroid.size(); j++) {
                if (c.centroid[j] == 0.0f) continue;
                snprintf(num, sizeof(num), "%.6f", (double)c.centroid[j]);
                out << " " << j << ":" << num;
            }
            out << "\n";
            for (size_t mi = 0; mi < c.moves.size(); mi++)
                out << "m " << c.moves[mi].sx << " " << c.moves[mi].sy << " "
                    << c.moves[mi].dx << " " << c.moves[mi].count << "\n";
        }
    }
    out.close();
    return true;
}

bool mlcLoadBook(const std::string& path, MlcBook& out, std::string& err) {
    std::ifstream in(path.c_str());
    if (!in.is_open()) { err = "cannot open " + path; return false; }
    out.buckets.clear();
    out.headerText.clear();
    mlcClear(out.start);
    bool sawStart = false;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        if (line.empty()) continue;
        if (line[0] == '#') { out.headerText += line; out.headerText += "\n"; continue; }
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "start") {
            std::string hex;
            ss >> hex;
            if (!readVecHex(hex, out.start)) { err = "bad start vector in " + path; return false; }
            sawStart = true;
        } else if (tag == "ply") {
            MlcBucket b;
            std::string w1, w2;
            int nclust = 0;
            ss >> b.ply >> w1 >> b.points >> w2 >> nclust;
            if (w1 != "points" || w2 != "clusters") { err = "bad ply header in " + path; return false; }
            b.clusters.reserve(nclust > 0 ? nclust : 0);
            out.buckets.push_back(b);
        } else if (tag == "c") {
            if (out.buckets.empty()) { err = "cluster before any ply header in " + path; return false; }
            MlcCluster c;
            int nnz = 0;
            ss >> c.size >> c.meanCos >> nnz;
            c.centroid.assign(MLC_DIM, 0.0f);
            for (int i = 0; i < nnz; i++) {
                std::string pair;
                if (!(ss >> pair)) { err = "truncated centroid in " + path; return false; }
                size_t colon = pair.find(':');
                if (colon == std::string::npos) { err = "bad centroid entry in " + path; return false; }
                int idx = atoi(pair.substr(0, colon).c_str());
                double val = atof(pair.substr(colon + 1).c_str());
                if (idx >= 0 && idx < MLC_DIM) c.centroid[idx] = (float)val;
            }
            out.buckets.back().clusters.push_back(c);
        } else if (tag == "m") {
            if (out.buckets.empty() || out.buckets.back().clusters.empty()) {
                err = "move before any cluster in " + path; return false;
            }
            MlcMove m;
            ss >> m.sx >> m.sy >> m.dx >> m.count;
            out.buckets.back().clusters.back().moves.push_back(m);
        }
    }
    in.close();
    if (!sawStart) { err = "no start vector in " + path; return false; }
    if (out.buckets.empty()) { err = "no ply buckets in " + path; return false; }
    return true;
}
