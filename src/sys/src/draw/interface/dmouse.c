#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: dmouse.c,v 1.9 1997/07/09 20:57:34 balay Exp bsmith $";
#endif
/*
       Provides the calling sequences for all the basic Draw routines.
*/
#include "src/draw/drawimpl.h"  /*I "draw.h" I*/

#undef __FUNC__  
#define __FUNC__ "DrawGetMouseButton" /* ADIC Ignore */
/*@
       DrawGetMouseButton - Returns location of mouse and which button was
            pressed. Waits for button to be pressed.

  Input Parameter:
.   draw - the window to be used

  Output Parameters:
.   button - one of BUTTON_LEFT, BUTTON_CENTER, BUTTON_RIGHT
.   x_user, y_user - user coordinates of location (user may pass in 0).
.   x_phys, y_phys - window coordinates (user may pass in 0).

  Notes:
    Only processor 0 of the communicator used to create the Draw may call this routine.

.seealso: DrawSyncGetMouseButton()
@*/
int DrawGetMouseButton(Draw draw,DrawButton *button,double* x_user,double *y_user,
                       double *x_phys,double *y_phys)
{
  PetscValidHeaderSpecific(draw,DRAW_COOKIE);
  *button = BUTTON_NONE;
  if (draw->type == DRAW_NULLWINDOW) return 0;
  if (!draw->ops.getmousebutton) return 0;
  return (*draw->ops.getmousebutton)(draw,button,x_user,y_user,x_phys,y_phys);
}

#undef __FUNC__  
#define __FUNC__ "DrawSyncGetMouseButton"
/*@
       DrawSyncGetMouseButton - Returns location of mouse and which button was
            pressed. Waits for button to be pressed.

  Input Parameter:
.   draw - the window to be used

  Output Parameters:
.   button - one of BUTTON_LEFT, BUTTON_CENTER, BUTTON_RIGHT
.   x_user, y_user - user coordinates of location (user may pass in 0).
.   x_phys, y_phys - window coordinates (user may pass in 0).

  Notes:
    All processors that share the drawable must call this routine.

.seealso: DrawGetMouseButton()
@*/
int DrawSyncGetMouseButton(Draw draw,DrawButton *button,double* x_user,double *y_user,
                       double *x_phys,double *y_phys)
{
  double bcast[4];
  int    ierr,rank;
  PetscValidHeaderSpecific(draw,DRAW_COOKIE);

  MPI_Comm_rank(draw->comm,&rank);
  if (!rank) {
    ierr = DrawGetMouseButton(draw,button,x_user,y_user,x_phys,y_phys); CHKERRQ(ierr);
  }
  if (button) {
     MPI_Bcast(button,1,MPI_INT,0,draw->comm);  
  }
  if (x_user) bcast[0] = *x_user;
  if (y_user) bcast[1] = *y_user;
  if (x_phys) bcast[2] = *x_phys;
  if (y_phys) bcast[3] = *y_phys;
  MPI_Bcast(bcast,4,MPI_DOUBLE,0,draw->comm);  
  if (x_user) *x_user = bcast[0];
  if (y_user) *y_user = bcast[1];
  if (x_phys) *x_phys = bcast[2];
  if (y_phys) *y_phys = bcast[3];
  return 0;
}






