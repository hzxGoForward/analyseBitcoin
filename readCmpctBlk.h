/*
本程序用于读取压缩区块中接收区块的信息,用于证明程序中交易池
*/

#include <cstdio>
#include <fstream>
#include <sstream>
#include <cstring>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include "util.h"
using namespace std;

// 压缩区块信息
struct CompactInfo{
    string ip, recvTime;
    string blockHash;
    int height;
    int blkSz;
    int txCnt;
    int preFillTxCnt;
    int mempoolTxCnt;
    int orphanTxCnt;
    bool decodeSuccess;
};

/// 完成映射信息
std::shared_ptr<CompactInfo> constructMap(const vector<string>& tmp){
    std::shared_ptr<CompactInfo> pCmpBlk = make_shared<CompactInfo>();
    pCmpBlk->ip = tmp[0];
    pCmpBlk->recvTime = tmp[1];
    pCmpBlk->blockHash = tmp[2];
    pCmpBlk->height = stoi(tmp[3]);
    pCmpBlk->blkSz = stoi(tmp[4]);
    pCmpBlk->txCnt = stoi(tmp[5]);
    pCmpBlk->preFillTxCnt = stoi(tmp[6]);
    pCmpBlk->mempoolTxCnt = stoi(tmp[7]);
    pCmpBlk->orphanTxCnt = stoi(tmp[8]);
    pCmpBlk->decodeSuccess = tmp[9] == "SUCCESS";
    return pCmpBlk;
}

void readCompactBlockInfo(const string& file_dir, map<int,std::shared_ptr<CompactInfo>>& mapCmpctInfo){
    fstream ifs(file_dir, ios::in);
    if(ifs.is_open()==false){
        printf("OPEN ERROR: %s\n", file_dir.data());
        return;
    }
    char buf[1024];
    while(!ifs.eof()){
        memset(buf, '\n', sizeof(buf));
        ifs.getline(buf, sizeof(buf));
        string line(buf);
        if(line.size()<2)
            continue;
        vector<string> tmp;
        split(line, ' ', tmp);
        if(tmp.size()<9)
            continue;
        mapCmpctInfo.emplace(stoi(tmp[3]), constructMap(tmp));
    }
}

void getCmpctInfo(const string& dir, const int dayBegin, const int dayEnd, map<int,std::shared_ptr<CompactInfo>>& mapCmpctInfo){
    int day = dayBegin;
    while(day <= dayEnd){
        string file_dir = dir;
        if(day<10)
            file_dir += format("0%d_compactBlockValidation.log",day);
        else 
            file_dir += format("%d_compactBlockValidation.log",day);
        readCompactBlockInfo(file_dir, mapCmpctInfo);
        day++;
    }
}


