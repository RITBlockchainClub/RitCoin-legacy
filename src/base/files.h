#ifndef FILES_H
#define FILES_H

#ifdef WIN32
#include <io.h> /* for _commit */
#endif

#include "util.h"

bool CheckDiskSpace(uint64_t nAdditionalBytes=0);
FILE* OpenBlockFile(unsigned int nFile, unsigned int nBlockPos, const char* pszMode="rb");
FILE* AppendBlockFile(unsigned int& nFileRet);
FILE* UpdateFutureBlockFile(unsigned int& nFileRet, bool newWrite);


#endif // FILES_H
