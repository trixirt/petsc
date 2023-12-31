/*
    Private Krylov Context Structure (KSP) for LCD

    This one is very simple. It contains a flag indicating the symmetry
   structure of the matrix and work space for (optionally) computing
   eigenvalues.

*/

#pragma once

/*
        Defines the basic KSP object
*/
#include <petsc/private/kspimpl.h>

typedef struct {
  PetscInt  restart;
  PetscInt  max_iters;
  PetscReal haptol;
  Vec      *P;
  Vec      *Q;
} KSP_LCD;
