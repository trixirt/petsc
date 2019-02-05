#include <../src/tao/leastsquares/impls/brgn/brgn.h>

#define BRGN_user               0
#define BRGN_l2prox             1
#define BRGN_l1dict             2
#define BRGNRegTypes            3

static const char *BRGN_Table[64] = {"user","l2prox","l1dict"};

static PetscErrorCode GNHessianProd(Mat H,Vec in,Vec out)
{
  TAO_BRGN              *gn;
  PetscErrorCode        ierr;
  
  PetscFunctionBegin;    
  ierr = MatShellGetContext(H,&gn);CHKERRQ(ierr);
  ierr = MatMult(gn->subsolver->ls_jac,in,gn->r_work);CHKERRQ(ierr);
  ierr = MatMultTranspose(gn->subsolver->ls_jac,gn->r_work,out);CHKERRQ(ierr);
  switch (gn->reg_type) {
  case BRGN_user:
    ierr = MatMult(gn->Hreg,in,gn->x_work);CHKERRQ(ierr);
    ierr = VecAXPY(out,gn->lambda,gn->x_work);CHKERRQ(ierr);
    break;
  case BRGN_l2prox:
    ierr = VecAXPY(out,gn->lambda,in);CHKERRQ(ierr);
    break;
  case BRGN_l1dict:
    /* out = out + lambda*D'*(diag.*(D*in)) */
    if (gn->D) {
      ierr = MatMult(gn->D,in,gn->y);CHKERRQ(ierr);/* y = D*in */
    } else {
      ierr = VecCopy(in,gn->y);CHKERRQ(ierr);
    }
    ierr = VecPointwiseMult(gn->y_work,gn->diag,gn->y);CHKERRQ(ierr);   /* y_work = diag.*(D*in), where diag = epsilon^2 ./ sqrt(x.^2+epsilon^2).^3 */
    if (gn->D) {
      ierr = MatMultTranspose(gn->D,gn->y_work,gn->x_work);CHKERRQ(ierr); /* x_work = D'*(diag.*(D*in)) */
    } else {
      ierr = VecCopy(gn->y_work,gn->x_work);CHKERRQ(ierr);
    }
    ierr = VecAXPY(out,gn->lambda,gn->x_work);CHKERRQ(ierr);
    break;
  }

  PetscFunctionReturn(0);
}

