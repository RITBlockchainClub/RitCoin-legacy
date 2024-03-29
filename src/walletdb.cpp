// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2012 The PPCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BOOST_NO_CXX11_SCOPED_ENUMS
#include <boost/filesystem.hpp>
#undef  BOOST_NO_CXX11_SCOPED_ENUMS

#include "walletdb.h"
#include "wallet.h"
#include "db.h"
#include "txdb-leveldb.h"

using namespace std;
using namespace boost;


static uint64_t nAccountingEntryNumber = 0;

extern CCriticalSection cs_db;
extern map<string, int> mapFileUseCount;
extern void CloseDb(const string& strFile);

//
// CWalletDB
//

bool CWalletDB::WriteName(const string& strAddress, const string& strName)
{
    nWalletDBUpdated++;
    return Write(make_pair(string("name"), strAddress), strName);
}

bool CWalletDB::EraseName(const string& strAddress)
{
    // This should only be used for sending addresses, never for receiving addresses,
    // receiving addresses must always have an address book entry if they're not change return.
    nWalletDBUpdated++;
    return Erase(make_pair(string("name"), strAddress));
}

bool CWalletDB::ReadAccount(const string& strAccount, CAccount& account)
{
    account.SetNull();
    return Read(make_pair(string("acc"), strAccount), account);
}

bool CWalletDB::WriteAccount(const string& strAccount, const CAccount& account)
{
    return Write(make_pair(string("acc"), strAccount), account);
}

bool CWalletDB::WriteAccountingEntry(const CAccountingEntry& acentry)
{
    return Write(boost::make_tuple(string("acentry"), acentry.strAccount, ++nAccountingEntryNumber), acentry);
}

int64_t CWalletDB::GetAccountCreditDebit(const string& strAccount)
{
    list<CAccountingEntry> entries;
    ListAccountCreditDebit(strAccount, entries);

    int64_t nCreditDebit = 0;
    BOOST_FOREACH (const CAccountingEntry& entry, entries)
        nCreditDebit += entry.nCreditDebit;

    return nCreditDebit;
}

void CWalletDB::ListAccountCreditDebit(const string& strAccount, list<CAccountingEntry>& entries)
{
    bool fAllAccounts = (strAccount == "*");

    Dbc* pcursor = GetCursor();
    if (!pcursor)
        throw runtime_error("CWalletDB::ListAccountCreditDebit() : cannot create DB cursor");
    unsigned int fFlags = DB_SET_RANGE;
    while(true)
    {
        // Read next record
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        if (fFlags == DB_SET_RANGE)
            ssKey << boost::make_tuple(string("acentry"), (fAllAccounts? string("") : strAccount), uint64_t(0));
        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        int ret = ReadAtCursor(pcursor, ssKey, ssValue, fFlags);
        fFlags = DB_NEXT;
        if (ret == DB_NOTFOUND)
            break;
        else if (ret != 0)
        {
            pcursor->close();
            throw runtime_error("CWalletDB::ListAccountCreditDebit() : error scanning DB");
        }

        // Unserialize
        string strType;
        ssKey >> strType;
        if (strType != "acentry")
            break;
        CAccountingEntry acentry;
        ssKey >> acentry.strAccount;
        if (!fAllAccounts && acentry.strAccount != strAccount)
            break;

        ssValue >> acentry;
        entries.push_back(acentry);
    }

    pcursor->close();
}


