#include "PCFG.h"
using namespace std;

void PriorityQueue::CalProb(PT &pt) {
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

void PriorityQueue::init() {
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

void PriorityQueue::PopNext() {
    // 对优先队列最前面的PT，首先利用这个PT生成一系列猜测
    // <=== 串行方法 ===>
    // Generate(priority.front());
    // <=== CUDA方法 ===>
    CUDAGenerate(priority.front());

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
vector<PT> PT::NewPTs() {
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
void PriorityQueue::Generate(PT pt) {
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

// <================ CUDA 实现 ================>

// 初始化
void PriorityQueue::InitCudaBuffers() {
    d_capacity = MAX_N;
    cudaMalloc(&d_values, d_capacity * MAX_STR_LEN);
    cudaMalloc(&d_output, d_capacity * MAX_GUESS_LEN);
    cudaMalloc(&d_prefix, MAX_STR_LEN);
}

// 释放
void PriorityQueue::FreeCudaBuffers() {
    if (d_values) cudaFree(d_values);
    if (d_output) cudaFree(d_output);
    if (d_prefix) cudaFree(d_prefix);
    d_values = d_output = d_prefix = nullptr;
    d_capacity = 0;
}

// CPU -> GPU
void PriorityQueue::LoadAllOrderedValuesToGPU() {
    for (auto &seg : m.letters) total_letter_count += seg.ordered_values.size();
    for (auto &seg : m.digits)  total_digit_count += seg.ordered_values.size();
    for (auto &seg : m.symbols) total_symbol_count += seg.ordered_values.size();

    char *h_letters_all = new char[total_letter_count * MAX_STR_LEN]();
    char *h_digits_all  = new char[total_digit_count * MAX_STR_LEN]();
    char *h_symbols_all = new char[total_symbol_count * MAX_STR_LEN]();

    vector<int> h_letter_offsets, h_digit_offsets, h_symbol_offsets;
    int offset = 0;
    for (auto &seg : m.letters) {
        h_letter_offsets.push_back(offset);
        for (auto &v : seg.ordered_values) {
            strncpy(h_letters_all + offset * MAX_STR_LEN, v.c_str(), MAX_STR_LEN - 1);
            offset++;
        }
    }
    offset = 0;
    for (auto &seg : m.digits) {
        h_digit_offsets.push_back(offset);
        for (auto &v : seg.ordered_values) {
            strncpy(h_digits_all + offset * MAX_STR_LEN, v.c_str(), MAX_STR_LEN - 1);
            offset++;
        }
    }
    offset = 0;
    for (auto &seg : m.symbols) {
        h_symbol_offsets.push_back(offset);
        for (auto &v : seg.ordered_values) {
            strncpy(h_symbols_all + offset * MAX_STR_LEN, v.c_str(), MAX_STR_LEN - 1);
            offset++;
        }
    }

    cudaMalloc(&d_letters_all, total_letter_count * MAX_STR_LEN);
    cudaMalloc(&d_digits_all,  total_digit_count  * MAX_STR_LEN);
    cudaMalloc(&d_symbols_all, total_symbol_count * MAX_STR_LEN);
    cudaMemcpy(d_letters_all, h_letters_all, total_letter_count * MAX_STR_LEN, cudaMemcpyHostToDevice);
    cudaMemcpy(d_digits_all,  h_digits_all,  total_digit_count  * MAX_STR_LEN, cudaMemcpyHostToDevice);
    cudaMemcpy(d_symbols_all, h_symbols_all, total_symbol_count * MAX_STR_LEN, cudaMemcpyHostToDevice);

    cudaMalloc(&d_letter_offsets, h_letter_offsets.size() * sizeof(int));
    cudaMalloc(&d_digit_offsets,  h_digit_offsets.size()  * sizeof(int));
    cudaMalloc(&d_symbol_offsets, h_symbol_offsets.size() * sizeof(int));
    cudaMemcpy(d_letter_offsets, h_letter_offsets.data(), h_letter_offsets.size() * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d_digit_offsets,  h_digit_offsets.data(),  h_digit_offsets.size()  * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d_symbol_offsets, h_symbol_offsets.data(), h_symbol_offsets.size() * sizeof(int), cudaMemcpyHostToDevice);

    // 缓存在 host 端
    h_letter_offsets_gpu = h_letter_offsets;
    h_digit_offsets_gpu  = h_digit_offsets;
    h_symbol_offsets_gpu = h_symbol_offsets;

    delete[] h_letters_all;
    delete[] h_digits_all;
    delete[] h_symbols_all;
}

// 释放
void PriorityQueue::FreeGlobalBuffers() {
    cudaFree(d_letters_all); cudaFree(d_digits_all); cudaFree(d_symbols_all);
    cudaFree(d_letter_offsets); cudaFree(d_digit_offsets); cudaFree(d_symbol_offsets);
}

__global__ void generate_kernel_indexed(char *d_values_all, int base_offset, int value_count, char *d_output, int max_len) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= value_count) return;
    char *src = d_values_all + (base_offset + idx) * max_len;
    char *dst = d_output + idx * max_len;
    for (int i = 0; i < max_len; i += 16) {
        int4 data = *((int4 *)(src + i));
        *((int4 *)(dst + i)) = data;
    }
}

__global__ void generate_kernel_indexed_concat(
    char *prefix, int prefix_len,
    char *d_values_all, int base_offset,
    int value_count, char *d_output, int max_suffix_len
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= value_count) return;
    char *src = d_values_all + (base_offset + idx) * max_suffix_len;
    char *dst = d_output + idx * MAX_GUESS_LEN;
    for (int i = 0; i < prefix_len; ++i)
        dst[i] = prefix[i];
    for (int i = 0; i < max_suffix_len; i += 16) {
        int4 data = *((int4 *)(src + i));
        *((int4 *)(dst + prefix_len + i)) = data;
    }
}

void PriorityQueue::CUDAGenerate(PT pt) {
    CalProb(pt);

    if (pt.content.size() == 1) {
        segment *a;
        char *d_pool = nullptr;
        int seg_idx = -1;

        if (pt.content[0].type == 1) {
            seg_idx = m.FindLetter(pt.content[0]);
            d_pool = d_letters_all;
        } else if (pt.content[0].type == 2) {
            seg_idx = m.FindDigit(pt.content[0]);
            d_pool = d_digits_all;
        } else {
            seg_idx = m.FindSymbol(pt.content[0]);
            d_pool = d_symbols_all;
        }

        int base_offset = (pt.content[0].type == 1) ? h_letter_offsets_gpu[seg_idx] :
                          (pt.content[0].type == 2) ? h_digit_offsets_gpu[seg_idx] :
                                                      h_symbol_offsets_gpu[seg_idx];

        a = (pt.content[0].type == 1) ? &m.letters[seg_idx] :
            (pt.content[0].type == 2) ? &m.digits[seg_idx] :
                                       &m.symbols[seg_idx];

        int N = pt.max_indices[0];
        if (N < 100000) {
            for (int i = 0; i < N; ++i)
                guesses.push_back(a->ordered_values[i]);
            total_guesses += N;
            return;
        }
        if (N > d_capacity) {
            std::cerr << "CUDAGenerate error: batch size " << N << " exceeds GPU buffer capacity " << d_capacity << "\n";
            return;
        }

        int blockSize = 256;
        int numBlocks = (N + blockSize - 1) / blockSize;
        generate_kernel_indexed<<<numBlocks, blockSize>>>(d_pool, base_offset, N, d_output, MAX_STR_LEN);
        cudaDeviceSynchronize();

        char *h_output = new char[N * MAX_STR_LEN]();
        cudaMemcpy(h_output, d_output, N * MAX_STR_LEN, cudaMemcpyDeviceToHost);
        for (int i = 0; i < N; ++i)
            guesses.emplace_back(h_output + i * MAX_STR_LEN);
        total_guesses += N;
        delete[] h_output;
    } else {
        // 多段
        string prefix;
        for (int seg_idx = 0; seg_idx < pt.content.size() - 1; ++seg_idx) {
            int idx = pt.curr_indices[seg_idx];
            if (pt.content[seg_idx].type == 1)
                prefix += m.letters[m.FindLetter(pt.content[seg_idx])].ordered_values[idx];
            else if (pt.content[seg_idx].type == 2)
                prefix += m.digits[m.FindDigit(pt.content[seg_idx])].ordered_values[idx];
            else
                prefix += m.symbols[m.FindSymbol(pt.content[seg_idx])].ordered_values[idx];
        }

        int seg_idx = -1;
        char *d_pool = nullptr;

        if (pt.content.back().type == 1) {
            seg_idx = m.FindLetter(pt.content.back());
            d_pool = d_letters_all;
        } else if (pt.content.back().type == 2) {
            seg_idx = m.FindDigit(pt.content.back());
            d_pool = d_digits_all;
        } else {
            seg_idx = m.FindSymbol(pt.content.back());
            d_pool = d_symbols_all;
        }

        int base_offset = (pt.content.back().type == 1) ? h_letter_offsets_gpu[seg_idx] :
                          (pt.content.back().type == 2) ? h_digit_offsets_gpu[seg_idx] :
                                                          h_symbol_offsets_gpu[seg_idx];

        segment *a = (pt.content.back().type == 1) ? &m.letters[seg_idx] :
                       (pt.content.back().type == 2) ? &m.digits[seg_idx] :
                                                       &m.symbols[seg_idx];

        int N = pt.max_indices.back();
        if (N < 100000) {
            for (int i = 0; i < N; ++i)
                guesses.push_back(prefix + a->ordered_values[i]);
            total_guesses += N;
            return;
        }
        if (N > d_capacity) {
            std::cerr << "CUDAGenerate error: batch size " << N << " exceeds GPU buffer capacity " << d_capacity << "\n";
            return;
        }

        char h_prefix[MAX_STR_LEN] = {};
        strncpy(h_prefix, prefix.c_str(), MAX_STR_LEN - 1);
        cudaMemcpy(d_prefix, h_prefix, MAX_STR_LEN, cudaMemcpyHostToDevice);

        int blockSize = 256;
        int numBlocks = (N + blockSize - 1) / blockSize;
        generate_kernel_indexed_concat<<<numBlocks, blockSize>>>(
            d_prefix, prefix.length(), d_pool, base_offset, N, d_output, MAX_STR_LEN);
        cudaDeviceSynchronize();

        char *h_output = new char[N * MAX_GUESS_LEN]();
        cudaMemcpy(h_output, d_output, N * MAX_GUESS_LEN, cudaMemcpyDeviceToHost);
        for (int i = 0; i < N; ++i)
            guesses.emplace_back(h_output + i * MAX_GUESS_LEN);
        total_guesses += N;
        delete[] h_output;
    }
}

// 核函数：每个线程负责一个PT的一个猜测
__global__ void generate_kernel_multi_pt(
    char *d_prefixes,           // batch_size * MAX_STR_LEN， 每个PT的prefix字符串
    int *d_last_segment_types,  // batch_size，最后segment类型
    int *d_last_segment_offsets,// batch_size，最后segment字符串的base offset
    int *d_last_segment_max_indices, // batch_size，最后segmentmax_indices
    int *d_pt_indices,          // total_guess_count，映射每个猜测属于哪个PT
    int *d_value_indices,       // total_guess_count，映射每个猜测在最后segment的哪个值
    char *d_letters_all,
    char *d_digits_all,
    char *d_symbols_all,
    char *d_output,
    int total_guess_count
) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= total_guess_count) return;

    int pt_idx = d_pt_indices[idx];
    int value_idx = d_value_indices[idx];

    // 计算prefix长度 - 修复：使用更安全的长度计算
    int prefix_len = 0;
    for (int i = 0; i < MAX_STR_LEN - 1; ++i) {  // 保留空间给'\0'
        if (d_prefixes[pt_idx * MAX_STR_LEN + i] == '\0') {
            prefix_len = i;
            break;
        }
        prefix_len = i + 1;  // 如果没有找到'\0'，则使用实际长度
    }

    // 输出指针
    char *dst = d_output + idx * MAX_GUESS_LEN;
    
    // 初始化输出缓冲区 - 修复：确保清零
    for (int i = 0; i < MAX_GUESS_LEN; ++i) {
        dst[i] = '\0';
    }

    // 拷贝 prefix
    for (int i = 0; i < prefix_len && i < MAX_GUESS_LEN - 1; ++i) {
        dst[i] = d_prefixes[pt_idx * MAX_STR_LEN + i];
    }

    // 根据最后segment类型，选对应的字符串池
    int seg_type = d_last_segment_types[pt_idx];
    int base_offset = d_last_segment_offsets[pt_idx];
    char *pool = nullptr;

    if (seg_type == 1) {
        pool = d_letters_all;
    } else if (seg_type == 2) {
        pool = d_digits_all;
    } else {
        pool = d_symbols_all;
    }

    // 修复：检查越界和安全拷贝
    if (pool != nullptr && value_idx >= 0) {
        char *src = pool + (base_offset + value_idx) * MAX_STR_LEN;  // 使用MAX_STR_LEN而不是max_suffix_len
        
        // 安全拷贝最后segment字符串
        int remaining_space = MAX_GUESS_LEN - prefix_len - 1;  // 保留空间给'\0'
        for (int i = 0; i < MAX_STR_LEN && i < remaining_space; ++i) {
            if (src[i] == '\0') break;  // 遇到字符串结束符就停止
            dst[prefix_len + i] = src[i];
        }
    }
}

