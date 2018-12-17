
static char help[] = "Tests HDF5 attribute I/O.\n\n";

#include <petscviewerhdf5.h>
#include <petscvec.h>

#define DF "ex48.h5"
/* TODO string reading doesn't work, remove to reproduce */
#define READ_STRING_TODO 1

int main(int argc,char **argv)
{
  PetscViewer    viewer;
  PetscBool      has;
  PetscInt       a,p,s,n=5;
#define np 4
  const char     path[np][128]  = {"/", "/group1", "/group1/group2", "/group1/nonExistingPath"};
#define na 6
  const char     attr[na][128]  = {"integer", "real",     "boolean0", "boolean1", "string",     "nonExistingAttribute"};
  PetscDataType  dts[na]        = {PETSC_INT, PETSC_REAL, PETSC_BOOL, PETSC_BOOL, PETSC_STRING, PETSC_INT};
#define ns 3
  const char     names[ns][128] = {"", "x", "nonExistingVec"}; /* test group and dataset (vector) attributes */
  Vec            vecs[ns];
  const char     *name,*group;
  char           buf[PETSC_MAX_PATH_LEN];
  PetscBool      boolean0       = PETSC_FALSE;
  PetscBool      boolean1       = PETSC_TRUE;
  PetscInt       integer        = -1234;
  PetscReal      real           = 3.14;
  const char     string[]       = "Test String";
#if !defined(READ_STRING_TODO)
  char           *string1;
#endif
  PetscMPIInt    rank;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = PetscInitialize(&argc, &argv, (char*) 0, help);if (ierr) return ierr;
  ierr = PetscPrintf(PETSC_COMM_WORLD, "na np ns %D %D %D\n", na, np, ns);CHKERRQ(ierr);
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD, &rank);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(NULL,NULL, "-n", &n, NULL);CHKERRQ(ierr);
  ierr = PetscMemzero(vecs, ns*sizeof(Vec));CHKERRQ(ierr);

  /* create & initialize vector vecs[1] "x" */
  ierr = VecCreate(PETSC_COMM_WORLD, &vecs[1]);CHKERRQ(ierr);
  ierr = PetscObjectSetName((PetscObject)vecs[1], names[1]);CHKERRQ(ierr);
  ierr = VecSetSizes(vecs[1], n, PETSC_DECIDE);CHKERRQ(ierr);
  ierr = VecSetFromOptions(vecs[1]);CHKERRQ(ierr);
  ierr = VecSetRandom(vecs[1],NULL);CHKERRQ(ierr);

  /* create & initialize vector vecs[2] "nonExistingVec" */
  ierr = VecDuplicate(vecs[1], &vecs[2]);CHKERRQ(ierr);
  ierr = VecCopy(vecs[1], vecs[2]);CHKERRQ(ierr);
  ierr = PetscObjectSetName((PetscObject)vecs[2], names[2]);CHKERRQ(ierr);

  ierr = PetscViewerHDF5Open(PETSC_COMM_WORLD, DF, FILE_MODE_WRITE, &viewer);CHKERRQ(ierr);

  for (p=0; p<np-1; p++) {
    ierr = PetscViewerHDF5PushGroup(viewer, path[p]);CHKERRQ(ierr);
    ierr = VecView(vecs[1], viewer);CHKERRQ(ierr);
    ierr = PetscViewerHDF5PopGroup(viewer);CHKERRQ(ierr);
  }

  /* test attribute writing */
  for (p=0; p<np-1; p++) for (s=0; s<ns-1; s++) {
    a = 0;
    ierr = PetscSNPrintf(buf, sizeof(buf), "%s/%s", path[p], names[s]);CHKERRQ(ierr);
    ierr = PetscViewerHDF5WriteAttribute(viewer, buf, attr[a], dts[a], &integer);CHKERRQ(ierr);  a++;
    ierr = PetscViewerHDF5WriteAttribute(viewer, buf, attr[a], dts[a], &real);CHKERRQ(ierr);     a++;
    ierr = PetscViewerHDF5WriteAttribute(viewer, buf, attr[a], dts[a], &boolean0);CHKERRQ(ierr); a++;
    ierr = PetscViewerHDF5WriteAttribute(viewer, buf, attr[a], dts[a], &boolean1);CHKERRQ(ierr); a++;
    ierr = PetscViewerHDF5WriteAttribute(viewer, buf, attr[a], dts[a], string);CHKERRQ(ierr);    a++;
    if (a != na-1) SETERRQ2(PETSC_COMM_WORLD, PETSC_ERR_ARG_OUTOFRANGE, "a != na-1, %D != %D", a, na-1);
  }

  ierr = PetscViewerDestroy(&viewer);CHKERRQ(ierr);
  ierr = PetscViewerHDF5Open(PETSC_COMM_WORLD, DF, FILE_MODE_READ, &viewer);CHKERRQ(ierr);

  /* test group/dataset querying */
  for (p=0; p<np; p++) {
    ierr = PetscViewerHDF5PushGroup(viewer, path[p]);CHKERRQ(ierr);
    ierr = PetscViewerHDF5GetGroup(viewer, &group);CHKERRQ(ierr);
    ierr = PetscStrcmp(path[p], group, &has);CHKERRQ(ierr);
    if (!has) SETERRQ2(PETSC_COMM_WORLD, PETSC_ERR_PLIB, "current group %s not equal to anticipated %s", group, path[p]);
    ierr = PetscViewerHDF5HasGroup(viewer, &has);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD, "Has %s group? %D\n", path[p], has);CHKERRQ(ierr);

    for (s=0; s<ns; s++) {
      if (vecs[s]) {
        ierr = PetscObjectGetName((PetscObject)vecs[s], &name);CHKERRQ(ierr);
        ierr = PetscStrcmp(names[s], name, &has);CHKERRQ(ierr);
        if (!has) SETERRQ2(PETSC_COMM_WORLD, PETSC_ERR_PLIB, "current object name %s not equal to anticipated %s", name, names[s]);
        ierr = PetscViewerHDF5HasObject(viewer, (PetscObject)vecs[s], &has);CHKERRQ(ierr);
        ierr = PetscPrintf(PETSC_COMM_WORLD, "-   %s/%s dataset? %D\n", group, name, has);CHKERRQ(ierr);
      }
    }

    ierr = PetscViewerHDF5PopGroup(viewer);CHKERRQ(ierr);
  }

  /* test attribute querying including nonexisting attributes */
  for (p=0; p<np; p++) for (s=0; s<ns; s++) {
    ierr = PetscSNPrintf(buf, sizeof(buf), "%s/%s", path[p], names[s]);CHKERRQ(ierr);
    for (a=0; a<na; a++) {
      ierr = PetscViewerHDF5HasAttribute(viewer, buf, attr[a], &has);CHKERRQ(ierr);
      ierr = PetscPrintf(PETSC_COMM_WORLD, "Has %s/%s? %D\n", buf, attr[a], has);CHKERRQ(ierr);
    }
  }

  /* test attribute reading */
  for (p=0; p<np-1; p++) for (s=0; s<ns-1; s++) {
    integer = -1;
    real = -1.0;
    boolean0 = -1;
    boolean1 = -1;
    a = 0;
    ierr = PetscSNPrintf(buf, sizeof(buf), "%s/%s", path[p], names[s]);CHKERRQ(ierr);
    ierr = PetscViewerHDF5ReadAttribute(viewer, buf, attr[a], dts[a], &integer);CHKERRQ(ierr);  a++;
    ierr = PetscViewerHDF5ReadAttribute(viewer, buf, attr[a], dts[a], &real);CHKERRQ(ierr);     a++;
    ierr = PetscViewerHDF5ReadAttribute(viewer, buf, attr[a], dts[a], &boolean0);CHKERRQ(ierr); a++;
    ierr = PetscViewerHDF5ReadAttribute(viewer, buf, attr[a], dts[a], &boolean1);CHKERRQ(ierr); a++;
#if !defined(READ_STRING_TODO)
    ierr = PetscViewerHDF5ReadAttribute(viewer, buf, attr[a], dts[a], &string1);CHKERRQ(ierr);  a++;
#else
    a++;
#endif
    if (a != na-1) SETERRQ2(PETSC_COMM_WORLD, PETSC_ERR_ARG_OUTOFRANGE, "a != na-1, %D != %D", a, na-1);
    a = 0;
    ierr = PetscViewerASCIIPrintf(PETSC_VIEWER_STDOUT_WORLD, "%s/%s=%D\n", buf, attr[a], integer);CHKERRQ(ierr);  a++;
    ierr = PetscViewerASCIIPrintf(PETSC_VIEWER_STDOUT_WORLD, "%s/%s=%f\n", buf, attr[a], real);CHKERRQ(ierr);     a++;
    ierr = PetscViewerASCIIPrintf(PETSC_VIEWER_STDOUT_WORLD, "%s/%s=%D\n", buf, attr[a], boolean0);CHKERRQ(ierr); a++;
    ierr = PetscViewerASCIIPrintf(PETSC_VIEWER_STDOUT_WORLD, "%s/%s=%D\n", buf, attr[a], boolean1);CHKERRQ(ierr); a++;
#if !defined(READ_STRING_TODO)
    ierr = PetscViewerASCIIPrintf(PETSC_VIEWER_STDOUT_WORLD, "%s/%s=%s\n", buf, attr[a], string1);CHKERRQ(ierr);  a++;
#else
    a++;
#endif
    if (a != na-1) SETERRQ2(PETSC_COMM_WORLD, PETSC_ERR_ARG_OUTOFRANGE, "a != na-1, %D != %D", a, na-1);
  }

  ierr = VecDestroy(&vecs[1]);CHKERRQ(ierr);
  ierr = VecDestroy(&vecs[2]);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(&viewer);CHKERRQ(ierr);
  ierr = PetscFinalize();
  return ierr;
}

/*TEST

     build:
       requires: hdf5

     test:
       suffix: 1
       nsize: {{1 2 4}}

TEST*/
