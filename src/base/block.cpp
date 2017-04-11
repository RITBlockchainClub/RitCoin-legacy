#include "block.h"
#include "util.h"
#include "statics.h"
#include "db.h"
#include "txdb-leveldb.h"
#include "boost/foreach.hpp"
#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include "base/files.h"
#include "base/mempool.h"
#include "net.h"
#include "dispatch.h"
#include "ui_interface.h"
#include "checkpoints.h"

using namespace std;
using namespace boost;


void InvalidChainFound(CBlockIndex* pindexNew)
{
    if (pindexNew->bnChainTrust > bnBestInvalidTrust)
    {
        bnBestInvalidTrust = pindexNew->bnChainTrust;
        CTxDB().WriteBestInvalidTrust(bnBestInvalidTrust);
        MainFrameRepaint();
    }
    printf("InvalidChainFound: invalid block=%s  height=%d  trust=%s\n", pindexNew->GetBlockHash().ToString().substr(0,20).c_str(), pindexNew->nHeight, CBigNum(pindexNew->bnChainTrust).ToString().c_str());
    printf("InvalidChainFound:  current best=%s  height=%d  trust=%s\n", hashBestChain.ToString().substr(0,20).c_str(), nBestHeight, CBigNum(bnBestChainTrust).ToString().c_str());
}
bool CheckProofOfWork(uint256 hash, unsigned int nBits)
{
    CBigNum bnTarget;
    bnTarget.SetCompact(nBits);

    // Check range
    if (bnTarget <= 0 || bnTarget > bnProofOfWorkLimit)
        return error("CheckProofOfWork() : nBits below minimum work");

    return true;
}
int64_t GetProofOfWorkReward()
{
    int64_t CMS = pindexBest->nMoneySupply; // CMS is current money supply
    if(CMS < MAX_MONEY)
    {
        return (MAX_MONEY - CMS);
    }
    else if(CMS == MAX_MONEY)
    {
        return 0; // shouldnt happen
    }
    // should only return 0 if for some reason money supply is higher than it should be, which would then be corrected as soon as some transactions are sent.

    return 0;
}





void CBlock::SetNull()
{
    nVersion = CBlock::CURRENT_VERSION;
    hashPrevBlock = 0;
    hashMerkleRoot = 0;
    nTime = 0;
    nBits = 0;
    nNonce = 0;
    vtx.clear();
    vMerkleTree.clear();
    nDoS = 0;
    Integrity.clear();
    Authenticity.clear();
}

bool CBlock::IsNull() const
{
    return (nBits == 0);
}

uint256 CBlock::GetHash() const
{
    return Hash(BEGIN(nVersion), END(nNonce));
}

int64_t CBlock::GetBlockTime() const
{
    return (int64_t)nTime;
}

void CBlock::UpdateTime(const CBlockIndex* pindexPrev)
{
    nTime = std::max(GetBlockTime(), GetAdjustedTime());
}




int64_t CBlock::GetMaxTransactionTime() const
{
    int64_t maxTransactionTime = 0;
    BOOST_FOREACH(const CTransaction& tx, vtx)
        maxTransactionTime = std::max(maxTransactionTime, (int64_t)tx.nTime);
    return maxTransactionTime;
}

uint256 CBlock::BuildMerkleTree() const
{
    vMerkleTree.clear();
    BOOST_FOREACH(const CTransaction& tx, vtx)
        vMerkleTree.push_back(tx.GetHash());
    int j = 0;
    for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
    {
        for (int i = 0; i < nSize; i += 2)
        {
            int i2 = std::min(i+1, nSize-1);
            vMerkleTree.push_back(Hash(BEGIN(vMerkleTree[j+i]),  END(vMerkleTree[j+i]),
                                       BEGIN(vMerkleTree[j+i2]), END(vMerkleTree[j+i2])));
        }
        j += nSize;
    }
    return (vMerkleTree.empty() ? 0 : vMerkleTree.back());
}

