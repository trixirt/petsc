
#ifndef lint
static char vcid[] = "$Id: vscat.c,v 1.47 1995/12/06 00:23:44 bsmith Exp bsmith $";
#endif

/*
     Code for creating scatters between vectors. This file 
  includes the code for scattering between sequential vectors.
*/

#include "is/isimpl.h"
#include "vecimpl.h"                     /*I "vec.h" I*/
#include "impls/dvecimpl.h"
#include "impls/mpi/pvecimpl.h"


/*
      This is special scatter code for when the entire parallel vector is 
   copied to each processor.

      This code was written by Cameron Cooper, Occidental College, Fall 1995.
*/
static int MPIToAll(Vec x,Vec y,InsertMode addv,int mode,VecScatter ctx)
{ 
  if (mode & SCATTER_REVERSE) {
    Vec_MPI             *yy = (Vec_MPI *) y->data;
    Vec_Seq             *xx = (Vec_Seq *) x->data;
    Scalar              *xv = xx->array, *yv = yy->array, *xvt, *xvt2;
    VecScatter_MPIToAll *scat = (VecScatter_MPIToAll *) ctx->todata;
    int                 i, size = xx->n;

    if (addv == INSERT_VALUES) {
      /* 
         copy the correct part of the local vector into the local storage of 
         the MPI one  Note: this operation only makes sense if all the local 
         vectors have the same values
      */
      PetscMemcpy(yv,xv+yy->ownership[yy->rank],yy->n*sizeof(Scalar));
    }
    else {
      if (scat->work) xvt = scat->work; 
      else {scat->work = xvt = (Scalar *) PetscMalloc(size*sizeof(Scalar));CHKPTRQ(xvt);}
      if (!yy->rank) { /* I am the zeroth processor, values are accumulated here */
        if (scat->work2) xvt2 = scat->work2; 
        else {scat->work2 = xvt2 = (Scalar *) PetscMalloc(size*sizeof(Scalar));CHKPTRQ(xvt2);}
        MPI_Gatherv(yv,yy->n,MPIU_SCALAR,xvt2,scat->count,yy->ownership,MPIU_SCALAR,0,ctx->comm);
        MPI_Reduce(xv, xvt, size, MPIU_SCALAR, MPI_SUM, 0, ctx->comm);
	for ( i=0; i<size; i++ ) {
	  xvt[i] += xvt2[i];
	}
        MPI_Scatterv(xvt,scat->count,yy->ownership,MPIU_SCALAR,yv,yy->n,MPIU_SCALAR,0,ctx->comm);
      }
      else {
        MPI_Gatherv(yv, yy->n, MPIU_SCALAR, 0,  0, 0, MPIU_SCALAR, 0, ctx->comm);
        MPI_Reduce(xv, xvt, size, MPIU_SCALAR, MPI_SUM, 0, ctx->comm);
        MPI_Scatterv(0,scat->count,yy->ownership,MPIU_SCALAR,yv,yy->n,MPIU_SCALAR,0,ctx->comm);
      }
    }
  }
  else {
    Vec_MPI             *xx = (Vec_MPI *) x->data;
    Vec_Seq             *yy = (Vec_Seq *) y->data;
    Scalar              *xv = xx->array, *yv = yy->array, *yvt;
    VecScatter_MPIToAll *scat = (VecScatter_MPIToAll *) ctx->todata;
    int                 i, size = yy->n;

    if (addv == INSERT_VALUES) {
      MPI_Allgatherv(xv,xx->n,MPIU_SCALAR,yv,scat->count,xx->ownership,MPIU_SCALAR,ctx->comm);
    }
    else {
      if (scat->work) yvt = scat->work; 
      else {scat->work = yvt = (Scalar *) PetscMalloc(size*sizeof(Scalar));CHKPTRQ(yvt);}
      MPI_Allgatherv(xv,xx->n,MPIU_SCALAR,yvt,scat->count,xx->ownership,MPIU_SCALAR,ctx->comm);
      for ( i=0; i<size; i++ ) {
	yv[i] += yvt[i];
      }
    }
  }
  return 0;
}

static int MPIToAllDestroy(PetscObject obj)
{
  VecScatter          ctx = (VecScatter) obj;
  VecScatter_MPIToAll *scat = (VecScatter_MPIToAll *) ctx->todata;

  PetscFree(scat->count);
  if (scat->work)  PetscFree(scat->work);
  if (scat->work2) PetscFree(scat->work2);
  PetscFree(ctx->todata); 
  PLogObjectDestroy(ctx);
  PetscHeaderDestroy(ctx);
  return 0;
}

