#include "PCFG.h"
using namespace std;

// 互斥锁，保护从猜测列表的读和写操作
pthread_mutex_t mutex_guess;

// 动态分配任务用，记录总分配进程
int pthread_global_index = 0;

// 全局线程池指针
ThreadPool* thread_pool = NULL;

void PriorityQueue::CalProb(PT &pt)
{
    // 计算PriorityQueue里面一个PT的流程如下：
    // 1. 首先需要计算一个PT本身的概率。例如，L6S1的概率为0.15
    // 2. 需要注意的是，Queue里面的PT不是“纯粹的”PT，而是除了最后一个segment以外，全部被value实例化的PT
    // 3. 所以，对于L6S1而言，其在Queue里面的实际PT可能是123456S1，其中“123456”为L6的一个具体value。
    // 4. 这个时候就需要计算123456在L6中出现的概率了。假设123456在所有L6 segment中的概率为0.1，那么123456S1的概率就是0.1*0.15

    // 计算一个PT本身的概率。后续所有具体segment value的概率，直接累乘在这个初始概率值上
    pt.prob = pt.preterm_prob;

    // index: 标注当前segment在PT中的位置
    int index = 0;


    for (int idx : pt.curr_indices)
    {
        // pt.content[index].PrintSeg();
        if (pt.content[index].type == 1)
        {
            // 下面这行代码的意义：
            // pt.content[index]：目前需要计算概率的segment
            // m.FindLetter(seg): 找到一个letter segment在模型中的对应下标
            // m.letters[m.FindLetter(seg)]：一个letter segment在模型中对应的所有统计数据
            // m.letters[m.FindLetter(seg)].ordered_values：一个letter segment在模型中，所有value的总数目
            pt.prob *= m.letters[m.FindLetter(pt.content[index])].ordered_freqs[idx];
            pt.prob /= m.letters[m.FindLetter(pt.content[index])].total_freq;
            // cout << m.letters[m.FindLetter(pt.content[index])].ordered_freqs[idx] << endl;
            // cout << m.letters[m.FindLetter(pt.content[index])].total_freq << endl;
        }
        if (pt.content[index].type == 2)
        {
            pt.prob *= m.digits[m.FindDigit(pt.content[index])].ordered_freqs[idx];
            pt.prob /= m.digits[m.FindDigit(pt.content[index])].total_freq;
            // cout << m.digits[m.FindDigit(pt.content[index])].ordered_freqs[idx] << endl;
            // cout << m.digits[m.FindDigit(pt.content[index])].total_freq << endl;
        }
        if (pt.content[index].type == 3)
        {
            pt.prob *= m.symbols[m.FindSymbol(pt.content[index])].ordered_freqs[idx];
            pt.prob /= m.symbols[m.FindSymbol(pt.content[index])].total_freq;
            // cout << m.symbols[m.FindSymbol(pt.content[index])].ordered_freqs[idx] << endl;
            // cout << m.symbols[m.FindSymbol(pt.content[index])].total_freq << endl;
        }
        index += 1;
    }
    // cout << pt.prob << endl;
}

void PriorityQueue::init()
{
    // cout << m.ordered_pts.size() << endl;
    // 用所有可能的PT，按概率降序填满整个优先队列
    for (PT pt : m.ordered_pts)
    {
        for (segment seg : pt.content)
        {
            if (seg.type == 1)
            {
                // 下面这行代码的意义：
                // max_indices用来表示PT中各个segment的可能数目。例如，L6S1中，假设模型统计到了100个L6，那么L6对应的最大下标就是99
                // （但由于后面采用了"<"的比较关系，所以其实max_indices[0]=100）
                // m.FindLetter(seg): 找到一个letter segment在模型中的对应下标
                // m.letters[m.FindLetter(seg)]：一个letter segment在模型中对应的所有统计数据
                // m.letters[m.FindLetter(seg)].ordered_values：一个letter segment在模型中，所有value的总数目
                pt.max_indices.emplace_back(m.letters[m.FindLetter(seg)].ordered_values.size());
            }
            if (seg.type == 2)
            {
                pt.max_indices.emplace_back(m.digits[m.FindDigit(seg)].ordered_values.size());
            }
            if (seg.type == 3)
            {
                pt.max_indices.emplace_back(m.symbols[m.FindSymbol(seg)].ordered_values.size());
            }
        }
        pt.preterm_prob = float(m.preterm_freq[m.FindPT(pt)]) / m.total_preterm;
        // pt.PrintPT();
        // cout << " " << m.preterm_freq[m.FindPT(pt)] << " " << m.total_preterm << " " << pt.preterm_prob << endl;

        // 计算当前pt的概率
        CalProb(pt);
        // 将PT放入优先队列
        priority.emplace_back(pt);
    }
    // cout << "priority size:" << priority.size() << endl;
}

