﻿#include <unordered_set>
#include <cstdio>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <exception>
#include <algorithm>
#include <unordered_map>
#include <map>
#include <set>
#include <cassert>
#include <cstring>
using namespace std;

vector<int> g_error_blk;
const int g_default_tx_sz = 400;

/*
1. 将新区块中的交易组织成结构体形式
2. 从新区块中挑出本地最近接收的一笔交易tx以及接收时间。
3. 将预测序列组织成vector形式，从中删除时间大于tx的所有交易
4. 重新生成预测序列的交易
5. 与新的区块中的交易序列进行比较，查看接近程度。
*/

// step 1. 新区块的交易格式
class Transaction {
public:
	//进入时间 交易哈希 对应索引 交易费用 交易大小 交易权重
	string entertime;
	string txhash;
	int fee;
	int sz;
	int weight;
	bool missed;
	bool onlyInSeq;
	Transaction(const string& e, const string& t,  const int f, const int s, const int w,const bool m = false, const bool seq = false) 
		:entertime(e), txhash(t), fee(f), sz(s), weight(w), missed(m), onlyInSeq(seq) {}
	void operator = (const Transaction& t) {
		entertime = t.entertime;
		txhash = t.txhash;
		fee = t.fee;
		sz = t.sz;
		weight = t.weight;
	}
};

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

/// <summary>
/// 从区块文件中读取交易数据，并且返回其中最大的时间戳的那笔交易
/// </summary>
/// <param name="vtx">vtx存储所有交易的信息</param>
/// <param name="delTxIndexSet">vdelTxIndex存储需要删除的交易的索引</param>
/// <param name="mapTxIndex">为每一笔交易建立哈希值到其索引的映射</param>
/// <param name="file_dir">文件路径</param>
/// <returns></returns>
string readTxSequence(vector<Transaction>& vtx, unordered_map<string,int>& mapTxIndex, const string& file_dir, int& blkSz) {
	ifstream ifs(file_dir, ios::in);
	if (!ifs.is_open()) {
		string msg = format("无法打开文件 %s", file_dir.data());
		std::printf("%s\n", msg.data());
		return "";
	}
	char buf[40960] = {};
	string minTime = "";
	string txid = "";
	string lasttxHash;// 最后一笔交易的哈希值
	while (!ifs.eof()) {
		memset(buf, '\n', 1024);
		ifs.getline(buf, sizeof(buf));
		if (buf[0] == '2' && buf[1] == '0' && buf[2] == '2' && buf[3] == '0' && buf[4] == '-') {
			string line(buf);
			line.pop_back();
			vector<string> tmp;
			split(line, ' ', tmp);
			// 记录交易的哈希值对应的索引
			mapTxIndex[tmp[1]] = vtx.size();
			bool missed = (tmp[2] == "Pool" || tmp[2] == "None");
			bool onlyInSeq = tmp[2] == "Seq";
			vtx.emplace_back(tmp[0], tmp[1], stoi(tmp[3]), stoi(tmp[4]), stoi(tmp[5]), missed, onlyInSeq);
			if (tmp[2] != "None" && tmp[0] > minTime) {
				minTime = tmp[0];
				txid = tmp[1];
			}
			lasttxHash = tmp[1];
		}
		else if (buf[0] == 'b'&&buf[1] == 'l'){
			string line(buf);
			line.pop_back();
			vector<string> tmp;
			split(line, ' ', tmp);
			blkSz = stoi(tmp[1]);
		}
		else if (buf[0] == 'p'||buf[0]=='[')
			break;
	}
	return minTime;
}


pair<int,int> getMissTxCntSize(const vector<Transaction>& vBlkTx, const vector<Transaction>& vPredTx, unordered_map<string, int>& mapPredTxIndex, unordered_map<int,string>& mapMissTx) {
	size_t sz=0;
	for (size_t i = 0; i < vBlkTx.size(); ++i) {
		if (vBlkTx[i].missed || mapPredTxIndex.count(vBlkTx[i].txhash)==0){
			mapMissTx[i] = vBlkTx[i].txhash;
			if (mapPredTxIndex.count(vBlkTx[i].txhash)) {
			const int index = mapPredTxIndex[vBlkTx[i].txhash];
			sz += vPredTx[index].sz;
			}
			else
				sz += g_default_tx_sz;
		}
	}
	return {mapMissTx.size(),sz};
}

