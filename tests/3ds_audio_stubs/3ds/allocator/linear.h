#pragma once
#include <stddef.h>
void* linearAlloc(size_t size);
void linearFree(void* pointer);
