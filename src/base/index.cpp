#include "index.h"
#include "block.h"
#include "files.h"
#include "statics.h"

using namespace std;
using namespace boost;


void CDiskTxPos::SetNull()
{
    nFile = -1;
    nBlockPos = 0;
    nTxPos = 0;
}
bool CDiskTxPos::IsNull() const
{
    return (nFile == -1);
}
std::string CDiskTxPos::ToString() const
{
    if (IsNull())
        return "null";
    else
        return strprintf("(nFile=%d, nBlockPos=%d, nTxPos=%d)", nFile, nBlockPos, nTxPos);
}
void CDiskTxPos::print() const
{
    printf("%s", ToString().c_str());
}


void CTxIndex::SetNull()
{
    pos.SetNull();
    vSpent.clear();
}
bool CTxIndex::IsNull()
{
    return pos.IsNull();
}
int CTxIndex::GetDepthInMainChain() const
{
    // Read block header
    CBlock block;
    if (!block.ReadFromDisk(pos.nFile, pos.nBlockPos, false))
        return 0;
    // Find the block in the index
    map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(block.GetHash());
    if (mi == mapBlockIndex.end())
        return 0;
    CBlockIndex* pindex = (*mi).second;
    if (!pindex || !pindex->IsInMainChain())
        return 0;
    return 1 + nBestHeight - pindex->nHeight;
}


CBlock CBlockIndex::GetBlockHeader() const
{
    CBlock block;
    block.nVersion       = nVersion;
    if (pprev)
        block.hashPrevBlock = pprev->GetBlockHash();
    block.hashMerkleRoot = hashMerkleRoot;
    block.nTime          = nTime;
    block.nBits          = nBits;
    block.nNonce         = nNonce;
    return block;
}
uint256 CBlockIndex::GetBlockHash() const
{
    return *phashBlock;
}
int64_t CBlockIndex::GetBlockTime() const
{
    return (int64_t)nTime;
}
CBigNum CBlockIndex::GetBlockTrust() const
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);
    if (bnTarget <= 0)
        return 0;
    return (1);
}
bool CBlockIndex::IsInMainChain() const
{
    return (pnext || this == pindexBest);
}
bool CBlockIndex::EraseBlockFromDisk()
{
    // Open history file
    CAutoFile fileout = CAutoFile(OpenBlockFile(nFile, nBlockPos, "rb+"), SER_DISK, CLIENT_VERSION);
    if (!fileout)
        return false;

    // Overwrite with empty null block
    CBlock block;
    block.SetNull();
    fileout << block;

    return true;
}
int64_t CBlockIndex::GetMedianTimePast() const
{
    int64_t pmedian[nMedianTimeSpan];
    int64_t* pbegin = &pmedian[nMedianTimeSpan];
    int64_t* pend = &pmedian[nMedianTimeSpan];

    const CBlockIndex* pindex = this;
    for (int i = 0; i < nMedianTimeSpan && pindex; i++, pindex = pindex->pprev)
        *(--pbegin) = pindex->GetBlockTime();

    std::sort(pbegin, pend);
    return pbegin[(pend - pbegin)/2];
}
int64_t CBlockIndex::GetMedianTime() const
{
    const CBlockIndex* pindex = this;
    for (int i = 0; i < nMedianTimeSpan/2; i++)
    {
        if (!pindex->pnext)
            return GetBlockTime();
        pindex = pindex->pnext;
    }
    return pindex->GetMedianTimePast();
}
std::string CBlockIndex::ToString() const
{
    return strprintf("CBlockIndex(nprev=%08x, pnext=%08x, nFile=%d, nBlockPos=%-6d nHeight=%d, nMint=%s, nMoneySupply=%s, nStakeTime=%d merkle=%s, hashBlock=%s)",
        pprev, pnext, nFile, nBlockPos, nHeight,
        FormatMoney(nMint).c_str(), FormatMoney(nMoneySupply).c_str(),
        hashMerkleRoot.ToString().substr(0,10).c_str(),
        GetBlockHash().ToString().substr(0,20).c_str());
}
void CBlockIndex::print() const
{
    printf("%s\n", ToString().c_str());
}

uint256 CDiskBlockIndex::GetBlockHash() const
{
    CBlock block;
    block.nVersion        = nVersion;
    block.hashPrevBlock   = hashPrev;
    block.hashMerkleRoot  = hashMerkleRoot;
    block.nTime           = nTime;
    block.nBits           = nBits;
    block.nNonce          = nNonce;
    return block.GetHash();
}
std::string CDiskBlockIndex::ToString() const
{
    std::string str = "CDiskBlockIndex(";
    str += CBlockIndex::ToString();
    str += strprintf("\n                hashBlock=%s, hashPrev=%s, hashNext=%s)",
        GetBlockHash().ToString().c_str(),
        hashPrev.ToString().substr(0,20).c_str(),
        hashNext.ToString().substr(0,20).c_str());
    return str;
}
void CDiskBlockIndex::print() const
{
    printf("%s\n", ToString().c_str());
}
