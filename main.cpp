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
#include "transaction.h"


using namespace std;
using namespace std::chrono;



// 返回重建区块的时间耗费
int64_t calculateCost(const UMapIndexTx& mapMissTx, CVT& vPredTx, CVT& vBlkTx, const int blkNum, ReconstructMsg& rm, ostringstream& os){
	vector<string> newBlkTx;
	string msg = rm.printMsg();
	auto timeuse = rm.reConstructBlock(mapMissTx,vPredTx,newBlkTx);
	bool rverifyRes = rm.verifyBlock(newBlkTx,vBlkTx, msg);
	if (!rverifyRes)
		g_error_blk.push_back(blkNum);
	msg += format("serialize: %d\n", GetSerializeSize(rm));
	std::printf("%s\n", msg.data());
	os << msg; 
	rm.printMsg();
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
	UMapIndexTx& mapMissTx,unordered_map<int,int>& vChangeRecord,
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
vector<int> CompareBlockTxandPredTx(const int blkNum, CVT& vBlkTx, CVT& vPredTx, 
	unordered_map<string, int>& mapBlkTxIndex, unordered_map<string, int>& mapPredTxIndex, 
	ostringstream& os, UMapIndexTx& mapMissTx, int& nMissTxSz) {

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
	os<< rm.GetDetail(vPredTx);
	vector<int> vcost;
	rm.getCost(vcost);
	vcost.push_back(GetSerializeSize(rm));
	vcost.push_back(cal_diff_time);
	vcost.push_back(rebuild_time);
	return std::move(vcost);
}


/// <summary>
/// 从固定目录中读取特定区块的预测数据，计算同步区块需要的数据大小
/// </summary>
/// <param name="rootDir"></param>
/// <param name="startBlkNum"></param>
/// <param name="endBlkNum"></param>
void calculate(CS& rootDir,map<int,std::shared_ptr<CompactInfo>> mapCmpctInfo, int startBlkNum, int endBlkNum, CS& newBlkSuffix, CS& predBlkSuffix, string exp) {
	CS save_dir = rootDir+"running.log";
	CS analyse_dir = format("%s%d_%d_analyse.txt", rootDir.data(), startBlkNum, endBlkNum);
	ostringstream os, analyseOS;
	int cmpctBlkHit = 0, missTxCnt = 0, blkCnt = 0;
	analyseOS<<"block_num block_sz tx_cnt total_cost extra_cost miss_tx_cnt miss_tx_sz changeCnt delCnt extraSeraSz cmpctSz cal_diff_time, rebuild_time mempool_cnt\n";
	while (startBlkNum <= endBlkNum) {
		CS str_blknum = to_string(startBlkNum);
		CS realBlkDir = rootDir + str_blknum + newBlkSuffix; // "_predBlk_NewBlk_Compare.log";
		CS predBlkDir = rootDir + str_blknum + predBlkSuffix;// "_predBlk_with_MissTx.log";
		// 读取交易序列
		VT vBlkTx, vPredTx;
		unordered_map<string, int> mapBlkTxIndex, mapPredTxIndex;
		BlockMsg blkMsg = readTxSequence(vBlkTx, mapBlkTxIndex, realBlkDir,  exp);
		if (blkMsg.txid.empty()) {
			++startBlkNum;
			continue;
		}
		BlockMsg blkMsg2 = readTxSequence(vPredTx, mapPredTxIndex, predBlkDir,exp);
		// 计算缺失交易
		UMapIndexTx umapMissTx;
		auto p = getMissTxCntSize(vBlkTx, vPredTx, mapPredTxIndex,umapMissTx);
		if ( p.first > 1)
			missTxCnt++;
		// 比较
		CS log = format("------------block height: %d------------- \n", startBlkNum);
		os<<log;

		// return std::move(vcost);
		// totalCost Dinos_cost Sc_cnt Sd_cnt GetSerializeSize(Dinos_cost) cal_diff_time rebuild_time
		auto vCost = CompareBlockTxandPredTx(startBlkNum, vBlkTx, vPredTx, mapBlkTxIndex, mapPredTxIndex, os, umapMissTx, p.second);
		
		// 计算压缩区块大小
		auto f = 6+getRandom(0.0, 0.2);
		int cmpctSz = f * vBlkTx.size();
		if(blkMsg.compactBlkSz>0){
			cmpctSz = blkMsg.compactBlkSz;
			cmpctBlkHit++;
		}
		else if(mapCmpctInfo.count(startBlkNum)){
			cmpctSz = mapCmpctInfo[startBlkNum]->blkSz;
			cmpctBlkHit++;
		}
		// block_num block_sz tx_cnt total_cost Dinos_cost miss_tx_cnt miss_tx_sz changeCnt delCnt extraSeraSz cmpctSz cal_diff_time, rebuild_time, mempool_cnt
        analyseOS << format("%d %d %d %d %d %d %d %d %d %d %d %d %d %d\n", startBlkNum, blkMsg.blockSize, vBlkTx.size(), 
		vCost[0], vCost[1], p.first, p.second, vCost[2], vCost[3], vCost[4], cmpctSz, vCost[5], vCost[6], blkMsg.mempool_tx_cnt);
		startBlkNum++;
		blkCnt++;
	}
	const string msg = format("压缩区块大小数: %d, 总区块数: %d, 缺失交易超过1笔的区块数: %d \n",cmpctBlkHit, blkCnt, missTxCnt);
	os << msg;
	writeFile(save_dir, os.str());
	writeFile(analyse_dir, analyseOS.str());
	std::printf("%s", msg.data());
}

// 计算experiment20200903试验的结果
void calculateexperiment20200903normal(){
	string rootDir = "/home/hzx/Documents/github/cpp/analyseBitcoin/experiment20200903/";
	string cmpctBlkdir = rootDir+"2020-09-";
	map<int,std::shared_ptr<CompactInfo>> mapCmpctInfo;
	getCmpctInfo(cmpctBlkdir, 3, 10, mapCmpctInfo);
	int startBlkNum = 646549;// 646549;
	int endBlkNum = 647557;// 647544;
	calculate(rootDir, mapCmpctInfo, startBlkNum, endBlkNum,"_predBlk_NewBlk_Compare.log", "_predBlk_with_MissTx.log","0903");
	std::printf("构造失败区块数量: %lu \n", g_error_blk.size());
	for(auto&blkNum:g_error_blk)
		std::printf("%d \n", blkNum);
}

// 计算experiment20200908试验的结果
void calculateexperiment20200908normal(){
	string rootDir = "/home/hzx/Documents/github/cpp/analyseBitcoin/experiment20200908/";
	string cmpctBlkdir = rootDir+"2020-09-";
	map<int,std::shared_ptr<CompactInfo>> mapCmpctInfo;
	getCmpctInfo(cmpctBlkdir, 1, 31, mapCmpctInfo);
	int startBlkNum = 647544;// 647544;
	int endBlkNum = 649905;// 649905;
	calculate(rootDir, mapCmpctInfo, startBlkNum, endBlkNum, "_new_block.log","_pred_block.log", "0908");
	std::printf("构造失败区块数量: %lu \n", g_error_blk.size());
	for(auto&blkNum:g_error_blk)
		std::printf("%d \n", blkNum);
}

// 计算experiment20200925试验的结果
void calculateexperiment20200925normal(){
	string rootDir = "/home/hzx/Documents/github/cpp/analyseBitcoin/experiment20200925/";
	string cmpctBlkdir = rootDir+"compactBlock/2020-09-";
	map<int,std::shared_ptr<CompactInfo>> mapCmpctInfo;
	// getCmpctInfo(cmpctBlkdir, 1, 31, mapCmpctInfo);
	int startBlkNum = 647545;// 647545;
	int endBlkNum = 651925;// 651925;
	calculate(rootDir, mapCmpctInfo, startBlkNum, endBlkNum, "_new_block.log","_pred_block.log", "0908");
	std::printf("构造失败区块数量: %lu \n", g_error_blk.size());
	for(auto&blkNum:g_error_blk)
		std::printf("%d \n", blkNum);
}
// // 计算experiment20200908试验的结果
// void calculateexperiment20200925(){
// 	string rootDir = "/home/hzx/Documents/github/cpp/analyseBitcoin/experiment20200925/";
// 	int startBlkNum = 649919, endBlkNum = 649962;// 649962;
// 	map<int,std::shared_ptr<CompactInfo>> mapCmpctInfo;
// 	// 读取交易序列
// 	ostringstream os, analyseOS;
// 	analyseOS<<"block_num block_sz tx_cnt total_cost extra_cost miss_tx_cnt miss_tx_sz changeCnt delCnt extraSeraSz cmpctSz cal_diff_time, rebuild_time mempool_cnt\n";
// 	while(startBlkNum <= endBlkNum){
// 		for(int i = 0; i < 1000; ++i){
			
// 			string realBlkDir_miner = rootDir + "simulate_miner_"+format("%d_%d.log", startBlkNum, i);
// 			VT vBlkTx;
// 			unordered_map<string, int> mapBlkTxIndex;
// 			BlockMsg blkMsg1 = readTxSequence(vBlkTx, mapBlkTxIndex, realBlkDir_miner,"0925");
// 			if (blkMsg1.txid.empty())
// 				continue;
// 			for(int j = 1; j< 2; ++j){
				
// 				VT  vPredTx;
// 				unordered_map<string, int> mapPredTxIndex;
// 				string realBlkDir_pred = rootDir + "simulate_pred_"+ format("%d_%d.log", startBlkNum, i+j);
// 				BlockMsg blkMsg2 = readTxSequence(vPredTx, mapPredTxIndex, realBlkDir_pred,"0925");
// 				if (blkMsg2.txid.empty())
// 					continue;
// 				os << format("------------block height: %d, simulate: %d, pred: %d ------------- \n", startBlkNum, i, i+j);
// 				UMapIndexTx umapMissTx;
// 				auto p = getMissTxCntSize(vBlkTx, vPredTx, mapPredTxIndex, umapMissTx);
// 				// if(p.first>1)
// 				// 	std::printf("%d %d %d miss交易多余1个\n", );
// 				auto v = CompareBlockTxandPredTx(1,startBlkNum,vBlkTx, vPredTx, mapBlkTxIndex, mapPredTxIndex, os, umapMissTx, p.second);
// 				std::printf("%s\n", os.str().data());
// 				// block_num block_sz tx_cnt total_cost extra_cost miss_tx_cnt miss_tx_sz changeCnt delCnt extraSeraSz cmpctSz cal_diff_time, rebuild_time, mempool_cnt
// 				analyseOS << format("%d %d %d %d %d %d %d %d %d %d %d %lld %lld\n", startBlkNum, blkMsg1.blockSize, vBlkTx.size(), v[0], v[1], p.first, p.second, v[2], v[3],v[4], blkMsg1.compactBlkSz,v[5],v[6], blkMsg1.mempool_tx_cnt);	
// 			}
// 		}
// 		startBlkNum++;
// 	}
// 	CS analyse_dir = rootDir + "res.txt";
// 	CS save_dir = rootDir+"重构区块_NEW.log";
// 	writeFile(analyse_dir, analyseOS.str());
// 	writeFile(save_dir, os.str());
// 	return;
// }

// 计算experiment20200908试验的结果
void calculateexperiment20200925(){
	string rootDir = "/home/hzx/Documents/github/cpp/analyseBitcoin/experiment20200925/";
	int startBlkNum = 651388, endBlkNum = 651925;// 649962;
	CS analyse_dir = rootDir + format("%d_%d_res.txt", startBlkNum, endBlkNum);
	map<int,std::shared_ptr<CompactInfo>> mapCmpctInfo;
	// 读取交易序列
	ostringstream os, analyseOS;
	analyseOS<<"block_num block_sz tx_cnt total_cost Dino_cost miss_tx_cnt miss_tx_sz changeCnt delCnt extraSeraSz cmpctSz cal_diff_time, rebuild_time mempool_cnt\n";
	while(startBlkNum <= endBlkNum){
		for(int i = 0; i < 50; ++i){
			string realBlkDir_miner = rootDir + "simulate_miner_"+format("%d_%d.log", startBlkNum, i);
			VT vBlkTx;
			unordered_map<string, int> mapBlkTxIndex;
			BlockMsg blkMsg1 = readTxSequence(vBlkTx, mapBlkTxIndex, realBlkDir_miner,"0925");
			if (blkMsg1.txid.empty())
				continue;
			VT  vPredTx;
			unordered_map<string, int> mapPredTxIndex;
			string realBlkDir_pred = rootDir + "simulate_pred_"+ format("%d_%d.log", startBlkNum, i);
			BlockMsg blkMsg2 = readTxSequence(vPredTx, mapPredTxIndex, realBlkDir_pred,"0925");
			if (blkMsg2.txid.empty())
				continue;
			os << format("------------block height: %d, simulate: %d, pred: %d ------------- \n", startBlkNum, i, i);
			UMapIndexTx umapMissTx;
			auto p = getMissTxCntSize(vBlkTx, vPredTx, mapPredTxIndex, umapMissTx);
			// if(p.first>1)
			// 	std::printf("%d %d %d miss交易多余1个\n", );
			auto v = CompareBlockTxandPredTx(startBlkNum,vBlkTx, vPredTx, mapBlkTxIndex, mapPredTxIndex, os, umapMissTx, p.second);
			std::printf("%s\n", os.str().data());
			// block_num block_sz tx_cnt total_cost extra_cost miss_tx_cnt miss_tx_sz changeCnt delCnt extraSeraSz cmpctSz cal_diff_time, rebuild_time, mempool_cnt
			analyseOS << format("%d %d %d %d %d %d %d %d %d %d %d %lld %lld\n", startBlkNum, blkMsg1.blockSize, vBlkTx.size(), v[0], v[1], p.first, p.second, v[2], v[3],v[4], blkMsg1.compactBlkSz,v[5],v[6], blkMsg1.mempool_tx_cnt);	
	}
		startBlkNum++;
	}
	
	CS save_dir = rootDir+"重构区块_NEW.log";
	writeFile(analyse_dir, analyseOS.str());
	writeFile(save_dir, os.str());
	return;
}

// 计算某个区块的Atom数据细节
void getRbmDetail(CS& rootDir, CS& blkSuffix, CS& predSuffix, const pair<int,int> range, CS& df){
	int startBlkNum = range.first;
	while(startBlkNum<=range.second){
		CS str_blknum = to_string(startBlkNum++);
		string realBlkDir = rootDir + str_blknum + blkSuffix;
		string predBlkDir = rootDir + str_blknum + predSuffix;
		VT vBlkTx, vPredTx;
		unordered_map<string, int> mapBlkTxIndex, mapPredTxIndex;
		BlockMsg blkMsg  = readTxSequence(vBlkTx, mapBlkTxIndex, realBlkDir, df);
		if (blkMsg.txid.empty()) {
			++startBlkNum;
			return;
		}
		readTxSequence(vPredTx, mapPredTxIndex, predBlkDir, df);
		UMapIndexTx umapMissTx;
		auto p = getMissTxCntSize(vBlkTx, vPredTx, mapPredTxIndex,umapMissTx);
		std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
		if (vBlkTx.size() <= 1) {
			std::printf("是空区块，总开销: %d 字节， 重建序列开销: %d \n", g_default_tx_sz+80, 2);
			return;
		}

		// 遍历vBlkTx中每一笔交易，找出存在于vPredTx中交易的最大时间戳
		string maxTxid = "", ts = "";
		for(auto&tx:mapBlkTxIndex){
			if(mapPredTxIndex.count(tx.first)){
				int idx = mapPredTxIndex[tx.first];
				if(ts.empty() || ts <vPredTx[idx].entertime){
					ts = vPredTx[idx].entertime;
					maxTxid = vPredTx[idx].txhash;
				}
			}
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
		string res = rm.GetDetail(vPredTx);
		res += format("time: %s maxtxid: %s, \n", ts.data(),maxTxid.data());
		res += format("tx_cnt: %d\n", vBlkTx.size());
		CS save_dir = rootDir+"analyse_res/"+str_blknum+"_atom.log";
		writeFile(save_dir, res);
	}
}


int main() {
	
	// CS blkSuffix = "_predBlk_NewBlk_Compare.log";
	// CS predSuffix = "_predBlk_with_MissTx.log";
	
	
	// CS rootDir = "/home/hzx/Documents/github/cpp/analyseBitcoin/experiment20200908/";
	// CS blkSuffix = "_new_block.log";
	// CS predSuffix = "_pred_block.log";
	// const pair<int,int> p = {649600, 649616};
	// CS df = "0908";
	// getRbmDetail(rootDir, blkSuffix, predSuffix, p, df);
    
	
	// calculateexperiment20200903normal();
	calculateexperiment20200908normal();
	// calculateexperiment20200925normal();

	// calculateexperiment20200925();
	return 0;
}