
#ifndef QUANTUM_HPP_
#define QUANTUM_HPP_

#include <algorithm>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <execinfo.h>
#include <fstream>
#include <initializer_list>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <sys/resource.h>
#include <sys/time.h>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#define TINY 1E-20

using namespace std;

namespace block2 {

inline void print_trace(int n_sig) {
    printf("print_trace: got signal %d\n", n_sig);

    void *array[32];
    size_t size = backtrace(array, 32);
    char **strings = backtrace_symbols(array, size);

    for (size_t i = 0; i < size; i++)
        fprintf(stderr, "%s\n", strings[i]);

    abort();
}

enum struct OpNames : uint8_t {
    H,
    I,
    N,
    NN,
    NUD,
    C,
    D,
    R,
    RD,
    A,
    AD,
    P,
    PD,
    B,
    Q,
    PDM1
};

inline ostream &operator<<(ostream &os, const OpNames c) {
    const static string repr[] = {"H",  "I", "N",  "NN",  "NUD", "C",
                                  "D",  "R", "RD", "A",   "AD",  "P",
                                  "PD", "B", "Q",  "PDM1"};
    os << repr[(uint8_t)c];
    return os;
}

template <typename T> struct StackAllocator {
    size_t size, used, shift;
    T *data;
    StackAllocator(T *ptr, size_t max_size)
        : size(max_size), used(0), shift(0), data(ptr) {}
    StackAllocator() : size(0), used(0), shift(0), data(0) {}
    T *allocate(size_t n) {
        assert(shift == 0);
        if (used + n >= size) {
            cout << "exceeding allowed memory" << endl;
            print_trace(11);
            return 0;
        } else
            return data + (used += n) - n;
    }
    void deallocate(void *ptr, size_t n) {
        if (n == 0)
            return;
        if (used < n || ptr != data + used - n) {
            cout << "deallocation not happening in reverse order" << endl;
            print_trace(11);
        } else
            used -= n;
    }
    T *reallocate(T *ptr, size_t n, size_t new_n) {
        ptr += shift;
        shift += new_n - n;
        used = used + new_n - n;
        if (ptr == data + used - new_n)
            shift = 0;
        return (T *)ptr;
    }
    friend ostream &operator<<(ostream &os, const StackAllocator &c) {
        os << "SIZE=" << c.size << " PTR=" << c.data << " USED=" << c.used
           << " SHIFT=" << (long)c.shift << endl;
        return os;
    }
};

extern StackAllocator<uint32_t> *ialloc;
extern StackAllocator<double> *dalloc;

struct Timer {
    double current;
    Timer() : current(0) {}
    double get_time() {
        struct timeval t;
        gettimeofday(&t, NULL);
        double previous = current;
        current = t.tv_sec + 1E-6 * t.tv_usec;
        return current - previous;
    }
};

struct Random {
    static mt19937 rng;
    static void rand_seed(unsigned i = 0) {
        rng = mt19937(
            i ? i : chrono::steady_clock::now().time_since_epoch().count());
    }
    // return a integer in [a, b)
    static int rand_int(int a, int b) {
        assert(b > a);
        return uniform_int_distribution<int>(a, b - 1)(rng);
    }
    // return a double in [a, b)
    static double rand_double(double a = 0, double b = 1) {
        assert(b > a);
        return uniform_real_distribution<double>(a, b)(rng);
    }
    template <typename T>
    static void fill_rand_double(const T &t, double a = 0, double b = 1) {
        uniform_real_distribution<double> distr(a, b);
        auto *ptr = t.data;
        for (long i = 0, n = t.get_len(); i < n; i++)
            ptr[i] =
                (typename remove_reference<decltype(ptr[i])>::type)distr(rng);
    }
};

struct Parsing {
    static vector<string> readlines(istream *input) {
        string h;
        vector<string> r;
        while (!input->eof()) {
            getline(*input, h);
            size_t idx = h.find("!");
            if (idx != string::npos)
                h = string(h, 0, idx);
            while ((idx = h.find("\r")) != string::npos)
                h.replace(idx, 1, "");
            r.push_back(h);
        }
        return r;
    }
    static vector<string> split(const string &s, const string &delim,
                                bool remove_empty = false) {
        vector<string> r;
        size_t last = 0;
        size_t index = s.find_first_of(delim, last);
        while (index != string::npos) {
            if (!remove_empty || index > last)
                r.push_back(s.substr(last, index - last));
            last = index + 1;
            index = s.find_first_of(delim, last);
        }
        if (index > last)
            r.push_back(s.substr(last, index - last));
        return r;
    }
    static string &lower(string &x) {
        transform(x.begin(), x.end(), x.begin(), ::tolower);
        return x;
    }
    static string &trim(string &x) {
        if (x.empty())
            return x;
        x.erase(0, x.find_first_not_of(" \t"));
        x.erase(x.find_last_not_of(" \t") + 1);
        return x;
    }
    template <typename T>
    static string join(T it_start, T it_end, const string &x) {
        stringstream r;
        for (T i = it_start; i != it_end; i++)
            r << *i << x;
        string rr = r.str();
        if (rr.size() != 0)
            rr.resize(rr.size() - x.length());
        return rr;
    }
    static int to_int(const string &x) { return atoi(x.c_str()); }
    static double to_double(const string &x) { return atof(x.c_str()); }
};

struct MatrixRef {
    int m, n;
    double *data;
    MatrixRef(double *data, int m, int n) : data(data), m(m), n(n) {}
    double &operator()(int i, int j) const {
        return *(data + (size_t)i * n + j);
    }
};

struct TInt {
    uint16_t n;
    double *data;
    TInt(uint16_t n) : n(n), data(nullptr) {}
    uint32_t find_index(uint16_t i, uint16_t j) const {
        return i < j ? ((uint32_t)j * (j + 1) >> 1) + i
                     : ((uint32_t)i * (i + 1) >> 1) + j;
    }
    size_t size() const { return ((size_t)n * (n + 1) >> 1); }
    void clear() { memset(data, 0, sizeof(double) * size()); }
    double &operator()(uint16_t i, uint16_t j) {
        return *(data + find_index(i, j));
    }
};

struct V8Int {
    uint32_t n, m;
    double *data;
    V8Int(uint32_t n) : n(n), m(n * (n + 1) >> 1), data(nullptr) {}
    size_t find_index(uint32_t i, uint32_t j) const {
        return i < j ? ((size_t)j * (j + 1) >> 1) + i
                     : ((size_t)i * (i + 1) >> 1) + j;
    }
    size_t find_index(uint16_t i, uint16_t j, uint16_t k, uint16_t l) const {
        uint32_t p = (uint32_t)find_index(i, j), q = (uint32_t)find_index(k, l);
        return find_index(p, q);
    }
    size_t size() const { return ((size_t)m * (m + 1) >> 1); }
    void clear() { memset(data, 0, sizeof(double) * size()); }
    double &operator()(uint16_t i, uint16_t j, uint16_t k, uint16_t l) {
        return *(data + find_index(i, j, k, l));
    }
};

struct V4Int {
    uint32_t n, m;
    double *data;
    V4Int(uint32_t n) : n(n), m(n * (n + 1) >> 1), data(nullptr) {}
    size_t find_index(uint32_t i, uint32_t j) const {
        return i < j ? ((size_t)j * (j + 1) >> 1) + i
                     : ((size_t)i * (i + 1) >> 1) + j;
    }
    size_t find_index(uint16_t i, uint16_t j, uint16_t k, uint16_t l) const {
        size_t p = (uint32_t)find_index(i, j), q = (uint32_t)find_index(k, l);
        return p * m + q;
    }
    size_t size() const { return (size_t)m * m; }
    void clear() { memset(data, 0, sizeof(double) * size()); }
    double &operator()(uint16_t i, uint16_t j, uint16_t k, uint16_t l) {
        return *(data + find_index(i, j, k, l));
    }
};

struct FCIDUMP {
    map<string, string> params;
    vector<TInt> ts;
    vector<V8Int> vs;
    vector<V4Int> vabs;
    double e;
    double *data;
    size_t total_memory;
    bool uhf;
    FCIDUMP() : e(0.0), uhf(false), total_memory(0) {}
    void read(const string &filename) {
        params.clear();
        ts.clear();
        vs.clear();
        vabs.clear();
        e = 0.0;
        ifstream ifs(filename.c_str());
        vector<string> lines = Parsing::readlines(&ifs);
        ifs.close();
        bool ipar = true;
        vector<string> pars, ints;
        for (size_t il = 0; il < lines.size(); il++) {
            string l(Parsing::lower(lines[il]));
            if (l.find("&fci") != string::npos)
                l.replace(l.find("&fci"), 4, "");
            if (l.find("/") != string::npos || l.find("&end") != string::npos)
                ipar = false;
            else if (ipar)
                pars.push_back(l);
            else
                ints.push_back(l);
        }
        string par = Parsing::join(pars.begin(), pars.end(), ",");
        for (size_t ip = 0; ip < par.length(); ip++)
            if (par[ip] == ' ')
                par[ip] = ',';
        pars = Parsing::split(par, ",", true);
        string p_key = "";
        for (auto &c : pars) {
            if (c.find("=") != string::npos || p_key.length() == 0) {
                vector<string> cs = Parsing::split(c, "=", true);
                p_key = Parsing::trim(cs[0]);
                params[p_key] = cs.size() == 2 ? Parsing::trim(cs[1]) : "";
            } else {
                string cc = Parsing::trim(c);
                if (cc.length() != 0)
                    params[p_key] = params[p_key].length() == 0
                                        ? cc
                                        : params[p_key] + "," + cc;
            }
        }
        vector<array<uint16_t, 4>> int_idx;
        vector<double> int_val;
        for (auto &l : ints) {
            string ll = Parsing::trim(l);
            if (ll.length() == 0 || ll[0] == '!')
                continue;
            vector<string> ls = Parsing::split(ll, " ", true);
            assert(ls.size() == 5);
            int_idx.push_back({(uint16_t)Parsing::to_int(ls[1]),
                               (uint16_t)Parsing::to_int(ls[2]),
                               (uint16_t)Parsing::to_int(ls[3]),
                               (uint16_t)Parsing::to_int(ls[4])});
            int_val.push_back(Parsing::to_double(ls[0]));
        }
        uint16_t n = (uint16_t)Parsing::to_int(params["norb"]);
        uhf = params.count("iuhf") != 0 && Parsing::to_int(params["iuhf"]) == 1;
        if (!uhf) {
            ts.push_back(TInt(n));
            vs.push_back(V8Int(n));
            total_memory = ts[0].size() + vs[0].size();
            data = dalloc->allocate(total_memory);
            ts[0].data = data;
            vs[0].data = data + ts[0].size();
            ts[0].clear();
            vs[0].clear();
            for (size_t i = 0; i < int_val.size(); i++) {
                if (int_idx[i][0] + int_idx[i][1] + int_idx[i][2] +
                        int_idx[i][3] ==
                    0)
                    e = int_val[i];
                else if (int_idx[i][2] + int_idx[i][3] == 0)
                    ts[0](int_idx[i][0] - 1, int_idx[i][1] - 1) = int_val[i];
                else
                    vs[0](int_idx[i][0] - 1, int_idx[i][1] - 1,
                          int_idx[i][2] - 1, int_idx[i][3] - 1) = int_val[i];
            }
        } else {
            ts.push_back(TInt(n));
            ts.push_back(TInt(n));
            vs.push_back(V8Int(n));
            vs.push_back(V8Int(n));
            vabs.push_back(V4Int(n));
            total_memory =
                ((ts[0].size() + vs[0].size()) << 1) + vabs[0].size();
            data = dalloc->allocate(total_memory);
            ts[0].data = data;
            ts[1].data = data + ts[0].size();
            vs[0].data = data + (ts[0].size() << 1);
            vs[1].data = data + (ts[0].size() << 1) + vs[0].size();
            vabs[0].data = data + ((ts[0].size() + vs[0].size()) << 1);
            ts[0].clear(), ts[1].clear();
            vs[0].clear(), vs[1].clear(), vabs[0].clear();
            int ip = 0;
            for (size_t i = 0; i < int_val.size(); i++) {
                if (int_idx[i][0] + int_idx[i][1] + int_idx[i][2] +
                        int_idx[i][3] ==
                    0) {
                    ip++;
                    if (ip == 6)
                        e = int_val[i];
                } else if (int_idx[i][2] + int_idx[i][3] == 0) {
                    ts[ip - 3](int_idx[i][0] - 1, int_idx[i][1] - 1) =
                        int_val[i];
                } else {
                    assert(ip <= 2);
                    if (ip < 2)
                        vs[ip](int_idx[i][0] - 1, int_idx[i][1] - 1,
                               int_idx[i][2] - 1, int_idx[i][3] - 1) =
                            int_val[i];
                    else
                        vabs[0](int_idx[i][0] - 1, int_idx[i][1] - 1,
                                int_idx[i][2] - 1, int_idx[i][3] - 1) =
                            int_val[i];
                }
            }
        }
    }
    uint16_t twos() const {
        return (uint16_t)Parsing::to_int(params.at("ms2"));
    }
    uint16_t n_sites() const {
        return (uint16_t)Parsing::to_int(params.at("norb"));
    }
    uint16_t n_elec() const {
        return (uint16_t)Parsing::to_int(params.at("nelec"));
    }
    uint8_t isym() const { return (uint8_t)Parsing::to_int(params.at("isym")); }
    vector<uint8_t> orb_sym() const {
        vector<string> x = Parsing::split(params.at("orbsym"), ",", true);
        vector<uint8_t> r;
        r.reserve(x.size());
        for (auto &xx : x)
            r.push_back((uint8_t)Parsing::to_int(xx));
        return r;
    }
    void deallocate() {
        assert(total_memory != 0);
        dalloc->deallocate(data, total_memory);
        data = nullptr;
        ts.clear();
        vs.clear();
        vabs.clear();
    }
};

struct CG {
    long double *sqrt_fact;
    int n_sf, n_twoj;
    CG() : n_sf(0), n_twoj(0), sqrt_fact(nullptr) {}
    CG(int n_sqrt_fact, int max_j) : n_sf(n_sqrt_fact), n_twoj(max_j) {}
    void initialize(double *ptr = 0) {
        assert(n_sf != 0);
        if (ptr == 0)
            ptr = dalloc->allocate(n_sf * 2);
        sqrt_fact = (long double *)ptr;
        sqrt_fact[0] = 1;
        for (int i = 1; i < n_sf; i++)
            sqrt_fact[i] = sqrt_fact[i - 1] * sqrtl(i);
    }
    void deallocate() {
        assert(n_sf != 0);
        dalloc->deallocate(sqrt_fact, n_sf * 2);
        sqrt_fact = nullptr;
    }
    static bool triangle(int tja, int tjb, int tjc) {
        return !((tja + tjb + tjc) & 1) && tjc <= tja + tjb &&
               tjc >= abs(tja - tjb);
    }
    long double sqrt_delta(int tja, int tjb, int tjc) const {
        return sqrt_fact[(tja + tjb - tjc) >> 1] *
               sqrt_fact[(tja - tjb + tjc) >> 1] *
               sqrt_fact[(-tja + tjb + tjc) >> 1] /
               sqrt_fact[(tja + tjb + tjc + 2) >> 1];
    }
    long double cg(int tja, int tjb, int tjc, int tma, int tmb, int tmc) const {
        return (((tmc + tja - tjb) & 2) ? -1 : 1) * sqrt(tjc + 1) *
               wigner_3j(tja, tjb, tjc, tma, tmb, -tmc);
    }
    // Albert Messiah, Quantum Mechanics. Vol 2. Eq. (C.21)
    // Also see Sebastian's CheMPS2 code Wigner.cpp
    long double wigner_3j(int tja, int tjb, int tjc, int tma, int tmb,
                          int tmc) const {
        if (tma + tmb + tmc != 0 || !triangle(tja, tjb, tjc) ||
            ((tja + tma) & 1) || ((tjb + tmb) & 1) || ((tjc + tmc) & 1))
            return 0;
        const int alpha1 = (tjb - tjc - tma) >> 1,
                  alpha2 = (tja - tjc + tmb) >> 1;
        const int beta1 = (tja + tjb - tjc) >> 1, beta2 = (tja - tma) >> 1,
                  beta3 = (tjb + tmb) >> 1;
        const int max_alpha = max(0, max(alpha1, alpha2));
        const int min_beta = min(beta1, min(beta2, beta3));
        if (max_alpha > min_beta)
            return 0;
        long double factor =
            (((tja - tjb - tmc) & 2) ? -1 : 1) * ((max_alpha & 1) ? -1 : 1) *
            sqrt_delta(tja, tjb, tjc) * sqrt_fact[(tja + tma) >> 1] *
            sqrt_fact[(tja - tma) >> 1] * sqrt_fact[(tjb + tmb) >> 1] *
            sqrt_fact[(tjb - tmb) >> 1] * sqrt_fact[(tjc + tmc) >> 1] *
            sqrt_fact[(tjc - tmc) >> 1];
        long double r = 0, rst;
        for (int t = max_alpha; t <= min_beta; ++t, factor = -factor) {
            rst = sqrt_fact[t] * sqrt_fact[t - alpha1] * sqrt_fact[t - alpha2] *
                  sqrt_fact[beta1 - t] * sqrt_fact[beta2 - t] *
                  sqrt_fact[beta3 - t];
            r += factor / (rst * rst);
        }
        return r;
    }
    // Albert Messiah, Quantum Mechanics. Vol 2. Eq. (C.36)
    // Also see Sebastian's CheMPS2 code Wigner.cpp
    long double wigner_6j(int tja, int tjb, int tjc, int tjd, int tje,
                          int tjf) const {
        if (!triangle(tja, tjb, tjc) || !triangle(tja, tje, tjf) ||
            !triangle(tjd, tjb, tjf) || !triangle(tjd, tje, tjc))
            return 0;
        const int alpha1 = (tja + tjb + tjc) >> 1,
                  alpha2 = (tja + tje + tjf) >> 1,
                  alpha3 = (tjd + tjb + tjf) >> 1,
                  alpha4 = (tjd + tje + tjc) >> 1;
        const int beta1 = (tja + tjb + tjd + tje) >> 1,
                  beta2 = (tjb + tjc + tje + tjf) >> 1,
                  beta3 = (tja + tjc + tjd + tjf) >> 1;
        const int max_alpha = max(alpha1, max(alpha2, max(alpha3, alpha4)));
        const int min_beta = min(beta1, min(beta2, beta3));
        if (max_alpha > min_beta)
            return 0;
        long double factor =
            ((max_alpha & 1) ? -1 : 1) * sqrt_delta(tja, tjb, tjc) *
            sqrt_delta(tja, tje, tjf) * sqrt_delta(tjd, tjb, tjf) *
            sqrt_delta(tjd, tje, tjc);
        long double r = 0, rst;
        for (int t = max_alpha; t <= min_beta; ++t, factor = -factor) {
            rst = sqrt_fact[t - alpha1] * sqrt_fact[t - alpha2] *
                  sqrt_fact[t - alpha3] * sqrt_fact[t - alpha4] *
                  sqrt_fact[beta1 - t] * sqrt_fact[beta2 - t] *
                  sqrt_fact[beta3 - t];
            r += factor * sqrt_fact[t + 1] * sqrt_fact[t + 1] / (rst * rst);
        }
        return r;
    }
    // Albert Messiah, Quantum Mechanics. Vol 2. Eq. (C.41)
    // Also see Sebastian's CheMPS2 code Wigner.cpp
    long double wigner_9j(int tja, int tjb, int tjc, int tjd, int tje, int tjf,
                          int tjg, int tjh, int tji) const {
        if (!triangle(tja, tjb, tjc) || !triangle(tjd, tje, tjf) ||
            !triangle(tjg, tjh, tji) || !triangle(tja, tjd, tjg) ||
            !triangle(tjb, tje, tjh) || !triangle(tjc, tjf, tji))
            return 0;
        const int alpha1 = abs(tja - tji), alpha2 = abs(tjd - tjh),
                  alpha3 = abs(tjb - tjf);
        const int beta1 = tja + tji, beta2 = tjd + tjh, beta3 = tjb + tjf;
        const int max_alpha = max(alpha1, max(alpha2, alpha3));
        const int min_beta = min(beta1, min(beta2, beta3));
        long double r = 0;
        for (int tg = max_alpha; tg <= min_beta; tg += 2) {
            r += (tg + 1) * wigner_6j(tja, tjb, tjc, tjf, tji, tg) *
                 wigner_6j(tjd, tje, tjf, tjb, tg, tjh) *
                 wigner_6j(tjg, tjh, tji, tg, tja, tjd);
        }
        return ((max_alpha & 1) ? -1 : 1) * r;
    }
    // D.M. Brink, G.R. Satchler. Angular Momentum. P142
    long double racah(int ta, int tb, int tc, int td, int te, int tf) {
        return (((ta + tb + tc + td) & 2) ? -1 : 1) *
               wigner_6j(ta, tb, te, td, tc, tf);
    }
};

struct SZLabel {
    uint32_t data;
    SZLabel() : data(0) {}
    SZLabel(uint32_t data) : data(data) {}
    SZLabel(int n, int twos, int pg)
        : data((uint32_t)((n << 24) | (twos << 8) | pg)) {}
    SZLabel(const SZLabel &other) : data(other.data) {}
    int n() const { return (int)(((int32_t)data) >> 24); }
    int twos() const { return (int)(int16_t)((data >> 8) & 0xFFU); }
    int pg() const { return (int)(data & 0xFFU); }
    void set_n(int n) { data = (data & 0xFFFFFFU) | ((uint32_t)(n << 24)); }
    void set_twos(int twos) {
        data = (data & (~0xFFFF00U)) | ((uint32_t)((twos & 0xFFU) << 8));
    }
    void set_pg(int pg) { data = (data & (~0xFFU)) | ((uint32_t)pg); }
    bool operator==(SZLabel other) const noexcept { return data == other.data; }
    bool operator!=(SZLabel other) const noexcept { return data != other.data; }
    bool operator<(SZLabel other) const noexcept { return data < other.data; }
    SZLabel operator-() const noexcept {
        return SZLabel((data & 0xFFU) | (((~data) + (1 << 8)) & (0xFF00U)) |
                       (((~data) + (1 << 24)) & (~0xFFFFFFU)));
    }
    SZLabel operator-(SZLabel other) const noexcept { return *this + (-other); }
    SZLabel operator+(SZLabel other) const noexcept {
        return SZLabel((((data & 0xFF00FF00U) + (other.data & 0xFF00FF00U)) &
                        0xFF00FF00U) |
                       ((data & 0xFFU) ^ (other.data & 0xFFU)));
    }
    SZLabel operator[](int i) const noexcept { return *this; }
    SZLabel get_ket() const noexcept { return *this; }
    SZLabel get_bra(SZLabel dq) const noexcept { return *this + dq; }
    size_t hash() const noexcept { return (size_t)data; }
    int count() const { return 1; }
    string to_str() const {
        stringstream ss;
        ss << "< N=" << n() << " SZ=";
        if (twos() & 1)
            ss << twos() << "/2";
        else
            ss << (twos() >> 1);
        ss << " PG=" << pg() << " >";
        return ss.str();
    }
    friend ostream &operator<<(ostream &os, SZLabel c) {
        os << c.to_str();
        return os;
    }
};

struct SpinLabel {
    uint32_t data;
    SpinLabel() : data(0) {}
    SpinLabel(uint32_t data) : data(data) {}
    SpinLabel(int n, int twos, int pg)
        : data((uint32_t)((n << 24) | (twos << 16) | (twos << 8) | pg)) {}
    SpinLabel(int n, int twos_low, int twos, int pg)
        : data((uint32_t)((n << 24) | (twos_low << 16) | (twos << 8) | pg)) {}
    SpinLabel(const SpinLabel &other) : data(other.data) {}
    int n() const { return (int)(((int32_t)data) >> 24); }
    int twos() const { return (int)(int16_t)((data >> 8) & 0xFFU); }
    int twos_low() const { return (int)(int16_t)((data >> 16) & 0xFFU); }
    int pg() const { return (int)(data & 0xFFU); }
    void set_n(int n) { data = (data & 0xFFFFFFU) | ((uint32_t)(n << 24)); }
    void set_twos(int twos) {
        data = (data & (~0xFFFF00U)) | ((uint32_t)((twos & 0xFFU) << 16)) |
               ((uint32_t)((twos & 0xFFU) << 8));
    }
    void set_twos_low(int twos) {
        data = (data & (~0xFF0000U)) | ((uint32_t)((twos & 0xFFU) << 16));
    }
    void set_pg(int pg) { data = (data & (~0xFFU)) | ((uint32_t)pg); }
    bool operator==(SpinLabel other) const noexcept {
        return data == other.data;
    }
    bool operator!=(SpinLabel other) const noexcept {
        return data != other.data;
    }
    bool operator<(SpinLabel other) const noexcept { return data < other.data; }
    SpinLabel operator-() const noexcept {
        return SpinLabel((data & 0xFFFFFFU) |
                         (((~data) + (1 << 24)) & (~0xFFFFFFU)));
    }
    SpinLabel operator-(SpinLabel other) const noexcept {
        return *this + (-other);
    }
    SpinLabel operator+(SpinLabel other) const noexcept {
        uint32_t add_data =
            ((data & 0xFF00FF00U) + (other.data & 0xFF00FF00U)) |
            ((data & 0xFFU) ^ (other.data & 0xFFU));
        uint32_t sub_data_lr =
            ((data & 0xFF00U) << 8) - (other.data & 0xFF0000U);
        sub_data_lr =
            (((sub_data_lr >> 7) + sub_data_lr) ^ (sub_data_lr >> 7)) &
            0xFF0000U;
        uint32_t sub_data_rl =
            ((other.data & 0xFF00U) << 8) - (data & 0xFF0000U);
        sub_data_rl =
            (((sub_data_rl >> 7) + sub_data_rl) ^ (sub_data_rl >> 7)) &
            0xFF0000U;
        return SpinLabel(add_data | min(sub_data_lr, sub_data_rl));
    }
    SpinLabel operator[](int i) const noexcept {
        return SpinLabel(((data + (i << 17)) & (~0x00FF00U)) |
                         (((data + (i << 17)) & 0xFF0000U) >> 8));
    }
    int find(SpinLabel x) const noexcept {
        if (((data ^ x.data) & 0xFF0000FFU) || ((x.data - data) & 0x100U) ||
            x.twos() < twos_low() || x.twos() > twos())
            return -1;
        else
            return ((x.data - data) & 0xFF00U) >> 9;
    }
    SpinLabel get_ket() const noexcept {
        return SpinLabel((data & 0xFF00FFFFU) | ((data & 0xFF00U) << 8));
    }
    SpinLabel get_bra(SpinLabel dq) const noexcept {
        return SpinLabel(((data & 0xFF000000U) + (dq.data & 0xFF000000U)) |
                         ((data & 0xFF0000U) >> 8) | (data & 0xFF0000U) |
                         ((data & 0xFFU) ^ (dq.data & 0xFFU)));
    }
    SpinLabel combine(SpinLabel bra, SpinLabel ket) const {
        ket.set_twos_low(bra.twos());
        if (ket.get_bra(*this) != bra ||
            !CG::triangle(ket.twos(), this->twos(), bra.twos()))
            return SpinLabel(0xFFFFFFFFU);
        return ket;
    }
    size_t hash() const noexcept { return (size_t)data; }
    int count() const {
        return (int)(((data >> 9) - (data >> 17)) & 0x7FU) + 1;
    }
    string to_str() const {
        stringstream ss;
        ss << "< N=" << n() << " S=";
        if (twos_low() != twos()) {
            if (twos_low() & 1)
                ss << twos_low() << "/2~";
            else
                ss << (twos_low() >> 1) << "~";
        }
        if (twos() & 1)
            ss << twos() << "/2";
        else
            ss << (twos() >> 1);
        ss << " PG=" << pg() << " >";
        return ss.str();
    }
    friend ostream &operator<<(ostream &os, SpinLabel c) {
        os << c.to_str();
        return os;
    }
};

enum struct OpTypes { Zero, Elem, Prod, Sum };

struct OpExpr {
    virtual const OpTypes get_type() const { return OpTypes::Zero; }
    bool operator==(const OpExpr &other) const { return true; }
};

struct OpElement : OpExpr {
    OpNames name;
    vector<uint8_t> site_index;
    double factor;
    SpinLabel q_label;
    OpElement(OpNames name, const vector<uint8_t> &site_index,
              SpinLabel q_label, double factor = 1.0)
        : name(name), site_index(site_index), factor(factor), q_label(q_label) {
    }
    const OpTypes get_type() const override { return OpTypes::Elem; }
    OpElement abs() const { return OpElement(name, site_index, q_label, 1.0); }
    OpElement operator*(double d) const {
        return OpElement(name, site_index, q_label, factor * d);
    }
    bool operator==(const OpElement &other) const {
        return name == other.name && site_index == other.site_index &&
               factor == other.factor;
    }
    bool operator<(const OpElement &other) const {
        if (name != other.name)
            return name < other.name;
        else if (site_index != other.site_index)
            return site_index < other.site_index;
        else if (factor != other.factor)
            return factor < other.factor;
        else
            return false;
    }
    size_t hash() const noexcept {
        size_t h = (size_t)name;
        for (auto r : site_index)
            h ^= (size_t)r + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<double>{}(factor) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
    friend ostream &operator<<(ostream &os, const OpElement &c) {
        if (c.factor != 1.0)
            os << "(" << c.factor << " " << c.abs() << ")";
        else if (c.site_index.size() == 0)
            os << c.name;
        else if (c.site_index.size() == 1)
            os << c.name << (int)c.site_index[0];
        else {
            os << c.name << "[ ";
            for (auto r : c.site_index)
                os << (int)r << " ";
            os << "]";
        }
        return os;
    }
};

struct OpString : OpExpr {
    double factor;
    vector<shared_ptr<OpElement>> ops;
    OpString(const vector<shared_ptr<OpElement>> &ops, double factor)
        : factor(factor), ops() {
        for (auto &elem : ops) {
            this->factor *= elem->factor;
            this->ops.push_back(make_shared<OpElement>(elem->abs()));
        }
    }
    const OpTypes get_type() const override { return OpTypes::Prod; }
    OpString abs() const { return OpString(ops, 1.0); }
    shared_ptr<OpElement> get_op() const {
        assert(ops.size() == 1);
        return ops[0];
    }
    OpString operator*(double d) const { return OpString(ops, factor * d); }
    bool operator==(const OpString &other) const {
        if (ops.size() != other.ops.size() || factor != other.factor)
            return false;
        for (size_t i = 0; i < ops.size(); i++)
            if (!(*ops[i] == *other.ops[i]))
                return false;
        return true;
    }
    friend ostream &operator<<(ostream &os, const OpString &c) {
        if (c.factor != 1.0)
            os << "(" << c.factor << " " << c.abs() << ")";
        else {
            for (auto r : c.ops)
                os << *r << " ";
        }
        return os;
    }
};

struct OpSum : OpExpr {
    vector<shared_ptr<OpString>> strings;
    OpSum(const vector<shared_ptr<OpString>> &strings) : strings(strings) {}
    const OpTypes get_type() const override { return OpTypes::Sum; }
    OpSum operator*(double d) const {
        vector<shared_ptr<OpString>> strs;
        strs.reserve(strings.size());
        for (auto &r : strings)
            strs.push_back(make_shared<OpString>(*r * d));
        return OpSum(strs);
    }
    OpSum abs() const {
        vector<shared_ptr<OpString>> strs;
        strs.reserve(strings.size());
        for (auto &r : strings)
            strs.push_back(make_shared<OpString>(r->abs()));
        return OpSum(strs);
    }
    bool operator==(const OpSum &other) const {
        if (strings.size() != other.strings.size())
            return false;
        for (size_t i = 0; i < strings.size(); i++)
            if (!(*strings[i] == *other.strings[i]))
                return false;
        return true;
    }
    friend ostream &operator<<(ostream &os, const OpSum &c) {
        for (size_t i = 0; i < c.strings.size() - 1; i++)
            os << *c.strings[i] << " + ";
        os << *c.strings.back();
        return os;
    }
};

inline size_t hash_value(const shared_ptr<OpExpr> &x) {
    assert(x->get_type() == OpTypes::Elem);
    return dynamic_pointer_cast<OpElement>(x)->hash();
}

inline shared_ptr<OpExpr> abs_value(const shared_ptr<OpExpr> &x) {
    if (x->get_type() == OpTypes::Zero)
        return x;
    else if (x->get_type() == OpTypes::Elem)
        return make_shared<OpElement>(
            dynamic_pointer_cast<OpElement>(x)->abs());
    else if (x->get_type() == OpTypes::Prod)
        return make_shared<OpString>(dynamic_pointer_cast<OpString>(x)->abs());
    else if (x->get_type() == OpTypes::Sum)
        return make_shared<OpSum>(dynamic_pointer_cast<OpSum>(x)->abs());
}

inline string to_str(const shared_ptr<OpExpr> &x) {
    stringstream ss;
    if (x->get_type() == OpTypes::Zero)
        ss << 0;
    else if (x->get_type() == OpTypes::Elem)
        ss << *dynamic_pointer_cast<OpElement>(x);
    else if (x->get_type() == OpTypes::Prod)
        ss << *dynamic_pointer_cast<OpString>(x);
    else if (x->get_type() == OpTypes::Sum)
        ss << *dynamic_pointer_cast<OpSum>(x);
    return ss.str();
}

inline bool operator==(const shared_ptr<OpExpr> &a,
                       const shared_ptr<OpExpr> &b) {
    if (a->get_type() != b->get_type())
        return false;
    else if (a->get_type() == OpTypes::Zero)
        return *a == *b;
    else if (a->get_type() == OpTypes::Elem)
        return *dynamic_pointer_cast<OpElement>(a) ==
               *dynamic_pointer_cast<OpElement>(b);
    else if (a->get_type() == OpTypes::Prod)
        return *dynamic_pointer_cast<OpString>(a) ==
               *dynamic_pointer_cast<OpString>(b);
    else if (a->get_type() == OpTypes::Sum)
        return *dynamic_pointer_cast<OpSum>(a) ==
               *dynamic_pointer_cast<OpSum>(b);
    else
        return false;
}

struct op_expr_less {
    bool operator()(const shared_ptr<OpExpr> &a,
                    const shared_ptr<OpExpr> &b) const {
        assert(a->get_type() == OpTypes::Elem &&
               b->get_type() == OpTypes::Elem);
        return *dynamic_pointer_cast<OpElement>(a) <
               *dynamic_pointer_cast<OpElement>(b);
    }
};

inline const shared_ptr<OpExpr> operator+(const shared_ptr<OpExpr> &a,
                                          const shared_ptr<OpExpr> &b) {
    if (a->get_type() == OpTypes::Zero)
        return b;
    else if (b->get_type() == OpTypes::Zero)
        return a;
    else if (a->get_type() == OpTypes::Elem) {
        if (b->get_type() == OpTypes::Elem) {
            vector<shared_ptr<OpString>> strs;
            strs.push_back(make_shared<OpString>(
                vector<shared_ptr<OpElement>>(
                    1, dynamic_pointer_cast<OpElement>(a)),
                1.0));
            strs.push_back(make_shared<OpString>(
                vector<shared_ptr<OpElement>>(
                    1, dynamic_pointer_cast<OpElement>(b)),
                1.0));
            return make_shared<OpSum>(strs);
        } else if (b->get_type() == OpTypes::Sum) {
            vector<shared_ptr<OpString>> strs;
            strs.reserve(dynamic_pointer_cast<OpSum>(b)->strings.size() + 1);
            strs.push_back(make_shared<OpString>(
                vector<shared_ptr<OpElement>>(
                    1, dynamic_pointer_cast<OpElement>(a)),
                1.0));
            for (auto &r : dynamic_pointer_cast<OpSum>(b)->strings)
                strs.push_back(r);
            return make_shared<OpSum>(strs);
        }
    } else if (a->get_type() == OpTypes::Prod) {
        if (b->get_type() == OpTypes::Sum) {
            vector<shared_ptr<OpString>> strs;
            strs.reserve(dynamic_pointer_cast<OpSum>(b)->strings.size() + 1);
            strs.push_back(dynamic_pointer_cast<OpString>(a));
            for (auto &r : dynamic_pointer_cast<OpSum>(b)->strings)
                strs.push_back(r);
            return make_shared<OpSum>(strs);
        } else if (b->get_type() == OpTypes::Prod) {
            vector<shared_ptr<OpString>> strs;
            strs.reserve(2);
            strs.push_back(dynamic_pointer_cast<OpString>(a));
            strs.push_back(dynamic_pointer_cast<OpString>(b));
            return make_shared<OpSum>(strs);
        }
    } else if (a->get_type() == OpTypes::Sum) {
        if (b->get_type() == OpTypes::Elem) {
            vector<shared_ptr<OpString>> strs;
            strs.reserve(dynamic_pointer_cast<OpSum>(a)->strings.size() + 1);
            for (auto &r : dynamic_pointer_cast<OpSum>(a)->strings)
                strs.push_back(r);
            strs.push_back(make_shared<OpString>(
                vector<shared_ptr<OpElement>>(
                    1, dynamic_pointer_cast<OpElement>(b)),
                1.0));
            return make_shared<OpSum>(strs);
        } else if (b->get_type() == OpTypes::Prod) {
            vector<shared_ptr<OpString>> strs;
            strs.reserve(dynamic_pointer_cast<OpSum>(a)->strings.size() + 1);
            for (auto &r : dynamic_pointer_cast<OpSum>(a)->strings)
                strs.push_back(r);
            strs.push_back(dynamic_pointer_cast<OpString>(b));
            return make_shared<OpSum>(strs);
        } else if (b->get_type() == OpTypes::Sum) {
            vector<shared_ptr<OpString>> strs;
            strs.reserve(dynamic_pointer_cast<OpSum>(a)->strings.size() +
                         dynamic_pointer_cast<OpSum>(b)->strings.size());
            for (auto &r : dynamic_pointer_cast<OpSum>(a)->strings)
                strs.push_back(r);
            for (auto &r : dynamic_pointer_cast<OpSum>(b)->strings)
                strs.push_back(r);
            return make_shared<OpSum>(strs);
        }
    }
}

inline const shared_ptr<OpExpr> operator*(const shared_ptr<OpExpr> &x,
                                          double d) {
    if (x->get_type() == OpTypes::Zero)
        return x;
    else if (d == 0.0)
        return make_shared<OpExpr>();
    else if (x->get_type() == OpTypes::Elem)
        return make_shared<OpElement>(*dynamic_pointer_cast<OpElement>(x) * d);
    else if (x->get_type() == OpTypes::Prod)
        return make_shared<OpString>(*dynamic_pointer_cast<OpString>(x) * d);
    else if (x->get_type() == OpTypes::Sum)
        return make_shared<OpSum>(*dynamic_pointer_cast<OpSum>(x) * d);
}

inline const shared_ptr<OpExpr> operator*(double d,
                                          const shared_ptr<OpExpr> &x) {
    return x * d;
}

inline const shared_ptr<OpExpr> operator*(const shared_ptr<OpExpr> &a,
                                          const shared_ptr<OpExpr> &b) {
    if (a->get_type() == OpTypes::Zero)
        return a;
    else if (b->get_type() == OpTypes::Zero)
        return b;
    else if (a->get_type() == OpTypes::Elem) {
        if (b->get_type() == OpTypes::Sum) {
            vector<shared_ptr<OpString>> strs;
            for (auto &r : dynamic_pointer_cast<OpSum>(b)->strings) {
                vector<shared_ptr<OpElement>> ops;
                ops.reserve(r->ops.size() + 1);
                ops.push_back(dynamic_pointer_cast<OpElement>(a));
                for (auto &rr : r->ops)
                    ops.push_back(rr);
                strs.push_back(make_shared<OpString>(ops, r->factor));
            }
            return make_shared<OpSum>(strs);
        } else if (b->get_type() == OpTypes::Elem) {
            vector<shared_ptr<OpElement>> ops;
            ops.push_back(dynamic_pointer_cast<OpElement>(a));
            ops.push_back(dynamic_pointer_cast<OpElement>(b));
            return make_shared<OpString>(ops, 1.0);
        } else if (b->get_type() == OpTypes::Prod) {
            vector<shared_ptr<OpElement>> ops;
            ops.reserve(dynamic_pointer_cast<OpString>(b)->ops.size() + 1);
            ops.push_back(dynamic_pointer_cast<OpElement>(a));
            for (auto &r : dynamic_pointer_cast<OpString>(b)->ops)
                ops.push_back(r);
            return make_shared<OpString>(
                ops, dynamic_pointer_cast<OpString>(b)->factor);
        }
    } else if (a->get_type() == OpTypes::Prod) {
        if (b->get_type() == OpTypes::Elem) {
            vector<shared_ptr<OpElement>> ops;
            ops.reserve(dynamic_pointer_cast<OpString>(a)->ops.size() + 1);
            for (auto &r : dynamic_pointer_cast<OpString>(a)->ops)
                ops.push_back(r);
            ops.push_back(dynamic_pointer_cast<OpElement>(b));
            return make_shared<OpString>(
                ops, dynamic_pointer_cast<OpString>(a)->factor);
        } else if (b->get_type() == OpTypes::Prod) {
            vector<shared_ptr<OpElement>> ops;
            ops.reserve(dynamic_pointer_cast<OpString>(a)->ops.size() +
                        dynamic_pointer_cast<OpString>(b)->ops.size());
            for (auto &r : dynamic_pointer_cast<OpString>(a)->ops)
                ops.push_back(r);
            for (auto &r : dynamic_pointer_cast<OpString>(b)->ops)
                ops.push_back(r);
            return make_shared<OpString>(
                ops, dynamic_pointer_cast<OpString>(a)->factor *
                         dynamic_pointer_cast<OpString>(b)->factor);
        }
    } else if (a->get_type() == OpTypes::Sum) {
        if (b->get_type() == OpTypes::Elem) {
            vector<shared_ptr<OpString>> strs;
            for (auto &r : dynamic_pointer_cast<OpSum>(a)->strings) {
                vector<shared_ptr<OpElement>> ops;
                ops.reserve(r->ops.size() + 1);
                for (auto &rr : r->ops)
                    ops.push_back(rr);
                ops.push_back(dynamic_pointer_cast<OpElement>(b));
                strs.push_back(make_shared<OpString>(ops, r->factor));
            }
            return make_shared<OpSum>(strs);
        }
    }
}

inline const shared_ptr<OpExpr> sum(const vector<shared_ptr<OpExpr>> &xs) {
    vector<shared_ptr<OpString>> strs;
    for (auto &r : xs)
        if (r->get_type() == OpTypes::Prod)
            strs.push_back(dynamic_pointer_cast<OpString>(r));
        else if (r->get_type() == OpTypes::Elem)
            strs.push_back(make_shared<OpString>(
                vector<shared_ptr<OpElement>>(
                    1, dynamic_pointer_cast<OpElement>(r)),
                1.0));
        else if (r->get_type() == OpTypes::Sum) {
            strs.reserve(dynamic_pointer_cast<OpSum>(r)->strings.size() +
                         strs.size());
            for (auto &rr : dynamic_pointer_cast<OpSum>(r)->strings)
                strs.push_back(rr);
        }
    return make_shared<OpSum>(strs);
}

inline const shared_ptr<OpExpr>
dot_product(const vector<shared_ptr<OpExpr>> &a,
            const vector<shared_ptr<OpExpr>> &b) {
    vector<shared_ptr<OpExpr>> xs;
    assert(a.size() == b.size());
    for (size_t k = 0; k < a.size(); k++)
        xs.push_back(a[k] * b[k]);
    return sum(xs);
}

inline ostream &operator<<(ostream &os, const shared_ptr<OpExpr> &c) {
    os << to_str(c);
    return os;
}

} // namespace block2

namespace std {

template <> struct hash<block2::SZLabel> {
    size_t operator()(const block2::SZLabel &s) const noexcept {
        return s.hash();
    }
};

template <> struct less<block2::SZLabel> {
    bool operator()(const block2::SZLabel &lhs,
                    const block2::SZLabel &rhs) const noexcept {
        return lhs < rhs;
    }
};

inline void swap(block2::SZLabel &a, block2::SZLabel &b) {
    a.data ^= b.data, b.data ^= a.data, a.data ^= b.data;
}

template <> struct hash<block2::SpinLabel> {
    size_t operator()(const block2::SpinLabel &s) const noexcept {
        return s.hash();
    }
};

template <> struct less<block2::SpinLabel> {
    bool operator()(const block2::SpinLabel &lhs,
                    const block2::SpinLabel &rhs) const noexcept {
        return lhs < rhs;
    }
};

inline void swap(block2::SpinLabel &a, block2::SpinLabel &b) {
    a.data ^= b.data, b.data ^= a.data, a.data ^= b.data;
}

template <> struct hash<block2::OpElement> {
    size_t operator()(const block2::OpElement &s) const noexcept {
        return s.hash();
    }
};

} // namespace std

namespace block2 {

enum struct SymTypes { RVec, CVec, Mat };

struct Symbolic {
    int m, n;
    vector<shared_ptr<OpExpr>> data;
    Symbolic(int m, int n) : m(m), n(n), data(){};
    virtual const SymTypes get_type() const = 0;
    virtual shared_ptr<OpExpr> &operator[](const initializer_list<int> ix) = 0;
};

struct SymbolicRowVector : Symbolic {
    SymbolicRowVector(int n) : Symbolic(1, n) {
        data = vector<shared_ptr<OpExpr>>(n, make_shared<OpExpr>());
    }
    const SymTypes get_type() const override { return SymTypes::RVec; }
    shared_ptr<OpExpr> &operator[](int i) { return data[i]; }
    shared_ptr<OpExpr> &operator[](const initializer_list<int> ix) override {
        auto i = ix.begin();
        return (*this)[*(++i)];
    }
};

struct SymbolicColumnVector : Symbolic {
    SymbolicColumnVector(int n) : Symbolic(n, 1) {
        data = vector<shared_ptr<OpExpr>>(n, make_shared<OpExpr>());
    }
    const SymTypes get_type() const override { return SymTypes::CVec; }
    shared_ptr<OpExpr> &operator[](int i) { return data[i]; }
    shared_ptr<OpExpr> &operator[](const initializer_list<int> ix) override {
        return (*this)[*ix.begin()];
    }
};

struct SymbolicMatrix : Symbolic {
    vector<pair<int, int>> indices;
    SymbolicMatrix(int m, int n) : Symbolic(m, n) {}
    const SymTypes get_type() const override { return SymTypes::Mat; }
    void add(int i, int j, const shared_ptr<OpExpr> elem) {
        indices.push_back(make_pair(i, j));
        data.push_back(elem);
    }
    shared_ptr<OpExpr> &operator[](const initializer_list<int> ix) override {
        auto j = ix.begin(), i = j++;
        add(*i, *j, make_shared<OpExpr>());
        return data.back();
    }
};

inline const shared_ptr<Symbolic> operator*(const shared_ptr<Symbolic> a,
                                            const shared_ptr<Symbolic> b) {
    assert(a->n == b->m);
    if (a->get_type() == SymTypes::RVec && b->get_type() == SymTypes::Mat) {
        shared_ptr<SymbolicRowVector> r(make_shared<SymbolicRowVector>(b->n));
        vector<pair<int, int>> &idx =
            dynamic_pointer_cast<SymbolicMatrix>(b)->indices;
        vector<shared_ptr<OpExpr>> xs[b->n];
        for (size_t k = 0; k < b->data.size(); k++) {
            int i = idx[k].first, j = idx[k].second;
            xs[j].push_back(a->data[i] * b->data[k]);
        }
        for (size_t j = 0; j < b->n; j++)
            (*r)[j] = sum(xs[j]);
        return r;
    } else if (a->get_type() == SymTypes::Mat &&
               b->get_type() == SymTypes::CVec) {
        shared_ptr<SymbolicColumnVector> r(
            make_shared<SymbolicColumnVector>(a->m));
        vector<pair<int, int>> &idx =
            dynamic_pointer_cast<SymbolicMatrix>(a)->indices;
        vector<shared_ptr<OpExpr>> xs[a->m];
        for (size_t k = 0; k < a->data.size(); k++) {
            int i = idx[k].first, j = idx[k].second;
            xs[i].push_back(a->data[k] * b->data[j]);
        }
        for (size_t i = 0; i < a->m; i++)
            (*r)[i] = sum(xs[i]);
        return r;
    } else if (a->get_type() == SymTypes::RVec &&
               b->get_type() == SymTypes::CVec) {
        shared_ptr<SymbolicColumnVector> r(
            make_shared<SymbolicColumnVector>(1));
        (*r)[0] = dot_product(a->data, b->data);
        return r;
    }
}

struct StateInfo {
    SpinLabel *quanta;
    uint16_t *n_states;
    int n_states_total, n;
    StateInfo() : quanta(0), n_states(0), n_states_total(0), n(0) {}
    StateInfo(SpinLabel q) {
        allocate(1);
        quanta[0] = q, n_states[0] = 1, n_states_total = 1;
    }
    // need length * 2
    void allocate(int length, uint32_t *ptr = 0) {
        if (ptr == 0)
            ptr = ialloc->allocate((length << 1) - (length >> 1));
        n = length;
        quanta = (SpinLabel *)ptr;
        n_states = (uint16_t *)(ptr + length);
    }
    void reallocate(int length) {
        uint32_t *ptr =
            ialloc->reallocate((uint32_t *)quanta, (n << 1) - (n >> 1),
                               (length << 1) - (length >> 1));
        if (ptr == (uint32_t *)quanta) {
            memmove(ptr + length, n_states, length * sizeof(uint16_t));
            n_states = (uint16_t *)(quanta + length);
        } else {
            memmove(ptr, quanta, length * sizeof(uint32_t));
            memmove(ptr + length, n_states, length * sizeof(uint16_t));
            quanta = (SpinLabel *)ptr;
            n_states = (uint16_t *)(quanta + length);
        }
        n = length;
    }
    void deallocate() {
        assert(n != 0);
        ialloc->deallocate((uint32_t *)quanta, (n << 1) - (n >> 1));
        quanta = 0;
        n_states = 0;
    }
    StateInfo deep_copy() const {
        StateInfo other;
        other.allocate(n);
        copy_data_to(other);
        other.n_states_total = n_states_total;
        return other;
    }
    void copy_data_to(StateInfo &other) const {
        assert(other.n == n);
        memcpy(other.quanta, quanta, ((n << 1) - (n >> 1)) * sizeof(uint32_t));
    }
    void sort_states() {
        int idx[n];
        SpinLabel q[n];
        uint16_t nq[n];
        memcpy(q, quanta, n * sizeof(SpinLabel));
        memcpy(nq, n_states, n * sizeof(uint16_t));
        for (int i = 0; i < n; i++)
            idx[i] = i;
        sort(idx, idx + n, [&q](int i, int j) { return q[i] < q[j]; });
        for (int i = 0; i < n; i++)
            quanta[i] = q[idx[i]], n_states[i] = nq[idx[i]];
        n_states_total = 0;
        for (int i = 0; i < n; i++)
            n_states_total += n_states[i];
    }
    void collect(SpinLabel target = 0x7FFFFFFF) {
        int k = -1;
        int nn = upper_bound(quanta, quanta + n, target) - quanta;
        for (int i = 0; i < nn; i++)
            if (n_states[i] == 0)
                continue;
            else if (k != -1 && quanta[i] == quanta[k])
                n_states[k] =
                    (uint16_t)min((uint32_t)n_states[k] + n_states[i], 65535U);
            else {
                k++;
                quanta[k] = quanta[i];
                n_states[k] = n_states[i];
            }
        reallocate(k + 1);
        n_states_total = 0;
        for (int i = 0; i < n; i++)
            n_states_total += n_states[i];
    }
    int find_state(SpinLabel q) const {
        auto p = lower_bound(quanta, quanta + n, q);
        if (p == quanta + n || *p != q)
            return -1;
        else
            return p - quanta;
    }
    static StateInfo tensor_product(const StateInfo &a, const StateInfo &b,
                                    SpinLabel target) {
        int nc = 0;
        for (int i = 0; i < a.n; i++)
            for (int j = 0; j < b.n; j++)
                nc += (a.quanta[i] + b.quanta[j]).count();
        StateInfo c;
        c.allocate(nc);
        for (int i = 0, ic = 0; i < a.n; i++)
            for (int j = 0; j < b.n; j++) {
                SpinLabel qc = a.quanta[i] + b.quanta[j];
                for (int k = 0; k < qc.count(); k++) {
                    c.quanta[ic + k] = qc[k];
                    uint32_t nprod =
                        (uint32_t)a.n_states[i] * (uint32_t)b.n_states[j];
                    c.n_states[ic + k] = (uint16_t)min(nprod, 65535U);
                }
                ic += qc.count();
            }
        c.sort_states();
        c.collect(target);
        return c;
    }
    static StateInfo filter(StateInfo &a, StateInfo &b, SpinLabel target) {
        a.n_states_total = 0;
        for (int i = 0; i < a.n; i++) {
            SpinLabel qb = target - a.quanta[i];
            int x = 0;
            for (int k = 0; k < qb.count(); k++) {
                int idx = b.find_state(qb[k]);
                x += idx == -1 ? 0 : b.n_states[idx];
            }
            a.n_states[i] = (uint16_t)min(x, (int)a.n_states[i]);
            a.n_states_total += a.n_states[i];
        }
        b.n_states_total = 0;
        for (int i = 0; i < b.n; i++) {
            SpinLabel qa = target - b.quanta[i];
            int x = 0;
            for (int k = 0; k < qa.count(); k++) {
                int idx = a.find_state(qa[k]);
                x += idx == -1 ? 0 : a.n_states[idx];
            }
            b.n_states[i] = (uint16_t)min(x, (int)b.n_states[i]);
            b.n_states_total += b.n_states[i];
        }
    }
    friend ostream &operator<<(ostream &os, const StateInfo &c) {
        for (int i = 0; i < c.n; i++)
            os << c.quanta[i].to_str() << " : " << c.n_states[i] << endl;
        return os;
    }
};

struct SparseMatrixInfo {
    SpinLabel *quanta;
    uint16_t *n_states_bra, *n_states_ket;
    uint32_t *n_states_total;
    SpinLabel delta_quantum;
    bool is_fermion;
    bool is_wavefunction;
    int n;
    SparseMatrixInfo() : n(-1) {}
    void initialize(const StateInfo &bra, const StateInfo &ket, SpinLabel dq,
                    bool is_fermion, bool wfn = false) {
        this->is_fermion = is_fermion;
        this->is_wavefunction = wfn;
        delta_quantum = dq;
        vector<SpinLabel> qs;
        qs.reserve(ket.n);
        for (int i = 0; i < ket.n; i++) {
            SpinLabel q = wfn ? -ket.quanta[i] : ket.quanta[i];
            SpinLabel bs = dq + q;
            for (int k = 0; k < bs.count(); k++)
                if (bra.find_state(bs[k]) != -1) {
                    q.set_twos_low(bs[k].twos());
                    qs.push_back(q);
                }
        }
        n = qs.size();
        allocate(n);
        if (n != 0) {
            memcpy(quanta, &qs[0], n * sizeof(SpinLabel));
            sort(quanta, quanta + n);
            for (int i = 0; i < n; i++) {
                n_states_ket[i] = ket.n_states[ket.find_state(
                    wfn ? -quanta[i].get_ket() : quanta[i].get_ket())];
                n_states_bra[i] =
                    bra.n_states[bra.find_state(quanta[i].get_bra(dq))];
            }
            n_states_total[0] = 0;
            for (int i = 0; i < n - 1; i++)
                n_states_total[i + 1] =
                    n_states_total[i] +
                    (uint32_t)n_states_bra[i] * n_states_ket[i];
        }
    }
    int find_state(SpinLabel q, int start = 0) const {
        auto p = lower_bound(quanta + start, quanta + n, q);
        if (p == quanta + n || *p != q)
            return -1;
        else
            return p - quanta;
    }
    uint32_t get_total_memory() const {
        return n == 0 ? 0
                      : n_states_total[n - 1] +
                            (uint32_t)n_states_bra[n - 1] * n_states_ket[n - 1];
    }
    void allocate(int length, uint32_t *ptr = 0) {
        if (ptr == 0)
            ptr = ialloc->allocate((length << 1) + length);
        quanta = (SpinLabel *)ptr;
        n_states_bra = (uint16_t *)(ptr + length);
        n_states_ket = (uint16_t *)(ptr + length) + length;
        n_states_total = ptr + (length << 1);
        n = length;
    }
    void deallocate() {
        assert(n != -1);
        ialloc->deallocate((uint32_t *)quanta, (n << 1) + n);
        quanta = nullptr;
        n_states_bra = nullptr;
        n_states_ket = nullptr;
        n_states_total = nullptr;
        n = -1;
    }
    void reallocate(int length) {
        uint32_t *ptr = ialloc->reallocate((uint32_t *)quanta, (n << 1) + n,
                                           (length << 1) + length);
        if (ptr == (uint32_t *)quanta)
            memmove(ptr + length, n_states_bra,
                    (length << 1) * sizeof(uint32_t));
        else {
            memmove(ptr, quanta, ((length << 1) + length) * sizeof(uint32_t));
            quanta = (SpinLabel *)ptr;
        }
        n_states_bra = (uint16_t *)(ptr + length);
        n_states_ket = (uint16_t *)(ptr + length) + length;
        n_states_total = ptr + (length << 1);
        n = length;
    }
    friend ostream &operator<<(ostream &os, const SparseMatrixInfo &c) {
        os << "DQ=" << c.delta_quantum << " N=" << c.n
           << " SIZE=" << c.get_total_memory() << endl;
        for (int i = 0; i < c.n; i++)
            os << "BRA " << c.quanta[i].get_bra(c.delta_quantum) << " KET "
               << c.quanta[i].get_ket() << " [ " << (int)c.n_states_bra[i]
               << "x" << (int)c.n_states_ket[i] << " ]" << endl;
        return os;
    }
};

struct SparseMatrix {
    shared_ptr<SparseMatrixInfo> info;
    double *data;
    double factor;
    size_t total_memory;
    bool conj;
    SparseMatrix()
        : info(nullptr), data(nullptr), factor(1.0), total_memory(0),
          conj(false) {}
    void copy_data(const SparseMatrix &other) {
        assert(total_memory == other.total_memory);
        memcpy(data, other.data, sizeof(double) * total_memory);
    }
    void allocate(const shared_ptr<SparseMatrixInfo> &info, double *ptr = 0) {
        this->info = info;
        total_memory = info->get_total_memory();
        if (total_memory == 0)
            return;
        if (ptr == 0) {
            data = dalloc->allocate(total_memory);
            memset(data, 0, sizeof(double) * total_memory);
        } else
            data = ptr;
    }
    void deallocate() {
        if (total_memory == 0) {
            assert(data == nullptr);
            return;
        }
        dalloc->deallocate(data, total_memory);
        total_memory = 0;
        data = nullptr;
    }
    MatrixRef operator[](SpinLabel q) const {
        return (*this)[info->find_state(q)];
    }
    MatrixRef operator[](int idx) const {
        assert(idx != -1);
        return MatrixRef(data + info->n_states_total[idx],
                         info->n_states_bra[idx], info->n_states_ket[idx]);
    }
    friend ostream &operator<<(ostream &os, const SparseMatrix &c) {
        os << "DATA = [ ";
        for (int i = 0; i < c.total_memory; i++)
            os << setw(20) << setprecision(14) << c.data[i] << " ";
        os << "]" << endl;
        return os;
    }
};

extern "C" {

// vector scale
// vector [sx] = double [sa] * vector [sx]
extern void dscal(const int *n, const double *sa, const double *sx,
                  const int *incx);

// vector addition
// vector [sy] = vector [sy] + double [sa] * vector [sx]
extern void daxpy(const int *n, const double *sa, const double *sx,
                  const int *incx, double *sy, const int *incy);

// matrix multiplication
// mat [c] = double [alpha] * mat [a] * mat [b] + double [beta] * mat [c]
extern void dgemm(const char *transa, const char *transb, const int *n,
                  const int *m, const int *k, const double *alpha,
                  const double *a, const int *lda, const double *b,
                  const int *ldb, const double *beta, double *c,
                  const int *ldc);
}

struct MatrixFunctions {
    static void iscale(const MatrixRef &a, double scale) {
        int n = a.m * a.n, inc = 1;
        dscal(&n, &scale, a.data, &inc);
    }
    static void iadd(const MatrixRef &a, const MatrixRef &b, double scale) {
        assert(a.m == b.m && a.n == b.n);
        int n = a.m * a.n, inc = 1;
        daxpy(&n, &scale, b.data, &inc, a.data, &inc);
    }
    static void multiply(const MatrixRef &a, bool conja, const MatrixRef &b,
                         bool conjb, const MatrixRef &c, double scale,
                         double cfactor) {
        if (!conja and !conjb) {
            assert(a.n == b.m && c.m == a.m && c.n == b.n);
            dgemm("n", "n", &b.n, &a.m, &b.m, &scale, b.data, &b.n, a.data,
                  &a.n, &cfactor, c.data, &b.n);
        } else
            assert(false);
    }
    static void tensor_product(const MatrixRef &a, bool conja,
                               const MatrixRef &b, bool conjb,
                               const MatrixRef &c, double scale,
                               uint32_t stride) {
        if (!conja and !conjb) {
            for (int i = 0, inc = 1; i < a.m; i++)
                for (int j = 0; j < a.n; j++) {
                    double factor = scale * a(i, j);
                    for (int k = 0; k < b.m; k++)
                        daxpy(&b.n, &factor, &b(k, 0), &inc,
                              &c(i * b.m + k, j * b.n + stride), &inc);
                }
        } else
            assert(false);
    }
};

struct OperatorFunctions {
    shared_ptr<CG> cg;
    OperatorFunctions(const shared_ptr<CG> &cg) : cg(cg) {}
    // a += b * scale
    void iadd(SparseMatrix &a, const SparseMatrix &b, double scale = 1.0) {
        assert(a.info->n == b.info->n && a.total_memory == b.total_memory);
        if (a.factor != 1.0) {
            MatrixFunctions::iscale(MatrixRef(a.data, 1, a.total_memory),
                                    1.0 / a.factor);
            a.factor = 1.0;
        }
        if (scale != 0.0)
            MatrixFunctions::iadd(MatrixRef(a.data, 1, a.total_memory),
                                  MatrixRef(b.data, 1, b.total_memory),
                                  scale * b.factor);
    }
    void tensor_product(const SparseMatrix &a, const SparseMatrix &b,
                        SparseMatrix &c, double scale = 1.0) {
        scale = scale * a.factor * b.factor;
        assert(c.factor == 1.0);
        if (abs(scale) < TINY)
            return;
        SpinLabel adq = a.info->delta_quantum, bdq = b.info->delta_quantum,
                  cdq = c.info->delta_quantum;
        assert(b.info->n <= 3);
        int adqs = adq.twos(), bdqs = bdq.twos(), cdqs = cdq.twos();
        for (int ic = 0; ic < c.info->n; ic++) {
            SpinLabel cq = c.info->quanta[ic].get_bra(cdq);
            SpinLabel cqprime = c.info->quanta[ic].get_ket();
            uint32_t row_stride = 0, col_stride = 0;
            for (int ib = 0; ib < b.info->n; ib++) {
                SpinLabel bq = b.info->quanta[ib].get_bra(bdq);
                SpinLabel bqprime = b.info->quanta[ib].get_ket();
                SpinLabel aqs = cq - bq;
                SpinLabel aqps = cqprime - bqprime;
                for (int k = 0; k < aqs.count(); k++) {
                    SpinLabel aq = aqs[k];
                    SpinLabel aqpds = aqs[k] - adq;
                    uint32_t n_bra = 0;
                    for (int l = 0; l < aqpds.count(); l++) {
                        SpinLabel aqprime = aqpds[l];
                        SpinLabel al = adq.combine(aq, aqprime);
                        uint32_t n_ket = 0;
                        if (aqps.find(aqprime) != -1 &&
                            al != SpinLabel(0xFFFFFFFFU)) {
                            int ia = a.info->find_state(al);
                            if (ia != -1) {
                                n_bra = a.info->n_states_bra[ia];
                                n_ket = a.info->n_states_ket[ia];
                                double factor = cg->wigner_9j(
                                    aqprime.twos(), bqprime.twos(),
                                    cqprime.twos(), adqs, bdqs, cdqs, aq.twos(),
                                    bq.twos(), cq.twos());
                                factor *=
                                    (b.info->is_fermion && (aqprime.n() & 1))
                                        ? -1
                                        : 1;
                                MatrixFunctions::tensor_product(
                                    a[ia], a.conj, b[ib], b.conj, c[ic],
                                    scale * factor,
                                    row_stride * c.info->n_states_ket[ic] +
                                        col_stride);
                            }
                        }
                        col_stride += n_ket * b.info->n_states_ket[ib];
                    }
                    row_stride += n_bra * b.info->n_states_bra[ib];
                }
            }
        }
    }
    // c = a * b * scale
    void product(const SparseMatrix &a, const SparseMatrix &b,
                 const SparseMatrix &c, double scale = 1.0) {
        scale = scale * a.factor * b.factor;
        assert(c.factor == 1.0);
        if (abs(scale) < TINY)
            return;
        int adq = a.info->delta_quantum.twos(),
            bdq = b.info->delta_quantum.twos(),
            cdq = c.info->delta_quantum.twos();
        for (int ic = 0; ic < c.info->n; ic++) {
            SpinLabel cq = c.info->quanta[ic].get_bra(c.info->delta_quantum);
            SpinLabel cqprime = c.info->quanta[ic].get_ket();
            SpinLabel aps = cq - a.info->delta_quantum;
            for (int k = 0; k < aps.count(); k++) {
                SpinLabel aqprime = aps[k], ac = aps[k];
                ac.set_twos_low(cq.twos());
                int ia = a.info->find_state(ac);
                if (ia != -1) {
                    SpinLabel bl =
                        b.info->delta_quantum.combine(aqprime, cqprime);
                    if (bl != SpinLabel(0xFFFFFFFFU)) {
                        int ib = b.info->find_state(bl);
                        if (ib != -1) {
                            int aqpj = aqprime.twos(), cqj = cq.twos(),
                                cqpj = cqprime.twos();
                            double factor =
                                cg->racah(cqpj, bdq, cqj, adq, aqpj, cdq);
                            factor *= sqrt((cdq + 1) * (aqpj + 1)) *
                                      (((adq + bdq - cdq) & 2) ? -1 : 1);
                            MatrixFunctions::multiply(a[ia], a.conj, b[ib],
                                                      b.conj, c[ic],
                                                      scale * factor, 1.0);
                        }
                    }
                }
            }
        }
    }
};

struct OperatorTensor {
    shared_ptr<Symbolic> lmat, rmat;
    map<shared_ptr<OpExpr>, shared_ptr<SparseMatrix>, op_expr_less> lop, rop;
    OperatorTensor() {}
};

struct TensorFunctions {
    static void left_contract(const shared_ptr<OperatorTensor> &a,
                              const shared_ptr<OperatorTensor> &b,
                              shared_ptr<OperatorTensor> &c) {
        assert(a->lmat != nullptr);
        assert(a->lmat->get_type() == SymTypes::RVec);
        assert(b->lmat != nullptr);
        assert(b->lmat->get_type() == SymTypes::Mat);
        assert(c->lmat != nullptr);
        assert(c->lmat->get_type() == SymTypes::RVec);
        assert(a->lmat->n == b->lmat->m && b->lmat->n == c->lmat->n);
        shared_ptr<Symbolic> exprs = a->lmat * b->lmat;
        assert(exprs->data.size() == c->lmat->data.size());
        for (size_t i = 0; i < exprs->data.size(); i++) {
            shared_ptr<OpElement> cop =
                dynamic_pointer_cast<OpElement>(c->lmat->data[i]);
            shared_ptr<OpExpr> op = abs_value(cop);
            shared_ptr<OpExpr> expr = exprs->data[i] * (1 / cop->factor);
        }
    }
};

struct MPO {
    vector<shared_ptr<OperatorTensor>> tensors;
    vector<shared_ptr<Symbolic>> left_operator_names;
    vector<shared_ptr<Symbolic>> right_operator_names;
    vector<shared_ptr<Symbolic>> middle_operator_names;
    int n_sites;
    MPO(int n_sites) : n_sites(n_sites) {}
    virtual void deallocate() = 0;
};

struct MPSInfo {
    int n_sites;
    SpinLabel vaccum;
    SpinLabel target;
    uint8_t *orbsym;
    StateInfo *basis, *left_dims_fci, *right_dims_fci;
    StateInfo *left_dims, *right_dims;
    uint16_t bond_dim;
    MPSInfo(int n_sites, SpinLabel vaccum, SpinLabel target, StateInfo *basis,
            uint8_t *orbsym)
        : n_sites(n_sites), vaccum(vaccum), target(target), orbsym(orbsym),
          basis(basis), bond_dim(0) {
        left_dims_fci = new StateInfo[n_sites + 1];
        left_dims_fci[0] = StateInfo(vaccum);
        for (int i = 0; i < n_sites; i++)
            left_dims_fci[i + 1] = StateInfo::tensor_product(
                left_dims_fci[i], basis[orbsym[i]], target);
        right_dims_fci = new StateInfo[n_sites + 1];
        right_dims_fci[n_sites] = StateInfo(vaccum);
        for (int i = n_sites - 1; i >= 0; i--)
            right_dims_fci[i] = StateInfo::tensor_product(
                basis[orbsym[i]], right_dims_fci[i + 1], target);
        for (int i = 0; i <= n_sites; i++)
            StateInfo::filter(left_dims_fci[i], right_dims_fci[i], target);
        for (int i = 0; i <= n_sites; i++)
            left_dims_fci[i].collect();
        for (int i = n_sites; i >= 0; i--)
            right_dims_fci[i].collect();
        left_dims = nullptr;
        right_dims = nullptr;
    }
    void set_bond_dimension(uint16_t m) {
        bond_dim = m;
        left_dims = new StateInfo[n_sites + 1];
        left_dims[0] = StateInfo(vaccum);
        for (int i = 0; i < n_sites; i++)
            left_dims[i + 1] = left_dims_fci[i + 1].deep_copy();
        for (int i = 0; i < n_sites; i++) {
            if (left_dims[i + 1].n_states_total > m) {
                int new_total = 0;
                for (int k = 0; k < left_dims[i + 1].n; k++) {
                    uint32_t new_n_states =
                        (uint32_t)(ceil((double)left_dims[i + 1].n_states[k] *
                                        m / left_dims[i + 1].n_states_total) +
                                   0.1);
                    left_dims[i + 1].n_states[k] =
                        (uint16_t)min(new_n_states, 65535U);
                    new_total += left_dims[i + 1].n_states[k];
                }
                left_dims[i + 1].n_states_total = new_total;
            }
            if (i != n_sites - 1) {
                StateInfo t = StateInfo::tensor_product(
                    left_dims[i + 1], basis[orbsym[i + 1]], target);
                int new_total = 0;
                for (int k = 0; k < left_dims[i + 2].n; k++) {
                    int tk = t.find_state(left_dims[i + 2].quanta[k]);
                    if (tk == -1)
                        left_dims[i + 2].n_states[k] = 0;
                    else if (left_dims[i + 2].n_states[k] > t.n_states[tk])
                        left_dims[i + 2].n_states[k] = t.n_states[tk];
                    new_total += left_dims[i + 2].n_states[k];
                }
                left_dims[i + 2].n_states_total = new_total;
                t.deallocate();
            }
        }
        right_dims = new StateInfo[n_sites + 1];
        right_dims[n_sites] = StateInfo(vaccum);
        for (int i = n_sites - 1; i >= 0; i--)
            right_dims[i] = right_dims_fci[i].deep_copy();
        for (int i = n_sites - 1; i >= 0; i--) {
            if (right_dims[i].n_states_total > m) {
                int new_total = 0;
                for (int k = 0; k < right_dims[i].n; k++) {
                    uint32_t new_n_states =
                        (uint32_t)(ceil((double)right_dims[i].n_states[k] * m /
                                        right_dims[i].n_states_total) +
                                   0.1);
                    right_dims[i].n_states[k] =
                        (uint16_t)min(new_n_states, 65535U);
                    new_total += right_dims[i].n_states[k];
                }
                right_dims[i].n_states_total = new_total;
            }
            if (i != 0) {
                StateInfo t = StateInfo::tensor_product(basis[orbsym[i - 1]],
                                                        right_dims[i], target);
                int new_total = 0;
                for (int k = 0; k < right_dims[i - 1].n; k++) {
                    int tk = t.find_state(right_dims[i - 1].quanta[k]);
                    if (tk == -1)
                        right_dims[i - 1].n_states[k] = 0;
                    else if (right_dims[i - 1].n_states[k] > t.n_states[tk])
                        right_dims[i - 1].n_states[k] = t.n_states[tk];
                    new_total += right_dims[i - 1].n_states[k];
                }
                right_dims[i - 1].n_states_total = new_total;
                t.deallocate();
            }
        }
    }
    void deallocate() {
        if (left_dims != nullptr) {
            for (int i = 0; i <= n_sites; i++)
                right_dims[i].deallocate();
            for (int i = n_sites; i >= 0; i--)
                left_dims[i].deallocate();
        }
        for (int i = 0; i <= n_sites; i++)
            right_dims_fci[i].deallocate();
        for (int i = n_sites; i >= 0; i--)
            left_dims_fci[i].deallocate();
    }
    ~MPSInfo() {
        if (left_dims != nullptr) {
            delete[] left_dims;
            delete[] right_dims;
        }
        delete[] left_dims_fci;
        delete[] right_dims_fci;
    }
};

struct MPS {
    int n_sites, center, dot;
    shared_ptr<MPSInfo> info;
    vector<shared_ptr<SparseMatrixInfo>> mat_infos;
    vector<shared_ptr<SparseMatrix>> tensors;
    string canonical_form;
    MPS(int n_sites, int center, int dot) : center(center), dot(dot) {
        canonical_form.resize(n_sites);
        for (int i = 0; i < center; i++)
            canonical_form[i] = 'L';
        for (int i = center; i < center + dot; i++)
            canonical_form[i] = 'C';
        for (int i = center + dot; i < n_sites; i++)
            canonical_form[i] = 'R';
    }
    void initialize(const shared_ptr<MPSInfo> &info) {
        this->info = info;
        mat_infos.resize(n_sites);
        tensors.resize(n_sites);
        for (int i = 0; i < center; i++) {
            StateInfo t = StateInfo::tensor_product(
                info->left_dims[i], info->basis[info->orbsym[i]], info->target);
            mat_infos[i] = make_shared<SparseMatrixInfo>();
            mat_infos[i]->initialize(t, info->left_dims[i + 1], info->vaccum,
                                     false);
            t.reallocate(0);
            mat_infos[i]->reallocate(mat_infos[i]->n);
        }
        mat_infos[center] = make_shared<SparseMatrixInfo>();
        if (dot == 1) {
            StateInfo t = StateInfo::tensor_product(
                info->left_dims[center], info->basis[info->orbsym[center]],
                info->target);
            mat_infos[center]->initialize(t, info->right_dims[center + dot],
                                          info->target, false, true);
            t.reallocate(0);
            mat_infos[center]->reallocate(mat_infos[center]->n);
        } else {
            StateInfo tl = StateInfo::tensor_product(
                info->left_dims[center], info->basis[info->orbsym[center]],
                info->target);
            StateInfo tr = StateInfo::tensor_product(
                info->basis[info->orbsym[center + 1]],
                info->right_dims[center + dot], info->target);
            mat_infos[center]->initialize(tl, tr, info->target, false, true);
            tl.reallocate(0);
            tr.reallocate(0);
            mat_infos[center]->reallocate(mat_infos[center]->n);
        }
        for (int i = center + dot; i < n_sites; i++) {
            StateInfo t = StateInfo::tensor_product(
                info->basis[info->orbsym[i]], info->right_dims[i + 1],
                info->target);
            mat_infos[i] = make_shared<SparseMatrixInfo>();
            mat_infos[i]->initialize(info->right_dims[i], t, info->vaccum,
                                     false);
            t.reallocate(0);
            mat_infos[i]->reallocate(mat_infos[i]->n);
        }
        for (int i = 0; i < n_sites; i++)
            if (mat_infos[i] != nullptr)
                tensors[i]->allocate(mat_infos[i]);
    }
    void deallocate() {
        for (int i = n_sites - 1; i >= 0; i--)
            if (tensors[i] != nullptr)
                tensors[i]->deallocate();
        for (int i = n_sites - 1; i >= 0; i--)
            if (mat_infos[i] != nullptr)
                mat_infos[i]->deallocate();
    }
};

struct Partition {
    shared_ptr<OperatorTensor> left;
    shared_ptr<OperatorTensor> right;
    vector<shared_ptr<OperatorTensor>> middle;
    Partition(const shared_ptr<OperatorTensor> &left,
              const shared_ptr<OperatorTensor> &right,
              const shared_ptr<OperatorTensor> &dot)
        : left(left), right(right), middle{dot} {}
    Partition(const shared_ptr<OperatorTensor> &left,
              const shared_ptr<OperatorTensor> &right,
              const shared_ptr<OperatorTensor> &ldot,
              const shared_ptr<OperatorTensor> &rdot)
        : left(left), right(right), middle{ldot, rdot} {}
    Partition(const Partition &other)
        : left(other.left), right(other.right), middle(other.middle) {}
};

struct MovingEnvironment {
    int n_sites, center, dot;
    shared_ptr<MPO> mpo;
    vector<shared_ptr<Partition>> envs;
    MovingEnvironment(int n_sites, int center, int dot,
                      const shared_ptr<MPO> &mpo)
        : n_sites(n_sites), center(center), dot(dot), mpo(mpo) {}
    void init_environments() {
        envs.clear();
        envs.resize(n_sites);
        envs[n_sites - 1] =
            make_shared<Partition>(nullptr, nullptr, mpo->tensors[n_sites - 1]);
        if (dot == 2)
            envs[n_sites - 2] = make_shared<Partition>(
                nullptr, nullptr, mpo->tensors[n_sites - 2],
                mpo->tensors[n_sites - 1]);
        for (int i = n_sites - dot - 1; i >= center; i--) {
            envs[i] = make_shared<Partition>(*envs[i + 1]);
            envs[i]->middle.insert(envs[i]->middle.begin(), mpo->tensors[i]);
        }
    }
};

struct Hamiltonian {
    SpinLabel vaccum, target;
    StateInfo *basis;
    vector<pair<SpinLabel, shared_ptr<SparseMatrixInfo>>> *site_op_infos;
    map<OpNames, shared_ptr<SparseMatrix>> op_prims[2];
    vector<pair<shared_ptr<OpExpr>, shared_ptr<SparseMatrix>>> *site_norm_ops;
    uint8_t n_sites, n_syms;
    bool su2;
    shared_ptr<FCIDUMP> fcidump;
    shared_ptr<OperatorFunctions> opf;
    vector<uint8_t> orb_sym;
    Hamiltonian(SpinLabel vaccum, SpinLabel target, int norb, bool su2,
                const shared_ptr<FCIDUMP> &fcidump,
                const vector<uint8_t> &orb_sym)
        : vaccum(vaccum), target(target), n_sites((uint8_t)norb), su2(su2),
          fcidump(fcidump), orb_sym(orb_sym) {
        assert((int)n_sites == norb);
        basis = new StateInfo[n_sites];
        n_syms = *max_element(orb_sym.begin(), orb_sym.end()) + 1;
        if (su2)
            for (int i = 0; i < n_syms; i++) {
                basis[i].allocate(3);
                basis[i].quanta[0] = vaccum;
                basis[i].quanta[1] = SpinLabel(1, 1, i);
                basis[i].quanta[2] = SpinLabel(2, 0, 0);
                basis[i].n_states[0] = basis[i].n_states[1] =
                    basis[i].n_states[2] = 1;
                basis[i].sort_states();
            }
        else
            for (int i = 0; i < n_syms; i++) {
                basis[i].allocate(4);
                basis[i].quanta[0] = vaccum;
                basis[i].quanta[1] = SpinLabel(1, -1, i);
                basis[i].quanta[2] = SpinLabel(1, 1, i);
                basis[i].quanta[3] = SpinLabel(2, 0, 0);
                basis[i].n_states[0] = basis[i].n_states[1] =
                    basis[i].n_states[2] = basis[i].n_states[3] = 1;
                basis[i].sort_states();
            }
        opf = make_shared<OperatorFunctions>(make_shared<CG>(100, 10));
        opf->cg->initialize();
        init_site_ops();
    }
    static uint8_t swap_d2h(uint8_t isym) {
        static uint8_t arr_swap[] = {8, 0, 7, 6, 1, 5, 2, 3, 4};
        return arr_swap[isym];
    }
    void init_site_ops() {
        site_op_infos =
            new vector<pair<SpinLabel, shared_ptr<SparseMatrixInfo>>>[n_syms];
        for (int i = 0; i < n_syms; i++) {
            map<SpinLabel, shared_ptr<SparseMatrixInfo>> info;
            info[vaccum] = nullptr;
            info[SpinLabel(1, 1, i)] = nullptr;
            info[SpinLabel(-1, 1, i)] = nullptr;
            for (int n = -2; n <= 2; n += 2)
                for (int s = 0; s <= 2; s += 2)
                    info[SpinLabel(n, s, 0)] = nullptr;
            site_op_infos[i] =
                vector<pair<SpinLabel, shared_ptr<SparseMatrixInfo>>>(
                    info.begin(), info.end());
            for (auto &p : site_op_infos[i]) {
                p.second = make_shared<SparseMatrixInfo>();
                p.second->initialize(basis[i], basis[i], p.first,
                                     p.first.twos() == 1);
            }
        }
        op_prims[0][OpNames::I] = make_shared<SparseMatrix>();
        op_prims[0][OpNames::I]->allocate(
            find_site_op_info(SpinLabel(0, 0, 0), 0));
        (*op_prims[0][OpNames::I])[SpinLabel(0, 0, 0, 0)](0, 0) = 1.0;
        (*op_prims[0][OpNames::I])[SpinLabel(1, 1, 1, 0)](0, 0) = 1.0;
        (*op_prims[0][OpNames::I])[SpinLabel(2, 0, 0, 0)](0, 0) = 1.0;
        op_prims[0][OpNames::N] = make_shared<SparseMatrix>();
        op_prims[0][OpNames::N]->allocate(
            find_site_op_info(SpinLabel(0, 0, 0), 0));
        (*op_prims[0][OpNames::N])[SpinLabel(0, 0, 0, 0)](0, 0) = 0.0;
        (*op_prims[0][OpNames::N])[SpinLabel(1, 1, 1, 0)](0, 0) = 1.0;
        (*op_prims[0][OpNames::N])[SpinLabel(2, 0, 0, 0)](0, 0) = 2.0;
        op_prims[0][OpNames::NN] = make_shared<SparseMatrix>();
        op_prims[0][OpNames::NN]->allocate(
            find_site_op_info(SpinLabel(0, 0, 0), 0));
        (*op_prims[0][OpNames::NN])[SpinLabel(0, 0, 0, 0)](0, 0) = 0.0;
        (*op_prims[0][OpNames::NN])[SpinLabel(1, 1, 1, 0)](0, 0) = 1.0;
        (*op_prims[0][OpNames::NN])[SpinLabel(2, 0, 0, 0)](0, 0) = 4.0;
        op_prims[0][OpNames::C] = make_shared<SparseMatrix>();
        op_prims[0][OpNames::C]->allocate(
            find_site_op_info(SpinLabel(1, 1, 0), 0));
        (*op_prims[0][OpNames::C])[SpinLabel(0, 1, 0, 0)](0, 0) = 1.0;
        (*op_prims[0][OpNames::C])[SpinLabel(1, 0, 1, 0)](0, 0) = -sqrt(2);
        op_prims[0][OpNames::D] = make_shared<SparseMatrix>();
        op_prims[0][OpNames::D]->allocate(
            find_site_op_info(SpinLabel(-1, 1, 0), 0));
        (*op_prims[0][OpNames::D])[SpinLabel(1, 0, 1, 0)](0, 0) = sqrt(2);
        (*op_prims[0][OpNames::D])[SpinLabel(2, 1, 0, 0)](0, 0) = 1.0;
        for (uint8_t s = 0; s < 2; s++) {
            op_prims[s][OpNames::A] = make_shared<SparseMatrix>();
            op_prims[s][OpNames::A]->allocate(
                find_site_op_info(SpinLabel(2, s * 2, 0), 0));
            opf->product(*op_prims[0][OpNames::C], *op_prims[0][OpNames::C],
                         *op_prims[s][OpNames::A]);
            op_prims[s][OpNames::AD] = make_shared<SparseMatrix>();
            op_prims[s][OpNames::AD]->allocate(
                find_site_op_info(SpinLabel(-2, s * 2, 0), 0));
            opf->product(*op_prims[0][OpNames::D], *op_prims[0][OpNames::D],
                         *op_prims[s][OpNames::AD]);
            op_prims[s][OpNames::B] = make_shared<SparseMatrix>();
            op_prims[s][OpNames::B]->allocate(
                find_site_op_info(SpinLabel(0, s * 2, 0), 0));
            opf->product(*op_prims[0][OpNames::C], *op_prims[0][OpNames::D],
                         *op_prims[s][OpNames::B]);
        }
        op_prims[0][OpNames::R] = make_shared<SparseMatrix>();
        op_prims[0][OpNames::R]->allocate(
            find_site_op_info(SpinLabel(-1, 1, 0), 0));
        opf->product(*op_prims[0][OpNames::B], *op_prims[0][OpNames::D],
                     *op_prims[0][OpNames::R]);
        op_prims[0][OpNames::RD] = make_shared<SparseMatrix>();
        op_prims[0][OpNames::RD]->allocate(
            find_site_op_info(SpinLabel(1, 1, 0), 0));
        opf->product(*op_prims[0][OpNames::C], *op_prims[0][OpNames::B],
                     *op_prims[0][OpNames::RD]);
        site_norm_ops = new vector<
            pair<shared_ptr<OpExpr>, shared_ptr<SparseMatrix>>>[n_syms];
        map<shared_ptr<OpExpr>, shared_ptr<SparseMatrix>, op_expr_less>
            ops[n_syms];
        for (uint8_t i = 0; i < n_syms; i++) {
            ops[i][make_shared<OpElement>(OpNames::I, vector<uint8_t>{},
                                          vaccum)] = nullptr;
            ops[i][make_shared<OpElement>(OpNames::N, vector<uint8_t>{},
                                          vaccum)] = nullptr;
            ops[i][make_shared<OpElement>(OpNames::NN, vector<uint8_t>{},
                                          vaccum)] = nullptr;
        }
        for (uint8_t m = 0; m < n_sites; m++) {
            ops[orb_sym[m]][make_shared<OpElement>(
                OpNames::C, vector<uint8_t>{m}, SpinLabel(1, 1, orb_sym[m]))] =
                nullptr;
            ops[orb_sym[m]][make_shared<OpElement>(
                OpNames::D, vector<uint8_t>{m}, SpinLabel(-1, 1, orb_sym[m]))] =
                nullptr;
            for (uint8_t s = 0; s < 2; s++) {
                ops[orb_sym[m]]
                   [make_shared<OpElement>(OpNames::A, vector<uint8_t>{m, m, s},
                                           SpinLabel(2, s * 2, 0))] = nullptr;
                ops[orb_sym[m]][make_shared<OpElement>(
                    OpNames::AD, vector<uint8_t>{m, m, s},
                    SpinLabel(-2, s * 2, 0))] = nullptr;
                ops[orb_sym[m]]
                   [make_shared<OpElement>(OpNames::B, vector<uint8_t>{m, m, s},
                                           SpinLabel(0, s * 2, 0))] = nullptr;
            }
        }
        for (uint8_t i = 0; i < n_syms; i++) {
            site_norm_ops[i] =
                vector<pair<shared_ptr<OpExpr>, shared_ptr<SparseMatrix>>>(
                    ops[i].begin(), ops[i].end());
            for (auto &p : site_norm_ops[i]) {
                OpElement &op = *dynamic_pointer_cast<OpElement>(p.first);
                p.second = make_shared<SparseMatrix>();
                switch (op.name) {
                case OpNames::I:
                case OpNames::N:
                case OpNames::NN:
                case OpNames::C:
                case OpNames::D:
                    p.second->allocate(find_site_op_info(op.q_label, i),
                                       op_prims[0][op.name]->data);
                    break;
                case OpNames::A:
                case OpNames::AD:
                case OpNames::B:
                    p.second->allocate(
                        find_site_op_info(op.q_label, i),
                        op_prims[op.site_index.back()][op.name]->data);
                    break;
                default:
                    assert(false);
                }
            }
        }
    }
    void get_site_ops(uint8_t m,
                      map<shared_ptr<OpExpr>, shared_ptr<SparseMatrix>,
                          op_expr_less> &ops) const {
        uint8_t i, j, k, s;
        shared_ptr<SparseMatrix> zero = make_shared<SparseMatrix>();
        shared_ptr<SparseMatrix> tmp = make_shared<SparseMatrix>();
        zero->factor = 0.0;
        for (auto &p : ops) {
            OpElement &op = *dynamic_pointer_cast<OpElement>(p.first);
            switch (op.name) {
            case OpNames::I:
            case OpNames::N:
            case OpNames::NN:
            case OpNames::C:
            case OpNames::D:
            case OpNames::A:
            case OpNames::AD:
            case OpNames::B:
                p.second = find_site_norm_op(p.first, orb_sym[m]);
                break;
            case OpNames::H:
                p.second = make_shared<SparseMatrix>();
                p.second->allocate(find_site_op_info(op.q_label, orb_sym[m]));
                (*p.second)[SpinLabel(0, 0, 0, 0)](0, 0) = 0.0;
                (*p.second)[SpinLabel(1, 1, 1, orb_sym[m])](0, 0) = t(m, m);
                (*p.second)[SpinLabel(2, 0, 0, 0)](0, 0) =
                    t(m, m) * 2 + v(m, m, m, m);
                break;
            case OpNames::R:
                i = op.site_index[0];
                if (orb_sym[i] != orb_sym[m] ||
                    (abs(t(i, m)) < TINY && abs(v(i, m, m, m)) < TINY))
                    p.second = zero;
                else {
                    p.second = make_shared<SparseMatrix>();
                    p.second->allocate(
                        find_site_op_info(op.q_label, orb_sym[m]));
                    p.second->copy_data(*op_prims[0].at(OpNames::D));
                    p.second->factor *= t(i, m) * sqrt(2) / 4;
                    tmp->allocate(find_site_op_info(op.q_label, orb_sym[m]));
                    tmp->copy_data(*op_prims[0].at(OpNames::R));
                    tmp->factor *= v(i, m, m, m);
                    opf->iadd(*p.second, *tmp);
                    tmp->deallocate();
                }
                break;
            case OpNames::RD:
                i = op.site_index[0];
                if (orb_sym[i] != orb_sym[m] ||
                    (abs(t(i, m)) < TINY && abs(v(i, m, m, m)) < TINY))
                    p.second = zero;
                else {
                    p.second = make_shared<SparseMatrix>();
                    p.second->allocate(
                        find_site_op_info(op.q_label, orb_sym[m]));
                    p.second->copy_data(*op_prims[0].at(OpNames::C));
                    p.second->factor *= t(i, m) * sqrt(2) / 4;
                    tmp->allocate(find_site_op_info(op.q_label, orb_sym[m]));
                    tmp->copy_data(*op_prims[0].at(OpNames::RD));
                    tmp->factor *= v(i, m, m, m);
                    opf->iadd(*p.second, *tmp);
                    tmp->deallocate();
                }
                break;
            case OpNames::P:
                i = op.site_index[0];
                k = op.site_index[1];
                s = op.site_index[2];
                if (abs(v(i, m, k, m)) < TINY)
                    p.second = zero;
                else {
                    p.second = make_shared<SparseMatrix>();
                    p.second->allocate(
                        find_site_op_info(op.q_label, orb_sym[m]),
                        op_prims[op.site_index.back()].at(OpNames::AD)->data);
                    p.second->factor *= v(i, m, k, m);
                }
                break;
            case OpNames::PD:
                i = op.site_index[0];
                k = op.site_index[1];
                s = op.site_index[2];
                if (abs(v(i, m, k, m)) < TINY)
                    p.second = zero;
                else {
                    p.second = make_shared<SparseMatrix>();
                    p.second->allocate(
                        find_site_op_info(op.q_label, orb_sym[m]),
                        op_prims[op.site_index.back()].at(OpNames::A)->data);
                    p.second->factor *= v(i, m, k, m);
                }
                break;
            case OpNames::Q:
                i = op.site_index[0];
                j = op.site_index[1];
                s = op.site_index[2];
                switch (s) {
                case 0:
                    if (abs(2 * v(i, j, m, m) - v(i, m, m, j)) < TINY)
                        p.second = zero;
                    else {
                        p.second = make_shared<SparseMatrix>();
                        p.second->allocate(
                            find_site_op_info(op.q_label, orb_sym[m]),
                            op_prims[0].at(OpNames::B)->data);
                        p.second->factor *= 2 * v(i, j, m, m) - v(i, m, m, j);
                    }
                    break;
                case 1:
                    if (abs(v(i, m, m, j)) < TINY)
                        p.second = zero;
                    else {
                        p.second = make_shared<SparseMatrix>();
                        p.second->allocate(
                            find_site_op_info(op.q_label, orb_sym[m]),
                            op_prims[1].at(OpNames::B)->data);
                        p.second->factor *= v(i, m, m, j);
                    }
                    break;
                }
                break;
            default:
                assert(false);
            }
        }
    }
    void filter_site_ops(uint8_t m, shared_ptr<Symbolic> &pmat,
                         map<shared_ptr<OpExpr>, shared_ptr<SparseMatrix>,
                             op_expr_less> &ops) const {
        for (auto &x : pmat->data) {
            switch (x->get_type()) {
            case OpTypes::Zero:
                break;
            case OpTypes::Elem:
                ops[abs_value(x)] = nullptr;
                break;
            case OpTypes::Sum:
                for (auto &r : dynamic_pointer_cast<OpSum>(x)->strings)
                    ops[abs_value(r->get_op())] = nullptr;
                break;
            default:
                assert(false);
            }
        }
        get_site_ops(m, ops);
        shared_ptr<OpExpr> zero = make_shared<OpExpr>();
        bool all_zero;
        for (auto &x : pmat->data) {
            shared_ptr<OpExpr> xx;
            switch (x->get_type()) {
            case OpTypes::Zero:
                break;
            case OpTypes::Elem:
                xx = abs_value(x);
                if (ops[xx]->factor == 0.0 || ops[xx]->info->n == 0)
                    x = zero;
                break;
            case OpTypes::Sum:
                all_zero = true;
                for (auto &r : dynamic_pointer_cast<OpSum>(x)->strings) {
                    xx = abs_value(r->get_op());
                    shared_ptr<SparseMatrix> &mat = ops[xx];
                    if (!(mat->factor == 0.0 || mat->info->n == 0)) {
                        all_zero = false;
                        break;
                    }
                }
                if (all_zero)
                    x = zero;
                break;
            default:
                assert(false);
            }
        }
        if (pmat->get_type() == SymTypes::Mat) {
            shared_ptr<SymbolicMatrix> smat =
                dynamic_pointer_cast<SymbolicMatrix>(pmat);
            size_t j = 0;
            for (size_t i = 0; i < smat->indices.size(); i++)
                if (smat->data[i]->get_type() != OpTypes::Zero) {
                    if (i != j)
                        smat->data[j] = smat->data[i],
                        smat->indices[j] = smat->indices[i];
                    j++;
                }
            smat->data.resize(j);
            smat->indices.resize(j);
        }
        for (auto it = ops.cbegin(); it != ops.cend();) {
            if (it->second->factor == 0.0 || it->second->info->n == 0)
                ops.erase(it++);
            else
                it++;
        }
    }
    static bool
    cmp_site_op_info(const pair<SpinLabel, shared_ptr<SparseMatrixInfo>> &p,
                     SpinLabel q) {
        return p.first < q;
    }
    static bool cmp_site_norm_op(
        const pair<shared_ptr<OpExpr>, shared_ptr<SparseMatrix>> &p,
        const shared_ptr<OpExpr> &q) {
        return op_expr_less()(p.first, q);
    }
    shared_ptr<SparseMatrixInfo> find_site_op_info(SpinLabel q,
                                                   uint8_t i_sym) const {
        auto p = lower_bound(site_op_infos[i_sym].begin(),
                             site_op_infos[i_sym].end(), q, cmp_site_op_info);
        if (p == site_op_infos[i_sym].end() || p->first != q)
            return nullptr;
        else
            return p->second;
    }
    shared_ptr<SparseMatrix> find_site_norm_op(const shared_ptr<OpExpr> &q,
                                               uint8_t i_sym) const {
        auto p = lower_bound(site_norm_ops[i_sym].begin(),
                             site_norm_ops[i_sym].end(), q, cmp_site_norm_op);
        if (p == site_norm_ops[i_sym].end() || !(p->first == q))
            return nullptr;
        else
            return p->second;
    }
    void deallocate() {
        if (site_norm_ops != nullptr)
            delete[] site_norm_ops;
        if (site_op_infos != nullptr) {
            for (auto name : vector<OpNames>{OpNames::RD, OpNames::R})
                op_prims[0][name]->deallocate();
            for (auto name :
                 vector<OpNames>{OpNames::B, OpNames::AD, OpNames::A})
                op_prims[1][name]->deallocate();
            for (auto name : vector<OpNames>{
                     OpNames::B, OpNames::AD, OpNames::A, OpNames::D,
                     OpNames::C, OpNames::NN, OpNames::N, OpNames::I})
                op_prims[0][name]->deallocate();
            for (int i = n_syms - 1; i >= 0; i--)
                for (int j = site_op_infos[i].size() - 1; j >= 0; j--)
                    site_op_infos[i][j].second->deallocate();
            delete[] site_op_infos;
        }
        opf->cg->deallocate();
        for (int i = n_syms - 1; i >= 0; i--)
            basis[i].deallocate();
        delete[] basis;
    }
    double v(uint8_t i, uint8_t j, uint8_t k, uint8_t l) const {
        return fcidump->vs[0](i, j, k, l);
    }
    double t(uint8_t i, uint8_t j) const { return fcidump->ts[0](i, j); }
    double e() const { return fcidump->e; }
};

struct QCMPO : MPO {
    QCMPO(const Hamiltonian &hamil) : MPO(hamil.n_sites) {
        shared_ptr<OpElement> h_op =
            make_shared<OpElement>(OpNames::H, vector<uint8_t>{}, hamil.vaccum);
        shared_ptr<OpElement> i_op =
            make_shared<OpElement>(OpNames::I, vector<uint8_t>{}, hamil.vaccum);
        shared_ptr<OpElement> c_op[hamil.n_sites], d_op[hamil.n_sites];
        shared_ptr<OpElement> mc_op[hamil.n_sites], md_op[hamil.n_sites];
        shared_ptr<OpElement> trd_op[hamil.n_sites], tr_op[hamil.n_sites];
        shared_ptr<OpElement> a_op[hamil.n_sites][hamil.n_sites][2];
        shared_ptr<OpElement> ad_op[hamil.n_sites][hamil.n_sites][2];
        shared_ptr<OpElement> b_op[hamil.n_sites][hamil.n_sites][2];
        shared_ptr<OpElement> p_op[hamil.n_sites][hamil.n_sites][2];
        shared_ptr<OpElement> pd_op[hamil.n_sites][hamil.n_sites][2];
        shared_ptr<OpElement> q_op[hamil.n_sites][hamil.n_sites][2];
        for (uint8_t m = 0; m < hamil.n_sites; m++) {
            c_op[m] = make_shared<OpElement>(OpNames::C, vector<uint8_t>{m},
                                             SpinLabel(1, 1, hamil.orb_sym[m]));
            d_op[m] =
                make_shared<OpElement>(OpNames::D, vector<uint8_t>{m},
                                       SpinLabel(-1, 1, hamil.orb_sym[m]));
            mc_op[m] =
                make_shared<OpElement>(OpNames::C, vector<uint8_t>{m},
                                       SpinLabel(1, 1, hamil.orb_sym[m]), -1.0);
            md_op[m] = make_shared<OpElement>(
                OpNames::D, vector<uint8_t>{m},
                SpinLabel(-1, 1, hamil.orb_sym[m]), -1.0);
            trd_op[m] =
                make_shared<OpElement>(OpNames::RD, vector<uint8_t>{m},
                                       SpinLabel(1, 1, hamil.orb_sym[m]), 2.0);
            tr_op[m] =
                make_shared<OpElement>(OpNames::R, vector<uint8_t>{m},
                                       SpinLabel(-1, 1, hamil.orb_sym[m]), 2.0);
        }
        for (uint8_t i = 0; i < hamil.n_sites; i++)
            for (uint8_t j = 0; j < hamil.n_sites; j++)
                for (uint8_t s = 0; s < 2; s++) {
                    a_op[i][j][s] = make_shared<OpElement>(
                        OpNames::A, vector<uint8_t>{i, j, s},
                        SpinLabel(2, s * 2,
                                  hamil.orb_sym[i] ^ hamil.orb_sym[j]));
                    ad_op[i][j][s] = make_shared<OpElement>(
                        OpNames::AD, vector<uint8_t>{i, j, s},
                        SpinLabel(-2, s * 2,
                                  hamil.orb_sym[i] ^ hamil.orb_sym[j]));
                    b_op[i][j][s] = make_shared<OpElement>(
                        OpNames::B, vector<uint8_t>{i, j, s},
                        SpinLabel(0, s * 2,
                                  hamil.orb_sym[i] ^ hamil.orb_sym[j]));
                    p_op[i][j][s] = make_shared<OpElement>(
                        OpNames::P, vector<uint8_t>{i, j, s},
                        SpinLabel(-2, s * 2,
                                  hamil.orb_sym[i] ^ hamil.orb_sym[j]));
                    pd_op[i][j][s] = make_shared<OpElement>(
                        OpNames::PD, vector<uint8_t>{i, j, s},
                        SpinLabel(2, s * 2,
                                  hamil.orb_sym[i] ^ hamil.orb_sym[j]));
                    q_op[i][j][s] = make_shared<OpElement>(
                        OpNames::Q, vector<uint8_t>{i, j, s},
                        SpinLabel(0, s * 2,
                                  hamil.orb_sym[i] ^ hamil.orb_sym[j]));
                }
        int p;
        for (uint8_t m = 0; m < hamil.n_sites; m++) {
            shared_ptr<Symbolic> pmat;
            int lshape = 2 + 2 * hamil.n_sites + 6 * m * m;
            int rshape = 2 + 2 * hamil.n_sites + 6 * (m + 1) * (m + 1);
            if (m == 0)
                pmat = make_shared<SymbolicRowVector>(rshape);
            else if (m == hamil.n_sites - 1)
                pmat = make_shared<SymbolicColumnVector>(lshape);
            else
                pmat = make_shared<SymbolicMatrix>(lshape, rshape);
            Symbolic &mat = *pmat;
            if (m == 0) {
                mat[{0, 0}] = h_op;
                mat[{0, 1}] = i_op;
                mat[{0, 2}] = c_op[m];
                mat[{0, 3}] = d_op[m];
                p = 4;
                for (uint8_t j = m + 1; j < hamil.n_sites; j++)
                    mat[{0, p + j - m - 1}] = trd_op[j];
                p += hamil.n_sites - (m + 1);
                for (uint8_t j = m + 1; j < hamil.n_sites; j++)
                    mat[{0, p + j - m - 1}] = tr_op[j];
                p += hamil.n_sites - (m + 1);
                for (uint8_t s = 0; s < 2; s++)
                    mat[{0, p + s}] = a_op[m][m][s];
                p += 2;
                for (uint8_t s = 0; s < 2; s++)
                    mat[{0, p + s}] = ad_op[m][m][s];
                p += 2;
                for (uint8_t s = 0; s < 2; s++)
                    mat[{0, p + s}] = b_op[m][m][s];
                p += 2;
                assert(p == mat.n);
            } else {
                mat[{0, 0}] = i_op;
                mat[{1, 0}] = h_op;
                p = 2;
                for (uint8_t j = 0; j < m; j++)
                    mat[{p + j, 0}] = tr_op[j];
                p += m;
                for (uint8_t j = 0; j < m; j++)
                    mat[{p + j, 0}] = trd_op[j];
                p += m;
                mat[{p, 0}] = d_op[m];
                p += hamil.n_sites - m;
                mat[{p, 0}] = c_op[m];
                p += hamil.n_sites - m;
                vector<double> su2_factor{-0.5, -0.5 * sqrt(3)};
                for (uint8_t s = 0; s < 2; s++)
                    for (uint8_t j = 0; j < m; j++) {
                        for (uint8_t k = 0; k < m; k++)
                            mat[{p + k, 0}] = su2_factor[s] * p_op[j][k][s];
                        p += m;
                    }
                for (uint8_t s = 0; s < 2; s++)
                    for (uint8_t j = 0; j < m; j++) {
                        for (uint8_t k = 0; k < m; k++)
                            mat[{p + k, 0}] = su2_factor[s] * pd_op[j][k][s];
                        p += m;
                    }
                su2_factor = {1.0, sqrt(3)};
                for (uint8_t s = 0; s < 2; s++)
                    for (uint8_t j = 0; j < m; j++) {
                        for (uint8_t k = 0; k < m; k++)
                            mat[{p + k, 0}] = su2_factor[s] * q_op[j][k][s];
                        p += m;
                    }
                assert(p == mat.m);
            }
            if (m != 0 && m != hamil.n_sites - 1) {
                mat[{1, 1}] = i_op;
                p = 2;
                // pointers
                int pi = 1, pc = 2, pd = 2 + m;
                int prd = 2 + m + m - m, pr = 2 + m + hamil.n_sites - m;
                int pa0 = 2 + (hamil.n_sites << 1) + m * m * 0;
                int pa1 = 2 + (hamil.n_sites << 1) + m * m * 1;
                int pad0 = 2 + (hamil.n_sites << 1) + m * m * 2;
                int pad1 = 2 + (hamil.n_sites << 1) + m * m * 3;
                int pb0 = 2 + (hamil.n_sites << 1) + m * m * 4;
                int pb1 = 2 + (hamil.n_sites << 1) + m * m * 5;
                // C
                for (uint8_t j = 0; j < m; j++)
                    mat[{pc + j, p + j}] = i_op;
                mat[{pi, p + m}] = c_op[m];
                p += m + 1;
                // D
                for (uint8_t j = 0; j < m; j++)
                    mat[{pd + j, p + j}] = i_op;
                mat[{pi, p + m}] = d_op[m];
                p += m + 1;
                // RD
                for (uint8_t i = m + 1; i < hamil.n_sites; i++) {
                    mat[{prd + i, p + i - (m + 1)}] = i_op;
                    mat[{pi, p + i - (m + 1)}] = trd_op[i];
                    for (uint8_t k = 0; k < m; k++) {
                        mat[{pd + k, p + i - (m + 1)}] =
                            2.0 * ((-0.5) * pd_op[i][k][0] +
                                   (-0.5 * sqrt(3)) * pd_op[i][k][1]);
                        mat[{pc + k, p + i - (m + 1)}] =
                            2.0 * ((0.5) * q_op[k][i][0] +
                                   (-0.5 * sqrt(3)) * q_op[k][i][1]);
                    }
                    for (uint8_t j = 0; j < m; j++)
                        for (uint8_t l = 0; l < m; l++) {
                            double f0 =
                                hamil.v(i, j, m, l) + hamil.v(i, l, m, j);
                            double f1 =
                                hamil.v(i, j, m, l) - hamil.v(i, l, m, j);
                            mat[{pa0 + j * m + l, p + i - (m + 1)}] =
                                f0 * (-0.5) * d_op[m];
                            mat[{pa1 + j * m + l, p + i - (m + 1)}] =
                                f1 * (0.5 * sqrt(3)) * d_op[m];
                        }
                    for (uint8_t k = 0; k < m; k++)
                        for (uint8_t l = 0; l < m; l++) {
                            double f =
                                2.0 * hamil.v(i, m, k, l) - hamil.v(i, l, k, m);
                            mat[{pb0 + l * m + k, p + i - (m + 1)}] =
                                f * c_op[m];
                        }
                    for (uint8_t j = 0; j < m; j++)
                        for (uint8_t k = 0; k < m; k++) {
                            double f = hamil.v(i, j, k, m) * sqrt(3);
                            mat[{pb1 + j * m + k, p + i - (m + 1)}] =
                                f * c_op[m];
                        }
                }
                p += hamil.n_sites - (m + 1);
                // R
                for (uint8_t i = m + 1; i < hamil.n_sites; i++) {
                    mat[{pr + i, p + i - (m + 1)}] = i_op;
                    mat[{pi, p + i - (m + 1)}] = tr_op[i];
                    for (uint8_t k = 0; k < m; k++) {
                        mat[{pc + k, p + i - (m + 1)}] =
                            2.0 * ((-0.5) * p_op[i][k][0] +
                                   (0.5 * sqrt(3)) * p_op[i][k][1]);
                        mat[{pd + k, p + i - (m + 1)}] =
                            2.0 * ((0.5) * q_op[i][k][0] +
                                   (0.5 * sqrt(3)) * q_op[i][k][1]);
                    }
                    for (uint8_t j = 0; j < m; j++)
                        for (uint8_t l = 0; l < m; l++) {
                            double f0 =
                                hamil.v(i, j, m, l) + hamil.v(i, l, m, j);
                            double f1 =
                                hamil.v(i, j, m, l) - hamil.v(i, l, m, j);
                            mat[{pad0 + j * m + l, p + i - (m + 1)}] =
                                f0 * (-0.5) * c_op[m];
                            mat[{pad1 + j * m + l, p + i - (m + 1)}] =
                                f1 * (-0.5 * sqrt(3)) * c_op[m];
                        }
                    for (uint8_t k = 0; k < m; k++)
                        for (uint8_t l = 0; l < m; l++) {
                            double f =
                                2.0 * hamil.v(i, m, k, l) - hamil.v(i, l, k, m);
                            mat[{pb0 + k * m + l, p + i - (m + 1)}] =
                                f * d_op[m];
                        }
                    for (uint8_t j = 0; j < m; j++)
                        for (uint8_t k = 0; k < m; k++) {
                            double f = (-1.0) * hamil.v(i, j, k, m) * sqrt(3);
                            mat[{pb1 + k * m + j, p + i - (m + 1)}] =
                                f * d_op[m];
                        }
                }
                p += hamil.n_sites - (m + 1);
                // A
                for (uint8_t s = 0; s < 2; s++) {
                    int pa = s ? pa1 : pa0;
                    for (uint8_t i = 0; i < m; i++)
                        for (uint8_t j = 0; j < m; j++)
                            mat[{pa + i * m + j, p + i * (m + 1) + j}] = i_op;
                    for (uint8_t i = 0; i < m; i++) {
                        mat[{pc + i, p + i * (m + 1) + m}] = c_op[m];
                        mat[{pc + i, p + m * (m + 1) + i}] =
                            s ? mc_op[m] : c_op[m];
                    }
                    mat[{pi, p + m * (m + 1) + m}] = a_op[m][m][s];
                    p += (m + 1) * (m + 1);
                }
                // AD
                for (uint8_t s = 0; s < 2; s++) {
                    int pad = s ? pad1 : pad0;
                    for (uint8_t i = 0; i < m; i++)
                        for (uint8_t j = 0; j < m; j++)
                            mat[{pad + i * m + j, p + i * (m + 1) + j}] = i_op;
                    for (uint8_t i = 0; i < m; i++) {
                        mat[{pd + i, p + i * (m + 1) + m}] =
                            s ? md_op[m] : d_op[m];
                        mat[{pd + i, p + m * (m + 1) + i}] = d_op[m];
                    }
                    mat[{pi, p + m * (m + 1) + m}] = ad_op[m][m][s];
                    p += (m + 1) * (m + 1);
                }
                // B
                for (uint8_t s = 0; s < 2; s++) {
                    int pb = s ? pb1 : pb0;
                    for (uint8_t i = 0; i < m; i++)
                        for (uint8_t j = 0; j < m; j++)
                            mat[{pb + i * m + j, p + i * (m + 1) + j}] = i_op;
                    for (uint8_t i = 0; i < m; i++) {
                        mat[{pc + i, p + i * (m + 1) + m}] = d_op[m];
                        mat[{pd + i, p + m * (m + 1) + i}] =
                            s ? mc_op[m] : c_op[m];
                    }
                    mat[{pi, p + m * (m + 1) + m}] = b_op[m][m][s];
                    p += (m + 1) * (m + 1);
                }
                assert(p == mat.n);
            }
            shared_ptr<OperatorTensor> opt = make_shared<OperatorTensor>();
            opt->lmat = opt->rmat = pmat;
            // operator names
            shared_ptr<SymbolicRowVector> plop;
            shared_ptr<SymbolicColumnVector> prop;
            if (m == hamil.n_sites - 1)
                plop = make_shared<SymbolicRowVector>(1);
            else
                plop = make_shared<SymbolicRowVector>(rshape);
            if (m == 0)
                prop = make_shared<SymbolicColumnVector>(1);
            else
                prop = make_shared<SymbolicColumnVector>(lshape);
            SymbolicRowVector &lop = *plop;
            SymbolicColumnVector &rop = *prop;
            lop[0] = h_op;
            if (m != hamil.n_sites - 1) {
                lop[1] = i_op;
                p = 2;
                for (uint8_t j = 0; j < m + 1; j++)
                    lop[p + j] = c_op[j];
                p += m + 1;
                for (uint8_t j = 0; j < m + 1; j++)
                    lop[p + j] = d_op[j];
                p += m + 1;
                for (uint8_t j = m + 1; j < hamil.n_sites; j++)
                    lop[p + j - (m + 1)] = trd_op[j];
                p += hamil.n_sites - (m + 1);
                for (uint8_t j = m + 1; j < hamil.n_sites; j++)
                    lop[p + j - (m + 1)] = tr_op[j];
                p += hamil.n_sites - (m + 1);
                for (uint8_t s = 0; s < 2; s++)
                    for (uint8_t j = 0; j < m + 1; j++) {
                        for (uint8_t k = 0; k < m + 1; k++)
                            lop[p + k] = a_op[j][k][s];
                        p += m + 1;
                    }
                for (uint8_t s = 0; s < 2; s++)
                    for (uint8_t j = 0; j < m + 1; j++) {
                        for (uint8_t k = 0; k < m + 1; k++)
                            lop[p + k] = ad_op[j][k][s];
                        p += m + 1;
                    }
                for (uint8_t s = 0; s < 2; s++)
                    for (uint8_t j = 0; j < m + 1; j++) {
                        for (uint8_t k = 0; k < m + 1; k++)
                            lop[p + k] = b_op[j][k][s];
                        p += m + 1;
                    }
                assert(p == rshape);
            }
            rop[0] = i_op;
            if (m != 0) {
                rop[1] = h_op;
                p = 2;
                for (uint8_t j = 0; j < m; j++)
                    rop[p + j] = tr_op[j];
                p += m;
                for (uint8_t j = 0; j < m; j++)
                    rop[p + j] = trd_op[j];
                p += m;
                for (uint8_t j = m; j < hamil.n_sites; j++)
                    rop[p + j - m] = d_op[j];
                p += hamil.n_sites - m;
                for (uint8_t j = m; j < hamil.n_sites; j++)
                    rop[p + j - m] = c_op[j];
                p += hamil.n_sites - m;
                vector<double> su2_factor{-0.5, -0.5 * sqrt(3)};
                for (uint8_t s = 0; s < 2; s++)
                    for (uint8_t j = 0; j < m; j++) {
                        for (uint8_t k = 0; k < m; k++)
                            rop[p + k] = su2_factor[s] * p_op[j][k][s];
                        p += m;
                    }
                for (uint8_t s = 0; s < 2; s++)
                    for (uint8_t j = 0; j < m; j++) {
                        for (uint8_t k = 0; k < m; k++)
                            rop[p + k] = su2_factor[s] * pd_op[j][k][s];
                        p += m;
                    }
                su2_factor = {1.0, sqrt(3)};
                for (uint8_t s = 0; s < 2; s++)
                    for (uint8_t j = 0; j < m; j++) {
                        for (uint8_t k = 0; k < m; k++)
                            rop[p + k] = su2_factor[s] * q_op[j][k][s];
                        p += m;
                    }
                assert(p == lshape);
            }
            hamil.filter_site_ops(m, opt->lmat, opt->lop);
            this->tensors.push_back(opt);
            this->left_operator_names.push_back(plop);
            this->right_operator_names.push_back(prop);
        }
    }
    void deallocate() override {
        for (uint8_t m = n_sites - 1; m < n_sites; m--)
            for (auto it = this->tensors[m]->lop.crbegin();
                 it != this->tensors[m]->lop.crend(); ++it) {
                OpElement &op = *dynamic_pointer_cast<OpElement>(it->first);
                if (op.name == OpNames::R || op.name == OpNames::RD ||
                    op.name == OpNames::H)
                    it->second->deallocate();
            }
    }
};

} // namespace block2

#endif /* QUANTUM_HPP_ */
