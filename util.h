#include <string>
#include <cstdio>
#include <sstream>
#include <vector>
#include <fstream>
#include <cassert>
#include <cstdio>
using namespace std;

vector<int> g_error_blk;                // 构建失败的区块,存放区块号
const int g_default_tx_sz = 400;        // 默认的交易大小

template< typename... Args >
std::string format(const char* format, Args... args);


void split(const string s, const char token, vector<string>& vs);

void writeFile(const string& filepath, const string& msg)
{
	ofstream ofs;
	ofs.open(filepath, ios::out);
	if (ofs.is_open()) {
		// cout << "正在打开文件写入..." << filepath << endl;
		ofs.write(msg.data(), msg.size());
		ofs.close();
	}
	else {
		std::printf("%s \n",("打开文件失败, 文件名： "+ filepath).data());
	}
	std::printf("%s \n", ("成功写入： " + filepath).data());
}


void split(const string s, const char token, vector<string>& vs) {
    if(s.empty())
        return;
	stringstream iss(s);
	string word;
	while (getline(iss, word, token)) {
		vs.push_back(word);
	}
}

// 仔细学习这个format函数的含义
template< typename... Args >
std::string format(const char* format, Args... args) {
	int length = std::snprintf(nullptr, 0, format, args...);
	assert(length >= 0);

	char* buf = new char[length + 1];
	std::snprintf(buf, length + 1, format, args...);

	std::string str(buf);
	delete[] buf;
	return str;
}