#include "ml_features.h"

// ============================================================
// MOVE GENERATION
// ============================================================
// Capture-first ordering mirrors ai_minimax.cpp so every consumer (explorers,
// policies, trainer) ranks the same candidate set the same way.
static int genWhite(Move* out) {
    int n = 0;
    for (int y = SIZE-2; y >= 0; y--)
        for (int x = 0; x < SIZE; x++) {
            if (board[x][y] != WHITE) continue;
            int ny = y + 1;
            if (x > 0      && board[x-1][ny] == BLACK) out[n++] = {x, y, x-1, ny, true};
            if (x < SIZE-1 && board[x+1][ny] == BLACK) out[n++] = {x, y, x+1, ny, true};
            if (x > 0      && board[x-1][ny] == EMPTY) out[n++] = {x, y, x-1, ny, false};
            if (x < SIZE-1 && board[x+1][ny] == EMPTY) out[n++] = {x, y, x+1, ny, false};
            if (              board[x  ][ny] == EMPTY) out[n++] = {x, y, x,   ny, false};
        }
    return n;
}
static int genBlack(Move* out) {
    int n = 0;
    for (int y = 1; y <= SIZE-1; y++)
        for (int x = 0; x < SIZE; x++) {
            if (board[x][y] != BLACK) continue;
            int ny = y - 1;
            if (x > 0      && board[x-1][ny] == WHITE) out[n++] = {x, y, x-1, ny, true};
            if (x < SIZE-1 && board[x+1][ny] == WHITE) out[n++] = {x, y, x+1, ny, true};
            if (x > 0      && board[x-1][ny] == EMPTY) out[n++] = {x, y, x-1, ny, false};
            if (x < SIZE-1 && board[x+1][ny] == EMPTY) out[n++] = {x, y, x+1, ny, false};
            if (              board[x  ][ny] == EMPTY) out[n++] = {x, y, x,   ny, false};
        }
    return n;
}
int generateMoves(int side, Move* out) {
    return (side == White) ? genWhite(out) : genBlack(out);
}

// ============================================================
// VALUE FEATURES (board -> white-centric float vector)
// ============================================================
// Layout (version 1). Keep names in sync with mlExtractValueFeatures below.
static const char* kValueNames[MLV_FEATURES] = {
    "mat_diff", "white_total", "black_total",
    "white_rank0","white_rank1","white_rank2","white_rank3",
    "white_rank4","white_rank5","white_rank6","white_rank7",
    "black_rank0","black_rank1","black_rank2","black_rank3",
    "black_rank4","black_rank5","black_rank6","black_rank7",
    "white_forward","black_forward",
    "white_phalanx","black_phalanx",
    "white_defended","black_defended",
    "white_threats","black_threats",
    "white_mobility","black_mobility",
    "side_to_move"
};

int         mlValueFeatureCount()       { return MLV_FEATURES; }
int         mlValueFeatureVersion()     { return 1; }
const char* mlValueFeatureName(int i)   { return (i >= 0 && i < MLV_FEATURES) ? kValueNames[i] : ""; }

