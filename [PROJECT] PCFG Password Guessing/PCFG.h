#include <string>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <queue>
#include <omp.h>
#include <pthread.h>    // 使用 pthread
#include <omp.h>        // 使用 OpenMP
#include <unistd.h>
#include <mpi.h>        // 使用 mpi
#include <condition_variable>
#include <atomic>
#include <cstring>
#include <algorithm>
// #include <chrono>   
// using namespace chrono;
using namespace std;

#define NUM_THREADS 8   // 设置线程数

#define POOL_CHUNK_SIZE 10000  // 设置 pthread pool 方法中一个任务处理的元素量

class segment
{
public:
    int type; // 0: 未设置, 1: 字母, 2: 数字, 3: 特殊字符
    int length; // 长度，例如S6的长度就是6
    segment(int type, int length)
    {
        this->type = type;
        this->length = length;
    };

    // 打印相关信息
    void PrintSeg();

    // 按照概率降序排列的value。例如，123是D3的一个具体value，其概率在D3的所有value中排名第三，那么其位置就是ordered_values[2]
    vector<string> ordered_values;

    // 按照概率降序排列的频数（概率）
    vector<int> ordered_freqs;

    // total_freq作为分母，用于计算每个value的概率
    int total_freq = 0;

    // 未排序的value，其中int就是对应的id
    unordered_map<string, int> values;

    // 根据id，在freqs中查找/修改一个value的频数
    unordered_map<int, int> freqs;


    void insert(string value);
    void order();
    void PrintValues();
};

class PT
{
public:
    // 例如，L6D1的content大小为2，content[0]为L6，content[1]为D1
    vector<segment> content;

    // pivot值，参见PCFG的原理
    int pivot = 0;
    void insert(segment seg);
    void PrintPT();

    // 导出新的PT
    vector<PT> NewPTs();

    // 记录当前每个segment（除了最后一个）对应的value，在模型中的下标
    vector<int> curr_indices;

    // 记录当前每个segment（除了最后一个）对应的value，在模型中的最大下标（即最大可以是max_indices[x]-1）
    vector<int> max_indices;
    // void init();
    float preterm_prob;
    float prob;
};

class model
{
public:
    // 对于PT/LDS而言，序号是递增的
    // 训练时每遇到一个新的PT/LDS，就获取一个新的序号，并且当前序号递增1
    int preterm_id = -1;
    int letters_id = -1;
    int digits_id = -1;
    int symbols_id = -1;
    int GetNextPretermID()
    {
        preterm_id++;
        return preterm_id;
    };
    int GetNextLettersID()
    {
        letters_id++;
        return letters_id;
    };
    int GetNextDigitsID()
    {
        digits_id++;
        return digits_id;
    };
    int GetNextSymbolsID()
    {
        symbols_id++;
        return symbols_id;
    };

    // C++上机和数据结构实验中，一般不允许使用stl
    // 这就导致大家对stl不甚熟悉。现在是时候体会stl的便捷之处了
    // unordered_map: 无序映射
    int total_preterm = 0;
    vector<PT> preterminals;
    int FindPT(PT pt);

    vector<segment> letters;
    vector<segment> digits;
    vector<segment> symbols;
    int FindLetter(segment seg);
    int FindDigit(segment seg);
    int FindSymbol(segment seg);

    unordered_map<int, int> preterm_freq;
    unordered_map<int, int> letters_freq;
    unordered_map<int, int> digits_freq;
    unordered_map<int, int> symbols_freq;

    vector<PT> ordered_pts;

    // 给定一个训练集，对模型进行训练
    void train(string train_path);

    // 对已经训练的模型进行保存
    void store(string store_path);

    // 从现有的模型文件中加载模型
    void load(string load_path);

    // 对一个给定的口令进行切分
    void parse(string pw);

    void order();

    // 打印模型
    void print();
};

// 优先队列，用于按照概率降序生成口令猜测
// 实际上，这个class负责队列维护、口令生成、结果存储的全部过程
class PriorityQueue
{
public:
    // 用vector实现的priority queue
    vector<PT> priority;

    // 模型作为成员，辅助猜测生成
    model m;

    // 计算一个pt的概率
    void CalProb(PT &pt);

    // 优先队列的初始化
    void init();

    // 对优先队列的一个PT，生成所有guesses
    void Generate(PT pt);

    // pthread 生成（无线程池）
    void PthreadGenerate(PT pt);

    // pthread 生成（有线程池）
    void PthreadPoolGenerate(PT pt);

    // openmp 生成
    void OpenMPGenerate(PT pt);

    // mpi 生成
    void MPIGenerate(PT pt);
    
    // mpi + openmp
    void MPIplusOpenMPGenerate(PT pt);

    // 将优先队列最前面的一个 PT
    void PopNext();
    
    // mpi 并行化的批量处理 PT
    void MPIPopNext();

    int total_guesses = 0;
    vector<string> guesses;
};

// [========== pthread 方法相关 ==========] //

// 线程数据结构
typedef struct {
    int t_id;                   // 线程 id
    segment* a;                 // segment 指针，取表中值生成猜测需调用
    int max_index;              // 该 segment 的取值总数，即表值最大索引
    vector<string>* guesses;    // 指向所有生成的猜测
    int* total_guesses;         // 指向生成猜测总量
    string init_guess;          // 猜测前缀，多 segment 的猜测时需调用
} threadParam_t;

// 线程函数
void* threadFunc(void* param);

// 线程任务结构
typedef struct {
    int t_start, t_end;         // 该任务生成猜测的起点和终点
    string t_preguess;          // 猜测前缀
    vector<string>* shared_values;  // 指向 segment 的所有取值的向量组
    string* shared_guesses;     // 指向总任务的猜测结果，所有线程共享一个字符串组
    bool t_active;              // 标识此任务是否是活跃状态（处于任务队列/正在被线程处理）
} threadTask_t;

// 线程池类
class ThreadPool
{
    private:
    // 线程列表
    vector<pthread_t> threads;
    // 任务队列
    queue<threadTask_t> tasks;
    // 活跃任务数（包括已分配给线程正在运行的，和已入列但未分配的）
    int active_tasks;
    
    // 条件变量：有新任务或需要开始其他线程操作
    pthread_cond_t active_cond;
    // 条件变量：所有任务完成
    pthread_cond_t alldone_cond;
    
    // 队列互斥锁
    pthread_mutex_t task_mutex;
    // 线程安全的停止符号
    bool terminate;
    
    public:
    // 构造函数
    ThreadPool();
    // 析构函数
    ~ThreadPool();
    
    // 添加任务
    void taskAppend(threadTask_t task);
    // 等待完成
    void waitAll();
    
private:
    // 线程任务函数
    static void* threadFunction(void* pool);
};

// 创建线程池
void initThreadPool();

// 删除线程池
void deleteThreadPool();

// [======================================] //