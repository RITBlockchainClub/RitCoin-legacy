#ifndef STATICS_H
#define STATICS_H

#include "bignum.h"

class CWallet;
class CBlock;
class CBlockIndex;

static const unsigned int MAX_BLOCK_SIZE = 10000000;
static const unsigned int MAX_BLOCK_SIZE_GEN = MAX_BLOCK_SIZE/2;
static const unsigned int MAX_BLOCK_SIGOPS = MAX_BLOCK_SIZE/50;
static const unsigned int MAX_ORPHAN_TRANSACTIONS = MAX_BLOCK_SIZE/100;
static const int64_t TX_FEE = CENT;
static const int64_t MAX_MONEY = 100000000 * COIN;
static const int64_t MIN_TXOUT_AMOUNT = TX_FEE;

inline bool MoneyRange(int64_t nValue)
{
    return (nValue >= 0 && nValue <= MAX_MONEY);
}
static std::string strMintWarning;
static std::string strMintMessage = ("Info: Minting suspended due to locked wallet.");

// Threshold for nLockTime: below this value it is interpreted as block number, otherwise as UNIX timestamp.
static const int LOCKTIME_THRESHOLD = 500000000; // Tue Nov  5 00:53:20 1985 UTC
static const int MIN_BLOCK_SPACING = 30; // 10-seconds minimum block time spacing
static const int MAX_BLOCK_SPACING = 1 * 60 * 60; // 1 hour max block spacing

#ifdef USE_UPNP
static const int fHaveUPnP = true;
#else
static const int fHaveUPnP = false;
#endif

static const uint256 hashGenesisBlockOfficial("0x033ea1201ea02d82d56f5aa01181ffcf01c62a050096c1177dbbfa03badf7308");
static const uint256 hashGenesisBlockTestNet ("0x0000000000000000000000000000000000000000000000000000000000000000");

static CBigNum bnProofOfWorkLimit(~uint256(0) >> 5);
static CBigNum bnInitialHashTarget(~uint256(0) >> 10);

static const int64_t nMaxClockDrift = 3 * 60;        // three minutes

static CMedianFilter<int> cPeerBlockCounts(5, 0); // Amount of blocks that other nodes claim to have

extern unsigned char pchMessageStartTest[4];
extern unsigned char pchMessageStart[4];

extern CScript COINBASE_FLAGS;

extern CCriticalSection cs_main;
extern CCriticalSection cs_deque;

extern std::map<uint256, CBlockIndex*> mapBlockIndex;

extern std::map<uint256, CDataStream*> mapOrphanTransactions;
extern std::map<uint256, std::map<uint256, CDataStream*> > mapOrphanTransactionsByPrev;

extern uint256 hashGenesisBlock;
extern int nCoinbaseMaturity;
extern CBlockIndex* pindexGenesisBlock;
extern int nBestHeight;
extern CBigNum bnBestChainTrust;
extern CBigNum bnBestInvalidTrust;
extern uint256 hashBestChain;
extern CBlockIndex* pindexBest;
extern unsigned int nTransactionsUpdated;
extern uint64_t nLastBlockTx;
extern uint64_t nLastBlockSize;
extern int64_t nLastCoinStakeSearchInterval;
extern const std::string strMessageMagic;
extern int64_t nTimeBestReceived;
extern CCriticalSection cs_setpwalletRegistered;
extern std::set<CWallet*> setpwalletRegistered;

// Settings
extern int64_t nTransactionFee;


#endif // STATICS_H