void PriorityQueue::PopNext()
{

    // 对优先队列最前面的PT，首先利用这个PT生成一系列猜测
    // <=== 串行方法 ===>
    // Generate(priority.front());
    // <= pthread 方法 =>
    // PthreadGenerate(priority.front());
    // PthreadPoolGenerate(priority.front());
    // <== openmp 方法 =>
    OpenMPGenerate(priority.front());

    // 然后需要根据即将出队的PT，生成一系列新的PT
    vector<PT> new_pts = priority.front().NewPTs();
    for (PT pt : new_pts)
    {
        // 计算概率
        CalProb(pt);
        // 接下来的这个循环，作用是根据概率，将新的PT插入到优先队列中
        for (auto iter = priority.begin(); iter != priority.end(); iter++)
        {
            // 对于非队首和队尾的特殊情况
            if (iter != priority.end() - 1 && iter != priority.begin())
            {
                // 判定概率
                if (pt.prob <= iter->prob && pt.prob > (iter + 1)->prob)
                {
                    priority.emplace(iter + 1, pt);
                    break;
                }
            }
            if (iter == priority.end() - 1)
            {
                priority.emplace_back(pt);
                break;
            }
            if (iter == priority.begin() && iter->prob < pt.prob)
            {
                priority.emplace(iter, pt);
                break;
            }
        }
    }

    // 现在队首的PT善后工作已经结束，将其出队（删除）
    priority.erase(priority.begin());
}

// 这个函数你就算看不懂，对并行算法的实现影响也不大
// 当然如果你想做一个基于多优先队列的并行算法，可能得稍微看一看了
vector<PT> PT::NewPTs()
{
    // 存储生成的新PT
    vector<PT> res;

    // 假如这个PT只有一个segment
    // 那么这个segment的所有value在出队前就已经被遍历完毕，并作为猜测输出
    // 因此，所有这个PT可能对应的口令猜测已经遍历完成，无需生成新的PT
    if (content.size() == 1)
    {
        return res;
    }
    else
    {
        // 最初的pivot值。我们将更改位置下标大于等于这个pivot值的segment的值（最后一个segment除外），并且一次只更改一个segment
        // 上面这句话里是不是有没看懂的地方？接着往下看你应该会更明白
        int init_pivot = pivot;

        // 开始遍历所有位置值大于等于init_pivot值的segment
        // 注意i < curr_indices.size() - 1，也就是除去了最后一个segment（这个segment的赋值预留给并行环节）
        for (int i = pivot; i < curr_indices.size() - 1; i += 1)
        {
            // curr_indices: 标记各segment目前的value在模型里对应的下标
            curr_indices[i] += 1;

            // max_indices：标记各segment在模型中一共有多少个value
            if (curr_indices[i] < max_indices[i])
            {
                // 更新pivot值
                pivot = i;
                res.emplace_back(*this);
            }

            // 这个步骤对于你理解pivot的作用、新PT生成的过程而言，至关重要
            curr_indices[i] -= 1;
        }
        pivot = init_pivot;
        return res;
    }

    return res;
}

