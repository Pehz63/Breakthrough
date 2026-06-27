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
