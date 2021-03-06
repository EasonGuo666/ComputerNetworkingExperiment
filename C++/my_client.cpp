#include "stdafx.h"
#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <string>
#include <time.h>
#include <fstream>
#include <math.h>
using namespace std;
#pragma comment(lib, "ws2_32.lib")

//待发送文件的路径
#define SEND_FILE_PATH "C:\\Users\\Administrator\\Desktop\\hello.c"

//网络端口
#define PORT 8888

//本地网络地址ID
#define LOCAL_ID "127.0.0.1"

//最大文件大小512*1024字节(512KB)
#define MAX_FILE_SIZE 524288

//10%概率错一帧
#define FILTER_ERROR 10

//10%概率丢失一帧
#define FILTER_LOST 10

//由数据字符串得到crc校验码(多项式G(x) = x16 + x12 + x5 + 1)
char* get_crc_code(char *data_str)
{
	//因为GenXString = "10001000000100001";
	long long int divisor = 0;
	divisor = (long long int)pow(2, 16) + (long long int)pow(2, 12) + (long long int)pow(2, 5) + 1;
	//data_str在向左移16位，右边用0补充，再讲data_str转化为用long long int表示的数
	long long int d0 = data_str[0] * (long long int)pow(2, 40);
	long long int d1 = data_str[1] * (long long int)pow(2, 32);
	long long int d2 = data_str[2] * (long long int)pow(2, 24);
	long long int d3 = data_str[3] * (long long int)pow(2, 16);

	//数据放在long long中方便移位                     
	long long int data = 0;
	data = d0 + d1 + d2 + d3;
	int position = 31;
	//一开始余数为倒数第31位前面的17位的值
	long long int remainder = 0;
	remainder = data >> position;
	
	while (position != 0)
	{
		//如果余数达到了17位，就进行抑或操作，否则什么都不做
		if (remainder & 0x10000)
		{
			remainder = remainder ^ divisor;
		}
		position--;
		//得到下一位（0或1）
		long long int next_position = (data >> position) % 2;
		remainder = remainder << 1;
		remainder += next_position;
	}
	//把运算结果余数放在两个字节中，除以256表示高8位的数据，%256表示低8位的数据，
	//因为这样算出来的结果取值区间是[0,255]，可能会超出char的表示范围,所以－128做线性搬移
	char result[3];
	result[0] = (remainder / 256) - 128;
	result[1] = (remainder % 256) - 128;
	result[2] = '\0';
	return result;
}

//读入一个4字节的数据字符串data_str，经过CRC校验在末位添加2字节校验码，返回一个6字节的待发送字符串
//data_str可以是asddkjfisc这样的任意字符，也可以是0,1字符数组
char* get_send_str(char *data_str)
{
	char *crc_code = get_crc_code(data_str);
	char send_str[7];
	memset(send_str, 0, sizeof(send_str));
	//发送字符串send_str的前4个字节是data数据位
	send_str[0] = data_str[0];
	send_str[1] = data_str[1];
	send_str[2] = data_str[2];
	send_str[3] = data_str[3];
	//后2字节是crc码
	send_str[4] = crc_code[0];
	send_str[5] = crc_code[1];
	send_str[6] = '\0';
	return send_str;
}

//向传输字符中加错,简单点就把每个字符加一
char* add_error(char *origin_str)
{
	char new_str[7];
	memset(new_str, 0, sizeof(new_str));
	for (int i = 0; i < 6; i++)
	{
		new_str[i] = origin_str[i] + 1;
	}
	return new_str;
}

//从source字符串填充destination_buff,每次填4字节
void fill(char *destination_buff, char *source)
{
	destination_buff[0] = source[0];
	destination_buff[1] = source[1];
	destination_buff[2] = source[2];
	destination_buff[3] = source[3];
}

