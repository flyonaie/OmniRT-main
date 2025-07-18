#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// LLM handle interface
typedef struct {
  // Text generation function
  const char* (*generate)(void* impl, const char* prompt, const char* params);
  
  // Text embedding function
  float* (*embedding)(void* impl, const char* text, size_t* dim);

  // Implementation pointer
  void* impl;  
}aimrt_llm_handle_base_t;

#ifdef __cplusplus
}
#endif