/*
    Sequential general to general scatter 
*/
static int SGtoSG(Vec x,Vec y,InsertMode addv,int mode,VecScatter ctx)
{
  VecScatter_General *gen_to = (VecScatter_General *) ctx->todata;
  VecScatter_General *gen_from = (VecScatter_General *) ctx->fromdata;
  int                i, n = gen_from->n, *fslots, *tslots;
  Vec_Seq            *xx = (Vec_Seq *) x->data,*yy = (Vec_Seq *) y->data;
  Scalar             *xv = xx->array, *yv = yy->array;
  
  if (mode & SCATTER_REVERSE ){
    gen_to   = (VecScatter_General *) ctx->fromdata;
    gen_from = (VecScatter_General *) ctx->todata;
    mode    -= SCATTER_REVERSE;
  }
  fslots = gen_from->slots;
  tslots = gen_to->slots;

  if (addv == INSERT_VALUES) {
    for ( i=0; i<n; i++ ) {yv[tslots[i]] = xv[fslots[i]];}
  }
  else {
    for ( i=0; i<n; i++ ) {yv[tslots[i]] += xv[fslots[i]];}
  }
  return 0;
}


static int SGtoSS_Stride1(Vec x,Vec y,InsertMode addv,int mode,VecScatter ctx)
{
  VecScatter_Stride  *gen_to = (VecScatter_Stride *) ctx->todata;
  VecScatter_General *gen_from = (VecScatter_General *) ctx->fromdata;
  int                i, n = gen_from->n, *fslots = gen_from->slots;
  int                first = gen_to->first;
  Vec_Seq            *xx = (Vec_Seq *) x->data,*yy = (Vec_Seq *) y->data;
  Scalar             *xv = xx->array, *yv = yy->array;
  
  if (mode & SCATTER_REVERSE ){
    xv += first;
    if (addv == INSERT_VALUES) {
      for ( i=0; i<n; i++ ) {yv[fslots[i]] = xv[i];}
    }
    else {
      for ( i=0; i<n; i++ ) {yv[fslots[i]] += xv[i];}
    }
  } else {
    yv += first;
    if (addv == INSERT_VALUES) {
      for ( i=0; i<n; i++ ) {yv[i] = xv[fslots[i]];}
    }
    else {
      for ( i=0; i<n; i++ ) {yv[i] += xv[fslots[i]];}
    }
  }
  return 0;
}

static int SGtoSS(Vec x,Vec y,InsertMode addv,int mode,VecScatter ctx)
{
  VecScatter_Stride  *gen_to = (VecScatter_Stride *) ctx->todata;
  VecScatter_General *gen_from = (VecScatter_General *) ctx->fromdata;
  int                i, n = gen_from->n, *fslots = gen_from->slots;
  int                first = gen_to->first,step = gen_to->step;
  Vec_Seq            *xx = (Vec_Seq *) x->data,*yy = (Vec_Seq *) y->data;
  Scalar             *xv = xx->array, *yv = yy->array;
  
  if (mode & SCATTER_REVERSE ){
    if (addv == INSERT_VALUES) {
      for ( i=0; i<n; i++ ) {yv[fslots[i]] = xv[first + i*step];}
    }
    else {
      for ( i=0; i<n; i++ ) {yv[fslots[i]] += xv[first + i*step];}
    }
  } else {
    if (addv == INSERT_VALUES) {
      for ( i=0; i<n; i++ ) {yv[first + i*step] = xv[fslots[i]];}
    }
    else {
      for ( i=0; i<n; i++ ) {yv[first + i*step] += xv[fslots[i]];}
    }
  }
  return 0;
}

static int SStoSG_Stride1(Vec x,Vec y,InsertMode addv,int mode,VecScatter ctx)
{
  VecScatter_Stride  *gen_from = (VecScatter_Stride *) ctx->fromdata;
  VecScatter_General *gen_to = (VecScatter_General *) ctx->todata;
  int                i, n = gen_from->n, *fslots = gen_to->slots;
  int                first = gen_from->first;
  Vec_Seq            *xx = (Vec_Seq *) x->data,*yy = (Vec_Seq *) y->data;
  Scalar             *xv = xx->array, *yv = yy->array;
  
  if (mode & SCATTER_REVERSE ){
    yv += first;
    if (addv == INSERT_VALUES) {
      for ( i=0; i<n; i++ ) {yv[i] = xv[fslots[i]];}
    }
    else {
      for ( i=0; i<n; i++ ) {yv[i] += xv[fslots[i]];}
    }
  } else {
    xv += first;
    if (addv == INSERT_VALUES) {
      for ( i=0; i<n; i++ ) {yv[fslots[i]] = xv[i];}
    }
    else {
      for ( i=0; i<n; i++ ) {yv[fslots[i]] += xv[i];}
    }
  }
  return 0;
}

