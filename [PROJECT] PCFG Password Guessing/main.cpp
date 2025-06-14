// #include "PCFG.h"
// #include <chrono>
// #include <fstream>
// #include "md5.h"
// #include <iomanip>
// #include <unordered_set>
// using namespace std;
// using namespace chrono;

// // 编译指令如下
// // g++ main.cpp train.cpp guessing.cpp md5.cpp -o main
// // g++ main.cpp train.cpp guessing.cpp md5.cpp -o main -O1
// // g++ main.cpp train.cpp guessing.cpp md5.cpp -o main -O2

// int main()
// {
//     initThreadPool();      // 创建线程池
//     double time_hash = 0;  // 用于MD5哈希的时间
//     double time_guess = 0; // 哈希和猜测的总时长
//     double time_train = 0; // 模型训练的总时长
//     PriorityQueue q;
//     auto start_train = system_clock::now();
//     q.m.train("/guessdata/Rockyou-singleLined-full.txt");
//     q.m.order();
//     auto end_train = system_clock::now();
//     auto duration_train = duration_cast<microseconds>(end_train - start_train);
//     time_train = double(duration_train.count()) * microseconds::period::num / microseconds::period::den;

//     // 加载一些测试数据
//     // unordered_set<std::string> test_set;
//     // ifstream test_data("/guessdata/Rockyou-singleLined-full.txt");
//     // int test_count=0;
//     // string pw;
//     // while(test_data>>pw)
//     // {   
//     //     test_count+=1;
//     //     test_set.insert(pw);
//     //     if (test_count>=1000000)
//     //     {
//     //         break;
//     //     }
//     // }
//     // int cracked=0;

//     q.init();
//     cout << "here" << endl;
//     int curr_num = 0;
//     auto start = system_clock::now();
//     // 由于需要定期清空内存，我们在这里记录已生成的猜测总数
//     int history = 0;
//     // std::ofstream a("./files/results.txt");
//     while (!q.priority.empty())
//     {
//         q.PopNext();
//         q.total_guesses = q.guesses.size();
//         if (q.total_guesses - curr_num >= 100000)
//         {
//             cout << "Guesses generated: " <<history + q.total_guesses << endl;
//             curr_num = q.total_guesses;

//             // 在此处更改实验生成的猜测上限
//             int generate_n=10000000;
//             if (history + q.total_guesses > generate_n)
//             {
//                 auto end = system_clock::now();
//                 auto duration = duration_cast<microseconds>(end - start);
//                 time_guess = double(duration.count()) * microseconds::period::num / microseconds::period::den;
//                 cout << "Guess time:" << time_guess - time_hash << "seconds"<< endl;
//                 cout << "Hash time:" << time_hash << "seconds"<<endl;
//                 cout << "Train time:" << time_train <<"seconds"<<endl;
//                 // cout<<"Cracked:"<< cracked<<endl;
//                 break;
//             }
//         }
//         // 为了避免内存超限，我们在q.guesses中口令达到一定数目时，将其中的所有口令取出并且进行哈希
//         // 然后，q.guesses将会被清空。为了有效记录已经生成的口令总数，维护一个history变量来进行记录
//         if (curr_num > 1000000)
//         {
//             auto start_hash = system_clock::now();

//         // <===== SIMD *2 =====>
//             // bit32 state[4 * 2];
//             // for (int i = 0; i < q.total_guesses; i+=2) {
//             //     string pw[2] = {"", ""};
//             //     for (int i1 = 0; i1 < 2 && (i + i1) < q.total_guesses; ++i1) {
//             //         pw[i1] = q.guesses[i + i1];
//             //     }
//             //     SIMDMD5Hash_2(pw, state);
//             // }

//         // <===== SIMD *4 =====>
//             bit32 state[4 * 4];
//             for (int i = 0; i < q.total_guesses; i+=4) {
//                 string pw[4] = {"", "", "", ""};
//                 for (int i1 = 0; i1 < 4 && (i + i1) < q.total_guesses; ++i1) {
//                     pw[i1] = q.guesses[i + i1];
//                     // if (test_set.find(pw[i1]) != test_set.end()) {
//                     //     cracked+=1;
//                     // }
//                 }
//                 SIMDMD5Hash_4(pw, state);
//             }

//         // <=== SIMD *8 (-) ===>
//             // bit32 state[4 * 8];
//             // for (int i = 0; i < q.total_guesses; i+=8) {
//             //     string pw[8] = {"", "", "", "", "", "", "", ""};
//             //     for (int i1 = 0; i1 < 8 && (i + i1) < q.total_guesses; ++i1) {
//             //         pw[i1] = q.guesses[i + i1];
//             //     }
//             //     SIMDMD5Hash_8basic(pw, state);
//             // }

//         // <=== SIMD *8 (+) ===>
//             // bit32 state[4 * 8];
//             // for (int i = 0; i < q.total_guesses; i+=8) {
//             //     string pw[8] = {"", "", "", "", "", "", "", ""};
//             //     for (int i1 = 0; i1 < 8 && (i + i1) < q.total_guesses; ++i1) {
//             //         pw[i1] = q.guesses[i + i1];
//             //     }
//             //     SIMDMD5Hash_8advanced(pw, state);
//             // }

