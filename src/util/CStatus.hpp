#ifndef UTIL_C_STATUS_HPP
#define UTIL_C_STATUS_HPP

#include <storm/List.hpp>
#include <storm/Log.hpp>

enum STATUS_TYPE {
    STATUS_INFO = 0x0,
    STATUS_WARNING = 0x1,
    STATUS_ERROR = 0x2,
    STATUS_FATAL = 0x3,
    STATUS_NUMTYPES = 0x4,
};

class CStatus {
    public:
        struct STATUSENTRY {
            char* text;
            STATUS_TYPE severity;
            TSLink<CStatus::STATUSENTRY> link;
        };

    public:
        // Static variables
        static CStatus s_errorList;

        // Member functions
        void Add(const CStatus&);
        void Add(STATUS_TYPE, const char*, ...);

    public:
        STORM_EXPLICIT_LIST(CStatus::STATUSENTRY, link) statusList;
};

class CWOWClientStatus : public CStatus {
    public:
        HSLOG m_logFile = nullptr;
};

CStatus& GetGlobalStatusObj(void);

#endif
