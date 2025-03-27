#include <iostream>
#include <windows.h>
#include <immintrin.h>
const int N = 33554432;
int a[N];
long long clock_freq_;

void Clear_() {
    for (int i = 0; i < N; i++) {
        a[i] = (i + 1) / 2;
    }
}

void Ordin_() {
    int sum = 0;
    long long clock_start_, clock_end_;
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_start_);
    for(int i = 0; i < N; i++) {
        sum += a[i];
    }
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_end_);
    std::cout << "Ordinary Algorithm span:" << (clock_end_ - clock_start_) * 1000.0 / clock_freq_ << "ms" << std::endl;
}

void Double_Chain_() {
    int sum1 = 0, sum2 = 0;
    long long clock_start_, clock_end_;
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_start_);
    for (int i = 0; i < N; i += 2) {
        sum1 += a[i];
        sum2 += a[i + 1];
    }
    int sum = sum1 + sum2;
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_end_);
    std::cout << "Double Chain span:" << (clock_end_ - clock_start_) * 1000.0 / clock_freq_ << "ms" << std::endl;
}


void Recursive_(int n){
    if (n == 1)
        return;
    else {
        for (int i = 0; i < n / 2; i++)
            a[i] += a[n - i - 1];
        n /= 2;
        Recursive_(n);
    }
}

void Recursion_(int n) {
    long long clock_start_, clock_end_;
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_start_);
    Recursive_(N);
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_end_);
    std::cout << "Recurssion span:" << (clock_end_ - clock_start_) * 1000.0 / clock_freq_ << "ms" << std::endl;
}

void Cycle_(){
    long long clock_start_, clock_end_;
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_start_);
    for (int n = N; n > 1; n /= 2) {
        for (int i = 0; i < n / 2; i++) {
            a[i] += a[n - i - 1];
        }
    }
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_end_);
    std::cout << "Cycle span:" << (clock_end_ - clock_start_) * 1000.0 / clock_freq_ << "ms" << std::endl;
}

void Cycle_AVX() {
    long long clock_start_, clock_end_;
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_start_);

    for (int n = N; n > 1; n /= 2) {
        int half = n / 2;
        int i = 0;

        for (; i <= half - 8; i += 8) {
            __m256i va = _mm256_loadu_si256((__m256i*)&a[i]);
            __m256i vb = _mm256_loadu_si256((__m256i*)&a[n - i - 8]);
            va = _mm256_add_epi32(va, vb);
            _mm256_storeu_si256((__m256i*)&a[i], va);
        }

        for (; i < half; i++) {
            a[i] += a[n - i - 1];
        }
    }

    QueryPerformanceCounter((LARGE_INTEGER*)&clock_end_);
    std::cout << "Cycle AVX span: " << (clock_end_ - clock_start_) * 1000.0 / clock_freq_ << "ms" << std::endl;
}

void Double_Cycle_() {
    long long clock_start_, clock_end_;
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_start_);
    for (int n = N; n > 1; n /= 2) {
        for (int i = 0; i < n / 2; i++) {
            a[i] = a[i * 2] + a[i * 2 + 1];
        }
    }
    QueryPerformanceCounter((LARGE_INTEGER*)&clock_end_);
    std::cout << "Double Cycle span:" << (clock_end_ - clock_start_) * 1000.0 / clock_freq_ << "ms" << std::endl;
}

int main() {
    QueryPerformanceFrequency((LARGE_INTEGER*)&clock_freq_);
    Clear_();
    Ordin_();
    //Clear_();
    Double_Chain_();
    Clear_();
    Recursion_(N);
    Clear_();
    Cycle_();
    Clear_();
    Cycle_AVX();
    Clear_();
    Double_Cycle_();
}
