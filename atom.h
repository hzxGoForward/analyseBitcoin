/*
本程序用于基于Atom协议重建区块
*/

#include <unordered_map>
#include <set>
#include <chrono>
#include <string>
#include "transaction.h"
#include "util.h"
#include "serialize.h"
using namespace std;
typedef pair<int,int> PII;

// 新建结构体,用于存储重建区块需要的数据结构
class ReconstructMsg{
public:
	unordered_map<int, int> vChangeRecord;
	set<int> vDelIndex;
	pair<int,int> range;
	int nMissTxCnt;
	int nMissTxSz;
	int txCnt;

	// 构造函数
	ReconstructMsg(
		unordered_map<int, int> vChange,
		set<int> vD,
		pair<int,int> rg,
		int nTxCnt,
		int nTxSz,
		int cnt
	):range(rg),vDelIndex(vD),vChangeRecord(vChange),nMissTxCnt(nTxCnt),nMissTxSz(nTxSz),txCnt(cnt){}


	/// 重建区块
	int64_t reConstructBlock(const unordered_map<int,string>& mapMissTx, CVT& vPredTx, vector<string>& newBlk){
		std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
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
				i++;
			}
			if(i<=range.second && j < txCnt)
				newBlk[j++] = vPredTx[i++].txhash;
		}

		std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
		chrono::milliseconds millsec = chrono::duration_cast<chrono::milliseconds>(end - start);
		return millsec.count();
	}

	// 验证区块合法性
	bool verifyBlock(const vector<string>& newBlkTx, CVT& vBlkTx, string& msg)const{
		if (newBlkTx.size() != vBlkTx.size()) {
		std::printf("ERROR: two block with different size\n");
		return false;
		}
		for (size_t i = 0; i < newBlkTx.size(); ++i) {
			if (newBlkTx[i] != vBlkTx[i].txhash) {
				std::printf("ERROR: %lu index not match with\n [%s] \n [%s]\n", i, newBlkTx[i].data(), vBlkTx[i].txhash.data());
				msg += format("Construct Failed ××××××××××××××××××××××××××\n\n");
				return false;
			}
		}
	msg += format("Construct Succeed √√√√√√√√√√√√√√√√√√√√√√√√√\n\n");
		return true;
	}


	// 计算删除元素的耗费
	int getDelIndexCost() const {
		vector<int> tmpVDelIndex;
		vector<PII> tmpVDelIndexPair,tmpVChangeIndex;
		vector<pair<PII,PII>> vChangeIndexPair;
		int pre = -1;
		
		// 将最后一个插入其中
		auto it = vDelIndex.begin();
		while(it != vDelIndex.end()){
			int tmp = *it;
			int last = *it;
			while(it != vDelIndex.end() && vDelIndex.count(*it+1)==1){
				last = *it;
				it++;
			}
			if(tmp == last)
				tmpVDelIndex.push_back(tmp);
			else
			{
				tmpVDelIndexPair.emplace_back(tmp,last);
			}
			if(it!= vDelIndex.end())
				it++;
		}
		return tmpVDelIndex.size() * 2 + tmpVDelIndexPair.size()*4 +6;
	}

	// 打印各个结果
	string printMsg() const{
		string msg = "";
		msg += format("Tx Count in New Block: %d\n", txCnt);
		msg += format("Predict Range: [%d, %d]\n", range.first, range.second);
		double nmissTxSize = nMissTxSz;
		msg += format("Missed Tx Count: %d, costs Bytes: %f\n", nMissTxCnt, nMissTxSz);

		int nchangeRecordTxSize = vChangeRecord.size() * 4;
		msg += format("Changed Index Count:%d , costs Bytes: %d\n", vChangeRecord.size(), nchangeRecordTxSize);

		int ndelCost = getDelIndexCost();
		msg += format("Index Deleted Count: %d, costs Bytes: %d \n", vDelIndex.size(), ndelCost);
		int rangeSz = 4;
		int extraCost = nMissTxCnt*2 + nchangeRecordTxSize + ndelCost  + rangeSz;
		int totalCost = 80 + nMissTxSz + extraCost;
		msg += format("total Sync Cost Bytes: %d, Missed Tx Percent %f , Extra Cost bytes: %d \n", totalCost, nmissTxSize / totalCost, extraCost);
		return msg;
	}


	string GetDetail(const vector<Transaction>& vPredTx){
		ostringstream os;
		os<<format("[%d, %d]\n", range.first, range.second);
		os<<"DelIndex: "<<vDelIndex.size()<<"\n";
		for(auto&e: vDelIndex){
			os<<vPredTx[e].entertime<<" "<<e<<"\n";
		}
		os<<"DelCost: "<<getDelIndexCost()<<"\n";
		os<<"ChangeIndex: "<<vChangeRecord.size()<<"\n";
		for(auto&e:vChangeRecord)
			os<<e.first<<"->"<<e.second<<"\n";
		os<<"\n";
		return os.str();
	}



		// 计算总字节,额外字节,缺失交易数,缺失交易大小
	void getCost(vector<int>& vCost){
		int rangeSz = 4;
		int nchangeRecordTxSize = vChangeRecord.size() * 4;
		int extraCost = getExtraCost() ;
		int totalCost = 80 + nMissTxSz + extraCost;
		vCost.push_back(totalCost);
		vCost.push_back(extraCost);
		vCost.push_back(vChangeRecord.size());
		vCost.push_back(vDelIndex.size());
	}

	// 获得额外花费
	int getExtraCost(){
		int nchangeRecordTxSize = vChangeRecord.size() * 4;
		int extraCost = nMissTxCnt*2 + nchangeRecordTxSize + getDelIndexCost()  + 4 ;
		return extraCost;
	}

	ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action)
    {
		vector<pair<short,short>> vC;
		for(auto&e:vChangeRecord)
			vC.emplace_back(static_cast<short>(e.first), static_cast<short>(e.second));
		vector<short> del;
		for(auto&e:vDelIndex)
			del.emplace_back(e);
        READWRITE(vC);
        READWRITE(del);
        READWRITE(pair<short,short>{static_cast<short>(range.first),static_cast<short>(range.second)});
        READWRITE(static_cast<short>(nMissTxCnt));
		READWRITE(static_cast<short>(txCnt));
    }
};
