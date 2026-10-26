/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2026-present Open Handheld Collective
 *   https://github.com/OpenHandheldCollective
 */

/**
  Input - maps firmware key events to UI inputs.

  Up and volume up are Up, down and volume down are Down, Esc and Backspace
  are Back, and Enter, Space and SCAN_SUSPEND (the power key on Qualcomm
  keypads) are Select. Other keys are ignored. Repeat and hold timing is
  left to the UI's key decoder (Ui/Ui.h).
*/

#ifndef __EFI_INPUT_H__
#define __EFI_INPUT_H__

#include "Base/OablBase.h"

#include "Base/Buffer.h"
#include "Ui/Ui.h"

typedef struct _INPUT_KEYS {
  KEYS    Decoder;
  BOOLEAN HwHeldAtStart[3];
  BOOLEAN HwPrevHeld[3];
  BOOLEAN PadHeldAtStart[4];
  BOOLEAN PadPrevHeld[4];
  BOOLEAN PadStickUp;
  BOOLEAN PadStickDown;
  BOOLEAN PadInitDone;
  BOOLEAN PadActive;
  BOOLEAN PadIsRetroid8250;
  UINT8   PadStep;
  UINT8   PadAttempts;
  UINT8   PadSeq;
  UINT16  PadEnGpio;
  UINT16  PadRstGpio;
  UINT64  PadUartBase;
  UINT64  PadNextMs;
  UINTN   PadRxLen;
  UINT8   PadRxBuf[256];
} INPUT_KEYS;

typedef struct _INPUT_EVENTS OABL_VEC (INPUT) INPUT_EVENTS;

STATIC inline INPUT_KEYS
InputKeysNew (VOID)
{
  return (INPUT_KEYS){0};
}

// Returns TRUE if Volume Down is currently held on the PMIC PON RESIN_N
// line, used by Early() as a Fastboot escape hatch before running quirks.
BOOLEAN
InputVolumeDownHeld (VOID);

// Returns TRUE if a key was already down at startup, as when volume down is
// held for the menu. Reads one mapped key.
BOOLEAN
InputKeysHeldAtStart (IN OUT INPUT_KEYS *Keys);

// Feeds every waiting key through the decoder at time Now (ms) and appends
// the resulting inputs to Out. Caller frees Out with VecFree.
VOID
InputKeysPoll (
  IN OUT INPUT_KEYS   *Keys,
  IN UINT64            Now,
  IN OUT INPUT_EVENTS *Out
);

// Quiesces the gamepad MCU and UART before leaving the interactive menu so
// the OS kernel sees an idle serial engine and quiet IRQ line at boot.
VOID
InputKeysClose (IN OUT INPUT_KEYS *Keys);

#endif
