#ifndef ORPHAN_H
#define ORPHAN_H

#include "base/block.h"

bool AddOrphanTx(const CDataStream& vMsg);
void EraseOrphanTx(uint256 hash);
unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans);


#endif // ORPHAN_H
