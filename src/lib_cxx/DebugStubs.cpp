#ifdef NDEBUG
#include <stdlib.h>
extern "C" {
    int _CrtDbgReport(int, const char*, int, const char*, const char*, ...) { return 0; }
    int _CrtDbgReportW(int, const wchar_t*, int, const wchar_t*, const wchar_t*, ...) { return 0; }
    void* _calloc_dbg(size_t num, size_t size, int, const char*, int) { return calloc(num, size); }

    void* __imp__CrtDbgReport = (void*)_CrtDbgReport;
    void* __imp__CrtDbgReportW = (void*)_CrtDbgReportW;
    void* __imp__calloc_dbg = (void*)_calloc_dbg;
}
#endif