std::vector<uint256> CBlock::GetMerkleBranch(int nIndex) const
{
    if (vMerkleTree.empty())
        BuildMerkleTree();
    std::vector<uint256> vMerkleBranch;
    int j = 0;
    for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
    {
        int i = std::min(nIndex^1, nSize-1);
        vMerkleBranch.push_back(vMerkleTree[j+i]);
        nIndex >>= 1;
        j += nSize;
    }
    return vMerkleBranch;
}

uint256 CBlock::CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex)
{
    if (nIndex == -1)
        return 0;
    BOOST_FOREACH(const uint256& otherside, vMerkleBranch)
    {
        if (nIndex & 1)
            hash = Hash(BEGIN(otherside), END(otherside), BEGIN(hash), END(hash));
        else
            hash = Hash(BEGIN(hash), END(hash), BEGIN(otherside), END(otherside));
        nIndex >>= 1;
    }
    return hash;
}


bool CBlock::WriteToDisk(unsigned int& nFileRet, unsigned int& nBlockPosRet)
{
    // Open history file to append
    CAutoFile fileout = CAutoFile(AppendBlockFile(nFileRet), SER_DISK, CLIENT_VERSION);
    if (!fileout)
    {
        return error("CBlock::WriteToDisk() : AppendBlockFile failed");
    }
    // Write index header
    unsigned int nSize = fileout.GetSerializeSize(*this);
    fileout << FLATDATA(pchMessageStart) << nSize;

    // Write block
    long fileOutPos = ftell(fileout);
    if (fileOutPos < 0)
        return error("CBlock::WriteToDisk() : ftell failed");
    nBlockPosRet = fileOutPos;
    fileout << *this;

    // Flush stdio buffers and commit to disk before returning
    fflush(fileout);
    if (!IsInitialBlockDownload() || (nBestHeight+1) % 500 == 0)
    {
        FileCommit(fileout);
    }

    return true;
}

bool CBlock::ReadFromDisk(unsigned int nFile, unsigned int nBlockPos, bool fReadTransactions)
{
    SetNull();

    // Open history file to read
    CAutoFile filein = CAutoFile(OpenBlockFile(nFile, nBlockPos, "rb"), SER_DISK, CLIENT_VERSION);
    if (!filein)
        return error("CBlock::ReadFromDisk() : OpenBlockFile failed");

    // Read block
    try {
        filein >> *this;
    }
    catch (std::exception &e) {
        return error("%s() : deserialize or I/O error", __PRETTY_FUNCTION__);
    }

    // Check the header
    if (fReadTransactions && !CheckProofOfWork(GetHash(), nBits))
    {
        printf("failure on block with hash %s \n", this->GetHash().ToString().c_str());
        return error("CBlock::ReadFromDisk() : errors in block header");
    }

    return true;
}

void CBlock::print() const
{
    printf("CBlock(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%d)\n",
        GetHash().ToString().substr(0,20).c_str(),
        nVersion,
        hashPrevBlock.ToString().substr(0,20).c_str(),
        hashMerkleRoot.ToString().substr(0,10).c_str(),
        nTime, nBits, nNonce,
        vtx.size());
    for (unsigned int i = 0; i < vtx.size(); i++)
    {
        printf("  ");
        vtx[i].print();
    }
    printf("  vMerkleTree: ");
    for (unsigned int i = 0; i < vMerkleTree.size(); i++)
        printf("%s ", vMerkleTree[i].ToString().substr(0,10).c_str());
    printf("\n");
}


bool CBlock::DisconnectBlock(CTxDB& txdb, CBlockIndex* pindex)
{
    // Disconnect in reverse order
    for (int i = vtx.size()-1; i >= 0; i--)
        if (!vtx[i].DisconnectInputs(txdb))
            return false;

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = 0;
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("DisconnectBlock() : WriteBlockIndex failed");
    }

    // ppcoin: clean up wallet after disconnecting coinstake
    BOOST_FOREACH(CTransaction& tx, vtx)
        SyncWithWallets(tx, this, false);

    return true;
}

