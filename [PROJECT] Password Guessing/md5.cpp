#include "md5.h"
#include <iomanip>
#include <assert.h>
#include <chrono>

using namespace std;
using namespace chrono;

/**
 * StringProcess: 将单个输入字符串转换成 MD5 计算所需的消息数组
 * @param input 输入
 * @param[out] n_byte 用于给调用者传递额外的返回值，即最终 Byte 数组的长度
 * @return Byte 消息数组
 */
Byte *StringProcess(string input, int *n_byte)
{
	// 将输入的字符串转换为 Byte 为单位的数组
	Byte *blocks = (Byte *)input.c_str();
	int length = input.length();

	// 计算原始消息长度（以比特为单位）
	int bitLength = length * 8;

	// paddingBits: 原始消息需要的 padding 长度（以 bit 为单位）
	// 对于给定的消息，将其补齐至 length%512==448 为止
	// 需要注意的是，即便给定的消息满足 length%512==448，也需要再 pad 512bits
	int paddingBits = bitLength % 512;
	if (paddingBits > 448)
	{
		paddingBits += 512 - (paddingBits - 448);
	}
	else if (paddingBits < 448)
	{
		paddingBits = 448 - paddingBits;
	}
	else if (paddingBits == 448)
	{
		paddingBits = 512;
	}

	// 原始消息需要的 padding 长度（以 Byte 为单位）
	int paddingBytes = paddingBits / 8;
	// 创建最终的字节数组
	// length + paddingBytes + 8:
	// 1. length 为原始消息的长度（bits）
	// 2. paddingBytes 为原始消息需要的 padding 长度（Bytes）
	// 3. 在 pad 到 length%512==448 之后，需要额外附加 64bits 的原始消息长度，即 8 个 bytes
	int paddedLength = length + paddingBytes + 8;
	Byte *paddedMessage = new Byte[paddedLength];

	// 复制原始消息
	memcpy(paddedMessage, blocks, length);

	// 添加填充字节。填充时，第一位为1，后面的所有位均为0。
	// 所以第一个 byte 是 0x80
	paddedMessage[length] = 0x80;							 // 添加一个 0x80 字节
	memset(paddedMessage + length + 1, 0, paddingBytes - 1); // 填充 0 字节

	// 添加消息长度（64比特，小端格式）
	for (int i = 0; i < 8; ++i)
	{
		// 特别注意此处应当将 bitLength 转换为 uint64_t
		// 这里的 length 是原始消息的长度
		paddedMessage[length + paddingBytes + i] = ((uint64_t)length * 8 >> (i * 8)) & 0xFF;
	}

	// 验证长度是否满足要求。此时长度应当是 512bit 的倍数
	int residual = 8 * paddedLength % 512;
	assert(residual == 0);

	// 在填充+添加长度之后，消息被分为 n_blocks 个 512bit 的部分
	*n_byte = paddedLength;
	return paddedMessage;
}

/**
 * CalculatePadded: SIMD 优化中用于计算单个口令填充后完整长度的辅助函数，将在 SIMDStringProcess 中调用
 *					为维持代码一致性和逻辑严谨性，基本只是直接复制了基础 StringProcess 方法中的相应代码
 * @param length 口令原长度（Byte）
 * @return 最终字节数组长度（Byte）
 */
static int CalculatePadded(int length) {
	int bitLength = length * 8;
	int paddingBits = bitLength % 512;
	if (paddingBits > 448)
	{
		paddingBits += 512 - (paddingBits - 448);
	}
	else if (paddingBits < 448)
	{
		paddingBits = 448 - paddingBits;
	}
	else if (paddingBits == 448)
	{
		paddingBits = 512;
	}
	int paddingBytes = paddingBits / 8;
	int paddedLength = length + paddingBytes + 8;
	return paddedLength;
}

/**
 * SIMDStringProcess: 将单个输入字符串转换成MD5计算所需的消息数组 -> *SIMD并行化版本*
 * @param inputs 输入
 * @param[out] n_byte 用于给调用者传递额外的返回值(1)，即各个口令最终 Byte 数组的长度
 * @param guess_num 并行同时处理的口令个数
 * @param[out] max_byte 用于给调用者传递额外的返回值(2)，即所有口令最终 Byte 数组长度中的最大值
 * @return Byte 消息数组
 */