int main()
{
	//winsocket库启动
	WORD socket_version = MAKEWORD(2, 2);
	WSADATA wsa_data;
	if (WSAStartup(socket_version, &wsa_data) != 0)
		return 0;

	//打开并读取待发送文件
	HANDLE  hFile;
	hFile = CreateFile(SEND_FILE_PATH, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("open file failed\n");
		getchar();
		return 0;
	}

	//获得待发送文件的大小
	DWORD file_size, dwHighSize;
	file_size = GetFileSize(hFile, &dwHighSize);
	if (dwHighSize)
	{
		CloseHandle(hFile);
		printf("dwHighSize is not 0!\n");
		getchar();
		return 0;
	}
	if (MAX_FILE_SIZE < file_size)
	{
		printf("file is too large in size!\n");
		getchar();
		return 0;
	}

	//读文件内容到str_file中
	char str_file[MAX_FILE_SIZE];
	memset(str_file,0,sizeof(str_file));
	BOOL r_success;
	DWORD dwBytesRead;
	r_success = ReadFile(hFile, str_file, file_size, &dwBytesRead, NULL);
	CloseHandle(hFile);
	//printf("%s", str_file);

	//建立UDP套接字
	SOCKET client_sckt = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	//发送端网络地址地址初始化
	SOCKADDR_IN add_s;
	add_s.sin_family = AF_INET;
	add_s.sin_port = htons(PORT);
	add_s.sin_addr.S_un.S_addr = inet_addr(LOCAL_ID);
	int len = sizeof(add_s);

	//接受端初始化
	SOCKADDR_IN add_r;
	add_r.sin_family = AF_INET;
	add_r.sin_port = htons(PORT);
	add_r.sin_addr.S_un.S_addr = inet_addr(LOCAL_ID);

	//接收超时等待设置
	struct timeval tv;
	tv.tv_sec = 5000;
	tv.tv_usec = 0;

	//发送的内容在str_file中的位置指针
	int position = 0;

	char data_buffer[4];
	int next_frame_to_send = 0;
	//发送帧的编号
	int frame_count = 0;
	char *send_str;
	char send_buffer[7];

	//先发送文件长度
	char file_len[2];
	file_len[0] = file_size / 256;
	file_len[1] = file_size % 256;
	sendto(client_sckt, file_len, 2, 0, (sockaddr *)&add_s, len);
	printf("file size:%d\n", file_size);

	while (position < strlen(str_file))
	{
		if (setsockopt(client_sckt, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(tv)) < 0)
		{
			printf("time out failure in line 175\n");
			return 0;
		}

		fill(data_buffer, &str_file[position]);
		send_str = get_send_str(data_buffer);
		memset(send_buffer, 0, sizeof(send_buffer));
		send_buffer[0] = send_str[0];
		send_buffer[1] = send_str[1];
		send_buffer[2] = send_str[2];
		send_buffer[3] = send_str[3];
		send_buffer[4] = send_str[4];
		send_buffer[5] = send_str[5];

		//10%的概率不发送（丢失）
		if (rand() % 100 < 10)
		{
			printf("next_frame_to_send:%d\n", next_frame_to_send);
			printf("frame %d is suppoesd to be lost!\n", frame_count);
		}
		else
		{
			//10%的概率加错
			if (rand() % 100 < 10)
			{
				printf("next_frame_to_send:%d\n", next_frame_to_send);
				printf("frame %d is supposed to be wrong!\n", frame_count);
				add_error(send_str);
				sendto(client_sckt, (char *)send_str, 6, 0, (sockaddr *)&add_s, len);
				//printf("send_str:%s\n", send_str);
			}
			else//正确发送一个帧
			{
				if (sendto(client_sckt, send_buffer, 6, 0, (sockaddr *)&add_s, len))
				{
					printf("send_str:%c%c%c%c%c%c\n", send_buffer[0], send_buffer[1], send_buffer[2], send_buffer[3], send_buffer[4], send_buffer[5]);
					printf("next_frame_to_send:%d\n", next_frame_to_send);
					printf("frame %d: send successfully!\n", frame_count);
				}
			}
		}

		//接受服务器返回字符串
		char recv[2];
		memset(recv, 0, sizeof(recv));
		int ret = recvfrom(client_sckt, recv, 2 * sizeof(char), 0, (sockaddr *)&add_r, &len);

		//接收成功
		if (ret>0)
		{
			printf("receiving message:%c\n", recv[0]);
			printf("receiving return_msg from server successfully\n");
			//如果接受到的信息没有错误，就可以发送下一帧
			if(recv[0]=='1')
			{
				next_frame_to_send = 1;
				frame_count++;
				position = position + 4;
			}
			else  printf("resending current frame for error communication\n");
		}
		else
		{
			printf("time out in 235, resending current frame for not receiving feedback\n");
			next_frame_to_send = 0;
		}
		printf("----------------------------------\n\n");
	}

	//关闭socket
	closesocket(client_sckt);
	WSACleanup();
	return 0;
}