bool CBlock::ConnectBlock(CTxDB& txdb, CBlockIndex* pindex)
{
    // Check it again in case a previous version let a bad block in
    if (!CheckBlock())
        return false;

    bool fStrictPayToScriptHash = true;
    unsigned int nTxPos = pindex->nBlockPos + ::GetSerializeSize(*this, SER_DISK, CLIENT_VERSION) - ::GetSerializeSize(vtx, SER_DISK, CLIENT_VERSION)
            + GetSizeOfCompactSize(vtx.size());

    std::map<uint256, CTxIndex> mapQueuedChanges;
    int64_t nFees = 0;
    int64_t nValueIn = 0;
    int64_t nValueOut = 0;
    unsigned int nSigOps = 0;
    BOOST_FOREACH(CTransaction& tx, vtx)
    {
        uint256 hashTx = tx.GetHash();
        CTxIndex txindexOld;
        if (txdb.ReadTxIndex(hashTx, txindexOld))
        {
            BOOST_FOREACH(CDiskTxPos &pos, txindexOld.vSpent)
            {
                if (pos.IsNull())
                {
                    return false;
                }
            }
        }

        nSigOps += tx.GetLegacySigOpCount();
        if (nSigOps > MAX_BLOCK_SIGOPS)
            return DoS(100, error("ConnectBlock() : too many sigops"));

        CDiskTxPos posThisTx(pindex->nFile, pindex->nBlockPos, nTxPos);
        nTxPos += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);

        MapPrevTx mapInputs;
        if (tx.IsCoinBase())
            nValueOut += tx.GetValueOut();
        else
        {
            bool fInvalid;
            if (!tx.FetchInputs(txdb, mapQueuedChanges, true, false, mapInputs, fInvalid))
                return false;

                // Add in sigops done by pay-to-script-hash inputs;
                // this is to prevent a "rogue miner" from creating
                // an incredibly-expensive-to-validate block.
                nSigOps += tx.GetP2SHSigOpCount(mapInputs);
                if (nSigOps > MAX_BLOCK_SIGOPS)
                    return DoS(100, error("ConnectBlock() : too many sigops"));

            int64_t nTxValueIn = tx.GetValueIn(mapInputs);
            int64_t nTxValueOut = tx.GetValueOut();
            nValueIn += nTxValueIn;
            nValueOut += nTxValueOut;

            if (!tx.ConnectInputs(txdb, mapInputs, mapQueuedChanges, posThisTx, pindex, true, false, fStrictPayToScriptHash))
                return false;
        }

        mapQueuedChanges[hashTx] = CTxIndex(posThisTx, tx.vout.size());
    }
    // Check coinbase reward
    if (vtx[0].GetValueOut() > GetProofOfWorkReward())
    {
        return DoS(50, error("CheckBlock() : coinbase reward exceeded"));
    }
    // track money supply and mint amount info
    pindex->nMint = nValueOut - nValueIn + nFees;
    pindex->nMoneySupply = (pindex->pprev? pindex->pprev->nMoneySupply : 0) + nValueOut - nValueIn;
    if (!txdb.WriteBlockIndex(CDiskBlockIndex(pindex)))
        return error("Connect() : WriteBlockIndex for pindex failed");

    // Write queued txindex changes
    for (std::map<uint256, CTxIndex>::iterator mi = mapQueuedChanges.begin(); mi != mapQueuedChanges.end(); ++mi)
    {
        if (!txdb.UpdateTxIndex((*mi).first, (*mi).second))
            return error("ConnectBlock() : UpdateTxIndex failed");
    }

    // Update block index on disk without changing it in memory.
    // The memory index structure will be changed after the db commits.
    if (pindex->pprev)
    {
        CDiskBlockIndex blockindexPrev(pindex->pprev);
        blockindexPrev.hashNext = pindex->GetBlockHash();
        if (!txdb.WriteBlockIndex(blockindexPrev))
            return error("ConnectBlock() : WriteBlockIndex for blockindexPrev failed");
    }
    // Watch for transactions paying to me
    BOOST_FOREACH(CTransaction& tx, vtx)
        SyncWithWallets(tx, this, true);
    return true;
}

