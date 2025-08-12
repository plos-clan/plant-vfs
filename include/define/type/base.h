// This code is released under the MIT License

#pragma once

#pragma GCC system_header

#include "00-include.h"

// gcc 扩展，让 C 中的 auto 具有和 C++ 类似的语义
// 神经你 C23 加这鬼玩意干嘛，没活可以咬个打火机
#if !defined(__cplusplus) && __STDC_VERSION__ < 202311L
#  define auto __auto_type
#endif

#define var auto
#define val const auto

#ifdef __cplusplus
static val null = nullptr;
#else
#  define null ((void *)0)
#endif

#define NULL 0

typedef __INT8_TYPE__  sbyte;
typedef __UINT8_TYPE__ byte;

typedef int errno_t;

// APPLE你在搞什么鬼？
#ifdef __APPLE__
typedef long time_t;
#else
typedef __INT64_TYPE__ time_t;
#endif
#ifdef __cplusplus
#  define lit constexpr
#endif
