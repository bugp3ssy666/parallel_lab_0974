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
 * SIMDMD5Hash_4: 将*多个*输入字符串转换成 MD5 -> *SIMD并行化版本*
 *				  --- 并行度 = 4
 * @param inputs 输入
 * @param[out] state 用于给调用者传递额外的返回值，即最终的缓冲区，也就是MD5的结果
 * @return Byte 消息数组
 */
void SIMDMD5Hash_4(string *inputs, bit32 *state)
{
	// PARA_NUM：常数，设置并行度，即并行同时处理的消息个数
	// 由于并行架构和指令的限制，一般默认是 4 不改动
	const int PARA_NUM = 4;

	// messageLenths：记录各个消息的 Byte 数组长度
	int *messageLengths = new int[PARA_NUM];

	// maxByte：记录最大Byte数组长度
	int maxByte = 0;

	// 对所有消息进行初始化，获得整合 *16 字节对齐* 内存块
	// 同时获取各个消息的 Byte 数组长度和最大 Byte 数组长度
	Byte *paddedData = SIMDStringProcess(inputs, messageLengths, PARA_NUM, maxByte);

	// 将各消息分为 n_blocks 个 512bit 的部分
	int n_blocks = messageLengths[0] / 64;

	// 使用 NEON 的 vdupq_n_u32 指令
	// 为 SIMD 并行 MD5 哈希计算 初始化 4 个 *并行* 的状态寄存器
	uint32x4_t state_a = vdupq_n_u32(0x67452301);
    uint32x4_t state_b = vdupq_n_u32(0xefcdab89);
    uint32x4_t state_c = vdupq_n_u32(0x98badcfe);
    uint32x4_t state_d = vdupq_n_u32(0x10325476);

	// 逐 block 地更新 state
	for (int i = 0; i < n_blocks; i += 1) {
		// 将每个 block 再分为 4 Byte 大小的 M 块，用作后面多轮位运算的参数
		// 同样使用 uint32x4_t 型，构造 4 个 32-bit 无符号整数的向量，满足并行化要求
		uint32x4_t M[16];

		// 逐次读取 block 再分解后的小块数据到 M 向量块中
		for (int i1 = 0; i1 < 16; ++i1)
		{
			uint32_t temp_vec[4] = {0};
			for (int i2 = 0; i2 < PARA_NUM; ++i2) {
				temp_vec[i2] = ((uint32_t)paddedData[4 * i1 + maxByte * i2 + i * 64]) |
						   	   ((uint32_t)paddedData[4 * i1 + 1 + maxByte * i2 + i * 64] << 8) |
						  	   ((uint32_t)paddedData[4 * i1 + 2 + maxByte * i2 + i * 64] << 16) |
						   	   ((uint32_t)paddedData[4 * i1 + 3 + maxByte * i2 + i * 64] << 24);
			}
			M[i1] = vld1q_u32(temp_vec);
		}

		// 记录初始状态
		uint32x4_t a = state_a, b = state_b, c = state_c, d = state_d;

		auto start = system_clock::now();
		/* Round 1 */
		FF_SIMD(a, b, c, d, M[0], s11, 0xd76aa478);
		FF_SIMD(d, a, b, c, M[1], s12, 0xe8c7b756);
		FF_SIMD(c, d, a, b, M[2], s13, 0x242070db);
		FF_SIMD(b, c, d, a, M[3], s14, 0xc1bdceee);
		FF_SIMD(a, b, c, d, M[4], s11, 0xf57c0faf);
		FF_SIMD(d, a, b, c, M[5], s12, 0x4787c62a);
		FF_SIMD(c, d, a, b, M[6], s13, 0xa8304613);
		FF_SIMD(b, c, d, a, M[7], s14, 0xfd469501);
		FF_SIMD(a, b, c, d, M[8], s11, 0x698098d8);
		FF_SIMD(d, a, b, c, M[9], s12, 0x8b44f7af);
		FF_SIMD(c, d, a, b, M[10], s13, 0xffff5bb1);
		FF_SIMD(b, c, d, a, M[11], s14, 0x895cd7be);
		FF_SIMD(a, b, c, d, M[12], s11, 0x6b901122);
		FF_SIMD(d, a, b, c, M[13], s12, 0xfd987193);
		FF_SIMD(c, d, a, b, M[14], s13, 0xa679438e);
		FF_SIMD(b, c, d, a, M[15], s14, 0x49b40821);

		/* Round 2 */
		GG_SIMD(a, b, c, d, M[1], s21, 0xf61e2562);
		GG_SIMD(d, a, b, c, M[6], s22, 0xc040b340);
		GG_SIMD(c, d, a, b, M[11], s23, 0x265e5a51);
		GG_SIMD(b, c, d, a, M[0], s24, 0xe9b6c7aa);
		GG_SIMD(a, b, c, d, M[5], s21, 0xd62f105d);
		GG_SIMD(d, a, b, c, M[10], s22, 0x2441453);
		GG_SIMD(c, d, a, b, M[15], s23, 0xd8a1e681);
		GG_SIMD(b, c, d, a, M[4], s24, 0xe7d3fbc8);
		GG_SIMD(a, b, c, d, M[9], s21, 0x21e1cde6);
		GG_SIMD(d, a, b, c, M[14], s22, 0xc33707d6);
		GG_SIMD(c, d, a, b, M[3], s23, 0xf4d50d87);
		GG_SIMD(b, c, d, a, M[8], s24, 0x455a14ed);
		GG_SIMD(a, b, c, d, M[13], s21, 0xa9e3e905);
		GG_SIMD(d, a, b, c, M[2], s22, 0xfcefa3f8);
		GG_SIMD(c, d, a, b, M[7], s23, 0x676f02d9);
		GG_SIMD(b, c, d, a, M[12], s24, 0x8d2a4c8a);

		/* Round 3 */
		HH_SIMD(a, b, c, d, M[5], s31, 0xfffa3942);
		HH_SIMD(d, a, b, c, M[8], s32, 0x8771f681);
		HH_SIMD(c, d, a, b, M[11], s33, 0x6d9d6122);
		HH_SIMD(b, c, d, a, M[14], s34, 0xfde5380c);
		HH_SIMD(a, b, c, d, M[1], s31, 0xa4beea44);
		HH_SIMD(d, a, b, c, M[4], s32, 0x4bdecfa9);
		HH_SIMD(c, d, a, b, M[7], s33, 0xf6bb4b60);
		HH_SIMD(b, c, d, a, M[10], s34, 0xbebfbc70);
		HH_SIMD(a, b, c, d, M[13], s31, 0x289b7ec6);
		HH_SIMD(d, a, b, c, M[0], s32, 0xeaa127fa);
		HH_SIMD(c, d, a, b, M[3], s33, 0xd4ef3085);
		HH_SIMD(b, c, d, a, M[6], s34, 0x4881d05);
		HH_SIMD(a, b, c, d, M[9], s31, 0xd9d4d039);
		HH_SIMD(d, a, b, c, M[12], s32, 0xe6db99e5);
		HH_SIMD(c, d, a, b, M[15], s33, 0x1fa27cf8);
		HH_SIMD(b, c, d, a, M[2], s34, 0xc4ac5665);

		/* Round 4 */
		II_SIMD(a, b, c, d, M[0], s41, 0xf4292244);
		II_SIMD(d, a, b, c, M[7], s42, 0x432aff97);
		II_SIMD(c, d, a, b, M[14], s43, 0xab9423a7);
		II_SIMD(b, c, d, a, M[5], s44, 0xfc93a039);
		II_SIMD(a, b, c, d, M[12], s41, 0x655b59c3);
		II_SIMD(d, a, b, c, M[3], s42, 0x8f0ccc92);
		II_SIMD(c, d, a, b, M[10], s43, 0xffeff47d);
		II_SIMD(b, c, d, a, M[1], s44, 0x85845dd1);
		II_SIMD(a, b, c, d, M[8], s41, 0x6fa87e4f);
		II_SIMD(d, a, b, c, M[15], s42, 0xfe2ce6e0);
		II_SIMD(c, d, a, b, M[6], s43, 0xa3014314);
		II_SIMD(b, c, d, a, M[13], s44, 0x4e0811a1);
		II_SIMD(a, b, c, d, M[4], s41, 0xf7537e82);
		II_SIMD(d, a, b, c, M[11], s42, 0xbd3af235);
		II_SIMD(c, d, a, b, M[2], s43, 0x2ad7d2bb);
		II_SIMD(b, c, d, a, M[9], s44, 0xeb86d391);

		// 使用 NEON 的 vaddq_u32 指令
		// 依次加回各消息的位运算最终结果
		state_a = vaddq_u32(state_a, a);
		state_b = vaddq_u32(state_b, b);
		state_c = vaddq_u32(state_c, c);
		state_d = vaddq_u32(state_d, d);

	}

	// 转置矩阵（为了方便最终的哈希值输出）
	uint32x4_t state_0 = {vgetq_lane_u32(state_a, 0), vgetq_lane_u32(state_b, 0), vgetq_lane_u32(state_c, 0), vgetq_lane_u32(state_d, 0)};
	uint32x4_t state_1 = {vgetq_lane_u32(state_a, 1), vgetq_lane_u32(state_b, 1), vgetq_lane_u32(state_c, 1), vgetq_lane_u32(state_d, 1)};
	uint32x4_t state_2 = {vgetq_lane_u32(state_a, 2), vgetq_lane_u32(state_b, 2), vgetq_lane_u32(state_c, 2), vgetq_lane_u32(state_d, 2)};
	uint32x4_t state_3 = {vgetq_lane_u32(state_a, 3), vgetq_lane_u32(state_b, 3), vgetq_lane_u32(state_c, 3), vgetq_lane_u32(state_d, 3)};

	// 保存最终结果
	vst1q_u32(state + 0, state_0);
	vst1q_u32(state + 4, state_1);
	vst1q_u32(state + 8, state_2);
	vst1q_u32(state + 12, state_3);

	// 将小端转换为大端格式
	// 最终 4*4 的 state被填满，每 4 个 state 代表一个 MD5，依次读取即可
	for (int i = 0; i < 4 * PARA_NUM; i++)
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
}