static int SStoSG(Vec x,Vec y,InsertMode addv,int mode,VecScatter ctx)
{
  VecScatter_Stride  *gen_from = (VecScatter_Stride *) ctx->fromdata;
  VecScatter_General *gen_to = (VecScatter_General *) ctx->todata;
  int                i, n = gen_from->n, *fslots = gen_to->slots;
  int                first = gen_from->first,step = gen_from->step;
  Vec_Seq            *xx = (Vec_Seq *) x->data,*yy = (Vec_Seq *) y->data;
  Scalar             *xv = xx->array, *yv = yy->array;
  
  if (mode & SCATTER_REVERSE ){
    if (addv == INSERT_VALUES) {
      for ( i=0; i<n; i++ ) {yv[first + i*step] = xv[fslots[i]];}
    }
    else {
      for ( i=0; i<n; i++ ) {yv[first + i*step] += xv[fslots[i]];}
    }
  } else {
    if (addv == INSERT_VALUES) {
      for ( i=0; i<n; i++ ) {yv[fslots[i]] = xv[first + i*step];}
    }
    else {
      for ( i=0; i<n; i++ ) {yv[fslots[i]] += xv[first + i*step];}
    }
  }
  return 0;
}

static int SStoSS(Vec x,Vec y,InsertMode addv,int mode,VecScatter ctx)
{
  VecScatter_Stride  *gen_to = (VecScatter_Stride *) ctx->todata;
  VecScatter_Stride  *gen_from = (VecScatter_Stride *) ctx->fromdata;
  int                i, n = gen_from->n, to_first = gen_to->first,to_step = gen_to->step;
  int                from_first = gen_from->first,from_step = gen_from->step;
  Vec_Seq            *xx = (Vec_Seq *) x->data,*yy = (Vec_Seq *) y->data;
  Scalar             *xv = xx->array, *yv = yy->array;
  
  if (addv == INSERT_VALUES) {
    if (to_step == 1 && from_step == 1) {
      PetscMemcpy(yv+to_first,xv+from_first,n*sizeof(Scalar));
    }
    else {
      for ( i=0; i<n; i++ ) {
        yv[to_first + i*to_step] = xv[from_first+i*from_step];
      }
    }
  }
  else {
    if (to_step == 1 && from_step == 1) {
      yv += to_first; xv += from_first;
      for ( i=0; i<n; i++ ) {
        yv[i] += xv[i];
      }
    }
    else {
      for ( i=0; i<n; i++ ) {
        yv[to_first + i*to_step] += xv[from_first+i*from_step];
      }
    }
  }
  return 0;
}
static int SGtoSGDestroy(PetscObject obj)
{
  VecScatter ctx = (VecScatter) obj;
  PetscFree(ctx->todata); PetscFree(ctx->fromdata); 
  PLogObjectDestroy(ctx);
  PetscHeaderDestroy(ctx);
  return 0;
}

