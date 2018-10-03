// ConsoleLogger_Demo.cpp : Defines the entry point for the console application.
//
#include "Console.h"
/*
int main(int argc, char* argv[])
{
	SetConsoleTitleA("Main Process");
	printf("Hi , I'm the main-process , and  I'm going to show you 2 more consoles...");
	CConsoleLoggerEx coloured_console;
	coloured_console.Create("File Stream");

	coloured_console.printf(" ");

	coloured_console.cprintf(CConsoleLoggerEx::COLOR_WHITE, " ");

	CConsoleLoggerEx coloured_console1;

	coloured_console1.Create("Text Stream");

	coloured_console1.cprintf(" ");

	getchar();

	return 0;
}*/

#pragma comment (lib, "ws2_32.lib")
#include <WS2tcpip.h>
#include <winsock2.h>
#include <iostream>
#include <string>
#include <fstream>
#include <ctime>
#include <sstream>
#include <vector>
#include <thread>
#include "sha512.hh"

//#include <winsock2.h>
//#include <minwinbase.h>

using namespace std;
using namespace sw;
static const string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz" "0123456789+/";
void decode64(string b64, string output);
void con_print(CConsoleLoggerEx stream, string msg);
void print_vector(vector<char> v);
string encode64(string filename);
string currentDateTime();
string base64_decode(std::string const& encoded_string);
string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len);
CConsoleLoggerEx text_stream;
CConsoleLoggerEx file_stream;
static inline bool is_base64(unsigned char c) {
	return (isalnum(c) || (c == '+') || (c == '/'));
}

int main()
{
	SetConsoleTitleA("Connection Stream");
	file_stream.Create("File Stream");
	text_stream.Create("Text Stream");
	locale swedish("swedish");
	locale::global(swedish);
	WSADATA data;
	WORD version = MAKEWORD(2, 2);
	int wsOk = WSAStartup(version, &data);
	if (wsOk != 0)
	{
		cout << "Can't start Winsock! " << wsOk;
		return 88;
	}

	SOCKET in = socket(AF_INET, SOCK_DGRAM, 0);
	sockaddr_in serverHint;
	serverHint.sin_addr.S_un.S_addr = ADDR_ANY;
	serverHint.sin_family = AF_INET;
	int port = 1337;
	serverHint.sin_port = htons(port);

	if (::bind(in, (sockaddr*)&serverHint, sizeof(serverHint)) == SOCKET_ERROR)
	{
		cout << "Can't bind socket! " << WSAGetLastError() << endl;
		return 66;
	}

	sockaddr_in client;
	int clientLength = sizeof(client);
	char sizebuf[1024];
	char typbuf[1024];
	char filename[4096];
	char hash[513];
	char split_sz[1024];
	char fclientip[256];
	ZeroMemory(fclientip, 256);
	cout << "Server now listening on port: " << port << endl;

	while (true)
	{
		ZeroMemory(&client, clientLength);
		ZeroMemory(sizebuf, 1024);
		ZeroMemory(typbuf, 1024);
		ZeroMemory(filename, 4096);
		ZeroMemory(hash, 512);
		ZeroMemory(split_sz, 1024);

		int pre_typ = recvfrom(in, typbuf, 1024, 0, (sockaddr*)&client, &clientLength); // gets type of the sent data
		string type = (string)typbuf;

		if (type == "TEXT") {
			int pre_bytes = recvfrom(in, sizebuf, 1024, 0, (sockaddr*)&client, &clientLength); // gets string size
			if (pre_bytes == SOCKET_ERROR) { // checks if size of string was successful
				cout << "pre_bytes error: " << WSAGetLastError() << endl;
				break;
			}
			vector<char> buf;
			int size = stoi(sizebuf) + 1;
			buf.resize(size);
			int bytesInput = recvfrom(in, &buf[0], buf.size(), 0, (sockaddr*)&client, &clientLength); //gets string -> vector with sizebuf size
			if (bytesInput == SOCKET_ERROR) { // checks if it could receive the string
				cout << "bytesInput error: " << WSAGetLastError() << endl;
				break;
			}
			char clientIp[256];
			ZeroMemory(clientIp, 256);

			inet_ntop(AF_INET, &client.sin_addr, clientIp, 256);
			if (equal(begin(clientIp), end(clientIp), begin(fclientip)))
			{
				stringstream temp;
				for (auto i = buf.begin(); i != buf.end(); ++i) {
					temp << *i;
				}
				temp << "\b";
				con_print(text_stream, temp.str());
			}
			else
			{
				cout << "(" << currentDateTime() << ") -> New [Text] recvRequest: [" << string(clientIp) << "]";
				stringstream temp;
				temp << clientIp << " : "<< "[" << typbuf << "]" << endl; //prints ip, type and message
				for (auto i = buf.begin(); i != buf.end(); ++i) {
					temp << *i;
				}
				temp << "\b";
				copy(begin(clientIp), end(clientIp), begin(fclientip));
				con_print(text_stream, temp.str());
			}

			// done getting message from user.
		}
		else if (type == "DATA") {
			const clock_t begin_time = clock();
			int pre_name = recvfrom(in, filename, 4096, 0, (sockaddr*)&client, &clientLength); // gets filename
			if (pre_name == SOCKET_ERROR) {
				cout << "pre_name error: " << WSAGetLastError() << endl;
				break;
			}
			int pre_hash = recvfrom(in, hash, 512, 0, (sockaddr*)&client, &clientLength); // gets file hash in sha512
			if (pre_hash == SOCKET_ERROR) {
				cout << "pre_hash error: " << WSAGetLastError() << endl;
				break;
			}
			int pre_bytes = recvfrom(in, sizebuf, 1024, 0, (sockaddr*)&client, &clientLength); // per split size
			if (pre_bytes == SOCKET_ERROR) {
				cout << "pre_bytes error: " << WSAGetLastError() << endl;
				break;
			}

			int pre_split = recvfrom(in, split_sz, 1024, 0, (sockaddr*)&client, &clientLength); // # of  splits
			if (pre_split == SOCKET_ERROR) {
				cout << "pre_bytes error: " << WSAGetLastError() << endl;
				break;
			}

			char client_ip[256];
			int size = stoi(sizebuf);
			string type_ = typbuf;
			string hases = hash;
			int split = stoi(split_sz);
			ZeroMemory(client_ip, 256);
			inet_ntop(AF_INET, &client.sin_addr, client_ip, 256);

			vector<char> kys;
			kys.resize(size*split + 1);

			cout << client_ip << " : " << "[" << typbuf << "] " << "Incoming file: " << filename << " | Split Size: " << sizebuf << " | Number of splits: " << split_sz << endl;
			for (int i = 0; i <= split; ++i) {
				if (i == 0) {
					int sendkys = recvfrom(in, &kys[0], size, 0, (sockaddr*)&client, &clientLength);
					if (sendkys == SOCKET_ERROR) {
						cout << "sendkys1 Error: " << WSAGetLastError() << endl;
					}
				}
				else if (i == 1) {
					int sendkys = recvfrom(in, &kys[size], size, 0, (sockaddr*)&client, &clientLength);
					if (sendkys == SOCKET_ERROR) {
						cout << "sendkys2 Error: " << WSAGetLastError() << endl;
					}
				}
				else if (i < split) {
					int sendkys = recvfrom(in, &kys[size*i], size, 0, (sockaddr*)&client, &clientLength);
					if (sendkys == SOCKET_ERROR) {
						cout << "sendkys3 Error: " << WSAGetLastError() << endl;
					}
				}
				cout << setprecision(1) << fixed << ((double)i / split) * 100 << "%\b\b\b\b\b\b";
			}
			stringstream ss;
			cout << "Reconstructing File..." << "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";
			for (auto f = kys.begin(); f != kys.end(); ++f) { ss << *f; }
			string base64 = ss.str();
			decode64(base64, filename);
			cout << "[RECEIVED FILE: \"" << filename << "\" in (" << float(clock() - begin_time) / CLOCKS_PER_SEC << "s)] ";
			string hash512 = (string)(hash);
			if ((string)sha512::file(filename) == (string)hash) {
				cout << "HASH: " << hash512.substr(0, 48) << "....(48 of 512) = CORRECT" << endl;
			}
			else {
				cout << "HASH: " << hash512.substr(0, 48) << "....(48 of 512) = FAULTY" << endl;
			}
			//done getting file from client.
		}
	}

	closesocket(in);

	WSACleanup();
}

