#pragma once

#include <../src/ksp/pc/impls/factor/factor.h>

/* Incomplete Cholesky factorization context */

typedef struct {
  PC_Factor hdr;
  void     *implctx;
} PC_ICC;
