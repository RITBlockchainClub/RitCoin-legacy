#ifndef BASE_MEMPOOL_H
#define BASE_MEMPOOL_H

#include "transaction.h"

class CTxMemPool
{
public:
    mutable CCriticalSection cs;
    std::map<uint256, CTransaction> mapTx;
    std::map<COutPoint, CInPoint> mapNextTx;

    bool accept(CTxDB& txdb, CTransaction &tx, bool fCheckInputs, bool* pfMissingInputs);
    bool addUnchecked(CTransaction &tx);
    bool remove(CTransaction &tx);
    void queryHashes(std::vector<uint256>& vtxid);
    unsigned long size();
    bool exists(uint256 hash);
    CTransaction& lookup(uint256 hash);
};

extern CTxMemPool mempool;


#endif // BASE_MEMPOOL_H