int PtoSScatterCreate(int,int *,int,int *,Vec,VecScatter);
int PtoPScatterCreate(int,int *,int,int *,Vec,Vec,VecScatter);
int StoPScatterCreate(int,int *,int,int *,Vec,VecScatter);
/* --------------------------------------------------------------*/
/*@C
   VecScatterCreate - Creates a vector scatter context. This routine
   should be called when you need to create a vector scatter context. 
   You cannot use a VecScatter in two or more simultaneous scatters. 

   Input Parameters:
.  xin - the vector to scatter from
.  yin - the vector to scatter to
.  ix - the indices in xin to scatter
.  iy - the indices in yin to put results

   Output Parameter:
.  newctx - location to store the scatter context

.keywords: vector, scatter, context, create

.seealso: VecScatterDestroy()
@*/
int VecScatterCreate(Vec xin,IS ix,Vec yin,IS iy,VecScatter *newctx)
{
  VecScatter ctx;
  int        len,size; 
  MPI_Comm   comm = xin->comm;

  /* next 2 lines insure that we use parallel comm if it exists */
  MPI_Comm_size(yin->comm,&size);
  if (size > 1) comm = yin->comm; 

  /* generate the Scatter context */
  PetscHeaderCreate(ctx,_VecScatter,VEC_SCATTER_COOKIE,0,comm);
  PLogObjectCreate(ctx);
  PLogObjectMemory(ctx,sizeof(struct _VecScatter));

  if (xin->type == VECSEQ && yin->type == VECSEQ) {
    if (ix->type == IS_SEQ && iy->type == IS_SEQ){
      int                nx,ny,*idx,*idy;
      VecScatter_General *to,*from;
      ISGetLocalSize(ix,&nx); ISGetIndices(ix,&idx);
      ISGetLocalSize(iy,&ny); ISGetIndices(iy,&idy);
      if (nx != ny) SETERRQ(1,"VecScatterCreate:Local scatter sizes don't match");
      len = sizeof(VecScatter_General) + nx*sizeof(int);
      to = (VecScatter_General *) PetscMalloc(len); CHKPTRQ(to)
      PLogObjectMemory(ctx,len);
      to->slots = (int *) (to + 1); to->n = nx; 
      PetscMemcpy(to->slots,idy,nx*sizeof(int));
      from = (VecScatter_General *) PetscMalloc(len); CHKPTRQ(from);
      from->slots = (int *) (from + 1); from->n = nx; 
      PetscMemcpy(from->slots,idx,nx*sizeof(int));
      ctx->todata = (void *) to; ctx->fromdata = (void *) from;
      ctx->scatterbegin = SGtoSG; ctx->destroy = SGtoSGDestroy;
      ctx->scatterend = 0; ctx->copy = 0;
      ctx->pipelinebegin = 0; ctx->pipelineend = 0;
      *newctx = ctx;
      return 0;
    }
    else if (ix->type == IS_STRIDE_SEQ &&  iy->type == IS_STRIDE_SEQ){
      int                nx,ny,to_first,to_step,from_first,from_step;
      VecScatter_Stride  *from,*to;
      ISGetLocalSize(ix,&nx); ISStrideGetInfo(ix,&from_first,&from_step);
      ISGetLocalSize(iy,&ny); ISStrideGetInfo(iy,&to_first,&to_step);
      if (nx != ny) SETERRQ(1,"VecScatterCreate:Local scatter sizes don't match");
      to = (VecScatter_Stride *) PetscMalloc(sizeof(VecScatter_Stride)); CHKPTRQ(to);
      to->n = nx; to->first = to_first; to->step = to_step;
      from = (VecScatter_Stride *) PetscMalloc(sizeof(VecScatter_Stride));CHKPTRQ(from);
      PLogObjectMemory(ctx,2*sizeof(VecScatter_Stride));
      from->n = nx; from->first = from_first; from->step = from_step;
      ctx->todata = (void *) to; ctx->fromdata = (void *) from;
      ctx->scatterbegin = SStoSS; ctx->destroy = SGtoSGDestroy;
      ctx->scatterend = 0; ctx->pipelinebegin = 0; 
      ctx->pipelineend = 0; ctx->copy = 0;
      *newctx = ctx;
      return 0;
    }
    else if (ix->type == IS_SEQ && iy->type == IS_STRIDE_SEQ){
      int                nx,ny,*idx,first,step;
      VecScatter_General *from;
      VecScatter_Stride  *to;
      ISGetLocalSize(ix,&nx); ISGetIndices(ix,&idx);
      ISGetLocalSize(iy,&ny); ISStrideGetInfo(iy,&first,&step);
      if (nx != ny) SETERRQ(1,"VecScatterCreate:Local scatter sizes don't match");
      to = (VecScatter_Stride *) PetscMalloc(sizeof(VecScatter_Stride)); CHKPTRQ(to);
      to->n = nx; to->first = first; to->step = step;
      len = sizeof(VecScatter_General) + nx*sizeof(int);
      from = (VecScatter_General *) PetscMalloc(len); CHKPTRQ(from);
      PLogObjectMemory(ctx,len + sizeof(VecScatter_Stride));
      from->slots = (int *) (from + 1); from->n = nx; 
      PetscMemcpy(from->slots,idx,nx*sizeof(int));
      ctx->todata = (void *) to; ctx->fromdata = (void *) from;
      if (step == 1)  ctx->scatterbegin = SGtoSS_Stride1;
      else            ctx->scatterbegin = SGtoSS;
      ctx->destroy = SGtoSGDestroy;
      ctx->scatterend = 0; ctx->pipelinebegin = 0;
      ctx->pipelineend = 0; ctx->copy = 0;
      *newctx = ctx;
      return 0;
    }
    else if (ix->type == IS_STRIDE_SEQ && iy->type == IS_SEQ){
      int                nx,ny,*idx,first,step;
      VecScatter_General *to;
      VecScatter_Stride  *from;
      ISGetLocalSize(ix,&nx); ISGetIndices(iy,&idx);
      ISGetLocalSize(iy,&ny); ISStrideGetInfo(ix,&first,&step);
      if (nx != ny) SETERRQ(1,"VecScatterCreate:Local scatter sizes don't match");
      from = (VecScatter_Stride *) PetscMalloc(sizeof(VecScatter_Stride));CHKPTRQ(from);
      from->n = nx; from->first = first; from->step = step;
      len = sizeof(VecScatter_General) + nx*sizeof(int);
      to = (VecScatter_General *) PetscMalloc(len); CHKPTRQ(to);
      PLogObjectMemory(ctx,len + sizeof(VecScatter_Stride));
      to->slots = (int *) (to + 1); to->n = nx; 
      PetscMemcpy(to->slots,idx,nx*sizeof(int));
      ctx->todata = (void *) to; ctx->fromdata = (void *) from;
      if (step == 1) ctx->scatterbegin = SStoSG_Stride1; 
      else           ctx->scatterbegin = SStoSG; 
      ctx->destroy = SGtoSGDestroy;
      ctx->scatterend = 0; ctx->pipelinebegin = 0; 
      ctx->pipelineend = 0; ctx->copy = 0;
      *newctx = ctx;
      return 0;
    }
    else {
      SETERRQ(1,"VecScatterCreate:Cannot generate such Scatter Context yet");
    }
  }
  if (xin->type == VECMPI && yin->type == VECSEQ) {
    /* special case extracting (subset of) local portion */ 
    if (ix->type == IS_STRIDE_SEQ && iy->type == IS_STRIDE_SEQ){
      Vec_MPI            *x = (Vec_MPI *)xin->data;
      int                nx,ny,to_first,to_step,from_first,from_step,islocal,cando;
      int                start = x->ownership[x->rank], end = x->ownership[x->rank+1];
      VecScatter_Stride  *from,*to;

      ISGetLocalSize(ix,&nx); ISStrideGetInfo(ix,&from_first,&from_step);
      ISGetLocalSize(iy,&ny); ISStrideGetInfo(iy,&to_first,&to_step);
      if (nx != ny) SETERRQ(1,"VecScatterCreate:Local scatter sizes don't match");
      if (ix->min >= start && ix->max < end ) islocal = 1; else islocal = 0;
      MPI_Allreduce( &islocal, &cando,1,MPI_INT,MPI_LAND,xin->comm);
      if (cando) {
        to = (VecScatter_Stride *) PetscMalloc(sizeof(VecScatter_Stride)); CHKPTRQ(to);
        to->n = nx; to->first = to_first; to->step = to_step;
        from = (VecScatter_Stride *) PetscMalloc(sizeof(VecScatter_Stride));CHKPTRQ(from);
        PLogObjectMemory(ctx,2*sizeof(VecScatter_Stride));
        from->n = nx; from->first = from_first-start; from->step = from_step;
        ctx->todata = (void *) to; ctx->fromdata = (void *) from;
        ctx->scatterbegin = SStoSS; ctx->destroy = SGtoSGDestroy;
        ctx->scatterend = 0; ctx->pipelinebegin = 0; 
        ctx->pipelineend = 0; ctx->copy = 0;
        *newctx = ctx;
        return 0;
      }
    }
    else {
      int cando,islocal = 0;
      MPI_Allreduce( &islocal, &cando,1,MPI_INT,MPI_LAND,yin->comm);
    }
    /* test for special case of all processors getting entire vector */
    if (ix->type == IS_STRIDE_SEQ && iy->type == IS_STRIDE_SEQ){
      Vec_MPI             *x = (Vec_MPI *)xin->data;
      int                 i,nx,ny,to_first,to_step,from_first,from_step,totalv,cando,*count;
      VecScatter_MPIToAll *sto;

      ISGetLocalSize(ix,&nx); ISStrideGetInfo(ix,&from_first,&from_step);
      ISGetLocalSize(iy,&ny); ISStrideGetInfo(iy,&to_first,&to_step);
      if (nx != ny) SETERRQ(1,"VecScatterCreate:Local scatter sizes don't match");
      if (from_first==0 && from_step==1 && from_first==to_first && from_step==to_step) {
        totalv = 1; 
      } else totalv = 0;
      MPI_Allreduce( &totalv, &cando,1,MPI_INT,MPI_LAND,xin->comm);

      if (cando) {
        sto = (VecScatter_MPIToAll *) PetscMalloc(sizeof(VecScatter_MPIToAll));CHKPTRQ(sto);
        MPI_Comm_size(ctx->comm,&size);
        count = (int *) PetscMalloc(size*sizeof(int));CHKPTRQ(count);
        for ( i=0; i<size; i++ ) {
	  count[i] = x->ownership[i+1]-x->ownership[i];
        }
        sto->count         = count;
        sto->work          = 0;
        sto->work2         = 0;
        PLogObjectMemory(ctx,sizeof(VecScatter_MPIToAll)+size*sizeof(int));
        ctx->todata       = (void *) sto;        ctx->fromdata = 0;
        ctx->scatterbegin = MPIToAll;             ctx->destroy = MPIToAllDestroy;
        ctx->scatterend   = 0;              ctx->pipelinebegin = 0; 
        ctx->pipelineend  = 0;                       ctx->copy = 0;
        *newctx = ctx;
        return 0;
      }
    }
    else {
      int cando,totalv = 0;
      MPI_Allreduce( &totalv, &cando,1,MPI_INT,MPI_LAND,yin->comm);
    }
    /* left over general case */
    {
      int ierr,nx,ny,*idx,*idy;
      ISGetLocalSize(ix,&nx); ISGetIndices(ix,&idx);
      ISGetLocalSize(iy,&ny); ISGetIndices(iy,&idy);
      if (nx != ny) SETERRQ(1,"VecScatterCreate:Local scatter sizes don't match");
      ierr = PtoSScatterCreate(nx,idx,ny,idy,xin,ctx); CHKERRQ(ierr);
      ISRestoreIndices(ix,&idx); ISRestoreIndices(iy,&idy);
      *newctx = ctx;
      return 0;
    }
  }
  if (xin->type == VECSEQ && yin->type == VECMPI) {
    /* special case local copy portion */ 
    if (ix->type == IS_STRIDE_SEQ && iy->type == IS_STRIDE_SEQ){
      Vec_MPI            *y = (Vec_MPI *)yin->data;
      int                nx,ny,to_first,to_step,from_step, start = y->ownership[y->rank];
      int                end = y->ownership[y->rank+1],islocal,cando,from_first;
      VecScatter_Stride  *from,*to;

      ISGetLocalSize(ix,&nx); ISStrideGetInfo(ix,&from_first,&from_step);
      ISGetLocalSize(iy,&ny); ISStrideGetInfo(iy,&to_first,&to_step);
      if (nx != ny) SETERRQ(1,"VecScatterCreate:Local scatter sizes don't match");
      if (iy->min >= start && iy->max < end ) islocal = 1; else islocal = 0;
      MPI_Allreduce( &islocal, &cando,1,MPI_INT,MPI_LAND,yin->comm);
      if (cando) {
        to = (VecScatter_Stride *) PetscMalloc(sizeof(VecScatter_Stride)); CHKPTRQ(to);
        to->n = nx; to->first = to_first-start; to->step = to_step;
        from = (VecScatter_Stride *) PetscMalloc(sizeof(VecScatter_Stride));CHKPTRQ(from);
        PLogObjectMemory(ctx,2*sizeof(VecScatter_Stride));
        from->n = nx; from->first = from_first; from->step = from_step;
        ctx->todata = (void *) to; ctx->fromdata = (void *) from;
        ctx->scatterbegin = SStoSS; ctx->destroy = SGtoSGDestroy;
        ctx->scatterend = 0; ctx->pipelinebegin = 0;
        ctx->pipelineend = 0; ctx->copy = 0;
        *newctx = ctx;
        return 0;
      }
    }
    else {
      int cando,islocal = 0;
      MPI_Allreduce( &islocal, &cando,1,MPI_INT,MPI_LAND,yin->comm);
    }
    {
      int ierr,nx,ny,*idx,*idy;
      ISGetLocalSize(ix,&nx); ISGetIndices(ix,&idx);
      ISGetLocalSize(iy,&ny); ISGetIndices(iy,&idy);
      if (nx != ny) SETERRQ(1,"VecScatterCreate:Local scatter sizes don't match");
      ierr = StoPScatterCreate(nx,idx,ny,idy,yin,ctx); CHKERRQ(ierr);
      ISRestoreIndices(ix,&idx); ISRestoreIndices(iy,&idy);
      *newctx = ctx;
      return 0;
    }
  }
  if (xin->type == VECMPI && yin->type == VECMPI) {
    /* no special cases for now */
    int ierr,nx,ny,*idx,*idy;
    ISGetLocalSize(ix,&nx); ISGetIndices(ix,&idx);
    ISGetLocalSize(iy,&ny); ISGetIndices(iy,&idy);
    if (nx != ny) SETERRQ(1,"VecScatterCreate:Local scatter sizes don't match");
    ierr = PtoPScatterCreate(nx,idx,ny,idy,xin,yin,ctx); CHKERRQ(ierr);
    ISRestoreIndices(ix,&idx); ISRestoreIndices(iy,&idy);
    *newctx = ctx;
    return 0;
  }
  SETERRQ(1,"VecScatterCreate:Cannot generate such Scatter Context yet");
}

