#include <unordered_set>
#include <cstdio>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <map>
#include <set>
#include <cstring>
#include <chrono>
#include "readCmpctBlk.h"
#include "redifine.h"
#include "atom.h"


using namespace std;
using namespace std::chrono;



// 返回重建区块的时间耗费
int64_t calculateCost(const unordered_map<int,string>& mapMissTx, CVT& vPredTx, CVT& vBlkTx, const int blkNum, ReconstructMsg& rm, ostringstream& os){
	vector<string> newBlkTx;
	string msg = rm.printMsg();
	auto timeuse = rm.reConstructBlock(mapMissTx,vPredTx,newBlkTx);
	bool rverifyRes = rm.verifyBlock(newBlkTx,vBlkTx, msg);
	if (!rverifyRes)
		g_error_blk.push_back(blkNum);
	msg += format("serialize: %d\n", GetSerializeSize(rm));
	printf("%s\n", msg.data());
	os << msg; rm.printMsg();
	return timeuse;
}

// 求每一个存在于vBlkTx中的交易,它的最长连续序列是多少
void getLCSeq(CPI& range, CVT& vBlkTx, CVT& vPredTx,UMapTxIndex& mapBlkTxIndex, UMapTxIndex& mapPredTxIndex,UMapTxIndex& ptr){

	int i = range.first;
	while(i<= range.second){
		if(mapBlkTxIndex.count(vPredTx[i].txhash)==0){
			++i;
			continue;
		}
		int b_i = mapBlkTxIndex[vPredTx[i].txhash];
		int end = i, cnt = 0;
		while(end <= range.second && b_i < vBlkTx.size() && (mapPredTxIndex.count(vPredTx[end].txhash)==0 || vBlkTx[b_i].txhash == vPredTx[end].txhash)){
			b_i++;
			if(mapPredTxIndex.count(vPredTx[end++].txhash))
				++cnt;
		}
		
		while(i<end){
			if(mapBlkTxIndex.count(vPredTx[i].txhash))
				ptr.emplace(vPredTx[i].txhash, cnt--);
			i++;
		}
	}
}

// 计算预测序列和实际区块序列误差,method指定了比较方法
void calDifference(
	CPI& range, CVT& vBlkTx, CVT& vPredTx, 
	UMapTxIndex& mapBlkTxIndex,UMapTxIndex& mapPredTxIndex,
	unordered_map<int,string>& mapMissTx,unordered_map<int,int>& vChangeRecord,
	set<int>& vDelIndex,int& nMissTxSz
){

	// 求解每个哈希值的最长子序列长度
	UMapTxIndex res;
	getLCSeq(range,vBlkTx, vPredTx, mapBlkTxIndex, mapPredTxIndex, res);
	// 将区块中[0, offset）的交易标记为miss状态，或者是索引发生变化的状态
	int pi = range.first;
	size_t bj = 0;
	unordered_set<string> visitSet;
	while(pi<=range.second && bj < vBlkTx.size()){
		if(visitSet.count(vBlkTx[bj].txhash))
			bj++;
		else if(visitSet.count(vPredTx[pi].txhash))
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
		// 如果是最后一个,可以直接放入vChangeRecord中,减少删除的index数量
		else if(bj == vBlkTx.size()-1){
			const int bj_idx = mapPredTxIndex[vBlkTx[bj].txhash];
			vChangeRecord[bj_idx] = bj;
			visitSet.insert(vBlkTx[bj++].txhash);
		}
		// 当前predTx序列不存在
		else if(mapBlkTxIndex.count(vPredTx[pi].txhash)==0)
			vDelIndex.insert(pi++);
		else {
			// 
			const int pi_len = res[vPredTx[pi].txhash];
			const int bj_len = res[vBlkTx[bj].txhash];
			if(pi_len < bj_len){
				// 记录pi在vBlkTx中的索引,然后pi跳过
				const int pi_idx = mapBlkTxIndex[vPredTx[pi].txhash];
				vChangeRecord[pi] = pi_idx;
				visitSet.insert(vPredTx[pi++].txhash);
			}
			else{
				// 记录当前bj在predtx中的索引
				const int bj_idx = mapPredTxIndex[vBlkTx[bj].txhash];
				vChangeRecord[bj_idx] = bj;
				visitSet.insert(vBlkTx[bj++].txhash);
			}
		}
	}
}


