#define PETSCKSP_DLL
/*
   Functions in this file reorder the Ritz values in the (modified) Leja order.
*/
#include <../src/ksp/ksp/impls/gmres/agmres/agmresimpl.h>

static PetscErrorCode KSPAGMRESLejafmaxarray(PetscScalar *re, PetscInt pt, PetscInt n, PetscInt *pos)
{
  PetscInt    i;
  PetscScalar mx;

  PetscFunctionBegin;
  mx   = re[0];
  *pos = 0;
  for (i = pt; i < n; i++) {
    if (mx < re[i]) {
      mx   = re[i];
      *pos = i;
    }
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

static PetscErrorCode KSPAGMRESLejaCfpdMax(PetscScalar *rm, PetscScalar *im, PetscInt *spos, PetscInt nbre, PetscInt n, PetscInt *rpos)
{
  PetscScalar rd, id, pd, max;
  PetscInt    i, j;

  PetscFunctionBegin;
  pd    = 1.0;
  max   = 0.0;
  *rpos = 0;
  for (i = 0; i < n; i++) {
    for (j = 0; j < nbre; j++) {
      rd = rm[i] - rm[spos[j]];
      id = im[i] - im[spos[j]];
      pd = pd * PetscSqrtReal(rd * rd + id * id);
    }
    if (max < pd) {
      *rpos = i;
      max   = pd;
    }
    pd = 1.0;
  }
  PetscFunctionReturn(PETSC_SUCCESS);
}

PetscErrorCode KSPAGMRESLejaOrdering(PetscScalar *re, PetscScalar *im, PetscScalar *rre, PetscScalar *rim, PetscInt m)
{
  PetscInt    *spos;
  PetscScalar *n_cmpl, temp;
  PetscInt     i, pos, j;

  PetscFunctionBegin;
  PetscCall(PetscMalloc1(m, &n_cmpl));
  PetscCall(PetscMalloc1(m, &spos));
  /* Check the proper order of complex conjugate pairs */
  j = 0;
  while (j < m) {
    if (im[j] != 0.0) {  /* complex eigenvalue */
      if (im[j] < 0.0) { /* change the order */
        temp      = im[j + 1];
        im[j + 1] = im[j];
        im[j]     = temp;
      }
      j += 2;
    } else j++;
  }

  for (i = 0; i < m; i++) n_cmpl[i] = PetscSqrtReal(re[i] * re[i] + im[i] * im[i]);
  PetscCall(KSPAGMRESLejafmaxarray(n_cmpl, 0, m, &pos));
  j = 0;
  if (im[pos] >= 0.0) {
    rre[0] = re[pos];
    rim[0] = im[pos];
    j++;
    spos[0] = pos;
  }
  while (j < (m)) {
    if (im[pos] > 0) {
      rre[j]  = re[pos + 1];
      rim[j]  = im[pos + 1];
      spos[j] = pos + 1;
      j++;
    }
    PetscCall(KSPAGMRESLejaCfpdMax(re, im, spos, j, m, &pos));
    if (im[pos] < 0) pos--;

    if ((im[pos] >= 0) && (j < m)) {
      rre[j]  = re[pos];
      rim[j]  = im[pos];
      spos[j] = pos;
      j++;
    }
  }
  PetscCall(PetscFree(spos));
  PetscCall(PetscFree(n_cmpl));
  PetscFunctionReturn(PETSC_SUCCESS);
}