//         // <= no-optimization =>
//             // bit32 state[4 * 4];
//             // for (string pw : q.guesses)
//             // {
//             //     // TODO：对于SIMD实验，将这里替换成你的SIMD MD5函数
//             //     MD5Hash(pw, state);
//             //     // 以下注释部分用于输出猜测和哈希，但是由于自动测试系统不太能写文件，所以这里你可以改成cout
//             //     // a<<pw<<"\t";
//             //     // for (int i1 = 0; i1 < 4; i1 += 1)
//             //     // {
//             //     //     a << std::setw(8) << std::setfill('0') << hex << state[i1];
//             //     // }
//             //     // a << endl;
//             // }

//             // 在这里对哈希所需的总时长进行计算
//             auto end_hash = system_clock::now();
//             auto duration = duration_cast<microseconds>(end_hash - start_hash);
//             time_hash += double(duration.count()) * microseconds::period::num / microseconds::period::den;

//             // 记录已经生成的口令总数
//             history += curr_num;
//             curr_num = 0;
//             q.guesses.clear();
//         }
//     }
//     deleteThreadPool();  // 清理线程池
// }



// 以下是 MPI 专用的 main 函数


#include "PCFG.h"
#include <mpi.h>
#include <fstream>
#include "md5.h"
#include <iomanip>
#include <unordered_set>
#include <vector>
#include <iostream>
#include <chrono>
using namespace std;
using namespace chrono;

int main(int argc, char *argv[])
{
    MPI_Init(&argc, &argv);
    int rank, size;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double time_train = 0.0, time_guess = 0.0, time_hash = 0.0;

    PriorityQueue q;

    // MPI计时：训练阶段
    double mpi_time_train_start = MPI_Wtime();
    if (rank == 0) {
        cout << "Starting model training..." << endl;
    }
    
    q.m.train("/guessdata/Rockyou-singleLined-full.txt");
    q.m.order();
    
    double mpi_time_train_end = MPI_Wtime();
    time_train = mpi_time_train_end - mpi_time_train_start;
    
    if (rank == 0) {
        cout << "Model training completed in " << time_train << " seconds" << endl;
    }

    // 等待所有进程完成训练
    MPI_Barrier(MPI_COMM_WORLD);

    q.init();

    // 广播模型数据给其他进程（假设模型支持广播，如果不支持可以先跳过）
    // 如果模型太复杂，可以考虑文件共享方式，当前我们假设只在rank 0生成猜测。

    if (rank == 0)
    {
        cout << "here" << endl;
    }

    int curr_num = 0;

    double mpi_time_guess_start = MPI_Wtime();

    int history = 0;

    // bool local_not_empty = !q.priority.empty();
    // int global_not_empty = 0;
    // MPI_Allreduce(&local_not_empty, &global_not_empty, 1, MPI_INT, MPI_LOR, MPI_COMM_WORLD);

    while (!q.priority.empty())
    {
        q.PopNext();

        // q.MPIPopNext(); // 并行化处理多个 PT
        
        // 收集所有进程的猜测总数
        int local_guesses = q.guesses.size();
        int global_guesses = 0;
        MPI_Allreduce(&local_guesses, &global_guesses, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
        q.total_guesses = global_guesses;
        
        if (q.total_guesses - curr_num >= 100000)
        {
            if (rank == 0) {
                cout << "Process " << rank << " - Global guesses generated: " << history + q.total_guesses << endl;
            }
            curr_num = q.total_guesses;

            // 在此处更改实验生成的猜测上限
            int generate_n = 10000000;
            if (history + q.total_guesses > generate_n)
            {
                double mpi_time_guess_end = MPI_Wtime();
                time_guess = mpi_time_guess_end - mpi_time_guess_start;
                
                if (rank == 0) {
                    cout << "=== MPI Timing Results (Process " << rank << ") ===" << endl;
                    cout << "Guess time: " << time_guess<< " seconds" << endl;
                    cout << "Hash time: " << time_hash << "seconds"<<endl;
                    cout << "Train time: " << time_train << " seconds" << endl;
                }
                break;
            }
        }

        if (curr_num > 1000000)
        {
            double start_hash = MPI_Wtime();

            const int batchSize = 4;
            const int numFullBatches = q.guesses.size() / batchSize;
            const int remainder = q.guesses.size() % batchSize;

            bit32 state[batchSize * 4]; // 每个哈希 4 个 bit32，连续内存

            // 预分配字符串数组
            string inputs[batchSize];

            // 处理完整批次
            for (int batch = 0; batch < numFullBatches; ++batch) {
                for (int i = 0; i < batchSize; ++i) {
                    inputs[i] = q.guesses[batch * batchSize + i];
                }
                SIMDMD5Hash_4(inputs, state);
            }

            // 处理剩余项（不足 4 个）
            if (remainder > 0) {
                for (int i = 0; i < remainder; ++i) {
                    inputs[i] = q.guesses[numFullBatches * batchSize + i];
                }
                // 剩余的用单个哈希函数处理
                for (int i = 0; i < remainder; ++i) {
                    bit32 singleState[4];
                    MD5Hash(inputs[i], singleState);
                }
            }

            double end_hash = MPI_Wtime();
            time_hash += end_hash - start_hash;

            history += curr_num;
            curr_num = 0;
            q.guesses.clear();

        }

        // local_not_empty = !q.priority.empty();
        // MPI_Allreduce(&local_not_empty, &global_not_empty, 1, MPI_INT, MPI_LOR, MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}
