#ifdef PETSC_RCS_HEADER
static char vcid[] = "$Id: send.c,v 1.79 1998/12/17 22:12:07 bsmith Exp balay $";
#endif

#include "petsc.h"
#include "sys.h"

#if defined(NEED_UTYPE_TYPEDEFS)
/* Some systems have inconsistent include files that use but don't
   ensure that the following definitions are made */
typedef unsigned char   u_char;
typedef unsigned short  u_short;
typedef unsigned short  ushort;
typedef unsigned int    u_int;
typedef unsigned long   u_long;
#endif

#if defined(HAVE_STDLIB_H)
#include <stdlib.h>
#endif
#include <sys/types.h>
#if defined(PARCH_rs6000)
#include <ctype.h>
#endif
#if defined(PARCH_alpha)
#include <machine/endian.h>
#endif
#if !defined(PARCH_win32)
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#if defined(HAVE_STROPTS_H)
#include <stropts.h>
#endif

#include "src/viewer/impls/matlab/matlab.h"
#include "pinclude/petscfix.h"

/*
     Many machines don't prototype many of the socket functions?
*/
#if defined(PARCH_sun4) || defined(PARCH_rs6000) || defined(PARCH_freebsd) \
    || defined(PARCH_hpux) || defined(PARCH_alpha) || defined(PARCH_solaris) \
    || defined(PARCH_linux)
EXTERN_C_BEGIN
#if !defined(PARCH_rs6000) && !defined(PARCH_freebsd) && !defined(PARCH_hpux) \
    && !defined(PARCH_alpha) && !defined(PARCH_solaris) && \
    !defined(PARCH_linux)
/*
    Some versions of the Gnu g++ compiler on the IBM RS6000 require the 
  prototype below.
*/
extern int setsockopt(int,int,int,char*,int);
#endif
extern int close(int);
#if !defined(PARCH_freebsd) && !defined(PARCH_linux) && !defined(PARCH_solaris)
extern int socket(int,int,int);
#if !defined(PARCH_hpux) && !defined(PARCH_alpha) && !defined(PARCH_solaris)
/*
    For some IBM rs6000 machines running AIX 3.2 uncomment the prototype 
   below for connect()
*/
/*
#if defined(PARCH_rs6000) && !defined(__cplusplus)
extern int connect(int,const struct sockaddr *,size_t);
#endif
*/

#if !defined(PARCH_rs6000)
extern int connect(int,struct sockaddr *,int);
#endif
#endif
#endif
#if !defined(PARCH_alpha)
/* 
   Some IBM rs6000 machines have the sleep prototype already declared
   in unistd.h, so just remove it below.
 */
#if defined(PARCH_rs6000)
extern unsigned int sleep(unsigned int);
#else
extern int sleep(unsigned);
#endif
#endif
EXTERN_C_END
#endif

#if (defined(PARCH_IRIX)  || defined(PARCH_IRIX64) || defined(PARCH_IRIX5)) && defined(__cplusplus)
extern "C" {
extern int sleep(unsigned);
extern int close(int);
};
#endif

#undef __FUNC__  
#define __FUNC__ "ViewerDestroy_Matlab"
static int ViewerDestroy_Matlab(Viewer viewer)
{
  Viewer_Matlab *vmatlab = (Viewer_Matlab *) viewer->data;

  PetscFunctionBegin;
  if (close(vmatlab->port)) {
    SETERRQ(PETSC_ERR_LIB,0,"System error closing socket");
  }
  PetscFree(vmatlab);
  PetscFunctionReturn(0);
}

