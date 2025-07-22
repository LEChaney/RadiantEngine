#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>
#include <numeric>
#include <cstring>

using namespace std;
using namespace std::chrono;

template <size_t N>
struct BigStruct {
    uint8_t data[N];
    BigStruct() { std::memset(data, 1, N); }
};

#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif


volatile uint64_t global_accumulator = 0;

inline void force_use(uint64_t x) {
    volatile uint64_t* sink = &global_accumulator;
    *sink += x;
}

template <typename T>
NOINLINE uint64_t sum_by_value(T s) {
    uint64_t sum = 0;
    for (size_t i = 0; i < sizeof(T); ++i) sum += s.data[i];
    return sum;
}

template <typename T>
NOINLINE uint64_t sum_by_ref(const T& s) {
    uint64_t sum = 0;
    for (size_t i = 0; i < sizeof(T); ++i) sum += s.data[i];
    return sum;
}

template <typename T>
NOINLINE uint64_t sum_by_ptr(const T* s) {
    uint64_t sum = 0;
    for (size_t i = 0; i < sizeof(T); ++i) sum += s->data[i];
    return sum;
}



template <typename F>
void stats_time_call(F func, int runs, int batch_size, double& mean, double& stddev) {
    std::vector<double> times;
    times.reserve(runs);
    // Warm-up phase
    constexpr int warmup_iters = 100000;
    for (int i = 0; i < warmup_iters; ++i) {
        force_use(func());
    }
    for (int r = 0; r < runs; ++r) {
        auto start = high_resolution_clock::now();
        for (int i = 0; i < batch_size; ++i) {
            force_use(func());
        }
        auto end = high_resolution_clock::now();
        double duration_ns = duration_cast<nanoseconds>(end - start).count();
        times.push_back(duration_ns);
    }
    // Compute mean and stddev (total time per batch)
    double total = std::accumulate(times.begin(), times.end(), 0.0);
    mean = total / runs / batch_size;
    double sq_sum = std::inner_product(times.begin(), times.end(), times.begin(), 0.0);
    stddev = std::sqrt(sq_sum / runs - (total / runs) * (total / runs)) / batch_size;
}

template <size_t sz>
void profile_struct_size() {
    using S = BigStruct<sz>;
    alignas(64) S s;

    constexpr int runs = 1000;
    constexpr int batch_size = 100000;
    double mean_val, std_val, mean_ref, std_ref, mean_ptr, std_ptr;

    auto by_value = [&]() { return sum_by_value(s); };
    auto by_ref = [&]() { return sum_by_ref(s); };
    auto by_ptr = [&]() { return sum_by_ptr(&s); };

    stats_time_call(by_value, runs, batch_size, mean_val, std_val);
    stats_time_call(by_ref, runs, batch_size, mean_ref, std_ref);
    stats_time_call(by_ptr, runs, batch_size, mean_ptr, std_ptr);

    cout << sz << "\t"
         << mean_val << " ± " << std_val << "\t"
         << mean_ref << " ± " << std_ref << "\t"
         << mean_ptr << " ± " << std_ptr << endl;
}

template <size_t... Sizes>
void profile_struct_passing_helper(std::index_sequence<Sizes...>) {
    (profile_struct_size<(Sizes + 1) * sizeof(uint32_t)>(), ...);
}

void profile_struct_passing() {
    cout << "Size\tByValue(ns)\tByRef(ns)\tByPtr(ns)" << endl;
    // 4, 8, 12, 16, ..., 128
    constexpr size_t num_steps = 128 / sizeof(uint32_t);
    profile_struct_passing_helper(std::make_index_sequence<num_steps>{});
}

int main() {
    profile_struct_passing();
    return 0;
}
