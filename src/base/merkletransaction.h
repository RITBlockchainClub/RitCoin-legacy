#ifndef BASE_MERKLETRANSACTION_H
#define BASE_MERKLETRANSACTION_H

#include "transaction.h"

/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx : public CTransaction
{
public:
    uint256 hashBlock;
    std::vector<uint256> vMerkleBranch;
    int nIndex;
    // memory only
    mutable bool fMerkleVerified;

    CMerkleTx()
    {
        Init();
    }
    CMerkleTx(const CTransaction& txIn) : CTransaction(txIn)
    {
        Init();
    }
    IMPLEMENT_SERIALIZE
    (
        nSerSize += SerReadWrite(s, *(CTransaction*)this, nType, nVersion, ser_action);
        nVersion = this->nVersion;
        READWRITE(hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(nIndex);
    )
    void Init();
    int SetMerkleBranch(const CBlock* pblock=NULL);
    int GetDepthInMainChain(CBlockIndex* &pindexRet) const;
    int GetDepthInMainChain() const;
    bool IsInMainChain() const;
    int GetBlocksToMaturity() const;
    bool AcceptToMemoryPool(CTxDB& txdb, bool fCheckInputs=true);
    bool AcceptToMemoryPool();
};

#endif // BASE_MERKLETRANSACTION_H