// 获取预测序列的起始和结束
pair<size_t,size_t> getPredictRange(const vector<Transaction>& vBlkTx, unordered_map<string, int>& mapPredTxIndex){
	pair<int,int> ans = {mapPredTxIndex.size(),0};
	pair<string,string> ansHash = {"",""};
	for(size_t i = 0; i < vBlkTx.size(); ++i){
		if(mapPredTxIndex.count(vBlkTx[i].txhash)){
			const int idx = mapPredTxIndex[vBlkTx[i].txhash];
			if(ans.first > idx){
				ansHash.first = vBlkTx[i].txhash;
				ans.first = idx;
			}
			if(ans.second < idx){
				ansHash.second = vBlkTx[i].txhash;
				ans.second = idx;
			}
		}
	}
	return ans;
}

void fillBeforeOffset(const vector<Transaction>& vBlkTx, const int offset, unordered_map<string, int>& mapPredTxIndex,unordered_map<int,string>& mapMissTx,unordered_map<int, int>& vChangeRecord, int& nMissTxSz){
	int bj = 0;
	while(bj < offset){
		//if(vBlkTx[bj].missed && mapPredTxIndex.count(vBlkTx[bj].txhash)==0){
		if(mapPredTxIndex.count(vBlkTx[bj].txhash)==0){
			if(mapMissTx.count(bj)==0){
				mapMissTx[bj] = vBlkTx[bj].txhash;
				if(vBlkTx[bj].sz>0)
					nMissTxSz += vBlkTx[bj].sz;
				else
				{
					nMissTxSz += g_default_tx_sz;
				}
			}
			bj++;
		}
		else if(mapPredTxIndex.count(vBlkTx[bj].txhash)){
			const int index = mapPredTxIndex[vBlkTx[bj].txhash];
			vChangeRecord[index] = bj;
		}
		bj++;
	}
}

// 新建结构体,用于存储重建区块需要的数据结构
class ReconstructMsg{
public:
	unordered_map<int, int> vChangeRecord;
	set<int> vDelIndex;
	pair<int,int> range;
	int nMissTxCnt;
	int nMissTxSz;
	int txCnt;

	// 移动构造函数
	ReconstructMsg(
		unordered_map<int, int> vChange,
		set<int> vD,
		pair<int,int> rg,
		int nTxCnt,
		int nTxSz,
		int cnt
	):range(rg),vDelIndex(vD),vChangeRecord(vChange),nMissTxCnt(nTxCnt),nMissTxSz(nTxSz),txCnt(cnt){}


	/// 重建区块
	void reConstructBlock(const unordered_map<int,string>& mapMissTx, const vector<Transaction>& vPredTx, vector<string>& newBlk){
		// 根据得到的误差重新构建新的区块大小
		newBlk.reserve(txCnt);
		newBlk.resize(txCnt, "");
		// 首先构建缺失的交易
		std::set<string> missHashSet;
		for (auto& p : mapMissTx){
			newBlk[p.first] = p.second;
			missHashSet.insert(p.second);
		}

		// 构建位置发生变化的交易
		for (auto& e : vChangeRecord)
			newBlk[e.second] = vPredTx[e.first].txhash;
		// 从vPredTx的range范围内构建交易
		int i = range.first, j = 0;
		while (i <= range.second&&j<txCnt) {
			// 如果j已经被填充了，则跳过j
			while (j<txCnt && newBlk[j] != "")
				j++;
			// 如果是需要删除的交易，或者是位置发生变化的交易
			while (i <= range.second&&(vChangeRecord.count(i) || vDelIndex.count(i)||missHashSet.count(vPredTx[i].txhash))){
				if(vDelIndex.count(i)){
					vDelIndex.erase(i);
				}
				i++;
			}
			if(i<=range.second && j < txCnt)
				newBlk[j++] = vPredTx[i++].txhash;
		}
	}

