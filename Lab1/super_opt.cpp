#include <iostream>
#include <windows.h>

const int N = 20000;
int a[N];
int b[N][N];
int sum[N] = { 0 };
long long clock_freq_;

void Clear_() {
    for (int i = 0; i < N; i++) {
        a[i] = i + 1;
    }
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            b[i][j] = (i * j) % 100;
        }
    }
    memset(sum, 0, sizeof(sum));
}

void Ordin_() {
    long long clock_start_, clock_end_;
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_start_);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            sum[i] += b[j][i] * a[j];
        }
    }
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_end_);
    std::cout << "Ordinary Algorithm span: "
              << (clock_end_ - clock_start_) * 1000.0 / clock_freq_ << " ms" << std::endl;
}

void Cache_() {
    long long clock_start_, clock_end_;
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_start_);
    for (int j = 0; j < N; j++) {
        for (int i = 0; i < N; i++) {
            sum[i] += b[j][i] * a[j];
        }
    }
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_end_);
    std::cout << "Cache Optimization span: "
              << (clock_end_ - clock_start_) * 1000.0 / clock_freq_ << " ms" << std::endl;
}

void Unroll_() {
    long long clock_start_, clock_end_;
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_start_);
    for (int j = 0; j < N; j++) {
        int tmp = a[j];
        int* b_j = b[j];
        for (int i = 0; i < N; i += 4) {
            sum[i]   += b_j[i]   * tmp;
            sum[i+1] += b_j[i+1] * tmp;
            sum[i+2] += b_j[i+2] * tmp;
            sum[i+3] += b_j[i+3] * tmp;
        }
    }
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_end_);
    std::cout << "Loop Unrolling Optimization span: "
              << (clock_end_ - clock_start_) * 1000.0 / clock_freq_ << " ms" << std::endl;
}

int main() {
    QueryPerformanceFrequency((LARGE_INTEGER*)&clock_freq_);
    Clear_();
    Ordin_();
    Clear_();
    Cache_();
    Clear_();
    Unroll_();
}