Byte *SIMDStringProcess(string *inputs, int *n_byte, int guess_num, int &max_byte)
{
	// maxPaddedLength：最大 Byte 数组长度
	int maxPaddedLength = 0;
	int *paddedLengths = new int[guess_num];

	// 计算每个输入消息的 Byte 数组长度并找到最大值
	for (int i = 0; i < guess_num; i++) {
		paddedLengths[i] = CalculatePadded(inputs[i].length());
		if (paddedLengths[i] > maxPaddedLength) {
			maxPaddedLength = paddedLengths[i];
		}
	}

	// paddedData：SIMD 优化所需的首地址对齐的整合内存块
	// ALIGNMENT： NEON SIMD 向量指令处理数据的对齐要求是 *16 字节对齐*
	// 已经求得所有消息中最大的 paddingByte 数组长度，根据这个长度进行内存对齐即可
	constexpr int ALIGNMENT = 16;
	Byte *paddedData = (Byte *)aligned_alloc(ALIGNMENT, maxPaddedLength * guess_num);

	// 依次对每个原始消息进行 padding
	for (int i = 0; i < guess_num; i++) {
		// 将输入的字符串转换为 Byte 为单位的数组
        Byte *blocks = (Byte *)inputs[i].c_str();
        int length = inputs[i].length();
        int paddedLength = paddedLengths[i];

		// 根据当前索引到整合内存块中取当前消息的 Byte 数组对应的起始地址，步长为 maxPaddedLength
        Byte *paddedMessage = paddedData + i * maxPaddedLength;

        // 复制原始消息
        memcpy(paddedMessage, blocks, length);

        // 添加填充字节。填充时，第一位为 1，后面的所有位均为 0。
		// 所以第一个 byte是 0x80
        paddedMessage[length] = 0x80;										// 添加一个 0x80 字节
        memset(paddedMessage + length + 1, 0, paddedLength - length - 9);	// 填充 0 字节

        // 添加消息长度（64比特，小端格式）
        for (int i1 = 0; i1 < 8; ++i1)
		{
			// 特别注意此处应当将 bitLength 转换为 uint64_t
			// 这里的 length 是原始消息的长度
			paddedMessage[paddedLength - 8 + i1] = ((uint64_t)length * 8 >> (i1 * 8)) & 0xFF;
		}

		// 验证长度是否满足要求。此时长度应当是 512bit 的倍数
		int residual = 8 * paddedLength % 512;
		assert(residual == 0);
    }
	// 将函数局部变量赋给用于给调用者传递额外的返回值的变量
	memcpy(n_byte, paddedLengths, guess_num * sizeof(int));
	max_byte = maxPaddedLength;

	// 善后操作
	delete[] paddedLengths;
	return paddedData;
}


/**
 * MD5Hash: 将单个输入字符串转换成 MD5
 * @param input 输入
 * @param[out] state 用于给调用者传递额外的返回值，即最终的缓冲区，也就是 MD5 的结果
 * @return Byte 消息数组
 */
