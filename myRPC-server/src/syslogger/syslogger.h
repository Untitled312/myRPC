#ifndef SYSLOGGER_H
#define SYSLOGGER_H
#endif

int syslogger(int level, char* file, char* text, ...);

enum {
    LOG_LVL_DEBUG,    // 0
    LOG_LVL_INFO,     // 1
    LOG_LVL_WARNING,  // 2
    LOG_LVL_ERROR,    // 3
    LOG_LVL_CRITICAL, // 4
};