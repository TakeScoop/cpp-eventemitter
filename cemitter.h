#pragma once
#ifndef _GLPK_EVENTEMITTER_CEMITER_H
#define _GLPK_EVENTEMITTER_CEMITER_H

#ifdef __cplusplus
extern "C" {
#endif
typedef void (*eventemitter_fn)(const char*, const char*);
typedef void (*eventemitter_fn_r)(const void* sender, const char*, const char*);
#ifdef __cplusplus
};
#endif

#endif
