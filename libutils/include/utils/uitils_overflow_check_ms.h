#pragma once
#include <cstdint>
#include <intrin.h>

#ifdef _MSC_VER
#include <intrin.h>

inline bool CheckAddOverflow( uint32_t a, uint32_t b, uint32_t& result )
{
    return _addcarry_u32( 0, a, b, &result );
}

inline bool CheckAddOverflow(int32_t a, int32_t b, int32_t& result)
{
    result = a + b;
    bool r = (result < a) || (result < b);
    return r;
}

inline bool CheckAddOverflow(int8_t a, int8_t b, int8_t& result)
{
    result = a + b;
    bool r = (result < a) || (result < b);
    return r;
}

inline bool CheckAddOverflow( uint64_t a, uint64_t b, uint64_t& result )
{
    return _addcarry_u64( 0, a, b, &result );
}

inline bool CheckAddOverflow(int64_t a, int64_t b, int64_t& result)
{
    result = a + b;
    bool r = (result < a) || (result < b);
    return r;
}

inline bool CheckAddOverflow(bool a, bool b, bool& result)
{
    result = a + b;
    return false;
}

inline bool CheckSubOverflow(int8_t a, int8_t b, int8_t& result )
{
    result = a - b;
    bool r = false;
    r = result > a;
    return r;
}

inline bool CheckSubOverflow(bool a, bool b, bool& result)
{
    result = a - b;
    bool r = false;
    r = result > a;
    return r;
}

inline bool CheckSubOverflow(uint32_t a, uint32_t b, uint32_t& result)
{
    return _subborrow_u32(0, a, b, &result);
}

inline bool CheckSubOverflow( uint16_t a, uint16_t b, uint16_t& result )
{
    return _subborrow_u16( 0, a, b, &result );
}

inline bool CheckSubOverflow( uint64_t a, uint64_t b, uint64_t& result )
{
    return _subborrow_u64( 0, a, b, &result );
}

inline bool CheckSubOverflow(int64_t a, int64_t b, int64_t& result)
{
    uint64_t c_result;
    bool r = _subborrow_u64(0, a, b, &c_result);
    result = c_result;
    return r;
}

inline bool CheckSubOverflow(int32_t a, int32_t b, int32_t& result)
{
    uint32_t c_result;
    bool r = _subborrow_u32(0, a, b, &c_result);
    result = c_result;
    return r;
}

template <typename _Tp>
inline  bool
__mul_overflowed( _Tp __a, _Tp __b, _Tp& __r )
{
    bool __did = __b && ( ( std::numeric_limits<_Tp>::max )( ) / __b ) < __a;
    __r = __a * __b;
    return __did;
}

inline unsigned short __lzcnt_switcher(
    unsigned short value
)
{
    return __lzcnt16(value);
}

inline unsigned int __lzcnt_switcher(
    unsigned int value
)
{
    return __lzcnt(value);
}

inline unsigned __int64 __lzcnt_switcher(
    unsigned __int64 value
)
{
    return __lzcnt64(value);
}

inline unsigned __int64 __lzcnt_switcher(
    int64_t value
)
{
    return __lzcnt64(static_cast<unsigned __int64>(value));
}

inline unsigned __int64 __lzcnt_switcher(
    int32_t value
)
{
    return __lzcnt(static_cast<unsigned int>(value));
}

#define __builtin_add_overflow(a, b, sum) CheckAddOverflow( (a), (b), (*sum) )
#define __builtin_sub_overflow(a, b, sum) CheckSubOverflow( (a), (b), (*sum) )
#define __builtin_mul_overflow( a, b, sum ) __mul_overflowed( (a), (b), (*sum) )
#define __builtin_clz(a) __lzcnt_switcher(a)
#define __builtin_clzl(a) __lzcnt_switcher(a)

#endif

