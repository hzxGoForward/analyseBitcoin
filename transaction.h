/*
本程序用于从文件中读取并组织成交易的数据结构
*/

#include <string>
#include <vector>
using namespace std;
class Transaction;
typedef vector<Transaction> VT;
typedef const VT CVT;
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

/// <summary>
/// 从区块文件中读取交易数据，并且返回其中最大的时间戳的那笔交易
/// </summary>
/// <param name="vtx">vtx存储所有交易的信息</param>
/// <param name="delTxIndexSet">vdelTxIndex存储需要删除的交易的索引</param>
/// <param name="mapTxIndex">为每一笔交易建立哈希值到其索引的映射</param>
/// <param name="file_dir">文件路径</param>
/// <returns></returns>
string readTxSequence(VT& vtx, UMapTxIndex& mapTxIndex, const string& file_dir, int& blkSz) {
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
		memset(buf, '\n', sizeof(buf));
		ifs.getline(buf, sizeof(buf));
		if (buf[0] == '2' && buf[1] == '0' && buf[2] == '2' && buf[3] == '0' && buf[4] == '-') {
			string line(buf);
			if(line.size()<2)
				continue;
			line.pop_back();
			vector<string> tmp;
			split(line, ' ', tmp);
			// 记录交易的哈希值对应的索引
			mapTxIndex[tmp[1]] = vtx.size();
			bool missed = (tmp[2] == "Pool" || tmp[2] == "None");
			bool onlyInSeq = tmp[2] == "Seq";
			vtx.emplace_back(tmp[0], tmp[1], stoi(tmp[3]), stoi(tmp[4]), stoi(tmp[5]), missed, onlyInSeq);
			if ((tmp[2] != "None" && tmp[2] != "Pool") && tmp[0] > minTime) {
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
	return txid;
}



pair<int,int> getMissTxCntSize(CVT& vBlkTx, CVT& vPredTx, unordered_map<string, int>& mapPredTxIndex, unordered_map<int,string>& mapMissTx) {
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
pair<size_t,size_t> getPredictRange(CVT& vBlkTx, unordered_map<string, int>& mapPredTxIndex){
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
