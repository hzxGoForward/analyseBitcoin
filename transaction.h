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