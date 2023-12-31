#pragma once

#include <../src/mat/impls/aij/mpi/mpiaij.h>

typedef struct {
  PetscInt dof; /* number of components */
  Mat      AIJ; /* representation of interpolation for one component */
} Mat_SeqMAIJ;

typedef struct {
  PetscInt   dof;       /* number of components */
  Mat        AIJ, OAIJ; /* representation of interpolation for one component */
  Mat        A;
  VecScatter ctx; /* update ghost points for parallel case */
  Vec        w;   /* work space for ghost values for parallel case */
} Mat_MPIMAIJ;

PETSC_INTERN PetscErrorCode MatProductSymbolic_PtAP_SeqAIJ_SeqMAIJ(Mat);
PETSC_INTERN PetscErrorCode MatProductSymbolic_PtAP_MPIAIJ_MPIMAIJ(Mat);