void con_print(CConsoleLoggerEx stream, string msg) {
	stream.printf(msg.c_str());
}

void print_vector(vector<char> v) {
	for (auto i = v.begin(); i != v.end(); ++i) {
		if (*i == '\0') {
			break;
		}
		else {
			cout << *i;
		}
	}
}

string currentDateTime() {
	std::time_t t = std::time(nullptr);
	std::tm tm = *std::localtime(&t);
	stringstream ff;
	ff << std::put_time(&tm, "%Y-%m-%d-%H.%M.%S");
	return ff.str();
}

string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
	std::string ret;
	int i = 0;
	int j = 0;
	unsigned char char_array_3[3];
	unsigned char char_array_4[4];

	while (in_len--) {
		char_array_3[i++] = *(bytes_to_encode++);
		if (i == 3) {
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for (i = 0; (i < 4); i++)
				ret += base64_chars[char_array_4[i]];
			i = 0;
		}
	}

	if (i)
	{
		for (j = i; j < 3; j++)
			char_array_3[j] = '\0';

		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
		char_array_4[3] = char_array_3[2] & 0x3f;

		for (j = 0; (j < i + 1); j++)
			ret += base64_chars[char_array_4[j]];

		while ((i++ < 3))
			ret += '=';
	}

	return ret;
}

string base64_decode(std::string const& encoded_string) {
	int in_len = encoded_string.size();
	int i = 0;
	int j = 0;
	int in_ = 0;
	unsigned char char_array_4[4], char_array_3[3];
	std::string ret;

	while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
		char_array_4[i++] = encoded_string[in_]; in_++;
		if (i == 4) {
			for (i = 0; i < 4; i++)
				char_array_4[i] = base64_chars.find(char_array_4[i]);

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (i = 0; (i < 3); i++)
				ret += char_array_3[i];
			i = 0;
		}
	}

	if (i) {
		for (j = i; j < 4; j++)
			char_array_4[j] = 0;

		for (j = 0; j < 4; j++)
			char_array_4[j] = base64_chars.find(char_array_4[j]);

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
		char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

		for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
	}

	return ret;
}

string encode64(string filename) {
	ifstream fin(filename, ios::binary);
	ostringstream ostring;
	ostring << fin.rdbuf();
	string dout = ostring.str();
	return base64_encode((const unsigned char *)dout.c_str(), dout.length());
}

void decode64(string b64, string output) {
	ofstream fout(output, ios::binary);
	string ofb64 = base64_decode(b64);
	fout << ofb64;
}