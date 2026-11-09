/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026-present Open Handheld Collective
 *   https://github.com/OpenHandheldCollective
 */

/**
  Rt - runtime support for the UEFI build, which has no C library.

  Provides the C functions the compiler calls on its own, and pool memory
  and CRC-32 from the boot services.
*/

#include <stddef.h>
#include <stdint.h>

#include "Base/Memory.h"
#include "Efi/Efi.h"

// The compiler calls the memory functions for struct copies and clears.
// __chkstk is the Windows ABI's stack probe for large frames. UEFI gives
// the whole stack at once, so it can just return. Objects that use floating
// point reference _fltused.

void *
memcpy (
  void       *Dst,
  const void *Src,
  size_t      N
)
{
  uint8_t       *D = Dst;
  const uint8_t *S = Src;
  if (((uintptr_t)D & 7U) == 0 && ((uintptr_t)S & 7U) == 0) {
    while (N >= 8U) {
      *(volatile uint64_t *)D = *(const volatile uint64_t *)S;
      D += 8;
      S += 8;
      N -= 8;
    }
  }
  while (N--) {
    *(volatile uint8_t *)D++ = *(const volatile uint8_t *)S++;
  }
  return Dst;
}

void *
memmove (
  void       *Dst,
  const void *Src,
  size_t      N
)
{
  uint8_t       *D = Dst;
  const uint8_t *S = Src;
  if (D < S) {
    if (((uintptr_t)D & 7U) == 0 && ((uintptr_t)S & 7U) == 0) {
      while (N >= 8U) {
        *(volatile uint64_t *)D = *(const volatile uint64_t *)S;
        D += 8;
        S += 8;
        N -= 8;
      }
    }
    while (N--) {
      *(volatile uint8_t *)D++ = *(const volatile uint8_t *)S++;
    }
  } else if (D > S) {
    if ((((uintptr_t)D + N) & 7U) == 0 && (((uintptr_t)S + N) & 7U) == 0) {
      while (N >= 8U) {
        N -= 8;
        *(volatile uint64_t *)(D + N) = *(const volatile uint64_t *)(S + N);
      }
    }
    while (N--) {
      ((volatile uint8_t *)D)[N] = ((const volatile uint8_t *)S)[N];
    }
  }
  return Dst;
}

void *
memset (
  void  *Dst,
  int    C,
  size_t N
)
{
  uint8_t *D = Dst;
  uint8_t  B = (uint8_t)C;
  if (((uintptr_t)D & 7U) == 0 && N >= 8U) {
    uint64_t W = (uint64_t)B * 0x0101010101010101ULL;
    while (N >= 8U) {
      *(volatile uint64_t *)D = W;
      D += 8;
      N -= 8;
    }
  }
  while (N--) {
    *(volatile uint8_t *)D++ = B;
  }
  return Dst;
}

int
memcmp (
  const void *A,
  const void *B,
  size_t      N
)
{
  const uint8_t *X = A;
  const uint8_t *Y = B;
  for (; N; N--, X++, Y++) {
    if (*X != *Y) {
      return *X - *Y;
    }
  }
  return 0;
}

__attribute__ ((naked)) void
__chkstk (void)
{
  __asm__ volatile ("ret");
}

int _fltused;

// Each block starts with a header holding its size, for ResizePool.

#define HEADER 16

VOID *
AllocatePool (IN UINTN Size)
{
  UINT8 *Block;

  if (EFI_ERROR (
        gBS->AllocatePool (EfiBootServicesData, Size + HEADER, (VOID **)&Block)
      )) {
    return NULL;
  }
  *(UINTN *)Block = Size;
  return Block + HEADER;
}

VOID
FreePool (IN VOID *Buffer OPTIONAL)
{
  if (Buffer) {
    gBS->FreePool ((UINT8 *)Buffer - HEADER);
  }
}

VOID *
AllocateZeroArray (
  IN UINTN Count,
  IN UINTN Size
)
{
  UINT8 *Buffer;

  if (Size && Count > (UINTN)-1 / Size) {
    return NULL;
  }
  Buffer = AllocatePool (Count * Size);
  if (Buffer) {
    __builtin_memset (Buffer, 0, Count * Size);
  }
  return Buffer;
}

VOID *
ResizePool (
  IN VOID *Buffer OPTIONAL,
  IN UINTN Size
)
{
  UINT8 *Moved = AllocatePool (Size);
  UINTN  OldSize;

  if (!Buffer || !Moved) {
    return Moved;
  }
  OldSize = *(UINTN *)((UINT8 *)Buffer - HEADER);
  __builtin_memcpy (Moved, Buffer, OldSize < Size ? OldSize : Size);
  FreePool (Buffer);
  return Moved;
}

UINT32
CalculateCrc32 (
  IN CONST VOID *Buffer,
  IN UINTN       Length
)
{
  UINT32 Crc = 0;

  // The boot service refuses an empty buffer, whose CRC is 0.
  if (!Length ||
      EFI_ERROR (gBS->CalculateCrc32 ((VOID *)Buffer, Length, &Crc))) {
    return 0;
  }
  return Crc;
}