int CWalletDB::LoadWallet(CWallet* pwallet)
{
    pwallet->vchDefaultKey = CPubKey();
    int nFileVersion = 0;
    vector<uint256> vWalletUpgrade;
    bool fIsEncrypted = false;

    //// todo: shouldn't we catch exceptions and try to recover and continue?
    {
        LOCK(pwallet->cs_wallet);
        int nMinVersion = 0;
        if (Read((string)"minversion", nMinVersion))
        {
            if (nMinVersion > CLIENT_VERSION)
                return DB_TOO_NEW;
            pwallet->LoadMinVersion(nMinVersion);
        }

        // Get cursor
        Dbc* pcursor = GetCursor();
        if (!pcursor)
        {
            printf("Error getting wallet database cursor\n");
            return DB_CORRUPT;
        }

        while(true)
        {
            // Read next record
            CDataStream ssKey(SER_DISK, CLIENT_VERSION);
            CDataStream ssValue(SER_DISK, CLIENT_VERSION);
            int ret = ReadAtCursor(pcursor, ssKey, ssValue);
            if (ret == DB_NOTFOUND)
                break;
            else if (ret != 0)
            {
                printf("Error reading next record from wallet database\n");
                return DB_CORRUPT;
            }

            // Unserialize
            // Taking advantage of the fact that pair serialization
            // is just the two items serialized one after the other
            string strType;
            ssKey >> strType;
            if (strType == "name")
            {
                string strAddress;
                ssKey >> strAddress;
                ssValue >> pwallet->mapAddressBook[CBitcoinAddress(strAddress).Get()];
            }
            else if (strType == "tx")
            {
                uint256 hash;
                ssKey >> hash;
                CWalletTx& wtx = pwallet->mapWallet[hash];
                ssValue >> wtx;
                wtx.BindWallet(pwallet);

                if (wtx.GetHash() != hash)
                    printf("Error in wallet.dat, hash mismatch\n");

                // Undo serialize changes in 31600
                if (31404 <= wtx.fTimeReceivedIsTxTime && wtx.fTimeReceivedIsTxTime <= 31703)
                {
                    if (!ssValue.empty())
                    {
                        char fTmp;
                        char fUnused;
                        ssValue >> fTmp >> fUnused >> wtx.strFromAccount;
                        printf("LoadWallet() upgrading tx ver=%d %d '%s' %s\n", wtx.fTimeReceivedIsTxTime, fTmp, wtx.strFromAccount.c_str(), hash.ToString().c_str());
                        wtx.fTimeReceivedIsTxTime = fTmp;
                    }
                    else
                    {
                        printf("LoadWallet() repairing tx ver=%d %s\n", wtx.fTimeReceivedIsTxTime, hash.ToString().c_str());
                        wtx.fTimeReceivedIsTxTime = 0;
                    }
                    vWalletUpgrade.push_back(hash);
                }

                //// debug print
                //printf("LoadWallet  %s\n", wtx.GetHash().ToString().c_str());
                //printf(" %12"PRI64d"  %s  %s  %s\n",
                //    wtx.vout[0].nValue,
                //    DateTimeStrFormat(wtx.GetBlockTime()).c_str(),
                //    wtx.hashBlock.ToString().substr(0,20).c_str(),
                //    wtx.mapValue["message"].c_str());
            }
            else if (strType == "acentry")
            {
                string strAccount;
                ssKey >> strAccount;
                uint64_t nNumber;
                ssKey >> nNumber;
                if (nNumber > nAccountingEntryNumber)
                    nAccountingEntryNumber = nNumber;
            }
            else if (strType == "key" || strType == "wkey")
            {
                vector<unsigned char> vchPubKey;
                ssKey >> vchPubKey;
                CKey key;
                if (strType == "key")
                {
                    CPrivKey pkey;
                    ssValue >> pkey;
                    key.SetPubKey(vchPubKey);
                    key.SetPrivKey(pkey);
                    if (key.GetPubKey() != vchPubKey)
                    {
                        printf("Error reading wallet database: CPrivKey pubkey inconsistency\n");
                        return DB_CORRUPT;
                    }
                    if (!key.IsValid())
                    {
                        printf("Error reading wallet database: invalid CPrivKey\n");
                        return DB_CORRUPT;
                    }
                }
                else
                {
                    CWalletKey wkey;
                    ssValue >> wkey;
                    key.SetPubKey(vchPubKey);
                    key.SetPrivKey(wkey.vchPrivKey);
                    if (key.GetPubKey() != vchPubKey)
                    {
                        printf("Error reading wallet database: CWalletKey pubkey inconsistency\n");
                        return DB_CORRUPT;
                    }
                    if (!key.IsValid())
                    {
                        printf("Error reading wallet database: invalid CWalletKey\n");
                        return DB_CORRUPT;
                    }
                }
                if (!pwallet->LoadKey(key))
                {
                    printf("Error reading wallet database: LoadKey failed\n");
                    return DB_CORRUPT;
                }
            }
            else if (strType == "mkey")
            {
                unsigned int nID;
                ssKey >> nID;
                CMasterKey kMasterKey;
                ssValue >> kMasterKey;
                if(pwallet->mapMasterKeys.count(nID) != 0)
                {
                    printf("Error reading wallet database: duplicate CMasterKey id %u\n", nID);
                    return DB_CORRUPT;
                }
                pwallet->mapMasterKeys[nID] = kMasterKey;
                if (pwallet->nMasterKeyMaxID < nID)
                    pwallet->nMasterKeyMaxID = nID;
            }
            else if (strType == "ckey")
            {
                vector<unsigned char> vchPubKey;
                ssKey >> vchPubKey;
                vector<unsigned char> vchPrivKey;
                ssValue >> vchPrivKey;
                if (!pwallet->LoadCryptedKey(vchPubKey, vchPrivKey))
                {
                    printf("Error reading wallet database: LoadCryptedKey failed\n");
                    return DB_CORRUPT;
                }
                fIsEncrypted = true;
            }
            else if (strType == "defaultkey")
            {
                ssValue >> pwallet->vchDefaultKey;
            }
            else if (strType == "pool")
            {
                int64_t nIndex;
                ssKey >> nIndex;
                pwallet->setKeyPool.insert(nIndex);
            }
            else if (strType == "version")
            {
                ssValue >> nFileVersion;
                if (nFileVersion == 10300)
                    nFileVersion = 300;
            }
            else if (strType == "cscript")
            {
                uint160 hash;
                ssKey >> hash;
                CScript script;
                ssValue >> script;
                if (!pwallet->LoadCScript(script))
                {
                    printf("Error reading wallet database: LoadCScript failed\n");
                    return DB_CORRUPT;
                }
            }
        }
        pcursor->close();
    }

    BOOST_FOREACH(uint256 hash, vWalletUpgrade)
        WriteTx(hash, pwallet->mapWallet[hash]);

    printf("nFileVersion = %d\n", nFileVersion);


    // Rewrite encrypted wallets of versions 0.4.0 and 0.5.0rc:
    if (fIsEncrypted && (nFileVersion == 40000 || nFileVersion == 50000))
        return DB_NEED_REWRITE;

    if (nFileVersion < CLIENT_VERSION) // Update
        WriteVersion(CLIENT_VERSION);

    return DB_LOAD_OK;
}