void MD5Hash(string input, bit32 *state)
{

	Byte *paddedMessage;
	int *messageLength = new int[1];
	for (int i = 0; i < 1; i += 1)
	{
		paddedMessage = StringProcess(input, &messageLength[i]);
		// cout<<messageLength[i]<<endl;
		assert(messageLength[i] == messageLength[0]);
	}
	int n_blocks = messageLength[0] / 64;

	// bit32* state= new bit32[4];
	state[0] = 0x67452301;
	state[1] = 0xefcdab89;
	state[2] = 0x98badcfe;
	state[3] = 0x10325476;

	// 逐 block 地更新 state
	for (int i = 0; i < n_blocks; i += 1)
	{
		bit32 x[16];

		// 下面的处理，在理解上较为复杂
		for (int i1 = 0; i1 < 16; ++i1)
		{
			x[i1] = (paddedMessage[4 * i1 + i * 64]) |
					(paddedMessage[4 * i1 + 1 + i * 64] << 8) |
					(paddedMessage[4 * i1 + 2 + i * 64] << 16) |
					(paddedMessage[4 * i1 + 3 + i * 64] << 24);
		}

		bit32 a = state[0], b = state[1], c = state[2], d = state[3];

		auto start = system_clock::now();
		/* Round 1 */
		FF(a, b, c, d, x[0], s11, 0xd76aa478);
		FF(d, a, b, c, x[1], s12, 0xe8c7b756);
		FF(c, d, a, b, x[2], s13, 0x242070db);
		FF(b, c, d, a, x[3], s14, 0xc1bdceee);
		FF(a, b, c, d, x[4], s11, 0xf57c0faf);
		FF(d, a, b, c, x[5], s12, 0x4787c62a);
		FF(c, d, a, b, x[6], s13, 0xa8304613);
		FF(b, c, d, a, x[7], s14, 0xfd469501);
		FF(a, b, c, d, x[8], s11, 0x698098d8);
		FF(d, a, b, c, x[9], s12, 0x8b44f7af);
		FF(c, d, a, b, x[10], s13, 0xffff5bb1);
		FF(b, c, d, a, x[11], s14, 0x895cd7be);
		FF(a, b, c, d, x[12], s11, 0x6b901122);
		FF(d, a, b, c, x[13], s12, 0xfd987193);
		FF(c, d, a, b, x[14], s13, 0xa679438e);
		FF(b, c, d, a, x[15], s14, 0x49b40821);

		/* Round 2 */
		GG(a, b, c, d, x[1], s21, 0xf61e2562);
		GG(d, a, b, c, x[6], s22, 0xc040b340);
		GG(c, d, a, b, x[11], s23, 0x265e5a51);
		GG(b, c, d, a, x[0], s24, 0xe9b6c7aa);
		GG(a, b, c, d, x[5], s21, 0xd62f105d);
		GG(d, a, b, c, x[10], s22, 0x2441453);
		GG(c, d, a, b, x[15], s23, 0xd8a1e681);
		GG(b, c, d, a, x[4], s24, 0xe7d3fbc8);
		GG(a, b, c, d, x[9], s21, 0x21e1cde6);
		GG(d, a, b, c, x[14], s22, 0xc33707d6);
		GG(c, d, a, b, x[3], s23, 0xf4d50d87);
		GG(b, c, d, a, x[8], s24, 0x455a14ed);
		GG(a, b, c, d, x[13], s21, 0xa9e3e905);
		GG(d, a, b, c, x[2], s22, 0xfcefa3f8);
		GG(c, d, a, b, x[7], s23, 0x676f02d9);
		GG(b, c, d, a, x[12], s24, 0x8d2a4c8a);

		/* Round 3 */
		HH(a, b, c, d, x[5], s31, 0xfffa3942);
		HH(d, a, b, c, x[8], s32, 0x8771f681);
		HH(c, d, a, b, x[11], s33, 0x6d9d6122);
		HH(b, c, d, a, x[14], s34, 0xfde5380c);
		HH(a, b, c, d, x[1], s31, 0xa4beea44);
		HH(d, a, b, c, x[4], s32, 0x4bdecfa9);
		HH(c, d, a, b, x[7], s33, 0xf6bb4b60);
		HH(b, c, d, a, x[10], s34, 0xbebfbc70);
		HH(a, b, c, d, x[13], s31, 0x289b7ec6);
		HH(d, a, b, c, x[0], s32, 0xeaa127fa);
		HH(c, d, a, b, x[3], s33, 0xd4ef3085);
		HH(b, c, d, a, x[6], s34, 0x4881d05);
		HH(a, b, c, d, x[9], s31, 0xd9d4d039);
		HH(d, a, b, c, x[12], s32, 0xe6db99e5);
		HH(c, d, a, b, x[15], s33, 0x1fa27cf8);
		HH(b, c, d, a, x[2], s34, 0xc4ac5665);

		/* Round 4 */
		II(a, b, c, d, x[0], s41, 0xf4292244);
		II(d, a, b, c, x[7], s42, 0x432aff97);
		II(c, d, a, b, x[14], s43, 0xab9423a7);
		II(b, c, d, a, x[5], s44, 0xfc93a039);
		II(a, b, c, d, x[12], s41, 0x655b59c3);
		II(d, a, b, c, x[3], s42, 0x8f0ccc92);
		II(c, d, a, b, x[10], s43, 0xffeff47d);
		II(b, c, d, a, x[1], s44, 0x85845dd1);
		II(a, b, c, d, x[8], s41, 0x6fa87e4f);
		II(d, a, b, c, x[15], s42, 0xfe2ce6e0);
		II(c, d, a, b, x[6], s43, 0xa3014314);
		II(b, c, d, a, x[13], s44, 0x4e0811a1);
		II(a, b, c, d, x[4], s41, 0xf7537e82);
		II(d, a, b, c, x[11], s42, 0xbd3af235);
		II(c, d, a, b, x[2], s43, 0x2ad7d2bb);
		II(b, c, d, a, x[9], s44, 0xeb86d391);

		state[0] += a;
		state[1] += b;
		state[2] += c;
		state[3] += d;
	}

	// 下面的处理，在理解上较为复杂
	for (int i = 0; i < 4; i++)
	{
		uint32_t value = state[i];
		state[i] = ((value & 0xff) << 24) |		 // 将最低字节移到最高位
				   ((value & 0xff00) << 8) |	 // 将次低字节左移
				   ((value & 0xff0000) >> 8) |	 // 将次高字节右移
				   ((value & 0xff000000) >> 24); // 将最高字节移到最低位
	}

	// 输出最终的 hash 结果
	// for (int i1 = 0; i1 < 4; i1 += 1)
	// {
	// 	cout << std::setw(8) << std::setfill('0') << hex << state[i1];
	// }
	// cout << endl;

	// 释放动态分配的内存
	// 实现 SIMD 并行算法的时候，也请记得及时回收内存！
	delete[] paddedMessage;
	delete[] messageLength;
}

