#include <ntddk.h>
#include <locale.h>
#include <__msvc_xlocinfo_types.hpp>
#include <xlocinfo>

extern "C" size_t __cdecl strcspn(
    const char* input,
    const char* control) {
    if (input == nullptr || control == nullptr) {
        return 0;
    }

    const char* i = input;
    for (; *i != '\0'; ++i) {
        const char* c = control;
        while (*c != '\0') {
            if (*c == *i) {
                return static_cast<size_t>(i - input);
            }
            ++c;
        }
    }
    return static_cast<size_t>(i - input);
}

namespace std {

class _Facet_base;

void __cdecl _Facet_Register(_Facet_base*) {}

void __cdecl _Facet_Unregister(_Facet_base*) {}

void __CLRCALL_PURE_OR_CDECL _Locinfo::_Locinfo_ctor(
    _Locinfo* info,
    const char* name) {
    if (info == nullptr) {
        return;
    }

    const char* resolved = (name != nullptr && name[0] != '\0') ? name : "C";
    info->_Days = nullptr;
    info->_Months = nullptr;
    info->_W_Days = nullptr;
    info->_W_Months = nullptr;
    info->_Oldlocname = resolved;
    info->_Newlocname = resolved;
}

void __CLRCALL_PURE_OR_CDECL _Locinfo::_Locinfo_ctor(
    _Locinfo* info,
    int,
    const char* name) {
    _Locinfo_ctor(info, name);
}

void __CLRCALL_PURE_OR_CDECL _Locinfo::_Locinfo_dtor(_Locinfo* info) {
    if (info == nullptr) {
        return;
    }

    info->_Days = nullptr;
    info->_Months = nullptr;
    info->_W_Days = nullptr;
    info->_W_Months = nullptr;
    info->_Newlocname = nullptr;
}

_Locinfo& __CLRCALL_PURE_OR_CDECL _Locinfo::_Locinfo_Addcats(
    _Locinfo* info,
    int,
    const char* name) {
    if (info == nullptr) {
        static _Locinfo fallback("C");
        return fallback;
    }

    if (name != nullptr && name[0] != '\0') {
        info->_Newlocname = name;
    }
    return *info;
}

}  // namespace std
extern "C" struct lconv* __cdecl localeconv(void) {
    static lconv conv = {};
    return &conv;
}

extern "C" _Ctypevec __cdecl _Getctype(void) noexcept {
    static const short table_raw[257] = {};
    static _Ctypevec vec = {
        0u,
        table_raw + 1,
        0,
        nullptr};
    return vec;
}

extern "C" _Cvtvec __cdecl _Getcvt(void) noexcept {
    static _Cvtvec vec = {};
    return vec;
}

extern "C" int __cdecl _Tolower(int ch, const _Ctypevec*) noexcept {
    if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A' + 'a';
    }
    return ch;
}

extern "C" int __cdecl _Toupper(int ch, const _Ctypevec*) noexcept {
    if (ch >= 'a' && ch <= 'z') {
        return ch - 'a' + 'A';
    }
    return ch;
}