void mlExtractValueFeatures(int turnColor, float* f) {
    int wRank[SIZE] = {0}, bRank[SIZE] = {0};
    int wTotal = 0, bTotal = 0;
    int wFurthest = -1, bFurthest = SIZE;        // white wants high y, black wants low y
    int wPhalanx = 0, bPhalanx = 0;
    int wDefended = 0, bDefended = 0;
    int wThreats = 0, bThreats = 0;

    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            char c = board[x][y];
            if (c == WHITE) {
                wRank[y]++; wTotal++;
                if (y > wFurthest) wFurthest = y;
                if (x < SIZE-1 && board[x+1][y] == WHITE) wPhalanx++;            // horizontal pair
                if (y > 0 && ((x > 0 && board[x-1][y-1] == WHITE) ||             // supporter behind
                              (x < SIZE-1 && board[x+1][y-1] == WHITE))) wDefended++;
                if (y < SIZE-1 && ((x > 0 && board[x-1][y+1] == BLACK) ||        // can capture forward
                                   (x < SIZE-1 && board[x+1][y+1] == BLACK))) wThreats++;
            } else if (c == BLACK) {
                bRank[y]++; bTotal++;
                if (y < bFurthest) bFurthest = y;
                if (x < SIZE-1 && board[x+1][y] == BLACK) bPhalanx++;
                if (y < SIZE-1 && ((x > 0 && board[x-1][y+1] == BLACK) ||
                                   (x < SIZE-1 && board[x+1][y+1] == BLACK))) bDefended++;
                if (y > 0 && ((x > 0 && board[x-1][y-1] == WHITE) ||
                              (x < SIZE-1 && board[x+1][y-1] == WHITE))) bThreats++;
            }
        }

    Move tmp[ML_MAX_MOVES];
    int wMob = generateMoves(White, tmp);
    int bMob = generateMoves(Black, tmp);

    float wAdv = (wFurthest >= 0)   ? (wFurthest) / 7.0f          : 0.0f;
    float bAdv = (bFurthest < SIZE) ? (SIZE-1 - bFurthest) / 7.0f : 0.0f;

    int k = 0;
    f[k++] = (wTotal - bTotal) / 16.0f;
    f[k++] = wTotal / 16.0f;
    f[k++] = bTotal / 16.0f;
    for (int y = 0; y < SIZE; y++) f[k++] = wRank[y] / 8.0f;
    for (int y = 0; y < SIZE; y++) f[k++] = bRank[y] / 8.0f;
    f[k++] = wAdv;
    f[k++] = bAdv;
    f[k++] = wPhalanx / 16.0f;
    f[k++] = bPhalanx / 16.0f;
    f[k++] = wDefended / 16.0f;
    f[k++] = bDefended / 16.0f;
    f[k++] = wThreats / 16.0f;
    f[k++] = bThreats / 16.0f;
    f[k++] = wMob / 48.0f;
    f[k++] = bMob / 48.0f;
    f[k++] = (turnColor == White) ? 1.0f : -1.0f;
    // k == MLV_FEATURES
}

// ============================================================
// MOVE FEATURES (move -> side-relative float vector)
// ============================================================
static const char* kMoveNames[MLM_FEATURES] = {
    "capture", "fwd_to", "fwd_from", "is_diagonal", "to_edge",
    "reaches_goal", "support_behind", "enemy_forward", "hanging"
};

int         mlMoveFeatureCount()      { return MLM_FEATURES; }
int         mlMoveFeatureVersion()    { return 1; }
const char* mlMoveFeatureName(int i)  { return (i >= 0 && i < MLM_FEATURES) ? kMoveNames[i] : ""; }

void mlExtractMoveFeatures(const Move& m, int side, float* f) {
    char mine  = (side == White) ? WHITE : BLACK;
    char enemy = (side == White) ? BLACK : WHITE;
    int  fwd   = (side == White) ? 1 : -1;          // direction toward the goal
    int  goal  = (side == White) ? SIZE-1 : 0;

    float advTo   = (side == White) ? m.dy / 7.0f : (SIZE-1 - m.dy) / 7.0f;
    float advFrom = (side == White) ? m.sy / 7.0f : (SIZE-1 - m.sy) / 7.0f;

    // supporter behind the destination (own piece that could recapture)
    int by = m.dy - fwd;
    bool support = (by >= 0 && by < SIZE) &&
                   (((m.dx > 0)      && board[m.dx-1][by] == mine) ||
                    ((m.dx < SIZE-1) && board[m.dx+1][by] == mine));
    // enemy diagonally in front of the destination (mutual threat square)
    int ey = m.dy + fwd;
    bool enemyFwd = (ey >= 0 && ey < SIZE) &&
                    (((m.dx > 0)      && board[m.dx-1][ey] == enemy) ||
                     ((m.dx < SIZE-1) && board[m.dx+1][ey] == enemy));

    int k = 0;
    f[k++] = m.capture ? 1.0f : 0.0f;
    f[k++] = advTo;
    f[k++] = advFrom;
    f[k++] = (m.dx != m.sx) ? 1.0f : 0.0f;
    f[k++] = (m.dx == 0 || m.dx == SIZE-1) ? 1.0f : 0.0f;
    f[k++] = (m.dy == goal) ? 1.0f : 0.0f;
    f[k++] = support ? 1.0f : 0.0f;
    f[k++] = enemyFwd ? 1.0f : 0.0f;
    f[k++] = (enemyFwd && !support) ? 1.0f : 0.0f;   // moving into an undefended attack
    // k == MLM_FEATURES
}
