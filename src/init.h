// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_INIT_H
#define BITCOIN_INIT_H

#include "wallet.h"

extern CWallet* pwalletMain;
extern RSA* rsaPubKey;


void StartShutdown();
void Shutdown(void* parg);
bool AppInit(int argc, char* argv[]);
bool AppInit2(int argc, char* argv[]);

#endif
