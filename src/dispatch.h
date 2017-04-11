#ifndef DISPATCH_H
#define DISPATCH_H

#include "wallet.h"

void RegisterWallet(CWallet* pwalletIn);

void UnregisterWallet(CWallet* pwalletIn);
// check whether the passed transaction is from us
bool IsFromMe(CTransaction& tx);
// get the wallet transaction with the given hash (if it exists)
bool GetTransaction(const uint256& hashTx, CWalletTx& wtx);
// erases transaction with the given hash from all wallets
void EraseFromWallets(uint256 hash);
// make sure all wallets know about the given transaction, in the given block
void SyncWithWallets(const CTransaction& tx, const CBlock* pblock, bool fUpdate);
// notify wallets about a new best chain
void UpdatedTransaction(const uint256& hashTx);
// dump all wallets
void PrintWallets(const CBlock& block);
// notify wallets about an incoming inventory (for request counts)
void Inventory(const uint256& hash);
// ask wallets to resend their transactions
void ResendWalletTransactions();

bool IsInitialBlockDownload();
#endif // DISPATCH_H
