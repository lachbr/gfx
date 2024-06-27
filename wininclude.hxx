#ifndef WININCLUDE_HXX
#define WININCLUDE_HXX

#ifdef _WIN32
#undef UNICODE
#undef _UNICODE
#define NOMINMAX
#include <Windows.h>
#endif // _WIN32

#endif // WININCLUDE_HXX