/* ------------------------------------------------------------------*/
/*@
   VecScatterBegin - Begins scattering from one vector to
   another. Complete the scattering phase with VecScatterEnd().
   This scatter is far more general than the conventional
   scatter, since it can be a gather or a scatter or a combination,
   depending on the indices ix and iy.  If x is a parallel vector and y
   is sequential, VecScatterBegin() can serve to gather values to a
   single processor.  Similarly, if y is parallel and x sequential, the
   routine can scatter from one processor to many processors.

   Input Parameters:
.  x - vector to scatter from
.  y - vector to scatter to
.  addv - either ADD_VALUES or INSERT_VALUES, depending whether values are
   added or set
.  mode - the scattering mode, usually SCATTER_ALL.  The available modes are:
$    SCATTER_ALL, SCATTER_UP, SCATTER_DOWN, 
$    SCATTER_REVERSE, SCATTER_ALL_REVERSE
.  inctx - scatter context generated by VecScatterCreate()

   Output Parameter:
.  y - vector to scatter to 

   Notes:
   y[iy[i]] = x[ix[i]], for i=0,...,ni-1

.keywords: vector, scatter, gather, begin

.seealso: VecScatterCreate(), VecScatterEnd()
@*/
int VecScatterBegin(Vec x,Vec y,InsertMode addv,ScatterMode mode,VecScatter inctx)
{
  int ierr;
  PETSCVALIDHEADERSPECIFIC(x,VEC_COOKIE); PETSCVALIDHEADERSPECIFIC(y,VEC_COOKIE);
  PETSCVALIDHEADERSPECIFIC(inctx,VEC_SCATTER_COOKIE);
  if (inctx->inuse) SETERRQ(1,"VecScatterBegin: Scatter ctx already in use");
  PLogEventBegin(VEC_ScatterBegin,inctx,x,y,0);
  ierr = (*inctx->scatterbegin)(x,y,addv,mode,inctx); CHKERRQ(ierr);
  PLogEventEnd(VEC_ScatterBegin,inctx,x,y,0);
  return 0;
}