// 批量处理函数
void PriorityQueue::CUDAPopNext() {
    int batch_size = BATCH_SIZE;
    if (batch_size > (int)priority.size()) batch_size = priority.size();
    if (batch_size == 0) return;

    // 复制批量PT
    vector<PT> batch_pts(priority.begin(), priority.begin() + batch_size);

    // 计算每个PT最后segment max_indices总和（总猜测数）
    int total_guess_count = 0;
    vector<int> last_seg_max_indices(batch_size);
    for (int i = 0; i < batch_size; ++i) {
        if (batch_pts[i].max_indices.empty()) {
            std::cerr << "Error: PT " << i << " has empty max_indices" << std::endl;
            return;
        }
        last_seg_max_indices[i] = batch_pts[i].max_indices.back();
        total_guess_count += last_seg_max_indices[i];
    }
    
    if (total_guess_count == 0) {
        std::cerr << "Warning: No guesses to generate" << std::endl;
        return;
    }
    
    if (total_guess_count > d_capacity) {
        std::cerr << "Batch total guesses " << total_guess_count << " exceed GPU capacity " << d_capacity << std::endl;
        return;
    }

    // Host端构造 prefix 字符串数组 - 修复：确保正确的字符串构造
    char *h_prefixes = new char[batch_size * MAX_STR_LEN]();
    for (int i = 0; i < batch_size; ++i) {
        string prefix;
        
        // 只处理前面的segment，最后一个segment在GPU上处理
        int num_prefix_segments = (int)batch_pts[i].content.size() - 1;
        for (int seg_idx = 0; seg_idx < num_prefix_segments; ++seg_idx) {
            if (seg_idx >= (int)batch_pts[i].curr_indices.size()) {
                std::cerr << "Error: curr_indices size mismatch for PT " << i << std::endl;
                break;
            }
            
            int idx = batch_pts[i].curr_indices[seg_idx];
            int seg_type = batch_pts[i].content[seg_idx].type;
            
            // 修复：添加边界检查
            if (seg_type == 1) {
                int letter_idx = m.FindLetter(batch_pts[i].content[seg_idx]);
                if (letter_idx >= 0 && letter_idx < (int)m.letters.size() && 
                    idx >= 0 && idx < (int)m.letters[letter_idx].ordered_values.size()) {
                    prefix += m.letters[letter_idx].ordered_values[idx];
                }
            } else if (seg_type == 2) {
                int digit_idx = m.FindDigit(batch_pts[i].content[seg_idx]);
                if (digit_idx >= 0 && digit_idx < (int)m.digits.size() && 
                    idx >= 0 && idx < (int)m.digits[digit_idx].ordered_values.size()) {
                    prefix += m.digits[digit_idx].ordered_values[idx];
                }
            } else {
                int symbol_idx = m.FindSymbol(batch_pts[i].content[seg_idx]);
                if (symbol_idx >= 0 && symbol_idx < (int)m.symbols.size() && 
                    idx >= 0 && idx < (int)m.symbols[symbol_idx].ordered_values.size()) {
                    prefix += m.symbols[symbol_idx].ordered_values[idx];
                }
            }
        }
        
        // 安全拷贝prefix
        size_t copy_len = min(prefix.length(), (size_t)(MAX_STR_LEN - 1));
        strncpy(h_prefixes + i * MAX_STR_LEN, prefix.c_str(), copy_len);
        h_prefixes[i * MAX_STR_LEN + copy_len] = '\0';  // 确保以'\0'结尾
    }

    // Host端准备最后segment类型数组和offset数组 - 修复：添加错误检查
    int *h_last_segment_types = new int[batch_size];
    int *h_last_segment_offsets = new int[batch_size];
    int *h_last_segment_max_indices = new int[batch_size];

    for (int i = 0; i < batch_size; ++i) {        
        int seg_type = batch_pts[i].content.back().type;
        h_last_segment_types[i] = seg_type;
        h_last_segment_max_indices[i] = batch_pts[i].max_indices.back();

        int seg_idx = -1;
        if (seg_type == 1) {
            seg_idx = m.FindLetter(batch_pts[i].content.back());
        } else if (seg_type == 2) {
            seg_idx = m.FindDigit(batch_pts[i].content.back());
        } else {
            seg_idx = m.FindSymbol(batch_pts[i].content.back());
        }

        // 计算偏移 - 修复：添加边界检查
        int offset = 0;
        if (seg_type == 1 && seg_idx < (int)h_letter_offsets_gpu.size()) {
            offset = h_letter_offsets_gpu[seg_idx];
        } else if (seg_type == 2 && seg_idx < (int)h_digit_offsets_gpu.size()) {
            offset = h_digit_offsets_gpu[seg_idx];
        } else if (seg_type == 3 && seg_idx < (int)h_symbol_offsets_gpu.size()) {
            offset = h_symbol_offsets_gpu[seg_idx];
        }

        h_last_segment_offsets[i] = offset;
    }

    // 为每个猜测准备PT索引和最后segment的value索引数组
    int *h_pt_indices = new int[total_guess_count];
    int *h_value_indices = new int[total_guess_count];

    int pos = 0;
    for (int i = 0; i < batch_size; ++i) {
        for (int v_idx = 0; v_idx < last_seg_max_indices[i]; ++v_idx) {
            h_pt_indices[pos] = i;
            h_value_indices[pos] = v_idx;
            pos++;
        }
    }

    // GPU内存分配和复制 - 修复：避免goto跳过变量初始化
    char *d_prefixes = nullptr;
    int *d_last_segment_types = nullptr;
    int *d_last_segment_offsets = nullptr;
    int *d_last_segment_max_indices = nullptr;
    int *d_pt_indices = nullptr;
    int *d_value_indices = nullptr;
    
    // 预先声明所有变量避免goto跳过初始化
    int blockSize = 256;
    int numBlocks = (total_guess_count + blockSize - 1) / blockSize;
    char *h_output = nullptr;
    int valid_guesses = 0;

    // GPU内存分配
    do {
        cudaMalloc(&d_prefixes, batch_size * MAX_STR_LEN);
        cudaMalloc(&d_last_segment_types, batch_size * sizeof(int));
        cudaMalloc(&d_last_segment_offsets, batch_size * sizeof(int));
        cudaMalloc(&d_last_segment_max_indices, batch_size * sizeof(int));
        cudaMalloc(&d_pt_indices, total_guess_count * sizeof(int));
        cudaMalloc(&d_value_indices, total_guess_count * sizeof(int));

        // 拷贝数据到GPU
        cudaMemcpy(d_prefixes, h_prefixes, batch_size * MAX_STR_LEN, cudaMemcpyHostToDevice);
        cudaMemcpy(d_last_segment_types, h_last_segment_types, batch_size * sizeof(int), cudaMemcpyHostToDevice);
        cudaMemcpy(d_last_segment_offsets, h_last_segment_offsets, batch_size * sizeof(int), cudaMemcpyHostToDevice);
        cudaMemcpy(d_last_segment_max_indices, h_last_segment_max_indices, batch_size * sizeof(int), cudaMemcpyHostToDevice);
        cudaMemcpy(d_pt_indices, h_pt_indices, total_guess_count * sizeof(int), cudaMemcpyHostToDevice);
        cudaMemcpy(d_value_indices, h_value_indices, total_guess_count * sizeof(int), cudaMemcpyHostToDevice);

        // 分配GPU输出缓冲区
        if (d_capacity < total_guess_count) {
            if (d_output) cudaFree(d_output);
            cudaMalloc(&d_output, total_guess_count * MAX_GUESS_LEN);
            d_capacity = total_guess_count;
        }

        // 调用核函数
        generate_kernel_multi_pt<<<numBlocks, blockSize>>>(
            d_prefixes,
            d_last_segment_types,
            d_last_segment_offsets,
            d_last_segment_max_indices,
            d_pt_indices,
            d_value_indices,
            d_letters_all,
            d_digits_all,
            d_symbols_all,
            d_output,
            total_guess_count
        );

        // 拷贝结果回Host并保存
        h_output = new char[total_guess_count * MAX_GUESS_LEN]();
        cudaMemcpy(h_output, d_output, total_guess_count * MAX_GUESS_LEN, cudaMemcpyDeviceToHost);
        
        // 检查生成的猜测是否有效
        for (int i = 0; i < total_guess_count; ++i) {
            char* guess_ptr = h_output + i * MAX_GUESS_LEN;
            if (strlen(guess_ptr) > 0) {  // 只添加非空猜测
                guesses.emplace_back(guess_ptr);
                valid_guesses++;
            }
        }
        total_guesses += valid_guesses;
                
    } while (false);  // 只执行一次的do-while循环，用于替代goto

    // 清理Host内存
    delete[] h_prefixes;
    delete[] h_last_segment_types;
    delete[] h_last_segment_offsets;
    delete[] h_last_segment_max_indices;
    delete[] h_pt_indices;
    delete[] h_value_indices;
    
    if (h_output) {
        delete[] h_output;
    }

    // 释放GPU内存
    if (d_prefixes) cudaFree(d_prefixes);
    if (d_last_segment_types) cudaFree(d_last_segment_types);
    if (d_last_segment_offsets) cudaFree(d_last_segment_offsets);
    if (d_last_segment_max_indices) cudaFree(d_last_segment_max_indices);
    if (d_pt_indices) cudaFree(d_pt_indices);
    if (d_value_indices) cudaFree(d_value_indices);

    // 删除优先队列中处理的PT
    priority.erase(priority.begin(), priority.begin() + batch_size);
}

// <==========================================>