	// 验证区块合法性
	bool verifyBlock(const vector<string>& newBlkTx, const vector<Transaction>& vBlkTx)const{
		if (newBlkTx.size() != vBlkTx.size()) {
		std::printf("ERROR: two block with different size\n");
		return false;
		}
		for (size_t i = 0; i < newBlkTx.size(); ++i) {
			if (newBlkTx[i] != vBlkTx[i].txhash) {
				std::printf("ERROR: %lu index not match with\n [%s] \n [%s]\n", i, newBlkTx[i].data(), vBlkTx[i].txhash.data());
				return false;
			}
		}
		return true;
	}

	// 打印各个结果
	string printMsg()const{
		string msg = "";
		msg += format("Tx Count in New Block: %d\n", txCnt);
		msg += format("Predict Range: [%d, %d]\n", range.first, range.second);
		double nmissTxSize = nMissTxSz;
		msg += format("Missed Tx Count: %d, costs Bytes: %f\n", nMissTxCnt, nMissTxSz);

		int nchangeRecordTxSize = vChangeRecord.size() * 4;
		msg += format("Changed Index Count:%d , costs Bytes: %d\n", vChangeRecord.size(), nchangeRecordTxSize);

		int ndelTxSize = vDelIndex.size() * 2;
		msg += format("Index Deleted Count: %d, costs Bytes: %d \n", vDelIndex.size(), ndelTxSize);
		int rangeSz = 4;
		int extraCost = nMissTxCnt*2 + nchangeRecordTxSize + ndelTxSize - vDelIndex.size()*2  + rangeSz ;
		int totalCost = 80 + nMissTxSz + extraCost;
		msg += format("total Sync Cost Bytes: %d, Missed Tx Percent %f , Extra Cost bytes: %d \n", totalCost, nmissTxSize / totalCost, extraCost);
		msg += format("dummy delIndex cnt: %u , save bytes: %d\n\n", vDelIndex.size(), vDelIndex.size()*2);
		return msg;
	}


	// 计算总字节,额外字节,缺失交易数,缺失交易大小
	void getCost(vector<int>& vCost){
		int rangeSz = 4;
		int nchangeRecordTxSize = vChangeRecord.size() * 4;
		int ndelTxSize = vDelIndex.size() * 2;
		int extraCost = nMissTxCnt*2 + nchangeRecordTxSize + ndelTxSize - vDelIndex.size()*2  + rangeSz ;
		int totalCost = 80 + nMissTxSz + extraCost;
		vCost.push_back(totalCost);
		vCost.push_back(extraCost);
		vCost.push_back(vChangeRecord.size());
		vCost.push_back(vDelIndex.size());
	}
};

void calculateCost(const unordered_map<int,string>& mapMissTx, const vector<Transaction>& vPredTx, const vector<Transaction>& vBlkTx, const int blkNum, ReconstructMsg& rm, ostringstream& os){
	vector<string> newBlkTx;

	rm.reConstructBlock(mapMissTx,vPredTx,newBlkTx);
	string msg = "";
	bool rverifyRes = rm.verifyBlock(newBlkTx,vBlkTx);
	msg += rm.printMsg();
	if (rverifyRes)
		msg += format("Construct Succeed √√√√√√√√√√√√√√√√√√√√√√√√√\n\n");
	else{
		msg += format("Construct Failed ××××××××××××××××××××××××××\n\n");
		g_error_blk.push_back(blkNum);
	}
	printf("%s\n", msg.data());
	os << msg; rm.printMsg();
}