/* --------------------------------------------------------------------*/
/*@
   VecScatterEnd - Ends scattering from one vector to another.
   Call after first calling VecScatterBegin().

   Input Parameters:
.  x - vector to scatter from
.  y - vector to scatter to 
.  addv - either ADD_VALUES or INSERT_VALUES, depending whether values are
   added or set
.  mode - the scattering mode, usually SCATTER_ALL.  The available modes are:
$    SCATTER_ALL, SCATTER_UP, SCATTER_DOWN, 
$    SCATTER_REVERSE, SCATTER_ALL_REVERSE
.  ctx - scatter context generated by VecScatterCreate()

   Output Parameters:
.  y - vector to scatter to 

   Notes:
   y[iy[i]] = x[ix[i]], for i=0,...,ni-1

.keywords: vector, scatter, gather, end

.seealso: VecScatterBegin(), VecScatterCreate()
@*/
int VecScatterEnd(Vec x,Vec y,InsertMode addv,ScatterMode mode, VecScatter ctx)
{
  int ierr;
  PETSCVALIDHEADERSPECIFIC(x,VEC_COOKIE); PETSCVALIDHEADERSPECIFIC(y,VEC_COOKIE);
  PETSCVALIDHEADERSPECIFIC(ctx,VEC_SCATTER_COOKIE);
  if (!(ctx)->scatterend) return 0;
  ctx->inuse = 0;
  PLogEventBegin(VEC_ScatterEnd,ctx,x,y,0);
  ierr = (*(ctx)->scatterend)(x,y,addv,mode,ctx); CHKERRQ(ierr);
  PLogEventEnd(VEC_ScatterEnd,ctx,x,y,0);
  return 0;
}