// 这个函数是PCFG并行化算法的主要载体
// 尽量看懂，然后进行并行实现
void PriorityQueue::Generate(PT pt)
{
    // 计算PT的概率，这里主要是给PT的概率进行初始化
    CalProb(pt);

    // 对于只有一个segment的PT，直接遍历生成其中的所有value即可
    if (pt.content.size() == 1)
    {
        // 指向最后一个segment的指针，这个指针实际指向模型中的统计数据
        segment *a;
        // 在模型中定位到这个segment
        if (pt.content[0].type == 1)
        {
            a = &m.letters[m.FindLetter(pt.content[0])];
        }
        if (pt.content[0].type == 2)
        {
            a = &m.digits[m.FindDigit(pt.content[0])];
        }
        if (pt.content[0].type == 3)
        {
            a = &m.symbols[m.FindSymbol(pt.content[0])];
        }

        // Multi-thread TODO：
        // 这个for循环就是你需要进行并行化的主要部分了，特别是在多线程&GPU编程任务中
        // 可以看到，这个循环本质上就是把模型中一个segment的所有value，赋值到PT中，形成一系列新的猜测
        // 这个过程是可以高度并行化的
        for (int i = 0; i < pt.max_indices[0]; i += 1)
        {
            string guess = a->ordered_values[i];
            // cout << guess << endl;
            guesses.emplace_back(guess);
            total_guesses += 1;
        }
    }
    else
    {
        string guess;
        int seg_idx = 0;
        // 这个for循环的作用：给当前PT的所有segment赋予实际的值（最后一个segment除外）
        // segment值根据curr_indices中对应的值加以确定
        // 这个for循环你看不懂也没太大问题，并行算法不涉及这里的加速
        for (int idx : pt.curr_indices)
        {
            if (pt.content[seg_idx].type == 1)
            {
                guess += m.letters[m.FindLetter(pt.content[seg_idx])].ordered_values[idx];
            }
            if (pt.content[seg_idx].type == 2)
            {
                guess += m.digits[m.FindDigit(pt.content[seg_idx])].ordered_values[idx];
            }
            if (pt.content[seg_idx].type == 3)
            {
                guess += m.symbols[m.FindSymbol(pt.content[seg_idx])].ordered_values[idx];
            }
            seg_idx += 1;
            if (seg_idx == pt.content.size() - 1)
            {
                break;
            }
        }

        // 指向最后一个segment的指针，这个指针实际指向模型中的统计数据
        segment *a;
        if (pt.content[pt.content.size() - 1].type == 1)
        {
            a = &m.letters[m.FindLetter(pt.content[pt.content.size() - 1])];
        }
        if (pt.content[pt.content.size() - 1].type == 2)
        {
            a = &m.digits[m.FindDigit(pt.content[pt.content.size() - 1])];
        }
        if (pt.content[pt.content.size() - 1].type == 3)
        {
            a = &m.symbols[m.FindSymbol(pt.content[pt.content.size() - 1])];
        }

        // Multi-thread TODO：
        // 这个for循环就是你需要进行并行化的主要部分了，特别是在多线程&GPU编程任务中
        // 可以看到，这个循环本质上就是把模型中一个segment的所有value，赋值到PT中，形成一系列新的猜测
        // 这个过程是可以高度并行化的
        for (int i = 0; i < pt.max_indices[pt.content.size() - 1]; i += 1)
        {
            string temp = guess + a->ordered_values[i];
            // cout << temp << endl;
            guesses.emplace_back(temp);
            total_guesses += 1;
        }
    }
}

// ===== pthread 相关实现（无线程池） ===== //

/**
 * threadFunc: 线程函数，单个线程单次执行任务
 * @param param 线程结构体
 */
void* threadFunc(void* param) {
    threadParam_t* p = (threadParam_t*) param;
    int t_id = p->t_id;
    segment* a = p->a;
    int max_index = p->max_index;
    string guess = p->init_guess;

    vector<string> my_guesses;
    int my_count = 0;
    int chunk_size = (max_index + NUM_THREADS - 1) / NUM_THREADS;
    if (chunk_size == 0) {
        chunk_size = 1;
    }

    int start = t_id * chunk_size;
    int end = min(max_index, start + chunk_size);
    for (int my_index = start; my_index < end; my_index++) {
        // 执行任务
        string temp = guess + a->ordered_values[my_index];
        my_guesses.emplace_back(temp);
        my_count++;
    }


    // 临界区：结果写入猜测序列
    pthread_mutex_lock(&mutex_guess);
    p->guesses->insert(p->guesses->end(), my_guesses.begin(), my_guesses.end());
    *(p->total_guesses) += my_count;
    pthread_mutex_unlock(&mutex_guess);

    pthread_exit(NULL);
}

/**
 * PthreadGenerate: pt 生成猜测的 pthread 并行方法（无线程池）
 * @param pt 生成用的原 pt
 */
