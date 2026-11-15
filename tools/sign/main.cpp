// gnarl_sign: Ed25519 keygen / sign / verify for release manifests (Monocypher, RFC 8032 Ed25519).
//   gnarl_sign keygen <secret-key-file>            -> writes the 64-byte secret key as hex (mode 600), prints the public key hex
//   gnarl_sign sign <secret-key-file|-env> <file>   -> writes <file>.sig (hex). "-env" reads GNARL_SIGNING_KEY
//   gnarl_sign verify <public-hex> <file>          -> checks <file>.sig, exit 0 on success
//   gnarl_sign sha256 <file>                       -> prints the SHA-256 hex digest
#include "monocypher.h"
#include "monocypher-ed25519.h"
#include <CommonCrypto/CommonDigest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/stat.h>
#include <vector>

static std::string hex(const uint8_t* p, size_t n) { static const char* d = "0123456789abcdef"; std::string s; for (size_t i = 0; i < n; ++i) { s += d[p[i] >> 4]; s += d[p[i] & 15]; } return s; }
static bool unhex(const std::string& h, uint8_t* out, size_t n) {
    std::string t; for (char c : h) if (!isspace((unsigned char) c)) t += c;
    if (t.size() != n * 2) return false;
    for (size_t i = 0; i < n; ++i) out[i] = (uint8_t) std::stoul(t.substr(i * 2, 2), nullptr, 16);
    return true;
}
static std::vector<uint8_t> readFile(const char* p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: gnarl_sign keygen|sign|verify|sha256 ...\n"); return 2; }
    std::string cmd = argv[1];
    if (cmd == "keygen") {
        uint8_t seed[32], sk[64], pk[32];
        arc4random_buf(seed, sizeof seed);
        crypto_ed25519_key_pair(sk, pk, seed);
        FILE* f = std::fopen(argv[2], "wx");
        if (!f) { std::fprintf(stderr, "refusing to overwrite %s\n", argv[2]); return 1; }
        std::fputs(hex(sk, 64).c_str(), f); std::fclose(f);
        chmod(argv[2], 0600);
        crypto_wipe(sk, sizeof sk);
        std::printf("%s\n", hex(pk, 32).c_str());
        return 0;
    }
    if (cmd == "sign" && argc >= 4) {
        std::string keyHex;
        if (std::string(argv[2]) == "-env") { const char* e = std::getenv("GNARL_SIGNING_KEY"); if (!e) { std::fprintf(stderr, "GNARL_SIGNING_KEY not set\n"); return 1; } keyHex = e; }
        else { auto k = readFile(argv[2]); keyHex.assign(k.begin(), k.end()); }
        uint8_t sk[64], sig[64];
        if (!unhex(keyHex, sk, 64)) { std::fprintf(stderr, "bad secret key\n"); return 1; }
        auto msg = readFile(argv[3]);
        crypto_ed25519_sign(sig, sk, msg.data(), msg.size());
        crypto_wipe(sk, sizeof sk);
        std::ofstream(std::string(argv[3]) + ".sig") << hex(sig, 64) << "\n";
        return 0;
    }
    if (cmd == "verify" && argc >= 4) {
        uint8_t pk[32], sig[64];
        if (!unhex(argv[2], pk, 32)) { std::fprintf(stderr, "bad public key\n"); return 1; }
        auto s = readFile((std::string(argv[3]) + ".sig").c_str());
        if (!unhex(std::string(s.begin(), s.end()), sig, 64)) { std::fprintf(stderr, "bad signature file\n"); return 1; }
        auto msg = readFile(argv[3]);
        int r = crypto_ed25519_check(sig, pk, msg.data(), msg.size());
        std::printf(r == 0 ? "signature OK\n" : "signature INVALID\n");
        return r == 0 ? 0 : 1;
    }
    if (cmd == "sha256") {
        auto d = readFile(argv[2]);
        uint8_t h[CC_SHA256_DIGEST_LENGTH];
        CC_SHA256(d.data(), (CC_LONG) d.size(), h);
        std::printf("%s\n", hex(h, sizeof h).c_str());
        return 0;
    }
    std::fprintf(stderr, "unknown command\n");
    return 2;
}
