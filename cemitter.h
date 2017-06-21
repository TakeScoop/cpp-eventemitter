#pragma once
#ifndef _GLPK_EVENTEMITTER_CEMITER_H
#define _GLPK_EVENTEMITTER_CEMITER_H

extern "C" {
typedef void (*eventemitter_fn)(const char*, const char*);
typedef void (*eventemitter_fn_r)(void* sender, const char*, const char*);
};

#endif
