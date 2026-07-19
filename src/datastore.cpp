#include "datastore.h"

// ============================================================
// POSITION KEY
// ============================================================
static string encodeBoard(bool mirror, int sideToMove) {
    string s;
    s.reserve(SIZE * SIZE + 1);
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++) {
            int xx = mirror ? (SIZE - 1 - x) : x;
            s.push_back(board[xx][y]);
        }
    s.push_back(sideToMove == White ? 'W' : 'B');
    return s;
}
static unsigned long long fnv1a(const string& s) {
    unsigned long long h = 1469598103934665603ULL;     // FNV offset basis
    for (char c : s) { h ^= (unsigned char)c; h *= 1099511628211ULL; }
    return h;
}
PosKey positionKey(int sideToMove, bool mirrorFold) {
    string a = encodeBoard(false, sideToMove);
    if (mirrorFold) {
        string b = encodeBoard(true, sideToMove);
        if (b < a) a.swap(b);
    }
    PosKey k;
    k.enc = a;
    k.hash = fnv1a(a);
    return k;
}

bool decodePositionEnc(const string& enc, int& sideToMove) {
    if ((int)enc.size() != SIZE * SIZE + 1) return false;
    char sc = enc[SIZE * SIZE];
    if (sc != 'W' && sc != 'B') return false;
    for (int i = 0; i < SIZE * SIZE; i++) {
        char c = enc[i];
        if (c != EMPTY && c != WHITE && c != BLACK) return false;
    }
    // Same square order as encodeBoard: index = y*SIZE + x.
    for (int y = 0; y < SIZE; y++)
        for (int x = 0; x < SIZE; x++)
            board[x][y] = enc[y * SIZE + x];
    sideToMove = (sc == 'W') ? White : Black;
    // Rebuild the incremental counters exactly the way reloadBoard seeds them.
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
    return true;
}

// ============================================================
// JSONL OUTPUT
// ============================================================
void dsAppendLine(const string& file, const string& jsonLine) {
    std::ofstream f(file, std::ios::app);
    if (!f.is_open()) return;
    f << jsonLine << "\n";
}

string dsJsonEscape(const string& s) {
    string o;
    o.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\t': o += "\\t";  break;
            case '\r': o += "\\r";  break;
            default:   o += c;       break;
        }
    }
    return o;
}
