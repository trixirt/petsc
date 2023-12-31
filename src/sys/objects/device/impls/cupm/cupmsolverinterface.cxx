#include <petsc/private/cupmsolverinterface.hpp>
#include <petsc/private/petscadvancedmacros.h>

namespace Petsc
{

namespace device
{

namespace cupm
{

namespace impl
{

#define PETSC_CUPMSOLVER_STATIC_VARIABLE_DEFN(THEIRS, DEVICE, OURS) const decltype(THEIRS) SolverInterfaceImpl<DeviceType::DEVICE>::OURS;

#define PETSC_CUPMSOLVER_DEFINE_STATIC_VARIABLE_EXACT(CUORIGINAL, HIPORIGINAL, OURS) \
  PetscIfPetscDefined(HAVE_CUDA, PETSC_CUPMSOLVER_STATIC_VARIABLE_DEFN, PetscExpandToNothing)(CUORIGINAL, CUDA, OURS) PetscIfPetscDefined(HAVE_HIP, PETSC_CUPMSOLVER_STATIC_VARIABLE_DEFN, PetscExpandToNothing)(HIPORIGINAL, HIP, OURS)

#define PETSC_CUPMSOLVER_DEFINE_STATIC_VARIABLE_MATCHING_SCHEME(SUFFIX) PETSC_CUPMSOLVER_DEFINE_STATIC_VARIABLE_EXACT(PetscConcat(CUSOLVER_, SUFFIX), PetscConcat(HIPSOLVER_, SUFFIX), PetscConcat(CUPMSOLVER_, SUFFIX))

PETSC_CUPMSOLVER_DEFINE_STATIC_VARIABLE_MATCHING_SCHEME(STATUS_SUCCESS)
PETSC_CUPMSOLVER_DEFINE_STATIC_VARIABLE_MATCHING_SCHEME(STATUS_NOT_INITIALIZED)
PETSC_CUPMSOLVER_DEFINE_STATIC_VARIABLE_MATCHING_SCHEME(STATUS_ALLOC_FAILED)
PETSC_CUPMSOLVER_DEFINE_STATIC_VARIABLE_MATCHING_SCHEME(STATUS_INTERNAL_ERROR)

PETSC_CUPMSOLVER_DEFINE_STATIC_VARIABLE_EXACT(CUBLAS_OP_T, HIPSOLVER_OP_T, CUPMSOLVER_OP_T)
PETSC_CUPMSOLVER_DEFINE_STATIC_VARIABLE_EXACT(CUBLAS_OP_N, HIPSOLVER_OP_N, CUPMSOLVER_OP_N)
PETSC_CUPMSOLVER_DEFINE_STATIC_VARIABLE_EXACT(CUBLAS_OP_C, HIPSOLVER_OP_C, CUPMSOLVER_OP_C)

PETSC_CUPMSOLVER_DEFINE_STATIC_VARIABLE_EXACT(CUBLAS_FILL_MODE_LOWER, HIPSOLVER_FILL_MODE_LOWER, CUPMSOLVER_FILL_MODE_LOWER)
PETSC_CUPMSOLVER_DEFINE_STATIC_VARIABLE_EXACT(CUBLAS_FILL_MODE_UPPER, HIPSOLVER_FILL_MODE_UPPER, CUPMSOLVER_FILL_MODE_UPPER)

PETSC_CUPMSOLVER_DEFINE_STATIC_VARIABLE_EXACT(CUBLAS_SIDE_LEFT, HIPSOLVER_SIDE_LEFT, CUPMSOLVER_SIDE_LEFT)
PETSC_CUPMSOLVER_DEFINE_STATIC_VARIABLE_EXACT(CUBLAS_SIDE_RIGHT, HIPSOLVER_SIDE_RIGHT, CUPMSOLVER_SIDE_RIGHT)

#if PetscDefined(HAVE_CUDA)
template struct SolverInterface<DeviceType::CUDA>;
#endif

#if PetscDefined(HAVE_HIP)
template struct SolverInterface<DeviceType::HIP>;
#endif

} // namespace impl

} // namespace cupm

} // namespace device

} // namespace Petsc