/*--------------------------------------------------------------*/
#undef __FUNC__  
#define __FUNC__ "SOCKCall_Private"
int SOCKCall_Private(char *hostname,int portnum,int *t)
{
  struct sockaddr_in sa;
  struct hostent     *hp;
  int                s = 0,flag = 1;
  
  PetscFunctionBegin;
  if ( (hp=gethostbyname(hostname)) == NULL ) {
    perror("SEND: error gethostbyname: ");   
    fprintf(stderr,"hostname tried %s\n",hostname);
    SETERRQ(PETSC_ERR_LIB,0,"system error open connection");
  }
  PetscMemzero(&sa,sizeof(sa));
  PetscMemcpy(&sa.sin_addr,hp->h_addr,hp->h_length);
  sa.sin_family = hp->h_addrtype;
  sa.sin_port = htons((u_short) portnum);
  while (flag) {
    if ( (s=socket(hp->h_addrtype,SOCK_STREAM,0)) < 0 ) {
      perror("SEND: error socket");  SETERRQ(PETSC_ERR_LIB,0,"system error");
    }
    if ( connect(s,(struct sockaddr *)&sa,sizeof(sa)) < 0 ) {
       if ( errno == EADDRINUSE ) {
        fprintf(stderr,"SEND: address is in use\n");
      }
#if !defined(PARCH_win32_gnu)
       else if ( errno == EALREADY ) {
        fprintf(stderr,"SEND: socket is non-blocking \n");
      }
      else if ( errno == EISCONN ) {
        fprintf(stderr,"SEND: socket already connected\n"); 
        sleep((unsigned) 1);
      }
#endif
      else if ( errno == ECONNREFUSED ) {
        /* fprintf(stderr,"SEND: forcefully rejected\n"); */
        sleep((unsigned) 1);
      } else {
        perror(NULL); SETERRQ(PETSC_ERR_LIB,0,"system error");
      }
      flag = 1; close(s);
    } 
    else flag = 0;
  }
  *t = s;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "ViewerMatlabOpen"
/*@C
   ViewerMatlabOpen - Opens a connection to a Matlab server.

   Collective on MPI_Comm

   Input Parameters:
+  comm - the MPI communicator
.  machine - the machine the server is running on
-  port - the port to connect to, use PETSC_DEFAULT for the default

   Output Parameter:
.  lab - a context to use when communicating with the server

   Notes:
   Most users should employ the following commands to access the 
   Matlab viewers
$
$    ViewerMatlabOpen(MPI_Comm comm, char *machine,int port,Viewer &viewer)
$    MatView(Mat matrix,Viewer viewer)
$
$                or
$
$    ViewerMatlabOpen(MPI_Comm comm, char *machine,int port,Viewer &viewer)
$    VecView(Vec vector,Viewer viewer)

   Options Database Keys:
   For use with the default Matlab viewer, VIEWER_MATLAB_WORLD or if 
    PETSC_NULL is passed for machine or PETSC_DEFAULT is passed for port
$    -viewer_matlab_machine <machine>
$    -viewer_matlab_port <port>

   Environmental variables:
.   PETSC_VIEWER_MATLAB_PORT portnumber

.keywords: Viewer, Matlab, open

.seealso: MatView(), VecView()
@*/
int ViewerMatlabOpen(MPI_Comm comm,const char machine[],int port,Viewer *lab)
{
  Viewer        v;
  int           t,rank,ierr,flag;
  char          mach[256];
  PetscTruth    tflag;
  Viewer_Matlab *vmatlab;

  PetscFunctionBegin;
  if (!machine) {
    ierr = OptionsGetString(PETSC_NULL,"-viewer_matlab_machine",mach,128,&flag);CHKERRQ(ierr);
    if (!flag) {
      ierr = PetscGetHostName(mach,256); CHKERRQ(ierr);
    }
  } else {
    PetscStrncpy(mach,machine,256);
  }

  if (port <= 0) {
    ierr = OptionsGetInt(PETSC_NULL,"-viewer_matlab_port",&port,&flag); CHKERRQ(ierr);
    if (!flag) {
      char portn[16];
      ierr = OptionsGetenv(comm,"PETSC_VIEWER_MATLAB_PORT",portn,16,&tflag);CHKERRQ(ierr);
      if (tflag) {
        port = OptionsAtoi(portn);
      } else {
        port = DEFAULTPORT;
      }
    }
  }

  PetscHeaderCreate(v,_p_Viewer,struct _ViewerOps,VIEWER_COOKIE,0,"Viewer",comm,ViewerDestroy,0);
  PLogObjectCreate(v);
  vmatlab = PetscNew(Viewer_Matlab);CHKPTRQ(vmatlab);
  v->data = (void *) vmatlab;

  MPI_Comm_rank(comm,&rank);
  if (!rank) {
    PLogInfo(0,"Connecting to matlab process on port %d machine %s\n",port,mach);
    ierr          = SOCKCall_Private(mach,port,&t);CHKERRQ(ierr);
    vmatlab->port = t;
  }
  v->ops->destroy = ViewerDestroy_Matlab;
  v->ops->flush   = 0;
  v->type_name    = (char *)PetscMalloc((1+PetscStrlen(MATLAB_VIEWER))*sizeof(char));CHKPTRQ(v->type_name);
  PetscStrcpy(v->type_name,MATLAB_VIEWER);

  *lab             = v;
  PetscFunctionReturn(0);
}

Viewer VIEWER_MATLAB_WORLD_PRIVATE = 0;

#undef __FUNC__  
#define __FUNC__ "ViewerInitializeMatlabWorld_Private"
int ViewerInitializeMatlabWorld_Private(void)
{
  int  ierr;

  PetscFunctionBegin;
  if (VIEWER_MATLAB_WORLD_PRIVATE) PetscFunctionReturn(0);
  ierr = ViewerMatlabOpen(PETSC_COMM_WORLD,0,0,&VIEWER_MATLAB_WORLD_PRIVATE); CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ "ViewerDestroyMatlab_Private"
int ViewerDestroyMatlab_Private(void)
{
  int ierr;

  PetscFunctionBegin;
  if (VIEWER_MATLAB_WORLD_PRIVATE) {
    ierr = ViewerDestroy(VIEWER_MATLAB_WORLD_PRIVATE); CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}
#else /* defined (PARCH_win32) */
 
#include "viewer.h"
Viewer VIEWER_MATLAB_WORLD_PRIVATE = 0;

int ViewerInitializeMatlabWorld_Private(void)
{ 
  PetscFunctionBegin;
  PetscFunctionReturn(0);
}

int ViewerMatlabOpen(MPI_Comm comm,const char machine[],int port,Viewer *lab)
{
  PetscFunctionBegin;
  PetscFunctionReturn(0);
}
int ViewerDestroyMatlab_Private(void)
{
  PetscFunctionBegin;
  PetscFunctionReturn(0);
}
#endif