void PriorityQueue::PthreadGenerate(PT pt)
{
    // 计算PT的概率，这里主要是给PT的概率进行初始化
    CalProb(pt);

    // 对于只有一个segment的PT，直接遍历生成其中的所有value即可
    if (pt.content.size() == 1)
    {
        // 指向最后一个segment的指针，这个指针实际指向模型中的统计数据
        segment *a;
        // 在模型中定位到这个segment
        if (pt.content[0].type == 1)
        {
            a = &m.letters[m.FindLetter(pt.content[0])];
        }
        if (pt.content[0].type == 2)
        {
            a = &m.digits[m.FindDigit(pt.content[0])];
        }
        if (pt.content[0].type == 3)
        {
            a = &m.symbols[m.FindSymbol(pt.content[0])];
        }

        // pthread
        pthread_t handles[NUM_THREADS];
        threadParam_t params[NUM_THREADS];
        pthread_global_index = 0;

        // 初始化锁
        pthread_mutex_init(&mutex_guess, NULL);

        for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
            // 初始化传入参数
            params[t_id].t_id = t_id;
            params[t_id].a = a;
            params[t_id].max_index = pt.max_indices[0];
            params[t_id].guesses = &guesses;
            params[t_id].total_guesses = &total_guesses;
            params[t_id].init_guess = "";

            // 开启线程
            pthread_create(&handles[t_id], NULL, threadFunc, (void*)&params[t_id]);
        }

        // 等待所有线程完成
        for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
            pthread_join(handles[t_id], NULL);
        }

        // 清理互斥锁
        pthread_mutex_destroy(&mutex_guess);
    }
    else
    {
        string guess;
        int seg_idx = 0;
        // 这个for循环的作用：给当前PT的所有segment赋予实际的值（最后一个segment除外）
        // segment值根据curr_indices中对应的值加以确定
        // 这个for循环你看不懂也没太大问题，并行算法不涉及这里的加速
        for (int idx : pt.curr_indices)
        {
            if (pt.content[seg_idx].type == 1)
            {
                guess += m.letters[m.FindLetter(pt.content[seg_idx])].ordered_values[idx];
            }
            if (pt.content[seg_idx].type == 2)
            {
                guess += m.digits[m.FindDigit(pt.content[seg_idx])].ordered_values[idx];
            }
            if (pt.content[seg_idx].type == 3)
            {
                guess += m.symbols[m.FindSymbol(pt.content[seg_idx])].ordered_values[idx];
            }
            seg_idx += 1;
            if (seg_idx == pt.content.size() - 1)
            {
                break;
            }
        }

        // 指向最后一个segment的指针，这个指针实际指向模型中的统计数据
        segment *a;
        if (pt.content[pt.content.size() - 1].type == 1)
        {
            a = &m.letters[m.FindLetter(pt.content[pt.content.size() - 1])];
        }
        if (pt.content[pt.content.size() - 1].type == 2)
        {
            a = &m.digits[m.FindDigit(pt.content[pt.content.size() - 1])];
        }
        if (pt.content[pt.content.size() - 1].type == 3)
        {
            a = &m.symbols[m.FindSymbol(pt.content[pt.content.size() - 1])];
        }

        // pthread
        pthread_t handles[NUM_THREADS];
        threadParam_t params[NUM_THREADS];
        pthread_global_index = 0;

        // 初始化锁
        pthread_mutex_init(&mutex_guess, NULL);

        for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
            // 初始化传入参数
            params[t_id].t_id = t_id;
            params[t_id].a = a;
            params[t_id].max_index = pt.max_indices[pt.content.size() - 1];
            params[t_id].guesses = &guesses;
            params[t_id].total_guesses = &total_guesses;
            params[t_id].init_guess = guess;

            // 开启线程
            pthread_create(&handles[t_id], NULL, threadFunc, (void*)&params[t_id]);
        }

        // 等待所有线程完成
        for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
            pthread_join(handles[t_id], NULL);
        }

        // 清理互斥锁
        pthread_mutex_destroy(&mutex_guess);
    }
}

// ======================================= //


// ===== pthread 相关实现（有线程池） ===== //
/**
 * ThreadPool: 线程池初始化构造函数
 */
ThreadPool::ThreadPool() : threads(), tasks(), active_tasks(0), terminate(false) {
    // 初始化锁和条件变量
    pthread_cond_init(&active_cond, NULL);
    pthread_cond_init(&alldone_cond, NULL);
    pthread_mutex_init(&task_mutex, NULL);
    threads.resize(NUM_THREADS);
    // 创建静态线程
    for (int t_id = 0; t_id < NUM_THREADS; t_id++) {
        pthread_create(&threads[t_id], NULL, &ThreadPool::threadFunction, this);
    }
}

/**
 * ~ThreadPool: 线程池关闭析构函数
 */
