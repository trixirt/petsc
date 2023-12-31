#pragma once

#include <petscdt.h>

struct _p_PetscQuadrature {
  PETSCHEADER(int);
  DMPolytopeType   ct;        /* The domain of integration */
  PetscInt         dim;       /* The spatial dimension */
  PetscInt         Nc;        /* The number of components */
  PetscInt         order;     /* The order, i.e. the highest degree polynomial that is exactly integrated */
  PetscInt         numPoints; /* The number of quadrature points on an element */
  const PetscReal *points;    /* The quadrature point coordinates */
  const PetscReal *weights;   /* The quadrature weights */
};

#if (!defined(PETSC_MISSING_LAPACK_STEQR) || !defined(PETSC_MISSING_LAPACK_STEGR))
  #define PETSCDTGAUSSIANQUADRATURE_EIG 1
#endif

PETSC_EXTERN PetscBool PetscDTGaussQuadratureNewton_Internal;