/*@C
   VecScatterDestroy - Destroys a scatter context created by 
   VecScatterCreate().

   Input Parameter:
.  ctx - the scatter context

.keywords: vector, scatter, context, destroy

.seealso: VecScatterCreate()
@*/
int VecScatterDestroy( VecScatter ctx )
{
  return (*ctx->destroy)((PetscObject)ctx);
}

/*@C
   VecScatterCopy - Makes a copy of a scatter context.

   Input Parameter:
.  sctx - the scatter context

   Output Parameter:
.  ctx - the context copy

.keywords: vector, scatter, copy, context

.seealso: VecScatterCreate()
@*/
int VecScatterCopy( VecScatter sctx,VecScatter *ctx )
{
  if (!sctx->copy) SETERRQ(1,"VecScatterCopy: cannot copy this type");
  /* generate the Scatter context */
  PetscHeaderCreate(*ctx,_VecScatter,VEC_SCATTER_COOKIE,0,sctx->comm);
  PLogObjectCreate(*ctx);
  PLogObjectMemory(*ctx,sizeof(struct _VecScatter));
  return (*sctx->copy)(sctx,*ctx);
}


/* ------------------------------------------------------------------*/
/*@ 
   VecPipelineBegin - Begins a vector pipeline operation.  

   Input Parameters:
.  x - vector to scatter from
.  y - vector to scatter to 
.  inctx - is used to coordinate communication
.  addv - either ADD_VALUES or INSERT_VALUES, depending whether values are
          added or set
.  inctx - scatter context generated by VecScatterCreate()
.  mode - pipelining mode, either PIPELINE_UP or PIPELINE_DOWN

   Output Parameter:
.  y - vector to scatter to 

  Notes:
  y[iy[i]] = x[ix[i]], for i=0,...,ni-1

  Most application programmers should not need to use this routine.

.keywords: vector, pipeline, begin

.seealso: VecPipelineEnd(), VecScatterCreate()
@*/
int VecPipelineBegin(Vec x,Vec y,InsertMode addv,PipelineMode mode,VecScatter inctx)
{
  int size;
  PETSCVALIDHEADERSPECIFIC(x,VEC_COOKIE); PETSCVALIDHEADERSPECIFIC(y,VEC_COOKIE);
  PETSCVALIDHEADERSPECIFIC(inctx,VEC_SCATTER_COOKIE);
  MPI_Comm_size(inctx->comm,&size);
  if (size == 1) return 0;
  if (!inctx->pipelinebegin) SETERRQ(1,"VecPipelineBegin:No pipeline for this context");
  return (*(inctx)->pipelinebegin)(x,y,addv,mode,inctx);
}