ThreadPool::~ThreadPool() {
    // 设置终止标志
    terminate = true;
    // 唤醒所有等待线程
    pthread_cond_broadcast(&active_cond);
    // 等待所有线程结束
    for (auto& thread : threads) {
        pthread_join(thread, NULL);
    }
    // 销毁锁和条件变量
    pthread_mutex_destroy(&task_mutex);
    pthread_cond_destroy(&active_cond);
    pthread_cond_destroy(&alldone_cond);
}

/**
 * taskAppend: 新任务入列
 * @param task 入列任务
 */
void ThreadPool::taskAppend(threadTask_t task) {
    pthread_mutex_lock(&task_mutex);
    tasks.push(task);
    active_tasks++;
    pthread_mutex_unlock(&task_mutex);
    pthread_cond_signal(&active_cond);
}
 
/**
 * waitAll: 阻塞等待所有任务执行完毕
 */
void ThreadPool::waitAll() {
    pthread_mutex_lock(&task_mutex);
    // 只要还有活跃任务，等待其全部完成
    while (active_tasks > 0 || !tasks.empty()) {
        pthread_cond_wait(&alldone_cond, &task_mutex);
    }
    pthread_mutex_unlock(&task_mutex);
}

/**
 * threadFunction: 线程任务函数，线程在任务列表中获取任务并进行具体处理，
                   直至任务列表清空
 * @param pool 全局线程池
 */
void* ThreadPool::threadFunction(void* pool) {
    ThreadPool* t_pool = static_cast<ThreadPool*>(pool);
    
    // 循环持续获取任务
    while (true) {
        // 初始未获得任务
        threadTask_t t_task;
        t_task.t_active = false;

        /* 获取任务：任务列表可能被多线程同时访问，以及可能同时进行新增操作，
                    故该过程需要上锁保持线程安全 */
        pthread_mutex_lock(&t_pool->task_mutex);
        
        // 挂起状态：等待新下发任务
        while (t_pool->tasks.empty() && !t_pool->terminate) {
            pthread_cond_wait(&t_pool->active_cond, &t_pool->task_mutex);
        }

        // 退出：线程池关闭且无任务
        if (t_pool->tasks.empty() && t_pool->terminate) {
            pthread_mutex_unlock(&t_pool->task_mutex);
            break;
        }

        // 活跃状态：从非空任务队列取出首个任务
        if (!t_pool->tasks.empty()) {
            t_task = t_pool->tasks.front();
            t_pool->tasks.pop();
        }
        pthread_mutex_unlock(&t_pool->task_mutex);

        // 如果成功获取任务，处理该任务
        if (t_task.t_active) {
            for (int i = t_task.t_start; i < t_task.t_end; i++) {
                string guess = t_task.t_preguess + (*(t_task.shared_values))[i];
                t_task.shared_guesses[i] = std::move(guess);
            }

            // 当前任务完成，任务计数减量
            pthread_mutex_lock(&t_pool->task_mutex);
            t_pool->active_tasks--;
            // 所有任务都完成，通知 waitAll 函数
            if (t_pool->active_tasks == 0 && t_pool->tasks.empty()) {
                pthread_cond_signal(&t_pool->alldone_cond);
            }
            pthread_mutex_unlock(&t_pool->task_mutex);
        }
    }
    return NULL;
}

/**
 * initThreadPool: 创建线程池
 */
void initThreadPool() {
    if (!thread_pool) {
        thread_pool = new ThreadPool();
    }
}

/**
 * deleteThreadPool: 删除线程池
 */
void deleteThreadPool() {
    if (thread_pool) {
        delete thread_pool;
        thread_pool = NULL;
    }
}

/**
 * PthreadPoolGenerate: pt 生成猜测的 pthread 并行方法（有线程池）
 * @param pt 生成用的原 pt
 */
