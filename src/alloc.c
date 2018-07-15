#include "rcheevos.h"

void* rc_alloc(void* pointer, int* offset, int size, void* dummy) {
  void* ptr;

  *offset = (*offset + RC_ALIGNMENT - 1) & -RC_ALIGNMENT;

  if (pointer != 0) {
    ptr = (void*)((char*)pointer + *offset);
  }
  else {
    ptr = dummy;
  }

  *offset += size;
  return ptr;
}
