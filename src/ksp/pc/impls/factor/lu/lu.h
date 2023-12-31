/*
   Private data structure for LU preconditioner.
*/
#pragma once

#include <../src/ksp/pc/impls/factor/factor.h>

typedef struct {
  PC_Factor hdr;
  IS        row, col; /* index sets used for reordering */
  PetscBool nonzerosalongdiagonal;
  PetscReal nonzerosalongdiagonaltol;
} PC_LU;