void PriorityQueue::PthreadPoolGenerate(PT pt)
{
    // 计算PT的概率，这里主要是给PT的概率进行初始化
    CalProb(pt);

    // 对于只有一个segment的PT，直接遍历生成其中的所有value即可
    if (pt.content.size() == 1)
    {
        // 指向最后一个segment的指针，这个指针实际指向模型中的统计数据
        segment *a;
        // 在模型中定位到这个segment
        if (pt.content[0].type == 1)
        {
            a = &m.letters[m.FindLetter(pt.content[0])];
        }
        if (pt.content[0].type == 2)
        {
            a = &m.digits[m.FindDigit(pt.content[0])];
        }
        if (pt.content[0].type == 3)
        {
            a = &m.symbols[m.FindSymbol(pt.content[0])];
        }

        // pthread pool
        int batch_size = pt.max_indices[0];

        // 提高任务粒度阈值，减少小任务的并行开销
        if (batch_size < 100000) {
            // 直接使用串行处理
            for (int i = 0; i < batch_size; i++) {
                guesses.push_back(a->ordered_values[i]);
            }
            total_guesses += batch_size;
            return;
        }
        
        int chunk_size = POOL_CHUNK_SIZE;
        int chunk_num = (batch_size + chunk_size - 1) / chunk_size;  // + chunk_size - 1 的目的是实现向上取整

        // 预分配避免频繁扩容
        guesses.reserve(guesses.size() + batch_size);

        // 所有任务共享结果 string 数组
        string* shared_guesses = new string[batch_size];

        // 划分任务并传递给线程池
        for (int id = 0; id < chunk_num; id++) {
            int start = id * chunk_size;
            int end = min(batch_size, start + chunk_size);

            // 创建任务
            threadTask_t task = {
                start,
                end,
                "",
                &a->ordered_values,
                shared_guesses,
                true
            };

            // 添加到任务队列
            thread_pool->taskAppend(task);
        }

        // 等待所有任务完成
        thread_pool->waitAll();

        // 结果拷贝到全局 guesses 中
        for (int g_id = 0; g_id < batch_size; g_id++) {
            guesses.push_back(std::move(shared_guesses[g_id]));
        }

        // 全局猜测结果计数器增量
        total_guesses += batch_size;

        // 回收分配资源
        delete[] shared_guesses;
    }
    else
    {
        string guess;
        int seg_idx = 0;
        // 这个for循环的作用：给当前PT的所有segment赋予实际的值（最后一个segment除外）
        // segment值根据curr_indices中对应的值加以确定
        // 这个for循环你看不懂也没太大问题，并行算法不涉及这里的加速
        for (int idx : pt.curr_indices)
        {
            if (pt.content[seg_idx].type == 1)
            {
                guess += m.letters[m.FindLetter(pt.content[seg_idx])].ordered_values[idx];
            }
            if (pt.content[seg_idx].type == 2)
            {
                guess += m.digits[m.FindDigit(pt.content[seg_idx])].ordered_values[idx];
            }
            if (pt.content[seg_idx].type == 3)
            {
                guess += m.symbols[m.FindSymbol(pt.content[seg_idx])].ordered_values[idx];
            }
            seg_idx += 1;
            if (seg_idx == pt.content.size() - 1)
            {
                break;
            }
        }

        // 指向最后一个segment的指针，这个指针实际指向模型中的统计数据
        segment *a;
        if (pt.content[pt.content.size() - 1].type == 1)
        {
            a = &m.letters[m.FindLetter(pt.content[pt.content.size() - 1])];
        }
        if (pt.content[pt.content.size() - 1].type == 2)
        {
            a = &m.digits[m.FindDigit(pt.content[pt.content.size() - 1])];
        }
        if (pt.content[pt.content.size() - 1].type == 3)
        {
            a = &m.symbols[m.FindSymbol(pt.content[pt.content.size() - 1])];
        }

        // pthread pool
        int batch_size = pt.max_indices[pt.content.size() - 1];

        // 提高任务粒度阈值，减少小任务的并行开销
        if (batch_size < 100000) {
            // 直接使用串行处理
            for (int i = 0; i < batch_size; i++) {
                guesses.push_back(a->ordered_values[i]);
            }
            total_guesses += batch_size;
            return;
        }
        
        int chunk_size = POOL_CHUNK_SIZE;
        int chunk_num = (batch_size + chunk_size - 1) / chunk_size;  // + chunk_size - 1 的目的是实现向上取整

        // 预分配避免频繁扩容
        guesses.reserve(guesses.size() + batch_size);

        // 所有任务共享结果 string 数组
        string* shared_guesses = new string[batch_size];

        // 划分任务并传递给线程池
        for (int id = 0; id < chunk_num; id++) {
            int start = id * chunk_size;
            int end = min(batch_size, start + chunk_size);

            // 创建任务
            threadTask_t task = {
                start,
                end,
                guess,
                &a->ordered_values,
                shared_guesses,
                true
            };

            // 添加到任务队列
            thread_pool->taskAppend(task);
        }

        // 等待所有任务完成
        thread_pool->waitAll();

        // 结果拷贝到全局 guesses 中
        for (int g_id = 0; g_id < batch_size; g_id++) {
            guesses.push_back(std::move(shared_guesses[g_id]));
        }

        // 全局猜测结果计数器增量
        total_guesses += batch_size;

        // 回收分配资源
        delete[] shared_guesses;
    }
}