/**
 * SIMDMD5Hash_2: 将*多个*输入字符串转换成 MD5 -> *SIMD并行化版本*（无注释）
 *				  --- 并行度 = 2
 * @param inputs 输入
 * @param[out] state 用于给调用者传递额外的返回值，即最终的缓冲区，也就是MD5的结果
 * @return Byte 消息数组
 */
void SIMDMD5Hash_2(string *inputs, bit32 *state)
{
	const int PARA_NUM = 2;
	int *messageLengths = new int[PARA_NUM];
	int maxByte = 0;
	Byte *paddedData = SIMDStringProcess(inputs, messageLengths, PARA_NUM, maxByte);
	
	int n_blocks = messageLengths[0] / 64;

	uint32x2_t state_a = vdup_n_u32(0x67452301);
    uint32x2_t state_b = vdup_n_u32(0xefcdab89);
    uint32x2_t state_c = vdup_n_u32(0x98badcfe);
    uint32x2_t state_d = vdup_n_u32(0x10325476);

	for (int i = 0; i < n_blocks; i += 1) {
		uint32x2_t M[16];

		for (int i1 = 0; i1 < 16; ++i1)
		{
			uint32_t temp_vec[2] = {0};
			for (int i2 = 0; i2 < PARA_NUM; ++i2) {
				temp_vec[i2] = ((uint32_t)paddedData[4 * i1 + maxByte * i2 + i * 64]) |
						   	   ((uint32_t)paddedData[4 * i1 + 1 + maxByte * i2 + i * 64] << 8) |
						  	   ((uint32_t)paddedData[4 * i1 + 2 + maxByte * i2 + i * 64] << 16) |
						   	   ((uint32_t)paddedData[4 * i1 + 3 + maxByte * i2 + i * 64] << 24);
			}
			M[i1] = vld1_u32(temp_vec);
		}

		uint32x2_t a = state_a, b = state_b, c = state_c, d = state_d;

		auto start = system_clock::now();

		FF_SIMD_2(a, b, c, d, M[0], s11, 0xd76aa478);
		FF_SIMD_2(d, a, b, c, M[1], s12, 0xe8c7b756);
		FF_SIMD_2(c, d, a, b, M[2], s13, 0x242070db);
		FF_SIMD_2(b, c, d, a, M[3], s14, 0xc1bdceee);
		FF_SIMD_2(a, b, c, d, M[4], s11, 0xf57c0faf);
		FF_SIMD_2(d, a, b, c, M[5], s12, 0x4787c62a);
		FF_SIMD_2(c, d, a, b, M[6], s13, 0xa8304613);
		FF_SIMD_2(b, c, d, a, M[7], s14, 0xfd469501);
		FF_SIMD_2(a, b, c, d, M[8], s11, 0x698098d8);
		FF_SIMD_2(d, a, b, c, M[9], s12, 0x8b44f7af);
		FF_SIMD_2(c, d, a, b, M[10], s13, 0xffff5bb1);
		FF_SIMD_2(b, c, d, a, M[11], s14, 0x895cd7be);
		FF_SIMD_2(a, b, c, d, M[12], s11, 0x6b901122);
		FF_SIMD_2(d, a, b, c, M[13], s12, 0xfd987193);
		FF_SIMD_2(c, d, a, b, M[14], s13, 0xa679438e);
		FF_SIMD_2(b, c, d, a, M[15], s14, 0x49b40821);

		GG_SIMD_2(a, b, c, d, M[1], s21, 0xf61e2562);
		GG_SIMD_2(d, a, b, c, M[6], s22, 0xc040b340);
		GG_SIMD_2(c, d, a, b, M[11], s23, 0x265e5a51);
		GG_SIMD_2(b, c, d, a, M[0], s24, 0xe9b6c7aa);
		GG_SIMD_2(a, b, c, d, M[5], s21, 0xd62f105d);
		GG_SIMD_2(d, a, b, c, M[10], s22, 0x2441453);
		GG_SIMD_2(c, d, a, b, M[15], s23, 0xd8a1e681);
		GG_SIMD_2(b, c, d, a, M[4], s24, 0xe7d3fbc8);
		GG_SIMD_2(a, b, c, d, M[9], s21, 0x21e1cde6);
		GG_SIMD_2(d, a, b, c, M[14], s22, 0xc33707d6);
		GG_SIMD_2(c, d, a, b, M[3], s23, 0xf4d50d87);
		GG_SIMD_2(b, c, d, a, M[8], s24, 0x455a14ed);
		GG_SIMD_2(a, b, c, d, M[13], s21, 0xa9e3e905);
		GG_SIMD_2(d, a, b, c, M[2], s22, 0xfcefa3f8);
		GG_SIMD_2(c, d, a, b, M[7], s23, 0x676f02d9);
		GG_SIMD_2(b, c, d, a, M[12], s24, 0x8d2a4c8a);

		HH_SIMD_2(a, b, c, d, M[5], s31, 0xfffa3942);
		HH_SIMD_2(d, a, b, c, M[8], s32, 0x8771f681);
		HH_SIMD_2(c, d, a, b, M[11], s33, 0x6d9d6122);
		HH_SIMD_2(b, c, d, a, M[14], s34, 0xfde5380c);
		HH_SIMD_2(a, b, c, d, M[1], s31, 0xa4beea44);
		HH_SIMD_2(d, a, b, c, M[4], s32, 0x4bdecfa9);
		HH_SIMD_2(c, d, a, b, M[7], s33, 0xf6bb4b60);
		HH_SIMD_2(b, c, d, a, M[10], s34, 0xbebfbc70);
		HH_SIMD_2(a, b, c, d, M[13], s31, 0x289b7ec6);
		HH_SIMD_2(d, a, b, c, M[0], s32, 0xeaa127fa);
		HH_SIMD_2(c, d, a, b, M[3], s33, 0xd4ef3085);
		HH_SIMD_2(b, c, d, a, M[6], s34, 0x4881d05);
		HH_SIMD_2(a, b, c, d, M[9], s31, 0xd9d4d039);
		HH_SIMD_2(d, a, b, c, M[12], s32, 0xe6db99e5);
		HH_SIMD_2(c, d, a, b, M[15], s33, 0x1fa27cf8);
		HH_SIMD_2(b, c, d, a, M[2], s34, 0xc4ac5665);

		II_SIMD_2(a, b, c, d, M[0], s41, 0xf4292244);
		II_SIMD_2(d, a, b, c, M[7], s42, 0x432aff97);
		II_SIMD_2(c, d, a, b, M[14], s43, 0xab9423a7);
		II_SIMD_2(b, c, d, a, M[5], s44, 0xfc93a039);
		II_SIMD_2(a, b, c, d, M[12], s41, 0x655b59c3);
		II_SIMD_2(d, a, b, c, M[3], s42, 0x8f0ccc92);
		II_SIMD_2(c, d, a, b, M[10], s43, 0xffeff47d);
		II_SIMD_2(b, c, d, a, M[1], s44, 0x85845dd1);
		II_SIMD_2(a, b, c, d, M[8], s41, 0x6fa87e4f);
		II_SIMD_2(d, a, b, c, M[15], s42, 0xfe2ce6e0);
		II_SIMD_2(c, d, a, b, M[6], s43, 0xa3014314);
		II_SIMD_2(b, c, d, a, M[13], s44, 0x4e0811a1);
		II_SIMD_2(a, b, c, d, M[4], s41, 0xf7537e82);
		II_SIMD_2(d, a, b, c, M[11], s42, 0xbd3af235);
		II_SIMD_2(c, d, a, b, M[2], s43, 0x2ad7d2bb);
		II_SIMD_2(b, c, d, a, M[9], s44, 0xeb86d391);

		state_a = vadd_u32(state_a, a);
		state_b = vadd_u32(state_b, b);
		state_c = vadd_u32(state_c, c);
		state_d = vadd_u32(state_d, d);

	}
	uint32x4_t state_0 = {vget_lane_u32(state_a, 0), vget_lane_u32(state_b, 0), vget_lane_u32(state_c, 0), vget_lane_u32(state_d, 0)};
	uint32x4_t state_1 = {vget_lane_u32(state_a, 1), vget_lane_u32(state_b, 1), vget_lane_u32(state_c, 1), vget_lane_u32(state_d, 1)};

	vst1q_u32(state + 0, state_0);
	vst1q_u32(state + 4, state_1);

	for (int i = 0; i < 4 * PARA_NUM; i++)
	{
		uint32_t value = state[i];
		state[i] = ((value & 0xff) << 24) |	
				   ((value & 0xff00) << 8) |
				   ((value & 0xff0000) >> 8) |
				   ((value & 0xff000000) >> 24); 
	}

	delete[] paddedData;
	delete[] messageLengths;
}

