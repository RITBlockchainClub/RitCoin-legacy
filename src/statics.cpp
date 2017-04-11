#include "statics.h"
#include "base/block.h"
#include "wallet.h"

using namespace std;
using namespace boost;

CCriticalSection cs_setpwalletRegistered;
std::set<CWallet*> setpwalletRegistered;

CCriticalSection cs_main;
CCriticalSection cs_deque;

unsigned int nTransactionsUpdated = 0;

int nCoinbaseMaturity = 5;

unsigned char pchMessageStartTest[4] = { 0xdb, 0xe4, 0xf6, 0xf1 };
unsigned char pchMessageStart[4] = { 0xad, 0xac, 0xaa, 0xab };

uint256 hashGenesisBlock = hashGenesisBlockOfficial;

CBlockIndex* pindexGenesisBlock = NULL;
int nBestHeight = -1;
CBigNum bnBestChainTrust = 0;
CBigNum bnBestInvalidTrust = 0;
uint256 hashBestChain = 0;
CBlockIndex* pindexBest = NULL;
int64_t nTimeBestReceived = 0;

std::map<uint256, CBlockIndex*> mapBlockIndex;

//Orphan Maps
std::map<uint256, CDataStream*> mapOrphanTransactions;
std::map<uint256, map<uint256, CDataStream*> > mapOrphanTransactionsByPrev;

// Constant stuff for coinbase transactions we create:
CScript COINBASE_FLAGS;

const string strMessageMagic = "rit Signed Message:\n";

// Settings
int64_t nTransactionFee = TX_FEE;
