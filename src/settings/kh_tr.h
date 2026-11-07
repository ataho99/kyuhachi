#pragma once
/* kh_tr.h -- t672 (#656): translation hook for the settings layer. settings/ must not include text/ (T11 deps), so messages built here call khtr::tr();
 * app/main_setup.h points khtr::fn() at phr_ at start-up. Without the hook (test tools) tr() returns the Korean key unchanged. */
#include <stdarg.h>
#include <stdio.h>
#include <string>
namespace khtr {
typedef const char * (*Fn)(const char *);
inline const char * ident(const char * s) { return s; }
inline Fn & fn() { static Fn f = ident; return f; }
inline const char * tr(const char * s) { return fn()(s); }
inline std::string fmt(const char * f, ...) {   /* printf into std::string (two-pass) */
    va_list ap; va_start(ap, f); va_list cp; va_copy(cp, ap); int n = vsnprintf(NULL, 0, f, cp); va_end(cp);
    std::string o; if (n > 0) { o.resize((size_t)n + 1); vsnprintf(&o[0], (size_t)n + 1, f, ap); o.resize((size_t)n); } va_end(ap); return o;
}
}