/* --------------------------------------------------------------------*/
/*@
   VecPipelineEnd - Sends results to next processor in pipeline.  Call
   after calling VecPipelineBegin().

   Input Parameters:
.  x - vector to scatter from
.  y - vector to scatter to 
.  inctx - is used to coordinate communication
.  addv - either ADD_VALUES or INSERT_VALUES, depending whether values are
          added or set
.  ctx - scatter context generated by VecScatterCreate()
.  mode - pipelining mode, either PIPELINE_UP or PIPELINE_DOWN

   Output Parameters:
.  y - vector to scatter to 

   Notes:
   y[iy[i]] = x[ix[i]], for i=0,...,ni-1

  Most application programmers should not need to use this routine.

.keywords: vector, pipeline, end

.seealso: VecPipelineBegin(), VecScatterCreate()
@*/
int VecPipelineEnd(Vec x,Vec y,InsertMode addv,PipelineMode mode,VecScatter ctx)
{
  int size;
  PETSCVALIDHEADERSPECIFIC(x,VEC_COOKIE); PETSCVALIDHEADERSPECIFIC(y,VEC_COOKIE);
  PETSCVALIDHEADERSPECIFIC(ctx,VEC_SCATTER_COOKIE);
  MPI_Comm_size(ctx->comm,&size);
  if (size == 1) return 0;
  if ((ctx)->pipelineend) return (*(ctx)->pipelineend)(x,y,addv,mode,ctx);
  else return 0;
}

/*@
     VecScatterView - View a vector scatter context.

  Input Parameters:
.  - ctx - the scatter context
.  - viewer - the viewer where one wishes to display the context

@*/
int VecScatterView(VecScatter ctx, Viewer viewer)
{
  PETSCVALIDHEADERSPECIFIC(ctx,VEC_SCATTER_COOKIE);
  if (ctx->view) return (*ctx->view)((PetscObject)ctx,viewer);
  else return 0;
}