/**
 * SIMDMD5Hash_8basic: 将*多个*输入字符串转换成 MD5 -> *SIMD并行化版本*
 *				  --- 并行度 = 8
 * @param inputs 输入
 * @param[out] state 用于给调用者传递额外的返回值，即最终的缓冲区，也就是MD5的结果
 * @return Byte 消息数组
 */
void SIMDMD5Hash_8basic(string *inputs, bit32 *state)
{
	const int PARA_NUM = 8;				// 8
	int *messageLengths = new int[PARA_NUM];
	int maxByte = 0;
	Byte *paddedData = SIMDStringProcess(inputs, messageLengths, PARA_NUM, maxByte);
	
	int n_blocks = messageLengths[0] / 64;

	// 更改点(1)：分区低 4 位元素和高 4 位元素
	uint32x4_t state_a_low = vdupq_n_u32(0x67452301), state_a_high = vdupq_n_u32(0x67452301);
	uint32x4_t state_b_low = vdupq_n_u32(0xefcdab89), state_b_high = vdupq_n_u32(0xefcdab89);
	uint32x4_t state_c_low = vdupq_n_u32(0x98badcfe), state_c_high = vdupq_n_u32(0x98badcfe);
	uint32x4_t state_d_low = vdupq_n_u32(0x10325476), state_d_high = vdupq_n_u32(0x10325476);

	for (int i = 0; i < n_blocks; i += 1) {
		// 更改点(2)：分区低 4 位和高 4 位
		uint32x4_t M_low[16], M_high[16];

		for (int i1 = 0; i1 < 16; ++i1)
		{
			// 更改点(3)：适配高低位的 16bit 分块方式
			uint32_t temp_vec_low[4] = {0}, temp_vec_high[4] = {0};
			for (int i2 = 0; i2 < PARA_NUM/2; ++i2) {
				temp_vec_low[i2] = ((uint32_t)paddedData[4 * i1 + maxByte * i2 + i * 64]) |
							   	   ((uint32_t)paddedData[4 * i1 + 1 + maxByte * i2 + i * 64] << 8) |
							   	   ((uint32_t)paddedData[4 * i1 + 2 + maxByte * i2 + i * 64] << 16) |
							   	   ((uint32_t)paddedData[4 * i1 + 3 + maxByte * i2 + i * 64] << 24);
			}
			for (int i2 = PARA_NUM/2; i2 < PARA_NUM; ++i2) {
				temp_vec_high[i2 - PARA_NUM/2] = ((uint32_t)paddedData[4 * i1 + maxByte * i2 + i * 64]) |
												 ((uint32_t)paddedData[4 * i1 + 1 + maxByte * i2 + i * 64] << 8) |
												 ((uint32_t)paddedData[4 * i1 + 2 + maxByte * i2 + i * 64] << 16) |
												 ((uint32_t)paddedData[4 * i1 + 3 + maxByte * i2 + i * 64] << 24);
			}
			M_low[i1] = vld1q_u32(temp_vec_low);
            M_high[i1] = vld1q_u32(temp_vec_high);
		}

		// 更改点(4)：初始状态分高低位
		uint32x4_t a_low = state_a_low, a_high = state_a_high,
				   b_low = state_b_low, b_high = state_b_high,
				   c_low = state_c_low, c_high = state_c_high,
				   d_low = state_d_low, d_high = state_d_high;

		auto start = system_clock::now();

		// 更改点(5)：高低位分别处理，比原来多一倍调用
		FF_SIMD(a_low, b_low, c_low, d_low, M_low[0], s11, 0xd76aa478);
		FF_SIMD(a_high, b_high, c_high, d_high, M_high[0], s11, 0xd76aa478);
		FF_SIMD(d_low, a_low, b_low, c_low, M_low[1], s12, 0xe8c7b756);
		FF_SIMD(d_high, a_high, b_high, c_high, M_high[1], s12, 0xe8c7b756);
		FF_SIMD(c_low, d_low, a_low, b_low, M_low[2], s13, 0x242070db);
		FF_SIMD(c_high, d_high, a_high, b_high, M_high[2], s13, 0x242070db);
		FF_SIMD(b_low, c_low, d_low, a_low, M_low[3], s14, 0xc1bdceee);
		FF_SIMD(b_high, c_high, d_high, a_high, M_high[3], s14, 0xc1bdceee);
		FF_SIMD(a_low, b_low, c_low, d_low, M_low[4], s11, 0xf57c0faf);
		FF_SIMD(a_high, b_high, c_high, d_high, M_high[4], s11, 0xf57c0faf);
		FF_SIMD(d_low, a_low, b_low, c_low, M_low[5], s12, 0x4787c62a);
		FF_SIMD(d_high, a_high, b_high, c_high, M_high[5], s12, 0x4787c62a);
		FF_SIMD(c_low, d_low, a_low, b_low, M_low[6], s13, 0xa8304613);
		FF_SIMD(c_high, d_high, a_high, b_high, M_high[6], s13, 0xa8304613);
		FF_SIMD(b_low, c_low, d_low, a_low, M_low[7], s14, 0xfd469501);
		FF_SIMD(b_high, c_high, d_high, a_high, M_high[7], s14, 0xfd469501);
		FF_SIMD(a_low, b_low, c_low, d_low, M_low[8], s11, 0x698098d8);
		FF_SIMD(a_high, b_high, c_high, d_high, M_high[8], s11, 0x698098d8);
		FF_SIMD(d_low, a_low, b_low, c_low, M_low[9], s12, 0x8b44f7af);
		FF_SIMD(d_high, a_high, b_high, c_high, M_high[9], s12, 0x8b44f7af);
		FF_SIMD(c_low, d_low, a_low, b_low, M_low[10], s13, 0xffff5bb1);
		FF_SIMD(c_high, d_high, a_high, b_high, M_high[10], s13, 0xffff5bb1);
		FF_SIMD(b_low, c_low, d_low, a_low, M_low[11], s14, 0x895cd7be);
		FF_SIMD(b_high, c_high, d_high, a_high, M_high[11], s14, 0x895cd7be);
		FF_SIMD(a_low, b_low, c_low, d_low, M_low[12], s11, 0x6b901122);
		FF_SIMD(a_high, b_high, c_high, d_high, M_high[12], s11, 0x6b901122);
		FF_SIMD(d_low, a_low, b_low, c_low, M_low[13], s12, 0xfd987193);
		FF_SIMD(d_high, a_high, b_high, c_high, M_high[13], s12, 0xfd987193);
		FF_SIMD(c_low, d_low, a_low, b_low, M_low[14], s13, 0xa679438e);
		FF_SIMD(c_high, d_high, a_high, b_high, M_high[14], s13, 0xa679438e);
		FF_SIMD(b_low, c_low, d_low, a_low, M_low[15], s14, 0x49b40821);
		FF_SIMD(b_high, c_high, d_high, a_high, M_high[15], s14, 0x49b40821);

		GG_SIMD(a_low, b_low, c_low, d_low, M_low[1], s21, 0xf61e2562);
		GG_SIMD(a_high, b_high, c_high, d_high, M_high[1], s21, 0xf61e2562);
		GG_SIMD(d_low, a_low, b_low, c_low, M_low[6], s22, 0xc040b340);
		GG_SIMD(d_high, a_high, b_high, c_high, M_high[6], s22, 0xc040b340);
		GG_SIMD(c_low, d_low, a_low, b_low, M_low[11], s23, 0x265e5a51);
		GG_SIMD(c_high, d_high, a_high, b_high, M_high[11], s23, 0x265e5a51);
		GG_SIMD(b_low, c_low, d_low, a_low, M_low[0], s24, 0xe9b6c7aa);
		GG_SIMD(b_high, c_high, d_high, a_high, M_high[0], s24, 0xe9b6c7aa);
		GG_SIMD(a_low, b_low, c_low, d_low, M_low[5], s21, 0xd62f105d);
		GG_SIMD(a_high, b_high, c_high, d_high, M_high[5], s21, 0xd62f105d);
		GG_SIMD(d_low, a_low, b_low, c_low, M_low[10], s22, 0x2441453);
		GG_SIMD(d_high, a_high, b_high, c_high, M_high[10], s22, 0x2441453);
		GG_SIMD(c_low, d_low, a_low, b_low, M_low[15], s23, 0xd8a1e681);
		GG_SIMD(c_high, d_high, a_high, b_high, M_high[15], s23, 0xd8a1e681);
		GG_SIMD(b_low, c_low, d_low, a_low, M_low[4], s24, 0xe7d3fbc8);
		GG_SIMD(b_high, c_high, d_high, a_high, M_high[4], s24, 0xe7d3fbc8);
		GG_SIMD(a_low, b_low, c_low, d_low, M_low[9], s21, 0x21e1cde6);
		GG_SIMD(a_high, b_high, c_high, d_high, M_high[9], s21, 0x21e1cde6);
		GG_SIMD(d_low, a_low, b_low, c_low, M_low[14], s22, 0xc33707d6);
		GG_SIMD(d_high, a_high, b_high, c_high, M_high[14], s22, 0xc33707d6);
		GG_SIMD(c_low, d_low, a_low, b_low, M_low[3], s23, 0xf4d50d87);
		GG_SIMD(c_high, d_high, a_high, b_high, M_high[3], s23, 0xf4d50d87);
		GG_SIMD(b_low, c_low, d_low, a_low, M_low[8], s24, 0x455a14ed);
		GG_SIMD(b_high, c_high, d_high, a_high, M_high[8], s24, 0x455a14ed);
		GG_SIMD(a_low, b_low, c_low, d_low, M_low[13], s21, 0xa9e3e905);
		GG_SIMD(a_high, b_high, c_high, d_high, M_high[13], s21, 0xa9e3e905);
		GG_SIMD(d_low, a_low, b_low, c_low, M_low[2], s22, 0xfcefa3f8);
		GG_SIMD(d_high, a_high, b_high, c_high, M_high[2], s22, 0xfcefa3f8);
		GG_SIMD(c_low, d_low, a_low, b_low, M_low[7], s23, 0x676f02d9);
		GG_SIMD(c_high, d_high, a_high, b_high, M_high[7], s23, 0x676f02d9);
		GG_SIMD(b_low, c_low, d_low, a_low, M_low[12], s24, 0x8d2a4c8a);
		GG_SIMD(b_high, c_high, d_high, a_high, M_high[12], s24, 0x8d2a4c8a);

		HH_SIMD(a_low, b_low, c_low, d_low, M_low[5], s31, 0xfffa3942);
		HH_SIMD(a_high, b_high, c_high, d_high, M_high[5], s31, 0xfffa3942);
		HH_SIMD(d_low, a_low, b_low, c_low, M_low[8], s32, 0x8771f681);
		HH_SIMD(d_high, a_high, b_high, c_high, M_high[8], s32, 0x8771f681);
		HH_SIMD(c_low, d_low, a_low, b_low, M_low[11], s33, 0x6d9d6122);
		HH_SIMD(c_high, d_high, a_high, b_high, M_high[11], s33, 0x6d9d6122);
		HH_SIMD(b_low, c_low, d_low, a_low, M_low[14], s34, 0xfde5380c);
		HH_SIMD(b_high, c_high, d_high, a_high, M_high[14], s34, 0xfde5380c);
		HH_SIMD(a_low, b_low, c_low, d_low, M_low[1], s31, 0xa4beea44);
		HH_SIMD(a_high, b_high, c_high, d_high, M_high[1], s31, 0xa4beea44);
		HH_SIMD(d_low, a_low, b_low, c_low, M_low[4], s32, 0x4bdecfa9);
		HH_SIMD(d_high, a_high, b_high, c_high, M_high[4], s32, 0x4bdecfa9);
		HH_SIMD(c_low, d_low, a_low, b_low, M_low[7], s33, 0xf6bb4b60);
		HH_SIMD(c_high, d_high, a_high, b_high, M_high[7], s33, 0xf6bb4b60);
		HH_SIMD(b_low, c_low, d_low, a_low, M_low[10], s34, 0xbebfbc70);
		HH_SIMD(b_high, c_high, d_high, a_high, M_high[10], s34, 0xbebfbc70);
		HH_SIMD(a_low, b_low, c_low, d_low, M_low[13], s31, 0x289b7ec6);
		HH_SIMD(a_high, b_high, c_high, d_high, M_high[13], s31, 0x289b7ec6);
		HH_SIMD(d_low, a_low, b_low, c_low, M_low[0], s32, 0xeaa127fa);
		HH_SIMD(d_high, a_high, b_high, c_high, M_high[0], s32, 0xeaa127fa);
		HH_SIMD(c_low, d_low, a_low, b_low, M_low[3], s33, 0xd4ef3085);
		HH_SIMD(c_high, d_high, a_high, b_high, M_high[3], s33, 0xd4ef3085);
		HH_SIMD(b_low, c_low, d_low, a_low, M_low[6], s34, 0x4881d05);
		HH_SIMD(b_high, c_high, d_high, a_high, M_high[6], s34, 0x4881d05);
		HH_SIMD(a_low, b_low, c_low, d_low, M_low[9], s31, 0xd9d4d039);
		HH_SIMD(a_high, b_high, c_high, d_high, M_high[9], s31, 0xd9d4d039);
		HH_SIMD(d_low, a_low, b_low, c_low, M_low[12], s32, 0xe6db99e5);
		HH_SIMD(d_high, a_high, b_high, c_high, M_high[12], s32, 0xe6db99e5);
		HH_SIMD(c_low, d_low, a_low, b_low, M_low[15], s33, 0x1fa27cf8);
		HH_SIMD(c_high, d_high, a_high, b_high, M_high[15], s33, 0x1fa27cf8);
		HH_SIMD(b_low, c_low, d_low, a_low, M_low[2], s34, 0xc4ac5665);
		HH_SIMD(b_high, c_high, d_high, a_high, M_high[2], s34, 0xc4ac5665);

		II_SIMD(a_low, b_low, c_low, d_low, M_low[0], s41, 0xf4292244);
		II_SIMD(a_high, b_high, c_high, d_high, M_high[0], s41, 0xf4292244);
		II_SIMD(d_low, a_low, b_low, c_low, M_low[7], s42, 0x432aff97);
		II_SIMD(d_high, a_high, b_high, c_high, M_high[7], s42, 0x432aff97);
		II_SIMD(c_low, d_low, a_low, b_low, M_low[14], s43, 0xab9423a7);
		II_SIMD(c_high, d_high, a_high, b_high, M_high[14], s43, 0xab9423a7);
		II_SIMD(b_low, c_low, d_low, a_low, M_low[5], s44, 0xfc93a039);
		II_SIMD(b_high, c_high, d_high, a_high, M_high[5], s44, 0xfc93a039);
		II_SIMD(a_low, b_low, c_low, d_low, M_low[12], s41, 0x655b59c3);
		II_SIMD(a_high, b_high, c_high, d_high, M_high[12], s41, 0x655b59c3);
		II_SIMD(d_low, a_low, b_low, c_low, M_low[3], s42, 0x8f0ccc92);
		II_SIMD(d_high, a_high, b_high, c_high, M_high[3], s42, 0x8f0ccc92);
		II_SIMD(c_low, d_low, a_low, b_low, M_low[10], s43, 0xffeff47d);
		II_SIMD(c_high, d_high, a_high, b_high, M_high[10], s43, 0xffeff47d);
		II_SIMD(b_low, c_low, d_low, a_low, M_low[1], s44, 0x85845dd1);
		II_SIMD(b_high, c_high, d_high, a_high, M_high[1], s44, 0x85845dd1);
		II_SIMD(a_low, b_low, c_low, d_low, M_low[8], s41, 0x6fa87e4f);
		II_SIMD(a_high, b_high, c_high, d_high, M_high[8], s41, 0x6fa87e4f);
		II_SIMD(d_low, a_low, b_low, c_low, M_low[15], s42, 0xfe2ce6e0);
		II_SIMD(d_high, a_high, b_high, c_high, M_high[15], s42, 0xfe2ce6e0);
		II_SIMD(c_low, d_low, a_low, b_low, M_low[6], s43, 0xa3014314);
		II_SIMD(c_high, d_high, a_high, b_high, M_high[6], s43, 0xa3014314);
		II_SIMD(b_low, c_low, d_low, a_low, M_low[13], s44, 0x4e0811a1);
		II_SIMD(b_high, c_high, d_high, a_high, M_high[13], s44, 0x4e0811a1);
		II_SIMD(a_low, b_low, c_low, d_low, M_low[4], s41, 0xf7537e82);
		II_SIMD(a_high, b_high, c_high, d_high, M_high[4], s41, 0xf7537e82);
		II_SIMD(d_low, a_low, b_low, c_low, M_low[11], s42, 0xbd3af235);
		II_SIMD(d_high, a_high, b_high, c_high, M_high[11], s42, 0xbd3af235);
		II_SIMD(c_low, d_low, a_low, b_low, M_low[2], s43, 0x2ad7d2bb);
		II_SIMD(c_high, d_high, a_high, b_high, M_high[2], s43, 0x2ad7d2bb);
		II_SIMD(b_low, c_low, d_low, a_low, M_low[9], s44, 0xeb86d391);
		II_SIMD(b_high, c_high, d_high, a_high, M_high[9], s44, 0xeb86d391);

		// 更改点(6)：适配高低位的每轮赋值方式
		state_a_low = vaddq_u32(state_a_low, a_low);
        state_a_high = vaddq_u32(state_a_high, a_high);
        state_b_low = vaddq_u32(state_b_low, b_low);
        state_b_high = vaddq_u32(state_b_high, b_high);
        state_c_low = vaddq_u32(state_c_low, c_low);
        state_c_high = vaddq_u32(state_c_high, c_high);
        state_d_low = vaddq_u32(state_d_low, d_low);
        state_d_high = vaddq_u32(state_d_high, d_high);

	}
	// 更改点(7)：适配高低位的矩阵转置
	uint32x4_t state_0 = {vgetq_lane_u32(state_a_low, 0), vgetq_lane_u32(state_b_low, 0), vgetq_lane_u32(state_c_low, 0), vgetq_lane_u32(state_d_low, 0)};
	uint32x4_t state_1 = {vgetq_lane_u32(state_a_low, 1), vgetq_lane_u32(state_b_low, 1), vgetq_lane_u32(state_c_low, 1), vgetq_lane_u32(state_d_low, 1)};
	uint32x4_t state_2 = {vgetq_lane_u32(state_a_low, 2), vgetq_lane_u32(state_b_low, 2), vgetq_lane_u32(state_c_low, 2), vgetq_lane_u32(state_d_low, 2)};
	uint32x4_t state_3 = {vgetq_lane_u32(state_a_low, 3), vgetq_lane_u32(state_b_low, 3), vgetq_lane_u32(state_c_low, 3), vgetq_lane_u32(state_d_low, 3)};
	uint32x4_t state_4 = {vgetq_lane_u32(state_a_high, 0), vgetq_lane_u32(state_b_high, 0), vgetq_lane_u32(state_c_high, 0), vgetq_lane_u32(state_d_high, 0)};
	uint32x4_t state_5 = {vgetq_lane_u32(state_a_high, 1), vgetq_lane_u32(state_b_high, 1), vgetq_lane_u32(state_c_high, 1), vgetq_lane_u32(state_d_high, 1)};
	uint32x4_t state_6 = {vgetq_lane_u32(state_a_high, 2), vgetq_lane_u32(state_b_high, 2), vgetq_lane_u32(state_c_high, 2), vgetq_lane_u32(state_d_high, 2)};
	uint32x4_t state_7 = {vgetq_lane_u32(state_a_high, 3), vgetq_lane_u32(state_b_high, 3), vgetq_lane_u32(state_c_high, 3), vgetq_lane_u32(state_d_high, 3)};

	// 更改点(8)：适配高低位的最终取值方式
	vst1q_u32(state + 0, state_0);
    vst1q_u32(state + 4, state_1);
    vst1q_u32(state + 8, state_2);
    vst1q_u32(state + 12, state_3);
    vst1q_u32(state + 16, state_4);
    vst1q_u32(state + 20, state_5);
    vst1q_u32(state + 24, state_6);
    vst1q_u32(state + 28, state_7);

	for (int i = 0; i < 4 * PARA_NUM; i++)
	{
		uint32_t value = state[i];
		state[i] = ((value & 0xff) << 24) |	
				   ((value & 0xff00) << 8) |
			 	   ((value & 0xff0000) >> 8) |
				   ((value & 0xff000000) >> 24); 
	}

	delete[] paddedData;
	delete[] messageLengths;
}