bool CBlock::ReadFromDisk(const CBlockIndex* pindex, bool fReadTransactions)
{
    if (!fReadTransactions)
    {
        *this = pindex->GetBlockHeader();
        return true;
    }
    if (!ReadFromDisk(pindex->nFile, pindex->nBlockPos, fReadTransactions))
        return false;
    if (GetHash() != pindex->GetBlockHash())
        return error("CBlock::ReadFromDisk() : GetHash() doesn't match index");
    return true;
}



bool CBlock::SetBestChain(CTxDB& txdb, CBlockIndex* pindexNew)
{
    uint256 hash = GetHash();
    if (!txdb.TxnBegin())
        return error("SetBestChain() : TxnBegin failed");

    if (pindexGenesisBlock == NULL && hash == hashGenesisBlock)
    {
        txdb.WriteHashBestChain(hash);
        if (!txdb.TxnCommit())
            return error("SetBestChain() : TxnCommit failed");
        pindexGenesisBlock = pindexNew;
    }
    else if (hashPrevBlock == hashBestChain)
    {
        if (!SetBestChainInner(txdb, pindexNew))
            return error("SetBestChain() : SetBestChainInner failed");
    }
    else if (hashPrevBlock != hashBestChain)
    {
        return error("SetBestChain(): This block is invalid, it does not have correct previous block hash and was probably not generated by a valid server node \n");
    }


    // Update best block in wallet (so we can detect restored wallets)
    bool fIsInitialDownload = IsInitialBlockDownload();
    // New best block
    hashBestChain = hash;
    pindexBest = pindexNew;
    nBestHeight = pindexBest->nHeight;
    bnBestChainTrust = pindexNew->bnChainTrust;
    nTimeBestReceived = GetTime();
    nTransactionsUpdated++;
    printf("SetBestChain: new best=%s  height=%d  trust=%s  moneysupply=%s\n", hashBestChain.ToString().substr(0,20).c_str(), nBestHeight, bnBestChainTrust.ToString().c_str(), FormatMoney(pindexBest->nMoneySupply).c_str());

    std::string strCmd = GetArg("-blocknotify", "");

    if (!fIsInitialDownload && !strCmd.empty())
    {
        boost::replace_all(strCmd, "%s", hashBestChain.GetHex());
        boost::thread t(runCommand, strCmd); // thread runs free
    }

    return true;
}

bool CBlock::AddToBlockIndex(unsigned int nFile, unsigned int nBlockPos)
{
    // Check for duplicate
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AddToBlockIndex() : %s already exists", hash.ToString().substr(0,20).c_str());

    // Construct new block index object
    CBlockIndex* pindexNew = new CBlockIndex(nFile, nBlockPos, this->nVersion, this->hashMerkleRoot, this->nTime, this->nBits, this->nNonce);
    if (!pindexNew)
        return error("AddToBlockIndex() : new CBlockIndex failed");

    pindexNew->phashBlock = &hash;
    std::map<uint256, CBlockIndex*>::iterator miPrev = mapBlockIndex.find(hashPrevBlock);
    if (miPrev != mapBlockIndex.end())
    {
        pindexNew->pprev = (*miPrev).second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
    }

    // ppcoin: compute chain trust score
    pindexNew->bnChainTrust = (pindexNew->pprev ? pindexNew->pprev->bnChainTrust : 0) + pindexNew->GetBlockTrust();

    // Add to mapBlockIndex
    std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.insert(make_pair(hash, pindexNew)).first;
    pindexNew->phashBlock = &((*mi).first);

    // Write to disk block index
    CTxDB txdb;
    if (!txdb.TxnBegin())
        return false;

    txdb.WriteBlockIndex(CDiskBlockIndex(pindexNew));

    if (!txdb.TxnCommit())
        return false;

    // New best
    if (pindexNew->bnChainTrust > bnBestChainTrust)
    {
        if (!SetBestChain(txdb, pindexNew))
        {
            return false;
        }
    }

    txdb.Close();
    if (pindexNew == pindexBest)
    {
        // Notify UI to display prev block's coinbase if it was ours
        static uint256 hashPrevBestCoinBase;
        UpdatedTransaction(hashPrevBestCoinBase);
        hashPrevBestCoinBase = vtx[0].GetHash();
    }
    MainFrameRepaint();
    return true;
}


