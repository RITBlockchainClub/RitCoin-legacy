#ifndef MESSAGES_H
#define MESSAGES_H

#include "db.h"
#include "net.h"
#include "txdb-leveldb.h"

bool AlreadyHave(CTxDB& txdb, const CInv& inv);
bool ProcessMessage(CNode* pfrom, std::string strCommand, CDataStream& vRecv);
bool ProcessMessages(CNode* pfrom);
bool SendMessages(CNode* pto, bool fSendTrickle);

bool ProcessBlock(CNode* pfrom, CBlock* pblock);

#endif // MESSAGES_H
