#pragma once
#include <petsc/private/fortranimpl.h>

/* PGI compilers pass in f90 pointers as 2 arguments */
#if defined(PETSC_HAVE_F90_2PTR_ARG)
  #define PETSC_F90_2PTR_PROTO_NOVAR , void *
  #define PETSC_F90_2PTR_PROTO(ptr)  , void *ptr
  #define PETSC_F90_2PTR_PARAM(ptr)  , ptr
#else
  #define PETSC_F90_2PTR_PROTO_NOVAR
  #define PETSC_F90_2PTR_PROTO(ptr)
  #define PETSC_F90_2PTR_PARAM(ptr)
#endif

typedef struct {
  char dummy;
} F90Array1d;
typedef struct {
  char dummy;
} F90Array2d;
typedef struct {
  char dummy;
} F90Array3d;
typedef struct {
  char dummy;
} F90Array4d;

PETSC_EXTERN PetscErrorCode F90Array1dCreate(void *, MPI_Datatype, PetscInt, PetscInt, F90Array1d *PETSC_F90_2PTR_PROTO_NOVAR);
PETSC_EXTERN PetscErrorCode F90Array1dAccess(F90Array1d *, MPI_Datatype, void **PETSC_F90_2PTR_PROTO_NOVAR);
PETSC_EXTERN PetscErrorCode F90Array1dDestroy(F90Array1d *, MPI_Datatype PETSC_F90_2PTR_PROTO_NOVAR);

PETSC_EXTERN PetscErrorCode F90Array2dCreate(void *, MPI_Datatype, PetscInt, PetscInt, PetscInt, PetscInt, F90Array2d *PETSC_F90_2PTR_PROTO_NOVAR);
PETSC_EXTERN PetscErrorCode F90Array2dAccess(F90Array2d *, MPI_Datatype, void **PETSC_F90_2PTR_PROTO_NOVAR);
PETSC_EXTERN PetscErrorCode F90Array2dDestroy(F90Array2d *, MPI_Datatype PETSC_F90_2PTR_PROTO_NOVAR);

PETSC_EXTERN PetscErrorCode F90Array3dCreate(void *, MPI_Datatype, PetscInt, PetscInt, PetscInt, PetscInt, PetscInt, PetscInt, F90Array3d *PETSC_F90_2PTR_PROTO_NOVAR);
PETSC_EXTERN PetscErrorCode F90Array3dAccess(F90Array3d *, MPI_Datatype, void **PETSC_F90_2PTR_PROTO_NOVAR);
PETSC_EXTERN PetscErrorCode F90Array3dDestroy(F90Array3d *, MPI_Datatype PETSC_F90_2PTR_PROTO_NOVAR);

PETSC_EXTERN PetscErrorCode F90Array4dCreate(void *, MPI_Datatype, PetscInt, PetscInt, PetscInt, PetscInt, PetscInt, PetscInt, PetscInt, PetscInt, F90Array4d *PETSC_F90_2PTR_PROTO_NOVAR);
PETSC_EXTERN PetscErrorCode F90Array4dAccess(F90Array4d *, MPI_Datatype, void **PETSC_F90_2PTR_PROTO_NOVAR);
PETSC_EXTERN PetscErrorCode F90Array4dDestroy(F90Array4d *, MPI_Datatype PETSC_F90_2PTR_PROTO_NOVAR);

/*
  F90Array1dCreate - Given a C pointer to a one dimensional
  array and its length; this fills in the appropriate Fortran 90
  pointer data structure.

  Input Parameters:
+   array - regular C pointer (address)
.   type  - DataType of the array
.   start - starting index of the array
-   len   - length of array (in items)

  Output Parameter:
.   ptr - Fortran 90 pointer
*/
