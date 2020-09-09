struct RebuildBlkMsg{
	unordered_map<int, string> umapMissTxIdx;
	unordered_map<int,int> umapChangeIdx;
	set<int> setDelIdx;
	int cost = 0;
	int extraCost = 0;
	bool addMissTx(const int idx, const string& hash, const int sz){
		if(umapMissTxIdx.count(idx) == 0){
			umapMissTxIdx[idx]= hash;
			cost += sz;
			return true;
		}
		return false;
	}

	bool addDelIdx(const int idx){
		if(setDelIdx.count(idx) == 0){
			setDelIdx.insert(idx);
			cost += 2;
			extraCost += 2;
			return true;
		}
		return false;
	}

	bool addChangeIdx(const int idx1, const int idx2){
		if(umapChangeIdx.count(idx1)==0){
			umapChangeIdx[idx1] = idx2;
			cost += 4;
			extraCost += 4;
			return true;
		}
		return false;
	}

	void clear(){
		umapChangeIdx.clear();
		umapMissTxIdx.clear();
		setDelIdx.clear();
		cost = 0;
	}
};


// void reConstructBlock(const vector<Transaction>& vPredTx, const int txCnt, const pair<int,int>& range, const RebuildBlkMsg& rbm, vector<string>& newBlk) {
// 	// 根据得到的误差重新构建新的区块大小
// 	newBlk.reserve(txCnt);
// 	newBlk.resize(txCnt, "");
// 	// 首先构建缺失的交易
// 	for (auto& p : rbm.umapMissTxIdx)
// 		newBlk[p.first] = p.second;

// 	// // 重建只存在于序列中的交易
// 	// for (auto& p : vInSeq)
// 	// 	newBlk[p.first] = p.second;

// 	// 构建位置发生变化的交易
// 	for (auto& e : rbm.umapChangeIdx)
// 		newBlk[e.second] = vPredTx[e.first].txhash;
// 	// 从vPredTx的range范围内构建交易
// 	int i = range.first, j = 0;
// 	while (i <= range.second&&j<txCnt) {
// 		// 如果j已经被填充了，则跳过j
// 		while (j<txCnt && newBlk[j] != "")
// 			j++;
// 		// 如果是需要删除的交易，或者是位置发生变化的交易
// 		while (i <= range.second&&(rbm.setDelIdx.count(i) || rbm.umapChangeIdx.count(i)))
// 			i++;
// 		if(i<=range.second && j < txCnt)
// 			newBlk[j++] = vPredTx[i++].txhash;
// 	}
// }

// /// <summary>
// /// 比较预测序列和真实的区块序列的区别
// /// </summary>
// /// <param name="vBlkTx">真实区块中的交易</param>
// /// <param name="vPredTx">预测的区块的交易</param>
// /// <param name="mapBlkTxIndex">真实区块交易和索引的映射</param>
// /// <param name="mapPredTxIndex">预测区块交易和索引的映射</param>
// /// <returns></returns>
// std::pair<int, int> CompareBlockTxandPredTxbyDP(const int blkNum, const vector<Transaction>& vBlkTx, const vector<Transaction>& vPredTx, unordered_map<string, int>& mapBlkTxIndex, unordered_map<string, int>& mapPredTxIndex, ostringstream& os, size_t missTxCnt, size_t missTxSz) {
// 	if (vBlkTx.size() <= 1) {
// 		std::printf("是空区块，总开销: %d 字节， 重建序列开销: %d \n", 580, 2);
// 		return {580, 2};
// 	}

// 	auto range = getPredictRange(vBlkTx, mapPredTxIndex);
// 	size_t start = range.first, end = range.second;
	
// 	// 将区块中[0, offset）的交易标记为miss状态，或者是索引发生变化的状态
// 	size_t pi = start, bj = 0;

//     // dp[i][j]表示长度为i的predTx转换到长度为j的vBlkTx的最小代价
//     // vector<vector<int>> dp(end - start+1, vector<int>(vBlkTx.size(), 1e9));
// 	map<pair<int,int>, RebuildBlkMsg> mapCost;
// 	vector<Transaction> tmp(vPredTx.begin()+start, vPredTx.begin()+end+1);
// 	RebuildBlkMsg rbm;
// 	mapCost[{0,0}] = rbm;