/// 比较预测序列和真实的区块序列的区别
vector<int> CompareBlockTxandPredTx(
	const int method,const int blkNum, CVT& vBlkTx, CVT& vPredTx, 
	unordered_map<string, int>& mapBlkTxIndex, unordered_map<string, int>& mapPredTxIndex, 
	ostringstream& os, unordered_map<int,string>& mapMissTx, int& nMissTxSz) {

	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	if (vBlkTx.size() <= 1) {
		std::printf("是空区块，总开销: %d 字节， 重建序列开销: %d \n", g_default_tx_sz+80, 2);
		return {g_default_tx_sz+80, 0,0,0, 0};
	}

	// 获取预测序列的范围
	auto range = getPredictRange(vBlkTx, mapPredTxIndex);
	set<int> vDelIndex;
	unordered_map<int, int> vChangeRecord;

	// 将区块中[0, offset）的交易标记为miss状态，或者是索引发生变化的状态
	calDifference(range,vBlkTx,vPredTx,mapBlkTxIndex,mapPredTxIndex,mapMissTx,vChangeRecord,vDelIndex,nMissTxSz);
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	auto cal_diff_time = (std::chrono::duration_cast<milliseconds>(end-start)).count();

	ReconstructMsg rm(std::move(vChangeRecord),vDelIndex, range,mapMissTx.size(), nMissTxSz, vBlkTx.size());
	auto rebuild_time = calculateCost(mapMissTx, vPredTx, vBlkTx, blkNum, rm, os);
	vector<int> cost;
	rm.getCost(cost);
	cost.push_back(GetSerializeSize(rm));
	cost.push_back(cal_diff_time);
	cost.push_back(rebuild_time);
	return std::move(cost);
}