void calDifferenceOne(
	const pair<int,int>& range, const vector<Transaction>& vBlkTx, const vector<Transaction>& vPredTx, 
	unordered_map<string,int>& mapBlkTxIndex,unordered_map<string,int>& mapPredTxIndex,
	unordered_map<int,string>& mapMissTx,unordered_map<int,int>& vChangeRecord,
	set<int>& vDelIndex,int& nMissTxSz
){

	// 将区块中[0, offset）的交易标记为miss状态，或者是索引发生变化的状态
	int pi = range.first;
	size_t bj = 0;
	unordered_set<string> visitSet;
	while(pi<=range.second && bj < vBlkTx.size()){
		if(visitSet.count(vBlkTx[bj].txhash))
			bj++;
		// miss 的交易
		else if(mapPredTxIndex.count(vBlkTx[bj].txhash) == 0){
			if(mapMissTx.count(bj)==0){
				mapMissTx[bj] = vBlkTx[bj].txhash;
				if(vBlkTx[bj].sz>0)
					nMissTxSz += vBlkTx[bj].sz;
				else
				{
					nMissTxSz += 500;
				}
			}
			bj++;
		}
		// 前后序列相同
		else if(vBlkTx[bj].txhash == vPredTx[pi].txhash){
			pi++;
			bj++;
		}
		// 当前predTx序列不存在
		else if(mapBlkTxIndex.count(vPredTx[pi].txhash)==0){
			vDelIndex.insert(pi++);
		}
		else {
			// 记录pi在vBlkTx中的索引,然后pi跳过
			const int pi_idx = mapBlkTxIndex[vPredTx[pi].txhash];
			vChangeRecord[pi] = pi_idx;
			visitSet.insert(vPredTx[pi++].txhash);
		}
	}
}

void calDifferenceTwo(
	const pair<int,int>& range, const vector<Transaction>& vBlkTx, const vector<Transaction>& vPredTx, 
	unordered_map<string,int>& mapBlkTxIndex,unordered_map<string,int>& mapPredTxIndex,
	unordered_map<int,string>& mapMissTx,unordered_map<int,int>& vChangeRecord,
	set<int>& vDelIndex,int& nMissTxSz
){
	// 将区块中[0, offset）的交易标记为miss状态，或者是索引发生变化的状态
	int pi = range.first;
	size_t bj = 0;
	unordered_set<string> visitSet;
	while(pi<=range.second && bj < vBlkTx.size()){
		if(visitSet.count(vPredTx[pi].txhash))
			pi++;
		// miss 的交易
		else if(mapPredTxIndex.count(vBlkTx[bj].txhash) == 0){
			if(mapMissTx.count(bj)==0){
				mapMissTx[bj] = vBlkTx[bj].txhash;
				if(vBlkTx[bj].sz>0)
					nMissTxSz += vBlkTx[bj].sz;
				else
				{
					nMissTxSz += 500;
				}
			}
			bj++;
		}
		// 前后序列相同
		else if(vBlkTx[bj].txhash == vPredTx[pi].txhash){
			pi++;
			bj++;
		}
		// 当前predTx序列不存在
		else if(mapBlkTxIndex.count(vPredTx[pi].txhash)==0){
			vDelIndex.insert(pi++);
		}
		else {
			// 记录当前bj在predtx中的索引
			const int bj_idx = mapPredTxIndex[vBlkTx[bj].txhash];
			vChangeRecord[bj_idx] = bj;
			visitSet.insert(vBlkTx[bj++].txhash);
		}
	}
}

/// 比较预测序列和真实的区块序列的区别
vector<int> CompareBlockTxandPredTx(
	const int method,
	const int blkNum, const vector<Transaction>& vBlkTx, const vector<Transaction>& vPredTx, 
	unordered_map<string, int>& mapBlkTxIndex, unordered_map<string, int>& mapPredTxIndex, 
	ostringstream& os, unordered_map<int,string>& mapMissTx, int& nMissTxSz) {
	if (vBlkTx.size() <= 1) {
		std::printf("是空区块，总开销: %d 字节， 重建序列开销: %d \n", 580, 2);
		return {580, 2};
	}

	// 获取预测序列的范围
	auto range = getPredictRange(vBlkTx, mapPredTxIndex);
	set<int> vDelIndex;
	unordered_map<int, int> vChangeRecord;

	// 将区块中[0, offset）的交易标记为miss状态，或者是索引发生变化的状态
	if(method == 1)
		calDifferenceOne(range,vBlkTx,vPredTx,mapBlkTxIndex,mapPredTxIndex,mapMissTx,vChangeRecord,vDelIndex,nMissTxSz);
	else
		calDifferenceTwo(range,vBlkTx,vPredTx,mapBlkTxIndex,mapPredTxIndex,mapMissTx,vChangeRecord,vDelIndex,nMissTxSz);

	ReconstructMsg rm(std::move(vChangeRecord),vDelIndex, range,mapMissTx.size(), nMissTxSz, vBlkTx.size());
	calculateCost(mapMissTx, vPredTx, vBlkTx, blkNum, rm, os);
	vector<int> cost;
	rm.getCost(cost);
	return std::move(cost);
}