/**
 * SIMDMD5Hash_8advanced: 将*多个*输入字符串转换成 MD5 -> *SIMD并行化版本*（无注释，指令全部换成整合的双向量即可）
 *				  --- 并行度 = 8
 * @param inputs 输入
 * @param[out] state 用于给调用者传递额外的返回值，即最终的缓冲区，也就是MD5的结果
 * @return Byte 消息数组
 */
void SIMDMD5Hash_8advanced(string *inputs, bit32 *state)
{
	const int PARA_NUM = 8;				// 8
	int *messageLengths = new int[PARA_NUM];
	int maxByte = 0;
	Byte *paddedData = SIMDStringProcess(inputs, messageLengths, PARA_NUM, maxByte);
	
	int n_blocks = messageLengths[0] / 64;

	// (1)
	uint32x4x2_t state_a, state_b, state_c, state_d;
	state_a.val[0] = vdupq_n_u32(0x67452301);
	state_a.val[1] = vdupq_n_u32(0x67452301);
	state_b.val[0] = vdupq_n_u32(0xefcdab89);
	state_b.val[1] = vdupq_n_u32(0xefcdab89);
	state_c.val[0] = vdupq_n_u32(0x98badcfe);
	state_c.val[1] = vdupq_n_u32(0x98badcfe);
	state_d.val[0] = vdupq_n_u32(0x10325476);
	state_d.val[1] = vdupq_n_u32(0x10325476);

	for (int i = 0; i < n_blocks; i += 1) {
		// (2)
		uint32x4x2_t M[16];

		for (int i1 = 0; i1 < 16; ++i1)
		{
			// (3)
			uint32_t temp_vec_low[4] = {0}, temp_vec_high[4] = {0};
			for (int i2 = 0; i2 < PARA_NUM/2; ++i2) {
				temp_vec_low[i2] = ((uint32_t)paddedData[4 * i1 + maxByte * i2 + i * 64]) |
								   ((uint32_t)paddedData[4 * i1 + 1 + maxByte * i2 + i * 64] << 8) |
								   ((uint32_t)paddedData[4 * i1 + 2 + maxByte * i2 + i * 64] << 16) |
								   ((uint32_t)paddedData[4 * i1 + 3 + maxByte * i2 + i * 64] << 24);
			}
			for (int i2 = PARA_NUM/2; i2 < PARA_NUM; ++i2) {
				temp_vec_high[i2 - PARA_NUM/2] = ((uint32_t)paddedData[4 * i1 + maxByte * i2 + i * 64]) |
											 	 ((uint32_t)paddedData[4 * i1 + 1 + maxByte * i2 + i * 64] << 8) |
												 ((uint32_t)paddedData[4 * i1 + 2 + maxByte * i2 + i * 64] << 16) |
												 ((uint32_t)paddedData[4 * i1 + 3 + maxByte * i2 + i * 64] << 24);
			}
			M[i1].val[0] = vld1q_u32(temp_vec_low);
			M[i1].val[1] = vld1q_u32(temp_vec_high);
		}

		// (4)
		uint32x4x2_t a = state_a, b = state_b, c = state_c, d = state_d;

		auto start = system_clock::now();

		// (5)
        /* Round 1 */
        FF_SIMD_8advanced(a, b, c, d, M[0], s11, 0xd76aa478);
        FF_SIMD_8advanced(d, a, b, c, M[1], s12, 0xe8c7b756);
        FF_SIMD_8advanced(c, d, a, b, M[2], s13, 0x242070db);
        FF_SIMD_8advanced(b, c, d, a, M[3], s14, 0xc1bdceee);
        FF_SIMD_8advanced(a, b, c, d, M[4], s11, 0xf57c0faf);
        FF_SIMD_8advanced(d, a, b, c, M[5], s12, 0x4787c62a);
        FF_SIMD_8advanced(c, d, a, b, M[6], s13, 0xa8304613);
        FF_SIMD_8advanced(b, c, d, a, M[7], s14, 0xfd469501);
        FF_SIMD_8advanced(a, b, c, d, M[8], s11, 0x698098d8);
        FF_SIMD_8advanced(d, a, b, c, M[9], s12, 0x8b44f7af);
        FF_SIMD_8advanced(c, d, a, b, M[10], s13, 0xffff5bb1);
        FF_SIMD_8advanced(b, c, d, a, M[11], s14, 0x895cd7be);
        FF_SIMD_8advanced(a, b, c, d, M[12], s11, 0x6b901122);
        FF_SIMD_8advanced(d, a, b, c, M[13], s12, 0xfd987193);
        FF_SIMD_8advanced(c, d, a, b, M[14], s13, 0xa679438e);
        FF_SIMD_8advanced(b, c, d, a, M[15], s14, 0x49b40821);

        /* Round 2 */
        GG_SIMD_8advanced(a, b, c, d, M[1], s21, 0xf61e2562);
        GG_SIMD_8advanced(d, a, b, c, M[6], s22, 0xc040b340);
        GG_SIMD_8advanced(c, d, a, b, M[11], s23, 0x265e5a51);
        GG_SIMD_8advanced(b, c, d, a, M[0], s24, 0xe9b6c7aa);
        GG_SIMD_8advanced(a, b, c, d, M[5], s21, 0xd62f105d);
        GG_SIMD_8advanced(d, a, b, c, M[10], s22, 0x2441453);
        GG_SIMD_8advanced(c, d, a, b, M[15], s23, 0xd8a1e681);
        GG_SIMD_8advanced(b, c, d, a, M[4], s24, 0xe7d3fbc8);
        GG_SIMD_8advanced(a, b, c, d, M[9], s21, 0x21e1cde6);
        GG_SIMD_8advanced(d, a, b, c, M[14], s22, 0xc33707d6);
        GG_SIMD_8advanced(c, d, a, b, M[3], s23, 0xf4d50d87);
        GG_SIMD_8advanced(b, c, d, a, M[8], s24, 0x455a14ed);
        GG_SIMD_8advanced(a, b, c, d, M[13], s21, 0xa9e3e905);
        GG_SIMD_8advanced(d, a, b, c, M[2], s22, 0xfcefa3f8);
        GG_SIMD_8advanced(c, d, a, b, M[7], s23, 0x676f02d9);
        GG_SIMD_8advanced(b, c, d, a, M[12], s24, 0x8d2a4c8a);

        /* Round 3 */
        HH_SIMD_8advanced(a, b, c, d, M[5], s31, 0xfffa3942);
        HH_SIMD_8advanced(d, a, b, c, M[8], s32, 0x8771f681);
        HH_SIMD_8advanced(c, d, a, b, M[11], s33, 0x6d9d6122);
        HH_SIMD_8advanced(b, c, d, a, M[14], s34, 0xfde5380c);
        HH_SIMD_8advanced(a, b, c, d, M[1], s31, 0xa4beea44);
        HH_SIMD_8advanced(d, a, b, c, M[4], s32, 0x4bdecfa9);
        HH_SIMD_8advanced(c, d, a, b, M[7], s33, 0xf6bb4b60);
        HH_SIMD_8advanced(b, c, d, a, M[10], s34, 0xbebfbc70);
        HH_SIMD_8advanced(a, b, c, d, M[13], s31, 0x289b7ec6);
        HH_SIMD_8advanced(d, a, b, c, M[0], s32, 0xeaa127fa);
        HH_SIMD_8advanced(c, d, a, b, M[3], s33, 0xd4ef3085);
        HH_SIMD_8advanced(b, c, d, a, M[6], s34, 0x4881d05);
        HH_SIMD_8advanced(a, b, c, d, M[9], s31, 0xd9d4d039);
        HH_SIMD_8advanced(d, a, b, c, M[12], s32, 0xe6db99e5);
        HH_SIMD_8advanced(c, d, a, b, M[15], s33, 0x1fa27cf8);
        HH_SIMD_8advanced(b, c, d, a, M[2], s34, 0xc4ac5665);

        /* Round 4 */
        II_SIMD_8advanced(a, b, c, d, M[0], s41, 0xf4292244);
        II_SIMD_8advanced(d, a, b, c, M[7], s42, 0x432aff97);
        II_SIMD_8advanced(c, d, a, b, M[14], s43, 0xab9423a7);
        II_SIMD_8advanced(b, c, d, a, M[5], s44, 0xfc93a039);
        II_SIMD_8advanced(a, b, c, d, M[12], s41, 0x655b59c3);
        II_SIMD_8advanced(d, a, b, c, M[3], s42, 0x8f0ccc92);
        II_SIMD_8advanced(c, d, a, b, M[10], s43, 0xffeff47d);
        II_SIMD_8advanced(b, c, d, a, M[1], s44, 0x85845dd1);
        II_SIMD_8advanced(a, b, c, d, M[8], s41, 0x6fa87e4f);
        II_SIMD_8advanced(d, a, b, c, M[15], s42, 0xfe2ce6e0);
        II_SIMD_8advanced(c, d, a, b, M[6], s43, 0xa3014314);
        II_SIMD_8advanced(b, c, d, a, M[13], s44, 0x4e0811a1);
        II_SIMD_8advanced(a, b, c, d, M[4], s41, 0xf7537e82);
        II_SIMD_8advanced(d, a, b, c, M[11], s42, 0xbd3af235);
        II_SIMD_8advanced(c, d, a, b, M[2], s43, 0x2ad7d2bb);
        II_SIMD_8advanced(b, c, d, a, M[9], s44, 0xeb86d391);

		// (6)
        state_a.val[0] = vaddq_u32(state_a.val[0], a.val[0]);
        state_a.val[1] = vaddq_u32(state_a.val[1], a.val[1]);
        state_b.val[0] = vaddq_u32(state_b.val[0], b.val[0]);
        state_b.val[1] = vaddq_u32(state_b.val[1], b.val[1]);
        state_c.val[0] = vaddq_u32(state_c.val[0], c.val[0]);
        state_c.val[1] = vaddq_u32(state_c.val[1], c.val[1]);
        state_d.val[0] = vaddq_u32(state_d.val[0], d.val[0]);
        state_d.val[1] = vaddq_u32(state_d.val[1], d.val[1]);
	}
	// (7)
	uint32x4_t state_0 = {vgetq_lane_u32(state_a.val[0], 0), vgetq_lane_u32(state_b.val[0], 0), vgetq_lane_u32(state_c.val[0], 0), vgetq_lane_u32(state_d.val[0], 0)};
	uint32x4_t state_1 = {vgetq_lane_u32(state_a.val[0], 1), vgetq_lane_u32(state_b.val[0], 1), vgetq_lane_u32(state_c.val[0], 1), vgetq_lane_u32(state_d.val[0], 1)};
	uint32x4_t state_2 = {vgetq_lane_u32(state_a.val[0], 2), vgetq_lane_u32(state_b.val[0], 2), vgetq_lane_u32(state_c.val[0], 2), vgetq_lane_u32(state_d.val[0], 2)};
	uint32x4_t state_3 = {vgetq_lane_u32(state_a.val[0], 3), vgetq_lane_u32(state_b.val[0], 3), vgetq_lane_u32(state_c.val[0], 3), vgetq_lane_u32(state_d.val[0], 3)};
	uint32x4_t state_4 = {vgetq_lane_u32(state_a.val[1], 0), vgetq_lane_u32(state_b.val[1], 0), vgetq_lane_u32(state_c.val[1], 0), vgetq_lane_u32(state_d.val[1], 0)};
	uint32x4_t state_5 = {vgetq_lane_u32(state_a.val[1], 1), vgetq_lane_u32(state_b.val[1], 1), vgetq_lane_u32(state_c.val[1], 1), vgetq_lane_u32(state_d.val[1], 1)};
	uint32x4_t state_6 = {vgetq_lane_u32(state_a.val[1], 2), vgetq_lane_u32(state_b.val[1], 2), vgetq_lane_u32(state_c.val[1], 2), vgetq_lane_u32(state_d.val[1], 2)};
	uint32x4_t state_7 = {vgetq_lane_u32(state_a.val[1], 3), vgetq_lane_u32(state_b.val[1], 3), vgetq_lane_u32(state_c.val[1], 3), vgetq_lane_u32(state_d.val[1], 3)};

	// (8)
    vst1q_u32(state + 0, state_0);
    vst1q_u32(state + 4, state_1);
    vst1q_u32(state + 8, state_2);
    vst1q_u32(state + 12, state_3);
    vst1q_u32(state + 16, state_4);
    vst1q_u32(state + 20, state_5);
    vst1q_u32(state + 24, state_6);
    vst1q_u32(state + 28, state_7);

	for (int i = 0; i < 4 * PARA_NUM; i++)
	{
		uint32_t value = state[i];
		state[i] = ((value & 0xff) << 24) |	
				((value & 0xff00) << 8) |
					((value & 0xff0000) >> 8) |
				((value & 0xff000000) >> 24); 
	}

	delete[] paddedData;
	delete[] messageLengths;
}