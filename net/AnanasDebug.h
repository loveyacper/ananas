#ifndef BERT_ANANASDEBUG_H
#define BERT_ANANASDEBUG_H

#include <mutex>
#include "log/Logger.h"

namespace ananas
{

namespace internal
{
    
extern std::shared_ptr<Logger> g_debug;
extern std::once_flag g_logInit;
void InitDebugLog(unsigned int level);

} // end namespace internal

} // end namespace ananas

#endif