/**
 * SIMDMD5Hash: 将*多个*输入字符串转换成 MD5 -> *SIMD并行化版本*
 * @param inputs 输入
 * @param[out] state 用于给调用者传递额外的返回值，即最终的缓冲区，也就是MD5的结果
 * @return Byte 消息数组
 */
void SIMDMD5Hash(string *inputs, bit32 *state)
{
	// PARA_NUM：常数，设置并行度，即并行同时处理的消息个数
	// 由于并行架构和指令的限制，一般默认是 4 且不改动
	const int PARA_NUM = 4;

	// messageLenths：记录各个消息的 Byte 数组长度
	int *messageLengths = new int[PARA_NUM];

	// maxByte：记录最大Byte数组长度
	int maxByte = 0;

	// 对所有消息进行初始化，获得整合 *16 字节对齐* 内存块
	// 同时获取各个消息的 Byte 数组长度和最大 Byte 数组长度
	Byte *paddedData = SIMDStringProcess(inputs, messageLengths, PARA_NUM, maxByte);

	// 将各消息分为 n_blocks 个 512bit 的部分
	int *n_blocks = new int[PARA_NUM];
	for (int i = 0; i < PARA_NUM; i++) {
		n_blocks[i] = messageLengths[i] / 64;
	}

	// maxBlocks：最长 Byte 数组对应的 512bit 部分数
	// 这也是后面位处理所需的总循环轮数
	const int maxBlocks = maxByte / 64;

	// 使用 NEON 的 vdupq_n_u32 指令
	// 为 SIMD 并行 MD5 哈希计算 初始化 4 个 *并行* 的状态寄存器
	uint32x4_t state_a = vdupq_n_u32(0x67452301);
    uint32x4_t state_b = vdupq_n_u32(0xefcdab89);
    uint32x4_t state_c = vdupq_n_u32(0x98badcfe);
    uint32x4_t state_d = vdupq_n_u32(0x10325476);

	// 逐 block 地更新 state
	for (int i = 0; i < maxBlocks; i += 1) {
		// 将每个 block 再分为 4 Byte 大小的 M 块，用作后面多轮位运算的参数
		// 同样使用 uint32x4_t 型，构造 4 个 32-bit 无符号整数的向量，满足并行化要求
		uint32x4_t M[16];

		// 逐次读取 block 再分解后的小块数据到 M 向量块中
		for (int i1 = 0; i1 < 16; ++i1)
		{
			uint32_t temp_vec[4];
			for (int i2 = 0; i2 < PARA_NUM; ++i2) {
				temp_vec[i2] = ((uint32_t)paddedData[4 * i1 + maxByte * i2 + i * 64]) |
						   ((uint32_t)paddedData[4 * i1 + 1 + maxByte * i2 + i * 64] << 8) |
						   ((uint32_t)paddedData[4 * i1 + 2 + maxByte * i2 + i * 64] << 16) |
						   ((uint32_t)paddedData[4 * i1 + 3 + maxByte * i2 + i * 64] << 24);
			}
			M[i1] = vld1q_u32(temp_vec);
		}

		// maskArray：掩码数组
		// activeMask：掩码向量
		// 构造掩码的原因是，每个消息最终 Byte 数组的大小可能不同
		// 如果该轮运算某一消息的 Byte 数组已经结束，若再对其进行位运算，最终可能会产生错误的 MD5
		// 掩码记录当前消息是否还需要继续进行位运算（是否还有 Byte 数组剩余）：
		// *是 ==> 0xFFFFFFFF*
		// *否 ==> 0x00000000*
		// 将在每个位运算函数内检查掩码，如果全为 0，对相应消息就不再进行该位运算
		uint32_t maskArray[PARA_NUM];
		for (int i1 = 0; i1 < PARA_NUM; ++i1) {
			maskArray[i1] = (i < n_blocks[i1]) ? 0xFFFFFFFF : 0x00000000;
		}
		uint32x4_t activeMask = vld1q_u32(maskArray);

		// 记录初始状态
		uint32x4_t a = state_a, b = state_b, c = state_c, d = state_d;

		auto start = system_clock::now();
		/* Round 1 */
		FF_SIMD(a, b, c, d, M[0], s11, 0xd76aa478, activeMask);
		FF_SIMD(d, a, b, c, M[1], s12, 0xe8c7b756, activeMask);
		FF_SIMD(c, d, a, b, M[2], s13, 0x242070db, activeMask);
		FF_SIMD(b, c, d, a, M[3], s14, 0xc1bdceee, activeMask);
		FF_SIMD(a, b, c, d, M[4], s11, 0xf57c0faf, activeMask);
		FF_SIMD(d, a, b, c, M[5], s12, 0x4787c62a, activeMask);
		FF_SIMD(c, d, a, b, M[6], s13, 0xa8304613, activeMask);
		FF_SIMD(b, c, d, a, M[7], s14, 0xfd469501, activeMask);
		FF_SIMD(a, b, c, d, M[8], s11, 0x698098d8, activeMask);
		FF_SIMD(d, a, b, c, M[9], s12, 0x8b44f7af, activeMask);
		FF_SIMD(c, d, a, b, M[10], s13, 0xffff5bb1, activeMask);
		FF_SIMD(b, c, d, a, M[11], s14, 0x895cd7be, activeMask);
		FF_SIMD(a, b, c, d, M[12], s11, 0x6b901122, activeMask);
		FF_SIMD(d, a, b, c, M[13], s12, 0xfd987193, activeMask);
		FF_SIMD(c, d, a, b, M[14], s13, 0xa679438e, activeMask);
		FF_SIMD(b, c, d, a, M[15], s14, 0x49b40821, activeMask);

		/* Round 2 */
		GG_SIMD(a, b, c, d, M[1], s21, 0xf61e2562, activeMask);
		GG_SIMD(d, a, b, c, M[6], s22, 0xc040b340, activeMask);
		GG_SIMD(c, d, a, b, M[11], s23, 0x265e5a51, activeMask);
		GG_SIMD(b, c, d, a, M[0], s24, 0xe9b6c7aa, activeMask);
		GG_SIMD(a, b, c, d, M[5], s21, 0xd62f105d, activeMask);
		GG_SIMD(d, a, b, c, M[10], s22, 0x2441453, activeMask);
		GG_SIMD(c, d, a, b, M[15], s23, 0xd8a1e681, activeMask);
		GG_SIMD(b, c, d, a, M[4], s24, 0xe7d3fbc8, activeMask);
		GG_SIMD(a, b, c, d, M[9], s21, 0x21e1cde6, activeMask);
		GG_SIMD(d, a, b, c, M[14], s22, 0xc33707d6, activeMask);
		GG_SIMD(c, d, a, b, M[3], s23, 0xf4d50d87, activeMask);
		GG_SIMD(b, c, d, a, M[8], s24, 0x455a14ed, activeMask);
		GG_SIMD(a, b, c, d, M[13], s21, 0xa9e3e905, activeMask);
		GG_SIMD(d, a, b, c, M[2], s22, 0xfcefa3f8, activeMask);
		GG_SIMD(c, d, a, b, M[7], s23, 0x676f02d9, activeMask);
		GG_SIMD(b, c, d, a, M[12], s24, 0x8d2a4c8a, activeMask);

		/* Round 3 */
		HH_SIMD(a, b, c, d, M[5], s31, 0xfffa3942, activeMask);
		HH_SIMD(d, a, b, c, M[8], s32, 0x8771f681, activeMask);
		HH_SIMD(c, d, a, b, M[11], s33, 0x6d9d6122, activeMask);
		HH_SIMD(b, c, d, a, M[14], s34, 0xfde5380c, activeMask);
		HH_SIMD(a, b, c, d, M[1], s31, 0xa4beea44, activeMask);
		HH_SIMD(d, a, b, c, M[4], s32, 0x4bdecfa9, activeMask);
		HH_SIMD(c, d, a, b, M[7], s33, 0xf6bb4b60, activeMask);
		HH_SIMD(b, c, d, a, M[10], s34, 0xbebfbc70, activeMask);
		HH_SIMD(a, b, c, d, M[13], s31, 0x289b7ec6, activeMask);
		HH_SIMD(d, a, b, c, M[0], s32, 0xeaa127fa, activeMask);
		HH_SIMD(c, d, a, b, M[3], s33, 0xd4ef3085, activeMask);
		HH_SIMD(b, c, d, a, M[6], s34, 0x4881d05, activeMask);
		HH_SIMD(a, b, c, d, M[9], s31, 0xd9d4d039, activeMask);
		HH_SIMD(d, a, b, c, M[12], s32, 0xe6db99e5, activeMask);
		HH_SIMD(c, d, a, b, M[15], s33, 0x1fa27cf8, activeMask);
		HH_SIMD(b, c, d, a, M[2], s34, 0xc4ac5665, activeMask);

		/* Round 4 */
		II_SIMD(a, b, c, d, M[0], s41, 0xf4292244, activeMask);
		II_SIMD(d, a, b, c, M[7], s42, 0x432aff97, activeMask);
		II_SIMD(c, d, a, b, M[14], s43, 0xab9423a7, activeMask);
		II_SIMD(b, c, d, a, M[5], s44, 0xfc93a039, activeMask);
		II_SIMD(a, b, c, d, M[12], s41, 0x655b59c3, activeMask);
		II_SIMD(d, a, b, c, M[3], s42, 0x8f0ccc92, activeMask);
		II_SIMD(c, d, a, b, M[10], s43, 0xffeff47d, activeMask);
		II_SIMD(b, c, d, a, M[1], s44, 0x85845dd1, activeMask);
		II_SIMD(a, b, c, d, M[8], s41, 0x6fa87e4f, activeMask);
		II_SIMD(d, a, b, c, M[15], s42, 0xfe2ce6e0, activeMask);
		II_SIMD(c, d, a, b, M[6], s43, 0xa3014314, activeMask);
		II_SIMD(b, c, d, a, M[13], s44, 0x4e0811a1, activeMask);
		II_SIMD(a, b, c, d, M[4], s41, 0xf7537e82, activeMask);
		II_SIMD(d, a, b, c, M[11], s42, 0xbd3af235, activeMask);
		II_SIMD(c, d, a, b, M[2], s43, 0x2ad7d2bb, activeMask);
		II_SIMD(b, c, d, a, M[9], s44, 0xeb86d391, activeMask);

		// 使用 NEON 的 vaddq_u32 指令
		// 依次加回各消息的位运算最终结果
		state_a = vaddq_u32(state_a, a);
		state_b = vaddq_u32(state_b, b);
		state_c = vaddq_u32(state_c, c);
		state_d = vaddq_u32(state_d, d);

	}

	// 保存最终结果
	vst1q_u32(state + 0, state_a);
	vst1q_u32(state + 4, state_b);
	vst1q_u32(state + 8, state_c);
	vst1q_u32(state + 12, state_d);

	// 将小端转换为大端格式
	// 最终 4*4 的 state被填满，每 4 个 state 代表一个 MD5，依次读取即可
	for (int i = 0; i < 16; i++)
	{
		uint32_t value = state[i];
		state[i] = ((value & 0xff) << 24) |		 // 将最低字节移到最高位
					((value & 0xff00) << 8) |	 // 将次低字节左移
					((value & 0xff0000) >> 8) |	 // 将次高字节右移
					((value & 0xff000000) >> 24); // 将最高字节移到最低位
	}

	// 回收内存
	delete[] paddedData;
	delete[] messageLengths;
	delete[] n_blocks;
}