static PetscErrorCode GNObjectiveGradientEval(Tao tao,Vec X,PetscReal *fcn,Vec G,void *ptr)
{
  TAO_BRGN              *gn = (TAO_BRGN *)ptr;
  PetscInt              K;                    /* dimension of D*X */
  PetscScalar           yESum;
  PetscErrorCode        ierr;
  PetscReal             f_reg;
  
  PetscFunctionBegin;
    /* compute objective *fcn*/
  /* compute first term 0.5*||ls_res||_2^2 */
  ierr = TaoComputeResidual(tao,X,tao->ls_res);CHKERRQ(ierr);
  ierr = VecDot(tao->ls_res,tao->ls_res,fcn);CHKERRQ(ierr);
  *fcn *= 0.5;
  /* compute gradient G */
  ierr = TaoComputeResidualJacobian(tao,X,tao->ls_jac,tao->ls_jac_pre);CHKERRQ(ierr);
  ierr = MatMultTranspose(tao->ls_jac,tao->ls_res,G);CHKERRQ(ierr);
  /* add the regularization contribution */
  switch (gn->reg_type) {
  case BRGN_user:
    ierr = (*gn->regularizerobjandgrad)(tao,X,&f_reg,gn->x_work,gn->reg_obj_ctx);CHKERRQ(ierr);
    *fcn += gn->lambda*f_reg;
    ierr = VecAXPY(G,gn->lambda,gn->x_work);CHKERRQ(ierr);
    break;
  case BRGN_l2prox:
    /* compute f = f + lambda*0.5*(xk - xkm1)^T(xk - xkm1) */
    ierr = VecAXPBYPCZ(gn->x_work,1.0,-1.0,0.0,X,gn->x_old);CHKERRQ(ierr); /*TODO: no need to use VecAXPBYPCZ for x - xkm1 */
    ierr = VecDot(gn->x_work,gn->x_work,&f_reg);CHKERRQ(ierr);
    *fcn += gn->lambda*0.5*f_reg;
    /* compute G = G + lambda*(xk - xkm1) */
    ierr = VecAXPBYPCZ(G,gn->lambda,-gn->lambda,1.0,X,gn->x_old);CHKERRQ(ierr);
    break;
  case BRGN_l1dict:
    /* compute f = f + lambda*sum(sqrt(y.^2+epsilon^2) - epsilon), where y = D*x*/
    if (gn->D) {
      ierr = MatMult(gn->D,X,gn->y);CHKERRQ(ierr);/* y = D*x */
    } else {
      ierr = VecCopy(X,gn->y);CHKERRQ(ierr);
    }
    ierr = VecPointwiseMult(gn->y_work,gn->y,gn->y);CHKERRQ(ierr);
    ierr = VecShift(gn->y_work,gn->epsilon*gn->epsilon);CHKERRQ(ierr);
    ierr = VecSqrtAbs(gn->y_work);CHKERRQ(ierr);  /* gn->y_work = sqrt(y.^2+epsilon^2) */ 
    ierr = VecSum(gn->y_work,&yESum);CHKERRQ(ierr);CHKERRQ(ierr);
    ierr = VecGetSize(gn->y,&K);CHKERRQ(ierr);
    *fcn += gn->lambda*(yESum - K*gn->epsilon);
    /* compute G = G + lambda*D'*(y./sqrt(y.^2+epsilon^2)),where y = D*x */  
    ierr = VecPointwiseDivide(gn->y_work,gn->y,gn->y_work);CHKERRQ(ierr); /* reuse y_work = y./sqrt(y.^2+epsilon^2) */
    if (gn->D) {
      ierr = MatMultTranspose(gn->D,gn->y_work,gn->x_work);CHKERRQ(ierr);
    } else {
      ierr = VecCopy(gn->y_work,gn->x_work);CHKERRQ(ierr);
    }
    ierr = VecAXPY(G,gn->lambda,gn->x_work);CHKERRQ(ierr);
    break;
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode GNComputeHessian(Tao tao,Vec X,Mat H,Mat Hpre,void *ptr)
{ 
  TAO_BRGN              *gn = (TAO_BRGN *)ptr;
  PetscErrorCode ierr;
  
  PetscFunctionBegin;
  ierr = TaoComputeResidualJacobian(tao,X,tao->ls_jac,tao->ls_jac_pre);CHKERRQ(ierr);

  switch (gn->reg_type) {
  case BRGN_user:
    ierr = (*gn->regularizerhessian)(tao,X,gn->Hreg,gn->reg_hess_ctx);CHKERRQ(ierr);
    break;
  case BRGN_l2prox:
    break;
  case BRGN_l1dict:
    /* calculate and store diagonal matrix as a vector: diag = epsilon^2 ./ sqrt(x.^2+epsilon^2).^3* --> diag = epsilon^2 ./ sqrt(y.^2+epsilon^2).^3,where y = D*x */  
    if (gn->D) {
      ierr = MatMult(gn->D,X,gn->y);CHKERRQ(ierr);/* y = D*x */
    } else {
      ierr = VecCopy(X,gn->y);CHKERRQ(ierr);
    }
    ierr = VecPointwiseMult(gn->y_work,gn->y,gn->y);CHKERRQ(ierr);
    ierr = VecShift(gn->y_work,gn->epsilon*gn->epsilon);CHKERRQ(ierr);
    ierr = VecCopy(gn->y_work,gn->diag);CHKERRQ(ierr);                  /* gn->diag = y.^2+epsilon^2 */
    ierr = VecSqrtAbs(gn->y_work);CHKERRQ(ierr);                        /* gn->y_work = sqrt(y.^2+epsilon^2) */ 
    ierr = VecPointwiseMult(gn->diag,gn->y_work,gn->diag);CHKERRQ(ierr);/* gn->diag = sqrt(y.^2+epsilon^2).^3 */
    ierr = VecReciprocal(gn->diag);CHKERRQ(ierr);
    ierr = VecScale(gn->diag,gn->epsilon*gn->epsilon);CHKERRQ(ierr);
    break;
  }

  PetscFunctionReturn(0);
}

static PetscErrorCode GNHookFunction(Tao tao,PetscInt iter)
{
  TAO_BRGN              *gn = (TAO_BRGN *)tao->user_update;
  PetscErrorCode        ierr;
  
  PetscFunctionBegin;
  /* Update basic tao information from the subsolver */
  gn->parent->nfuncs = tao->nfuncs;
  gn->parent->ngrads = tao->ngrads;
  gn->parent->nfuncgrads = tao->nfuncgrads;
  gn->parent->nhess = tao->nhess;
  gn->parent->niter = tao->niter;
  gn->parent->ksp_its = tao->ksp_its;
  gn->parent->ksp_tot_its = tao->ksp_tot_its;
  ierr = TaoGetConvergedReason(tao,&gn->parent->reason);CHKERRQ(ierr);
  /* Update the solution vectors */
  if (iter == 0) {
    ierr = VecSet(gn->x_old,0.0);CHKERRQ(ierr);
  } else {
    ierr = VecCopy(tao->solution,gn->x_old);CHKERRQ(ierr);
    ierr = VecCopy(tao->solution,gn->parent->solution);CHKERRQ(ierr);
  }
  /* Update the gradient */
  ierr = VecCopy(tao->gradient,gn->parent->gradient);CHKERRQ(ierr);
  /* Call general purpose update function */
  if (gn->parent->ops->update) {
    ierr = (*gn->parent->ops->update)(gn->parent,gn->parent->niter);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode TaoSolve_BRGN(Tao tao)
{
  TAO_BRGN              *gn = (TAO_BRGN *)tao->data;
  PetscErrorCode        ierr;

  PetscFunctionBegin;
  ierr = TaoSolve(gn->subsolver);CHKERRQ(ierr);
  /* Update basic tao information from the subsolver */
  tao->nfuncs = gn->subsolver->nfuncs;
  tao->ngrads = gn->subsolver->ngrads;
  tao->nfuncgrads = gn->subsolver->nfuncgrads;
  tao->nhess = gn->subsolver->nhess;
  tao->niter = gn->subsolver->niter;
  tao->ksp_its = gn->subsolver->ksp_its;
  tao->ksp_tot_its = gn->subsolver->ksp_tot_its;
  ierr = TaoGetConvergedReason(gn->subsolver,&tao->reason);CHKERRQ(ierr);
  /* Update vectors */
  ierr = VecCopy(gn->subsolver->solution,tao->solution);CHKERRQ(ierr);
  ierr = VecCopy(gn->subsolver->gradient,tao->gradient);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static PetscErrorCode TaoSetFromOptions_BRGN(PetscOptionItems *PetscOptionsObject,Tao tao)
{
  TAO_BRGN              *gn = (TAO_BRGN *)tao->data;
  PetscErrorCode        ierr;

  PetscFunctionBegin;
  ierr = PetscOptionsHead(PetscOptionsObject,"least-squares problems with regularizer: ||f(x)||^2 + lambda*g(x), g(x) = ||xk-xkm1||^2 or ||Dx||_1 or user defined function.");CHKERRQ(ierr);
  ierr = PetscOptionsReal("-tao_brgn_lambda","regularizer weight (default 1e-4)","",gn->lambda,&gn->lambda,NULL);CHKERRQ(ierr);
  ierr = PetscOptionsReal("-tao_brgn_epsilon","L1-norm smooth approximation parameter: ||x||_1 = sum(sqrt(x.^2+epsilon^2)-epsilon) (default 1e-6)","",gn->epsilon,&gn->epsilon,NULL);CHKERRQ(ierr);
  ierr = PetscOptionsEList("-tao_brgn_reg_type","regularization type", "",BRGN_Table,BRGNRegTypes,BRGN_Table[gn->reg_type],&gn->reg_type,NULL);CHKERRQ(ierr);
  ierr = PetscOptionsTail();CHKERRQ(ierr);
  ierr = TaoSetFromOptions(gn->subsolver);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static PetscErrorCode TaoView_BRGN(Tao tao,PetscViewer viewer)
{
  TAO_BRGN              *gn = (TAO_BRGN *)tao->data;
  PetscErrorCode        ierr;

  PetscFunctionBegin;
  ierr = PetscViewerASCIIPushTab(viewer);CHKERRQ(ierr);
  ierr = TaoView(gn->subsolver,viewer);CHKERRQ(ierr);
  ierr = PetscViewerASCIIPopTab(viewer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static PetscErrorCode TaoSetUp_BRGN(Tao tao)
{
  TAO_BRGN              *gn = (TAO_BRGN *)tao->data;
  PetscErrorCode        ierr;
  PetscBool             is_bnls,is_bntr,is_bntl;
  PetscInt              i,n,N,K; /* dict has size K*N*/

  PetscFunctionBegin;
  if (!tao->ls_res) SETERRQ(PetscObjectComm((PetscObject)tao),PETSC_ERR_ORDER,"TaoSetResidualRoutine() must be called before setup!");
  ierr = PetscObjectTypeCompare((PetscObject)gn->subsolver,TAOBNLS,&is_bnls);CHKERRQ(ierr);
  ierr = PetscObjectTypeCompare((PetscObject)gn->subsolver,TAOBNTR,&is_bntr);CHKERRQ(ierr);
  ierr = PetscObjectTypeCompare((PetscObject)gn->subsolver,TAOBNTL,&is_bntl);CHKERRQ(ierr);
  if ((is_bnls || is_bntr || is_bntl) && !tao->ls_jac) SETERRQ(PetscObjectComm((PetscObject)tao),PETSC_ERR_ORDER,"TaoSetResidualJacobianRoutine() must be called before setup!");
  if (!tao->gradient) {
    ierr = VecDuplicate(tao->solution,&tao->gradient);CHKERRQ(ierr);
  }
  if (!gn->x_work) {
    ierr = VecDuplicate(tao->solution,&gn->x_work);CHKERRQ(ierr);
  }
  if (!gn->r_work) {
    ierr = VecDuplicate(tao->ls_res,&gn->r_work);CHKERRQ(ierr);
  }
  if (!gn->x_old) {
    ierr = VecDuplicate(tao->solution,&gn->x_old);CHKERRQ(ierr);
    ierr = VecSet(gn->x_old,0.0);CHKERRQ(ierr);
  }
  
  /*ierr = VecGetSize(tao->solution,&N);CHKERRQ(ierr);*/  
  if (BRGN_l1dict == gn->reg_type) {
    if (gn->D) {
      /* XH: debug: check matrix */
#if 0
      ierr = PetscPrintf(PETSC_COMM_SELF,"-------- Check D matrix: -------- \n"); CHKERRQ(ierr);
      ierr = MatView(gn->D,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
#endif
      ierr = MatGetSize(gn->D,&K,&N);CHKERRQ(ierr); /* Shell matrices still must have sizes defined. K = N for identity matrix, K=N-1 or N for gradient matrix */
    } else {
      ierr = VecGetSize(tao->solution,&K);CHKERRQ(ierr); /* If user does not setup dict matrix, use identiy matrix, K=N */
    }
    if (!gn->y) {    
      ierr = VecCreate(PETSC_COMM_SELF,&gn->y);CHKERRQ(ierr);
      ierr = VecSetSizes(gn->y,PETSC_DECIDE,K);CHKERRQ(ierr);
      ierr = VecSetFromOptions(gn->y);CHKERRQ(ierr);
      ierr = VecSet(gn->y,0.0);CHKERRQ(ierr);

    }
    if (!gn->y_work) {
      ierr = VecDuplicate(gn->y,&gn->y_work);CHKERRQ(ierr);
    }
    if (!gn->diag) {
      ierr = VecDuplicate(gn->y,&gn->diag);CHKERRQ(ierr);
      ierr = VecSet(gn->diag,0.0);CHKERRQ(ierr);
    }
  }


  if (!tao->setupcalled) {
    /* Hessian setup */
    ierr = VecGetLocalSize(tao->solution,&n);CHKERRQ(ierr);
    ierr = VecGetSize(tao->solution,&N);CHKERRQ(ierr);
    ierr = MatSetSizes(gn->H,n,n,N,N);CHKERRQ(ierr);
    ierr = MatSetType(gn->H,MATSHELL);CHKERRQ(ierr);
    ierr = MatSetUp(gn->H);CHKERRQ(ierr);
    ierr = MatShellSetOperation(gn->H,MATOP_MULT,(void (*)(void))GNHessianProd);CHKERRQ(ierr);
    ierr = MatShellSetContext(gn->H,(void*)gn);CHKERRQ(ierr);
    /* Subsolver setup,include initial vector and dicttionary D */
    ierr = TaoSetUpdate(gn->subsolver,GNHookFunction,(void*)gn);CHKERRQ(ierr);
    ierr = TaoSetInitialVector(gn->subsolver,tao->solution);CHKERRQ(ierr);
    if (tao->bounded) {
      ierr = TaoSetVariableBounds(gn->subsolver,tao->XL,tao->XU);CHKERRQ(ierr);
    }
    ierr = TaoSetResidualRoutine(gn->subsolver,tao->ls_res,tao->ops->computeresidual,tao->user_lsresP);CHKERRQ(ierr);
    ierr = TaoSetJacobianResidualRoutine(gn->subsolver,tao->ls_jac,tao->ls_jac,tao->ops->computeresidualjacobian,tao->user_lsjacP);CHKERRQ(ierr);
    ierr = TaoSetObjectiveAndGradientRoutine(gn->subsolver,GNObjectiveGradientEval,(void*)gn);CHKERRQ(ierr);
    ierr = TaoSetHessianRoutine(gn->subsolver,gn->H,gn->H,GNComputeHessian,(void*)gn);CHKERRQ(ierr);
    /* Propagate some options down */
    ierr = TaoSetTolerances(gn->subsolver,tao->gatol,tao->grtol,tao->gttol);CHKERRQ(ierr);
    ierr = TaoSetMaximumIterations(gn->subsolver,tao->max_it);CHKERRQ(ierr);
    ierr = TaoSetMaximumFunctionEvaluations(gn->subsolver,tao->max_funcs);CHKERRQ(ierr);
    for (i=0; i<tao->numbermonitors; ++i) {
      ierr = TaoSetMonitor(gn->subsolver,tao->monitor[i],tao->monitorcontext[i],tao->monitordestroy[i]);CHKERRQ(ierr);
      ierr = PetscObjectReference((PetscObject)(tao->monitorcontext[i]));CHKERRQ(ierr);
    }
    ierr = TaoSetUp(gn->subsolver);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode TaoDestroy_BRGN(Tao tao)
{
  TAO_BRGN              *gn = (TAO_BRGN *)tao->data;
  PetscErrorCode        ierr;

  PetscFunctionBegin;
  if (tao->setupcalled) {
    ierr = VecDestroy(&tao->gradient);CHKERRQ(ierr);
    ierr = VecDestroy(&gn->x_work);CHKERRQ(ierr);
    ierr = VecDestroy(&gn->r_work);CHKERRQ(ierr);
    ierr = VecDestroy(&gn->x_old);CHKERRQ(ierr);
    ierr = VecDestroy(&gn->diag);CHKERRQ(ierr);
    ierr = VecDestroy(&gn->y);CHKERRQ(ierr);
    ierr = VecDestroy(&gn->y_work);CHKERRQ(ierr);
  }
  ierr = MatDestroy(&gn->H);CHKERRQ(ierr);
  ierr = MatDestroy(&gn->D);CHKERRQ(ierr);
  ierr = TaoDestroy(&gn->subsolver);CHKERRQ(ierr);
  gn->parent = NULL;
  ierr = PetscFree(tao->data);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*MC
  TAOBRGN - Bounded Regularized Gauss-Newton method for solving nonlinear least-squares 
            problems with bound constraints. This algorithm is a thin wrapper around TAOBNTL 
            that constructs the Guass-Newton problem with the user-provided least-squares 
            residual and Jacobian. The problem is regularized with an L2-norm proximal point 
            term.

  Options Database Keys:
  + -tao_bqnk_max_cg_its - maximum number of bounded conjugate-gradient iterations taken in each Newton loop
  . -tao_bqnk_init_type - trust radius initialization method ("constant", "direction", "interpolation")
  . -tao_bqnk_update_type - trust radius update method ("step", "direction", "interpolation")
  - -tao_bqnk_as_type - active-set estimation method ("none", "bertsekas")

  Level: beginner
M*/
PETSC_EXTERN PetscErrorCode TaoCreate_BRGN(Tao tao)
{
  TAO_BRGN       *gn;
  PetscErrorCode ierr;
  
  PetscFunctionBegin;
  ierr = PetscNewLog(tao,&gn);CHKERRQ(ierr);
  
  tao->ops->destroy = TaoDestroy_BRGN;
  tao->ops->setup = TaoSetUp_BRGN;
  tao->ops->setfromoptions = TaoSetFromOptions_BRGN;
  tao->ops->view = TaoView_BRGN;
  tao->ops->solve = TaoSolve_BRGN;
  
  tao->data = (void*)gn;
  gn->lambda = 1e-4;
  gn->epsilon = 1e-6;
  gn->parent = tao;
  
  ierr = MatCreate(PetscObjectComm((PetscObject)tao),&gn->H);CHKERRQ(ierr);
  ierr = MatSetOptionsPrefix(gn->H,"tao_brgn_hessian_");CHKERRQ(ierr);
  
  ierr = TaoCreate(PetscObjectComm((PetscObject)tao),&gn->subsolver);CHKERRQ(ierr);
  ierr = TaoSetType(gn->subsolver,TAOBNLS);CHKERRQ(ierr);
  ierr = TaoSetOptionsPrefix(gn->subsolver,"tao_brgn_subsolver_");CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/*@C
  TaoBRGNGetSubsolver - Get the pointer to the subsolver inside BRGN

  Collective on Tao

  Level: developer
  
  Input Parameters:
+  tao - the Tao solver context
-  subsolver - the Tao sub-solver context
@*/
PetscErrorCode TaoBRGNGetSubsolver(Tao tao,Tao *subsolver)
{
  TAO_BRGN       *gn = (TAO_BRGN *)tao->data;
  
  PetscFunctionBegin;
  *subsolver = gn->subsolver;
  PetscFunctionReturn(0);
}

/*@C
  TaoBRGNSetL1RegularizerWeight - Set the L1-norm regularizer weight for the Gauss-Newton least-squares algorithm

  Collective on Tao

  Level: developer
  
  Input Parameters:
+  tao - the Tao solver context
-  lambda - L1-norm regularizer weight
@*/
PetscErrorCode TaoBRGNSetRegularizerWeight(Tao tao,PetscReal lambda)
{
  TAO_BRGN       *gn = (TAO_BRGN *)tao->data;
  
  /* Initialize lambda here */

  PetscFunctionBegin;
  gn->lambda = lambda;
  PetscFunctionReturn(0);
}

/*@C
  TaoBRGNSetL1SmoothEpsilon - Set the L1-norm smooth approximation parameter for L1-regularized least-squares algorithm

  Collective on Tao

  Level: developer
  
  Input Parameters:
+  tao - the Tao solver context
-  epsilon - L1-norm smooth approximation parameter
@*/
PetscErrorCode TaoBRGNSetL1SmoothEpsilon(Tao tao,PetscReal epsilon)
{
  TAO_BRGN       *gn = (TAO_BRGN *)tao->data;
  
  /* Initialize epsilon here */

  PetscFunctionBegin;
  gn->epsilon = epsilon;
  PetscFunctionReturn(0);
}

/*@C
   TaoBRGNSetDictionaryMatrix - bind the dictionary matrix from user application context to gn->D, for compressed sensing (with least-squares problem)

   Input Parameters:
+  tao  - the Tao context
.  dict - the user specified dictionary matrix

    Level: developer
@*/
PetscErrorCode TaoBRGNSetDictionaryMatrix(Tao tao,Mat dict)  
{
  TAO_BRGN       *gn = (TAO_BRGN *)tao->data;
  PetscErrorCode ierr;
  PetscFunctionBegin;
  PetscValidHeaderSpecific(tao,TAO_CLASSID,1);
  if (dict) {
    PetscValidHeaderSpecific(dict,MAT_CLASSID,2);
    PetscCheckSameComm(tao,1,dict,2);
    ierr = PetscObjectReference((PetscObject)dict);CHKERRQ(ierr);
  }
  ierr = MatDestroy(&gn->D);CHKERRQ(ierr);
  gn->D = dict;  /* We allow to set a null dictionary, which means we just use default identity matrix? */
  PetscFunctionReturn(0);
}

/*@C
@*/
PetscErrorCode TaoBRGNSetRegularizerObjectiveAndGradientRoutine(Tao tao,PetscErrorCode (*func)(Tao,Vec,PetscReal *,Vec,void*),void *ctx)
{
  TAO_BRGN       *gn = (TAO_BRGN *)tao->data;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tao,TAO_CLASSID,1);
  if (ctx) {
    gn->reg_obj_ctx = ctx;
  }
  if (func) {
    gn->regularizerobjandgrad = func;
  }
  PetscFunctionReturn(0);
}

/*@C
@*/
PetscErrorCode TaoBRGNSetRegularizerHessianRoutine(Tao tao,Mat Hreg,PetscErrorCode (*func)(Tao,Vec,Mat,void*),void *ctx)
{
  TAO_BRGN       *gn = (TAO_BRGN *)tao->data;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  PetscValidHeaderSpecific(tao,TAO_CLASSID,1);
  if (Hreg) {
    PetscValidHeaderSpecific(Hreg,MAT_CLASSID,2);
    PetscCheckSameComm(tao,1,Hreg,2);
  } else {
    SETERRQ(PetscObjectComm((PetscObject)tao),PETSC_ERR_ARG_WRONG,"NULL Hessian detected! User must provide valid Hessian for the regularizer.");
  }
  if (ctx) {
    gn->reg_hess_ctx = ctx;
  }
  if (func) {
    gn->regularizerhessian = func;
  }
  if (Hreg) {
    ierr = PetscObjectReference((PetscObject)Hreg);CHKERRQ(ierr);
    ierr = MatDestroy(&gn->Hreg);CHKERRQ(ierr);
    gn->Hreg = Hreg;
  }
  PetscFunctionReturn(0);
}