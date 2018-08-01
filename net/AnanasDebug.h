#ifndef BERT_ANANASDEBUG_H
#define BERT_ANANASDEBUG_H

#include <mutex>
#include "ananas/util/Logger.h"

namespace ananas {

namespace internal {

extern std::shared_ptr<Logger> g_debug;
extern std::once_flag g_logInit;
void InitDebugLog(unsigned int level);

} // end namespace internal

} // end namespace ananas

#define ANANAS_DBG DBG(ananas::internal::g_debug)
#define ANANAS_INF INF(ananas::internal::g_debug)
#define ANANAS_WRN WRN(ananas::internal::g_debug)
#define ANANAS_ERR ERR(ananas::internal::g_debug)
#define ANANAS_USR USR(ananas::internal::g_debug)

#endif

