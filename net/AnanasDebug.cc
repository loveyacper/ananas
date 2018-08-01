#include "AnanasDebug.h"

namespace ananas {

namespace internal {

std::shared_ptr<Logger> g_debug;
std::once_flag g_logInit;

void InitDebugLog(unsigned int level) {
    std::call_once(g_logInit, [level]() {
        g_debug = LogManager::Instance().CreateLog(level, logFile, "ananas_debug_log");
    });
}

} // end namespace internal

} // end namespace ananas

