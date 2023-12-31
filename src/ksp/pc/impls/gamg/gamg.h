#pragma once

#include <petsc/private/pcgamgimpl.h>
#include <../src/mat/impls/aij/seq/aij.h>
#include <../src/mat/impls/aij/mpi/mpiaij.h>

PETSC_INTERN PetscErrorCode PCGAMGSquareGraph_GAMG(PC, Mat, Mat *);