/// <summary>
/// 从固定目录中读取特定区块的预测数据，计算同步区块需要的数据大小
/// </summary>
/// <param name="rootDir"></param>
/// <param name="startBlkNum"></param>
/// <param name="endBlkNum"></param>
void calculate(const string& rootDir, int startBlkNum, int endBlkNum) {
	double totalCostNone = 0, extraCostNone = 0;
	vector<vector<int>> vpCostNone;
	const string save_dir = rootDir+"重构区块_NEW.log";
	const string analyse_dir = rootDir + "res.txt";
	ostringstream os, ansOs;
	ansOs<<"block_num block_sz tx_cnt total_cost extra_cost miss_tx_cnt miss_tx_sz changeCnt delCnt\n";
	while (startBlkNum <= endBlkNum) {
		const string str_blknum = to_string(startBlkNum);
		string blk_dir = rootDir + str_blknum + "_predBlk_NewBlk_Compare.log";
		string predBlkwithNone_dir = rootDir + str_blknum + "_predBlk_with_MissTx.log";
		vector<Transaction> vBlkTx, vPredTxNone;
		unordered_map<string, int> mapBlkTxIndex, mapPredTxNoneIndex;
		// 读取交易序列
		int blkSz = 0;
		string res = readTxSequence(vBlkTx, mapBlkTxIndex, blk_dir, blkSz);
		if (res.empty()) {
			++startBlkNum;
			continue;
		}
		readTxSequence(vPredTxNone, mapPredTxNoneIndex, predBlkwithNone_dir, blkSz);
		unordered_map<int,string> umapMissTx;
		auto p = getMissTxCntSize(vBlkTx, vPredTxNone, mapPredTxNoneIndex,umapMissTx);
		// 比较
		std::printf("------------block height: %d------------- \n", startBlkNum);
		os << format("------------block height: %d------------- \n", startBlkNum);

		auto p1 = CompareBlockTxandPredTx(1,startBlkNum,vBlkTx, vPredTxNone, mapBlkTxIndex, mapPredTxNoneIndex, os, umapMissTx, p.second);
		auto p2 = CompareBlockTxandPredTx(2,startBlkNum,vBlkTx, vPredTxNone, mapBlkTxIndex, mapPredTxNoneIndex, os, umapMissTx, p.second);
		if(p2[1] < p1[1])
			vpCostNone.push_back(p2);
		else
			vpCostNone.push_back(p1);
		totalCostNone += vpCostNone.back()[0];
		extraCostNone += vpCostNone.back()[1];
        ansOs << format("%d %d %d %d %d %d %d\n", startBlkNum, blkSz, vBlkTx.size(), vpCostNone.back()[0], vpCostNone.back()[1],p.first, p.second, vpCostNone.back()[2], vpCostNone.back()[3]);
		startBlkNum++;
	}
	string msg = "";

	msg += format("3-------Total Block Count: %d, Total Cost Bytes: %f, Total Extra Cost Bytes: %f \n", vpCostNone.size(), totalCostNone, extraCostNone);

	msg += format("3-------Per Block Cost: %f, Per ExtraCost: %f\n", totalCostNone / vpCostNone.size(), extraCostNone / vpCostNone.size());
	os << msg;
	writeFile(save_dir, os.str());
	writeFile(analyse_dir, ansOs.str());
	std::printf("%s", msg.data());
}

int main() {
	string rootDir = "/home/hzx/Documents/github/cpp/analyseBitcoin/experiment20200903/";
	int startBlkNum = 646560;// 646560;
	int endBlkNum = 647486;// 647486;
	calculate(rootDir, startBlkNum, endBlkNum);
	printf("构造失败区块数量: %lu \n", g_error_blk.size());
	for(auto&blkNum:g_error_blk)
		printf("%d \n", blkNum);
    return 0;
}