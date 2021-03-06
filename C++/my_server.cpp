﻿#include "stdafx.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<winsock2.h>
#include<iostream>
#include<string>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

//最大文件大小512*1024字节(512KB)
#define MAX_FILE_SIZE 524288

//网络端口
#define PORT 8888

//本地网络地址ID
#define LOCAL_ID "127.0.0.1"

//写入文件的路径
#define WRITE_FILE_PATH "C:\\Users\\Administrator\\Desktop\\嘿嘿你看这个面.c"

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
	//把运算结果余数放在两个字节中，除以256表示高8位的数据，%256表示低8位的数据
	char result[3];
	result[0] = (remainder / 256) - 128;
	result[1] = (remainder % 256) - 128;
	result[2] = '\0';
	return result;
}

//读入一个4字节的数据字符串data_str，经过CRC校验在末位添加2字节校验码，返回一个6字节的待发送字符串
//data_str可以是abwd这样的任意字符，也可以是0,1字符数组
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

//从source字符串填充destination_buff,每次填4字节
void fill(char *destination_buff, char *source)
{
	destination_buff[0] = source[0];
	destination_buff[1] = source[1];
	destination_buff[2] = source[2];
	destination_buff[3] = source[3];
}

//检错函数
BOOL find_error(char *recvd_str)
{
	//先把原数据串读进来
	char data_str[4];
	memset(data_str, 0, sizeof(data_str));
	fill(data_str, recvd_str);

	//计算实际传输后得到的字符应该的crc码
	char *correct_crc;
	correct_crc = get_crc_code(data_str);

	//读入接收到的crc
	char recvd_crc[2];
	memset(recvd_crc, 0, sizeof(recvd_crc));
	recvd_crc[0] = recvd_str[4];
	recvd_crc[1] = recvd_str[5];

	//检错
	if ((correct_crc[0] == recvd_crc[0]) && (correct_crc[1] == recvd_crc[1]))
		return false;
	else  return true;
}

int main()
{
	//启动winsocket库
	WSADATA wsaData;
	WORD sockVersion = MAKEWORD(2, 2);
	if (WSAStartup(sockVersion, &wsaData) != 0)
	{
		return 0;
	}

	//打开待写入文件
	errno_t err;
	FILE *write_file;
	err = fopen_s(&write_file, WRITE_FILE_PATH, "w");

	SOCKET server_sckt = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (server_sckt == INVALID_SOCKET)
	{
		printf("socket error!\n");
		return 0;
	}

	//接受端初始化
	SOCKADDR_IN add_r;
	add_r.sin_family = AF_INET;
	add_r.sin_port = htons(PORT);
	add_r.sin_addr.S_un.S_addr = inet_addr(LOCAL_ID);
	//add_r.sin_addr.S_un.S_addr = INADDR_ANY;
	int addr_len = sizeof(add_r);

	if (bind(server_sckt, (sockaddr *)&add_r, sizeof(add_r)) == SOCKET_ERROR)
	{
		printf("bind error!\n");
		closesocket(server_sckt);
		return 0;
	}
	printf("bind succes!\n");

	//发送端初始化
	SOCKADDR_IN add_s;
	//add_s.sin_family = AF_INET;
	//add_s.sin_port = htons(PORT);
	//add_s.sin_addr.S_un.S_addr = inet_addr(LOCAL_ID);

	//服务器从客户端受到的消息
	char recvd_str[7];

	//服务器发回客户端的返回信息
	char return_msg[2];

	//写回文件中去的字符串
	char write_str[MAX_FILE_SIZE];
	int position = 0;
	int frame_expected = 0;
	memset(write_str, 0, sizeof(write_str));

	//先接收文件长度
	int ret;
	char file_len[2] = {'\0','\0'};
	int file_size = 0;
	ret = recvfrom(server_sckt, file_len, 2, 0, (sockaddr *)&add_s, &addr_len);
	file_size = file_len[0] * 256 + file_len[1];
	printf("file_size :%d\n", file_size);

	//10秒不接受客户端消息就
	int Timeout = 10000;

	while (true)
	{
		if (position >= file_size)
		{
			printf("file receiving completed!\n");
			break;
		}

		//超时结束
		if (SOCKET_ERROR == setsockopt(server_sckt, SOL_SOCKET, SO_RCVTIMEO, (char *)&Timeout, sizeof(int)))
		{
			printf("time out!\r\n");
			break;
		}

		memset(recvd_str, 0, sizeof(recvd_str));
		ret = recvfrom(server_sckt, recvd_str, 6, 0, (sockaddr *)&add_s, &addr_len);
		//printf("recv_data:%c*%c*%c*%c*%c*%c*\n", recvd_str[0], recvd_str[1], recvd_str[2], recvd_str[3], recvd_str[4], recvd_str[5]);

		memset(return_msg, 0, sizeof(return_msg));
		//如果找到错误，就下令客户端重发
		if (find_error(recvd_str))
		{
			return_msg[0] = '0';
			printf("error frame received\n");
			sendto(server_sckt, return_msg, strlen(return_msg), 0, (sockaddr *)&add_s, addr_len);
		}
		else
		{//没错就写入文件
			return_msg[0] = '1';
			printf("frame %d received\n", frame_expected);
			printf("position is %d\n", position);
			frame_expected++;
			sendto(server_sckt, return_msg, strlen(return_msg), 0, (sockaddr *)&add_s, addr_len);
			write_str[position + 0] = recvd_str[0];
			write_str[position + 1] = recvd_str[1];
			write_str[position + 2] = recvd_str[2];
			write_str[position + 3] = recvd_str[3];
			position = position + 4;
		}
		
		printf("return_msg:%c\n", return_msg[0]);

		printf("---------------------------------------------------\n");
	}

	//我不知道接收字符串write_str中为啥会出现\r\n这种奇葩换行，只能再读一遍换成\n字符
	char write_str2[MAX_FILE_SIZE];
	memset(write_str2, 0, sizeof(write_str2));
	for (int i = 0, j = 0; i < file_size; i++, j++)
	{
		if (write_str[i] == '\r' && write_str[i + 1] == '\n')
		{
			write_str2[j] = '\n';
			i++;
		}
		else  write_str2[j] = write_str[i];
	}

	fputs(write_str2, write_file);

	closesocket(server_sckt);
	fclose(write_file);
	WSACleanup();
    return 0;
}