void ThreadFlushWalletDB(void* parg)
{
    const string& strFile = ((const string*)parg)[0];
    static bool fOneThread;
    if (fOneThread)
        return;
    fOneThread = true;
    if (!GetBoolArg("-flushwallet", true))
        return;

    unsigned int nLastSeen = nWalletDBUpdated;
    unsigned int nLastFlushed = nWalletDBUpdated;
    int64_t nLastWalletUpdate = GetTime();
    while (!fShutdown)
    {
        Sleep(500);

        if (nLastSeen != nWalletDBUpdated)
        {
            nLastSeen = nWalletDBUpdated;
            nLastWalletUpdate = GetTime();
        }

        if (nLastFlushed != nWalletDBUpdated && GetTime() - nLastWalletUpdate >= 2)
        {
            TRY_LOCK(bitdb.cs_db,lockDb);
            if (lockDb)
            {
                // Don't do this if any databases are in use
                int nRefCount = 0;
                map<string, int>::iterator mi = bitdb.mapFileUseCount.begin();
                while (mi != bitdb.mapFileUseCount.end())
                {
                    nRefCount += (*mi).second;
                    mi++;
                }
                if (nRefCount == 0 && !fShutdown)
                {
                    map<string, int>::iterator mi = bitdb.mapFileUseCount.find(strFile);
                    if (mi != bitdb.mapFileUseCount.end())
                    {
                        printf("Flushing wallet.dat\n");
                        nLastFlushed = nWalletDBUpdated;
                        // Flush wallet.dat so it's self contained
                        bitdb.CloseDb(strFile);
                        bitdb.CheckpointLSN(strFile);
                        bitdb.mapFileUseCount.erase(mi++);
                    }
                }
            }
        }
    }
}