// 	// 预处理i = 0的情况
// 	for(int j = 1; j < vBlkTx.size(); ++j){
// 		rbm.addMissTx(j, vBlkTx[j].txhash, vBlkTx[j].sz);
// 		mapCost[{0,j}] = rbm;
// 	}

// 	// 预处理j = 0 的情况
// 	rbm.clear();
// 	for(int i = 1;i<tmp.size(); ++i){
// 		rbm.addDelIdx(i);
// 		mapCost[{i, 0}] = rbm;
// 	}

// 	for(int i = 1; i<tmp.size(); ++i){
//         for(int j = 1; j< vBlkTx.size(); ++j){
// 			// 如果哈希值相等的时候
//             if(tmp[i].txhash == vBlkTx[j].txhash){
// 				mapCost[{i,j}] = mapCost[{i-1,j-1}];
// 			}
// 			// 如果预测序列中交易是多余交易
// 			else if(mapBlkTxIndex.count(tmp[i].txhash)){
// 				mapCost[{i,j}] = mapCost[{i-1, j}];
// 				mapCost[{i,j}].addDelIdx(i);
// 			}
// 			// 如果预测序列不包括这笔交易
// 			else if(mapPredTxIndex.count(vBlkTx[j].txhash)==0){
// 				mapCost[{i,j}] = mapCost[{i, j-1}];
// 				mapCost[{i,j}].addMissTx(j, vBlkTx[j].txhash,vBlkTx[j].sz);
// 			}
// 			// 两笔交易都在对方的预测序列中
// 			else{
// 				// 将tmp[i].hash放入change中
// 				int i_idx = mapBlkTxIndex[tmp[i].txhash];
// 				int j_idx = mapPredTxIndex[vPredTx[j].txhash];
// 				RebuildBlkMsg rbm1, rbm2, rbm3;
				
// 				rbm1 = mapCost[{i-1,j}];
// 				rbm2 = mapCost[{i, j-1}];
// 				rbm3 = mapCost[{i-1,j-1}];
// 				rbm1.addChangeIdx(i, i_idx);
// 				rbm2.addChangeIdx(j_idx, j);
// 				rbm3.addChangeIdx(i, i_idx);
// 				rbm3.addChangeIdx(j_idx, j);
// 				RebuildBlkMsg& rbm = rbm1;
// 				if(rbm2.cost < rbm.cost)
// 					rbm = rbm2;
// 				if(rbm3.cost < rbm.cost)
// 					rbm = rbm3;
// 				mapCost[{i,j}] = rbm;
// 			}
//         }
//     }

// 	const RebuildBlkMsg& resRbm = mapCost[{tmp.size()-1, vBlkTx.size()-1}];
// 	string msg = "";

// 	msg += format("Tx Count in New Block: %d\n", vBlkTx.size());
// 	msg += format("Predict Range: [%d, %d]\n", start, end);
	
// 	double nmissTxSize = missTxSz;
// 	msg += format("Missed Tx Count: %d, costs Bytes: %f\n", missTxCnt, nmissTxSize);
	

// 	msg += format("Changed Index Count:%d , costs Bytes: %d\n", resRbm.umapChangeIdx.size(), resRbm.umapChangeIdx.size()*4);


// 	msg += format("Index Deleted Count: %d, costs Bytes: %d \n", resRbm.setDelIdx.size(), resRbm.setDelIdx.size() * 2);

// 	int totalCost = 80 + 8 + resRbm.cost;
// 	msg += format("total Sync Cost Bytes: %d, Missed Tx Percent %f , Extra Cost bytes: %d \n", totalCost, nmissTxSize / totalCost, resRbm.extraCost);

// 	// 构建并验证
// 	vector<string> newBlkTx;
// 	bool verifyRes = reConstructBlock(vPredTx, vBlkTx, range, resRbm, newBlkTx);

// 	if (verifyRes)
// 		msg += format("Construct Succeed √√√√√√√√√√√√√√√√√√√√√√√√√\n\n");
// 	else{
// 		msg += format("Construct Failed ××××××××××××××××××××××××××\n\n");
// 		g_error_blk.push_back(blkNum);
// 	}
// 	std::printf("%s", msg.data());
// 	os << msg;

// 	return { totalCost , resRbm.extraCost};
// }