// ======================================= //

// =========== openmp 相关实现 =========== //

/**
 * OpenMPGenerate: pt 生成猜测的 OpenMP 并行方法
 * @param pt 生成用的原 pt
 */
void PriorityQueue::OpenMPGenerate(PT pt)
{
    // 计算PT的概率，这里主要是给PT的概率进行初始化
    CalProb(pt);

    // 对于只有一个segment的PT，直接遍历生成其中的所有value即可
    if (pt.content.size() == 1)
    {
        // 指向最后一个segment的指针，这个指针实际指向模型中的统计数据
        segment *a;
        // 在模型中定位到这个segment
        if (pt.content[0].type == 1)
        {
            a = &m.letters[m.FindLetter(pt.content[0])];
        }
        if (pt.content[0].type == 2)
        {
            a = &m.digits[m.FindDigit(pt.content[0])];
        }
        if (pt.content[0].type == 3)
        {
            a = &m.symbols[m.FindSymbol(pt.content[0])];
        }

        #pragma omp parallel
        {
            // 共享存储所有线程生成的猜测
            std::vector<std::string> thread_guesses;

            // 主要逻辑：循环生成猜测，OpenMP 自动分配线程任务
            #pragma omp for nowait
            for (int i = 0; i < pt.max_indices[0]; i++)
            {
                std::string guess = a->ordered_values[i];
                thread_guesses.emplace_back(guess);
            }

            // 临界区：生成结果写入 pt 的猜测向量组，保证线程访问独立安全
            #pragma omp critical
            {
                guesses.insert(guesses.end(), thread_guesses.begin(), thread_guesses.end());
                total_guesses += thread_guesses.size();
            }
        }
    }
    else
    {
        string guess;
        int seg_idx = 0;
        // 这个for循环的作用：给当前PT的所有segment赋予实际的值（最后一个segment除外）
        // segment值根据curr_indices中对应的值加以确定
        // 这个for循环你看不懂也没太大问题，并行算法不涉及这里的加速
        for (int idx : pt.curr_indices)
        {
            if (pt.content[seg_idx].type == 1)
            {
                guess += m.letters[m.FindLetter(pt.content[seg_idx])].ordered_values[idx];
            }
            if (pt.content[seg_idx].type == 2)
            {
                guess += m.digits[m.FindDigit(pt.content[seg_idx])].ordered_values[idx];
            }
            if (pt.content[seg_idx].type == 3)
            {
                guess += m.symbols[m.FindSymbol(pt.content[seg_idx])].ordered_values[idx];
            }
            seg_idx += 1;
            if (seg_idx == pt.content.size() - 1)
            {
                break;
            }
        }

        // 指向最后一个segment的指针，这个指针实际指向模型中的统计数据
        segment *a;
        if (pt.content[pt.content.size() - 1].type == 1)
        {
            a = &m.letters[m.FindLetter(pt.content[pt.content.size() - 1])];
        }
        if (pt.content[pt.content.size() - 1].type == 2)
        {
            a = &m.digits[m.FindDigit(pt.content[pt.content.size() - 1])];
        }
        if (pt.content[pt.content.size() - 1].type == 3)
        {
            a = &m.symbols[m.FindSymbol(pt.content[pt.content.size() - 1])];
        }

        #pragma omp parallel
        {
            // 共享存储所有线程生成的猜测
            std::vector<std::string> thread_guesses;

            // 主要逻辑：循环生成猜测，OpenMP 自动分配线程任务
            #pragma omp for nowait
            for (int i = 0; i < pt.max_indices[pt.content.size() - 1]; i++)
            {
                std::string temp = guess + a->ordered_values[i];
                thread_guesses.emplace_back(temp);
            }

            // 临界区：生成结果写入 pt 的猜测向量组，保证线程访问独立安全
            #pragma omp critical
            {
                guesses.insert(guesses.end(), thread_guesses.begin(), thread_guesses.end());
                total_guesses += thread_guesses.size();
            }
        }
    }
}

// ======================================= //