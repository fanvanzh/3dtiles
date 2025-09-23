#pragma once

#ifdef _WIN32

  // 在包含 windows.h 之前定义这些，让 windows.h 更精简并避免 min/max 宏污染
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif

  #ifndef NOMINMAX
    #define NOMINMAX
  #endif

  #include <windows.h>

  #ifndef WINGDIAPI
    #define WINGDIAPI __declspec(dllimport)
  #endif

#endif