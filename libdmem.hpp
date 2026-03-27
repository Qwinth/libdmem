#include "Arduino.h"
#include <stdint.h>
#include <cstdint>
#include <unistd.h>
#include <alloca.h>

struct memory_block {
  int16_t magic;

  int8_t unused_block;
  int32_t block_size;

  void* memory_block_top;

  memory_block* next;
  memory_block* prev;
};

#define __MBLOCK_SIZE (int16_t)sizeof(memory_block)

void* ram_start = nullptr;

memory_block* first_block = nullptr;
memory_block* top_block = nullptr;

int32_t __MALLOC_PREV_ALLOCATED;

void MemoryInit() {
  if (!ram_start) ram_start = sbrk(0);
}

memory_block* FindFreeBlock(int32_t block_size) {
  memory_block* curr_block = first_block;

  while (curr_block) {
    if (curr_block->unused_block && curr_block->block_size >= block_size) {
      return curr_block;
    }

    curr_block = curr_block->next;
  }

  return nullptr;
}

void* HeapTop() {
  if (top_block) return top_block->memory_block_top;

  return sbrk(0);
}

memory_block* AllocateNewBlock(int32_t block_size) {
  memory_block* block = (memory_block*)sbrk(block_size + __MBLOCK_SIZE);
  block->magic = 0xFD;
  block->block_size = ((char*)sbrk(0) - (char*)block) - __MBLOCK_SIZE;
  block->memory_block_top = (char*)(block + 1) + block->block_size;

  return block;
}

memory_block* SplitBlock(memory_block* curr_block, int32_t block_size) {
  if (!curr_block || block_size > curr_block->block_size) return nullptr;
  else if (block_size > curr_block->block_size - __MBLOCK_SIZE) return curr_block;

  memory_block curr_block_backup = *curr_block;

  memory_block* first_block = curr_block;
  memory_block* last_block = nullptr;

  first_block->block_size = block_size;
  first_block->memory_block_top = (char*)(curr_block + 1) + block_size;
  
  last_block = (memory_block*)first_block->memory_block_top;
  last_block->magic = 0xFD;
  last_block->block_size = ((char*)curr_block_backup.memory_block_top - (char*)first_block->memory_block_top) - __MBLOCK_SIZE;
  last_block->memory_block_top = curr_block_backup.memory_block_top;

  first_block->next = last_block;
  last_block->prev = first_block;

  if (curr_block_backup.next) curr_block_backup.next->prev = last_block;

  return first_block;
}

void* Malloc(int32_t m_size) {
  if (!ram_start) MemoryInit();

  memory_block* curr_block = FindFreeBlock(m_size);

  // Serial.print("FindFreeBlock: ");
  // Serial.println((intptr_t)curr_block);

  curr_block = SplitBlock(curr_block, m_size);

  // Serial.print("Using block: ");
  // Serial.println((intptr_t)curr_block);

  if (!curr_block) {
    if ((char*)HeapTop() + m_size >= alloca(0)) return nullptr;

    curr_block = AllocateNewBlock(m_size);
    curr_block->prev = top_block;
    curr_block->next = nullptr;

    // Serial.print("Allocate new block: ");
    // Serial.println(curr_block->block_size);
  
    if (!first_block) first_block = curr_block;

    top_block = curr_block;
  }

  curr_block->unused_block = false;

  if (curr_block->prev) curr_block->prev->next = curr_block;
  if (curr_block->next) curr_block->next->prev = curr_block;

  __MALLOC_PREV_ALLOCATED = curr_block->block_size;

  return (void*)(curr_block + 1);
}

void TrimBlock(memory_block* curr_block) {
  if (curr_block < top_block) return;

  sbrk(-(curr_block->block_size + __MBLOCK_SIZE));
}

void MallocTrimBack() {
  while (top_block && top_block->unused_block) {
    memory_block* curr_block = top_block;
    top_block = curr_block->prev;
    
    if (curr_block == first_block) first_block = nullptr;
    if (top_block) top_block->next = nullptr;

    // Serial.print("MallocTrimBack: ");
    // Serial.println((intptr_t)curr_block);

    TrimBlock(curr_block);
  }
}

void Free(void* ptr) {
  if (!ptr) return;

  memory_block* curr_block = (memory_block*)ptr - 1;
  curr_block->unused_block = true;

  // Serial.print("curr_block->magic: ");
  // Serial.println(curr_block->magic);

  if (curr_block == top_block) {
    if (curr_block != first_block) {
      top_block = curr_block->prev;
      top_block->next = nullptr;

      // Serial.print("Free top_block: ");
      // Serial.println(((char*)curr_block->memory_block_top - (char*)top_block->memory_block_top - __MBLOCK_SIZE));
    }

    else {
      // Serial.print("Free first_block: ");
      // Serial.println(curr_block->block_size);

      first_block = nullptr;
      top_block = nullptr;
    }

    TrimBlock(curr_block);
    MallocTrimBack();
  }

  // if (curr->prev) curr->prev->next = curr->next;
  // if (curr->next) curr->next->prev = curr->prev;
}

memory_block* GetMemoryBlockHeader(void* ptr) {
  if (!ptr) return nullptr;

  return (memory_block*)ptr - 1;
}

void MemoryReset() {
  memory_block* curr_block = top_block;

  while (curr_block) {
    curr_block->unused_block = true;
    curr_block = curr_block->prev;
  }

  MallocTrimBack();
}

void MemoryHardReset() {
  if (!ram_start) return;

  char* ptr = (char*)sbrk(0);

  if (ptr > (char*)ram_start) sbrk(-(ptr - (char*)ram_start));

  first_block = nullptr;
  top_block = nullptr;

  __MALLOC_PREV_ALLOCATED = 0;
}