/// <summary>
/// 从固定目录中读取特定区块的预测数据，计算同步区块需要的数据大小
/// </summary>
/// <param name="rootDir"></param>
/// <param name="startBlkNum"></param>
/// <param name="endBlkNum"></param>
void calculate(const string& rootDir,map<int,std::shared_ptr<CompactInfo>> mapCmpctInfo, int startBlkNum, int endBlkNum) {
	double totalCostNone = 0, extraCostNone = 0, seExtraCost = 0;
	vector<vector<int>> vpCostNone;
	const string save_dir = rootDir+"重构区块_NEW.log";
	const string analyse_dir = rootDir + "res.txt";
	ostringstream os, ansOs;
	int cmpctBlkHit = 0;
	ansOs<<"block_num block_sz tx_cnt total_cost extra_cost miss_tx_cnt miss_tx_sz changeCnt delCnt extraSeraSz cmpctSz cal_diff_time, rebuild_time\n";
	while (startBlkNum <= endBlkNum) {
		const string str_blknum = to_string(startBlkNum);
		string blk_dir = rootDir + str_blknum + "_predBlk_NewBlk_Compare.log";
		string predBlkwithNone_dir = rootDir + str_blknum + "_predBlk_with_MissTx.log";
		VT vBlkTx, vPredTxNone;
		unordered_map<string, int> mapBlkTxIndex, mapPredTxNoneIndex;
		// 读取交易序列
		int blkSz = 0;
		string txid = readTxSequence(vBlkTx, mapBlkTxIndex, blk_dir, blkSz);
		if (txid.empty()) {
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
		vpCostNone.emplace_back(std::move(p1));
		totalCostNone += vpCostNone.back()[0];
		extraCostNone += vpCostNone.back()[1];
		seExtraCost += vpCostNone.back().back();
		
		double cmpctSz = 6.2 * vBlkTx.size();
		if(mapCmpctInfo.count(startBlkNum)){
			cmpctSz = mapCmpctInfo[startBlkNum]->blkSz;
			cmpctBlkHit++;
		}
		const auto& v = vpCostNone.back();
        ansOs << format("%d %d %d %d %d %d %d %d %d %d %d %lld %lld\n", startBlkNum, blkSz, vBlkTx.size(), v[0], v[1], p.first, p.second, v[2], v[3],v[4], static_cast<int>(cmpctSz),v[5],v[6]);
		startBlkNum++;
	}
	string msg = "";
	size_t blkCnt = vpCostNone.size();
	msg += format("3-------Total:Block Count: %d, Cost Bytes: %f, Extra Cost Bytes: %f,seExtra cost: %f \n", blkCnt, totalCostNone, extraCostNone,seExtraCost );
	msg += format("3-------Per: Block Cost: %f, ExtraCost: %f,SerializeCost: %f\n", totalCostNone / blkCnt, extraCostNone / blkCnt, seExtraCost/blkCnt);
	msg += format("压缩区块大小数: %d, 总区块数: %d\n",cmpctBlkHit,blkCnt);
	os << msg;
	writeFile(save_dir, os.str());
	writeFile(analyse_dir, ansOs.str());
	std::printf("%s", msg.data());
}


// 计算experiment20200903试验的结果
void calculateexperiment20200903(){
	string rootDir = "/home/hzx/Documents/github/cpp/analyseBitcoin/experiment20200903/";
	string cmpctBlkdir = rootDir+"2020-09-";
	map<int,std::shared_ptr<CompactInfo>> mapCmpctInfo;
	getCmpctInfo(cmpctBlkdir, 3, 10, mapCmpctInfo);
	int startBlkNum = 646560;// 646560;
	int endBlkNum = 647544;// 647544;
	calculate(rootDir, mapCmpctInfo, startBlkNum, endBlkNum);
	printf("构造失败区块数量: %lu \n", g_error_blk.size());
	for(auto&blkNum:g_error_blk)
		printf("%d \n", blkNum);

}

// 计算某个区块的Atom数据细节
void getRbmDetail(){
	string rootDir = "/home/hzx/Documents/github/cpp/analyseBitcoin/experiment20200903/";
	int startBlkNum = 646610;
	// int endBlkNum = 646593;
	int blkSz = 0;
	const string str_blknum = to_string(startBlkNum);
	string blk_dir = rootDir + str_blknum + "_predBlk_NewBlk_Compare.log";
	string predBlkwithNone_dir = rootDir + str_blknum + "_predBlk_with_MissTx.log";
	VT vBlkTx, vPredTx;
	unordered_map<string, int> mapBlkTxIndex, mapPredTxIndex;
	string txid = readTxSequence(vBlkTx, mapBlkTxIndex, blk_dir, blkSz);
	if (txid.empty()) {
		++startBlkNum;
		return;
	}
	readTxSequence(vPredTx, mapPredTxIndex, predBlkwithNone_dir, blkSz);
	unordered_map<int,string> umapMissTx;
	auto p = getMissTxCntSize(vBlkTx, vPredTx, mapPredTxIndex,umapMissTx);
	ostringstream os;
		std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
	if (vBlkTx.size() <= 1) {
		std::printf("是空区块，总开销: %d 字节， 重建序列开销: %d \n", g_default_tx_sz+80, 2);
		return;
	}

	// 获取预测序列的范围
	auto range = getPredictRange(vBlkTx, mapPredTxIndex);
	set<int> vDelIndex;
	unordered_map<int, int> vChangeRecord;

	// 将区块中[0, offset）的交易标记为miss状态，或者是索引发生变化的状态
	calDifference(range,vBlkTx,vPredTx,mapBlkTxIndex,mapPredTxIndex,umapMissTx,vChangeRecord,vDelIndex,p.second);
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	auto cal_diff_time = (std::chrono::duration_cast<milliseconds>(end-start)).count();

	ReconstructMsg rm(std::move(vChangeRecord),vDelIndex, range,umapMissTx.size(), p.second, vBlkTx.size());
	rm.printfDetail(rootDir+"analyse_res/"+str_blknum+"_atom.log");
}

int main() {
	// getRbmDetail();
    calculateexperiment20200903();
	return 0;
}