bool CBlock::CheckWork() const
{
    if(this->GetHash() == hashGenesisBlock)
        return true;
    if(this->hashPrevBlock == hashGenesisBlock)
        return true;
    if(pindexBest->nHeight < 3)
        return true;

    // Get prev block index
    std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashPrevBlock);
    if (mi == mapBlockIndex.end())
        return false;
    CBlockIndex* pindexPrev = (*mi).second;
    unsigned int nextTarget = 0;
    int64_t nActualSpacing = pindexPrev->pprev->GetBlockTime() - pindexPrev->pprev->pprev->GetBlockTime();
    CBigNum bnNew;
    bnNew.SetCompact(pindexPrev->pprev->nBits);
    int64_t nTargetSpacing = MIN_BLOCK_SPACING / 4;
    int64_t nInterval = 60*60*24;
    bnNew *= ((nInterval - 1) * nTargetSpacing + nActualSpacing + nActualSpacing);
    bnNew /= ((nInterval + 1) * nTargetSpacing);
    if (bnNew > bnProofOfWorkLimit)
        bnNew = bnProofOfWorkLimit;
    nextTarget = bnNew.GetCompact();

    // Check proof-of-work
    if (nBits != nextTarget)
    {
        printf("nBits %u != nextTarget %u \n", nBits, nextTarget);
        return DoS(100, error("AcceptBlock() : incorrect proof-of-work"));
    }
    return true;
}

bool CBlock::CheckBlock() const
{
    // These are checks that are independent of context
    // that can be verified before saving an orphan block.

    // Size limits
    if (vtx.empty())
    {
        return DoS(100, error("CheckBlock() : vtx is empty"));
    }
    if(vtx.size() > MAX_BLOCK_SIZE)
    {
        return DoS(100, error("CheckBlock() : vtx over max_block_size"));
    }
    if(::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
    {
        return DoS(100, error("CheckBlock() : size limits failed"));
    }

    // Check proof of work matches claimed amount
    if (!CheckProofOfWork(GetHash(), nBits))
        return DoS(50, error("CheckBlock() : proof of work failed"));

    // Check timestamp
    if (GetBlockTime() > GetAdjustedTime() + nMaxClockDrift)
        return error("CheckBlock() : block timestamp too far in the future");

    // First transaction must be coinbase, the rest must not be
    if (vtx.empty() || !vtx[0].IsCoinBase())
        return DoS(100, error("CheckBlock() : first tx is not coinbase"));

    for (unsigned int i = 1; i < vtx.size(); i++)
    {
        if (vtx[i].IsCoinBase())
        {
            return DoS(100, error("CheckBlock() : more than one coinbase"));
        }
    }

    // Check coinbase timestamp
    int64_t blockTime = GetBlockTime();
    int64_t coinbaseTime = (int64_t)vtx[0].nTime + nMaxClockDrift;
    if(blockTime > coinbaseTime)
    {
        printf("blockTime = %" PRI64d " > coinbase timestamp = %" PRI64d " and it should not be \n\n", blockTime, coinbaseTime);
        return DoS(50, error("CheckBlock() : coinbase timestamp is too early"));
    }

    // Check transactions
    BOOST_FOREACH(const CTransaction& tx, vtx)
    {
        if (!tx.CheckTransaction())
            return DoS(tx.nDoS, error("CheckBlock() : CheckTransaction failed"));
    }

    // Check for duplicate txids. This is caught by ConnectInputs(),
    // but catching it earlier avoids a potential DoS attack:
    std::set<uint256> uniqueTx;
    BOOST_FOREACH(const CTransaction& tx, vtx)
    {
        uniqueTx.insert(tx.GetHash());
    }
    if (uniqueTx.size() != vtx.size())
        return DoS(100, error("CheckBlock() : duplicate transaction"));

    unsigned int nSigOps = 0;
    BOOST_FOREACH(const CTransaction& tx, vtx)
    {
        nSigOps += tx.GetLegacySigOpCount();
    }

    if (nSigOps > MAX_BLOCK_SIGOPS)
        return DoS(100, error("CheckBlock() : out-of-bounds SigOpCount"));

    // Check merkleroot
    if (hashMerkleRoot != BuildMerkleTree())
        return DoS(100, error("CheckBlock() : hashMerkleRoot mismatch"));

    return true;
}

bool CBlock::AcceptBlock()
{
    // Check for duplicate
    uint256 hash = GetHash();
    if (mapBlockIndex.count(hash))
        return error("AcceptBlock() : block already in mapBlockIndex");

    // Get prev block index
    std::map<uint256, CBlockIndex*>::iterator mi = mapBlockIndex.find(hashPrevBlock);
    if (mi == mapBlockIndex.end())
        return DoS(10, error("AcceptBlock() : prev block not found"));
    CBlockIndex* pindexPrev = (*mi).second;
    int nHeight = pindexPrev->nHeight+1;

    // Check timestamp against prev
    if (GetBlockTime() <= pindexPrev->GetBlockTime())
        return error("AcceptBlock() : block's timestamp is before the previous blocks");

    if( GetBlockTime() < pindexPrev->GetBlockTime() + MIN_BLOCK_SPACING)
        return error("AcceptBlock() : block's timestamp is too early and isnt following 30 second min block time rules");

    // Check that all transactions are finalized
    BOOST_FOREACH(const CTransaction& tx, vtx)
    {
        if (!tx.IsFinal(nHeight, GetBlockTime()))
        {
            return DoS(10, error("AcceptBlock() : contains a non-final transaction"));
        }
    }
    // Write block to history file
    if (!CheckDiskSpace(::GetSerializeSize(*this, SER_DISK, CLIENT_VERSION)))
        return error("AcceptBlock() : out of disk space");
    unsigned int nFile = -1;
    unsigned int nBlockPos = 0;
    if (!WriteToDisk(nFile, nBlockPos))
        return error("AcceptBlock() : WriteToDisk failed");
    if (!AddToBlockIndex(nFile, nBlockPos))
        return error("AcceptBlock() : AddToBlockIndex failed");
    // Relay inventory, but don't relay old inventory during initial block download
    int nBlockEstimate = GetTotalBlocksEstimate();
    if (hashBestChain == hash)
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode* pnode, vNodes)
        {
            if (nBestHeight > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate))
            {
                    pnode->PushInventory(CInv(MSG_BLOCK, hash));
            }
        }
    }
    return true;
}


// Called from inside SetBestChain: attaches a block to the new best chain being built
bool CBlock::SetBestChainInner(CTxDB& txdb, CBlockIndex *pindexNew)
{
    uint256 hash = GetHash();

    // Adding to current best branch
    if (!ConnectBlock(txdb, pindexNew) || !txdb.WriteHashBestChain(hash))
    {
        txdb.TxnAbort();
        InvalidChainFound(pindexNew);
        return false;
    }

    if (!txdb.TxnCommit())
        return error("SetBestChain() : TxnCommit failed");

    // Add to current best branch
    pindexNew->pprev->pnext = pindexNew;
    // Delete redundant memory transactions
    BOOST_FOREACH(CTransaction& tx, vtx)
        mempool.remove(tx);
    return true;
}
