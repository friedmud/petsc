#include <petsc/private/dmnetworkimpl.h>  /*I  "petscdmnetwork.h"  I*/
#include <petscdmplex.h>
#include <petscsf.h>

#undef __FUNCT__
#define __FUNCT__ "DMNetworkSetSizes"
/*@
  DMNetworkSetSizes - Sets the local and global vertices and edges.

  Collective on DM
  
  Input Parameters:
+ dm - the dm object
. nV - number of local vertices
. nE - number of local edges
. NV - number of global vertices (or PETSC_DETERMINE)
- NE - number of global edges (or PETSC_DETERMINE)

   Notes
   If one processor calls this with NV (NE) of PETSC_DECIDE then all processors must, otherwise the prgram will hang.

   You cannot change the sizes once they have been set

   Level: intermediate

.seealso: DMNetworkCreate
@*/
PetscErrorCode DMNetworkSetSizes(DM dm, PetscInt nV, PetscInt nE, PetscInt NV, PetscInt NE)
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*) dm->data;
  PetscInt       a[2],b[2];

  PetscFunctionBegin;
  PetscValidHeaderSpecific(dm,DM_CLASSID,1);
  if (NV > 0) PetscValidLogicalCollectiveInt(dm,NV,4);
  if (NE > 0) PetscValidLogicalCollectiveInt(dm,NE,5);
  if (NV > 0 && nV > NV) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_INCOMP,"Local vertex size %D cannot be larger than global vertex size %D",nV,NV);
  if (NE > 0 && nE > NE) SETERRQ2(PETSC_COMM_SELF,PETSC_ERR_ARG_INCOMP,"Local edge size %D cannot be larger than global edge size %D",nE,NE);
  if ((network->nNodes >= 0 || network->NNodes >= 0) && (network->nNodes != nV || network->NNodes != NV)) SETERRQ4(PETSC_COMM_SELF,PETSC_ERR_SUP,"Cannot change/reset vertex sizes to %D local %D global after previously setting them to %D local %D global",nV,NV,network->nNodes,network->NNodes);
  if ((network->nEdges >= 0 || network->NEdges >= 0) && (network->nEdges != nE || network->NEdges != NE)) SETERRQ4(PETSC_COMM_SELF,PETSC_ERR_SUP,"Cannot change/reset edge sizes to %D local %D global after previously setting them to %D local %D global",nE,NE,network->nEdges,network->NEdges);
  if (NE < 0 || NV < 0) {
    a[0] = nV; a[1] = nE;
    ierr = MPIU_Allreduce(a,b,2,MPIU_INT,MPI_SUM,PetscObjectComm((PetscObject)dm));CHKERRQ(ierr);
    NV = b[0]; NE = b[1];
  }
  network->nNodes = nV;
  network->NNodes = NV;
  network->nEdges = nE;
  network->NEdges = NE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkSetEdgeList"
/*@
  DMNetworkSetEdgeList - Sets the list of local edges (vertex connectivity) for the network

  Logically collective on DM

  Input Parameters:
. edges - list of edges

  Notes:
  There is no copy involved in this operation, only the pointer is referenced. The edgelist should
  not be destroyed before the call to DMNetworkLayoutSetUp

  Level: intermediate

.seealso: DMNetworkCreate, DMNetworkSetSizes
@*/
PetscErrorCode DMNetworkSetEdgeList(DM dm, int edgelist[])
{
  DM_Network *network = (DM_Network*) dm->data;
  
  PetscFunctionBegin;
  network->edges = edgelist;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkLayoutSetUp"
/*@
  DMNetworkLayoutSetUp - Sets up the bare layout (graph) for the network

  Collective on DM

  Input Parameters
. DM - the dmnetwork object

  Notes:
  This routine should be called after the network sizes and edgelists have been provided. It creates
  the bare layout of the network and sets up the network to begin insertion of components.

  All the components should be registered before calling this routine.

  Level: intermediate

.seealso: DMNetworkSetSizes, DMNetworkSetEdgeList
@*/
PetscErrorCode DMNetworkLayoutSetUp(DM dm)
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*) dm->data;
  PetscInt       dim = 1; /* One dimensional network */
  PetscInt       numCorners=2;
  PetscInt       spacedim=2;
  double         *vertexcoords=NULL;
  PetscInt       i;
  PetscInt       ndata;

  PetscFunctionBegin;
  if (network->nNodes) {
    ierr = PetscCalloc1(numCorners*network->nNodes,&vertexcoords);CHKERRQ(ierr);
  }
  ierr = DMPlexCreateFromCellList(PetscObjectComm((PetscObject)dm),dim,network->nEdges,network->nNodes,numCorners,PETSC_FALSE,network->edges,spacedim,vertexcoords,&network->plex);CHKERRQ(ierr);
  if (network->nNodes) {
    ierr = PetscFree(vertexcoords);CHKERRQ(ierr);
  }
  ierr = DMPlexGetChart(network->plex,&network->pStart,&network->pEnd);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(network->plex,0,&network->eStart,&network->eEnd);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(network->plex,1,&network->vStart,&network->vEnd);CHKERRQ(ierr);

  ierr = PetscSectionCreate(PetscObjectComm((PetscObject)dm),&network->DataSection);CHKERRQ(ierr);
  ierr = PetscSectionCreate(PetscObjectComm((PetscObject)dm),&network->DofSection);CHKERRQ(ierr);
  ierr = PetscSectionSetChart(network->DataSection,network->pStart,network->pEnd);CHKERRQ(ierr);
  ierr = PetscSectionSetChart(network->DofSection,network->pStart,network->pEnd);CHKERRQ(ierr);

  network->dataheadersize = sizeof(struct _p_DMNetworkComponentHeader)/sizeof(DMNetworkComponentGenericDataType);
  ierr = PetscCalloc1(network->pEnd-network->pStart,&network->header);CHKERRQ(ierr);
  for (i = network->pStart; i < network->pEnd; i++) {
    network->header[i].ndata = 0;
    ndata = network->header[i].ndata;
    ierr = PetscSectionAddDof(network->DataSection,i,network->dataheadersize);CHKERRQ(ierr);
    network->header[i].offset[ndata] = 0;
  }
  ierr = PetscMalloc1(network->pEnd-network->pStart,&network->cvalue);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkRegisterComponent"
/*@
  DMNetworkRegisterComponent - Registers the network component

  Logically collective on DM

  Input Parameters
+ dm   - the network object
. name - the component name
- size - the storage size in bytes for this component data

   Output Parameters
.   key - an integer key that defines the component

   Notes
   This routine should be called by all processors before calling DMNetworkLayoutSetup().

   Level: intermediate

.seealso: DMNetworkLayoutSetUp, DMNetworkCreate
@*/
PetscErrorCode DMNetworkRegisterComponent(DM dm,const char *name,PetscInt size,PetscInt *key)
{
  PetscErrorCode        ierr;
  DM_Network            *network = (DM_Network*) dm->data;
  DMNetworkComponent    *component=&network->component[network->ncomponent];
  PetscBool             flg=PETSC_FALSE;
  PetscInt              i;

  PetscFunctionBegin;

  for (i=0; i < network->ncomponent; i++) {
    ierr = PetscStrcmp(component->name,name,&flg);CHKERRQ(ierr);
    if (flg) {
      *key = i;
      PetscFunctionReturn(0);
    }
  }
  
  ierr = PetscStrcpy(component->name,name);CHKERRQ(ierr);
  component->size = size/sizeof(DMNetworkComponentGenericDataType);
  *key = network->ncomponent;
  network->ncomponent++;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkGetVertexRange"
/*@
  DMNetworkGetVertexRange - Get the bounds [start, end) for the vertices.

  Not Collective

  Input Parameters:
+ dm - The DMNetwork object

  Output Paramters:
+ vStart - The first vertex point
- vEnd   - One beyond the last vertex point

  Level: intermediate

.seealso: DMNetworkGetEdgeRange
@*/
PetscErrorCode DMNetworkGetVertexRange(DM dm,PetscInt *vStart,PetscInt *vEnd)
{
  DM_Network     *network = (DM_Network*)dm->data;

  PetscFunctionBegin;
  if (vStart) *vStart = network->vStart;
  if (vEnd) *vEnd = network->vEnd;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkGetEdgeRange"
/*@
  DMNetworkGetEdgeRange - Get the bounds [start, end) for the edges.

  Not Collective

  Input Parameters:
+ dm - The DMNetwork object

  Output Paramters:
+ eStart - The first edge point
- eEnd   - One beyond the last edge point

  Level: intermediate

.seealso: DMNetworkGetVertexRange
@*/
PetscErrorCode DMNetworkGetEdgeRange(DM dm,PetscInt *eStart,PetscInt *eEnd)
{
  DM_Network     *network = (DM_Network*)dm->data;

  PetscFunctionBegin;
  if (eStart) *eStart = network->eStart;
  if (eEnd) *eEnd = network->eEnd;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkAddComponent"
/*@
  DMNetworkAddComponent - Adds a network component at the given point (vertex/edge)

  Not Collective

  Input Parameters:
+ dm           - The DMNetwork object
. p            - vertex/edge point
. componentkey - component key returned while registering the component
- compvalue    - pointer to the data structure for the component

  Level: intermediate

.seealso: DMNetworkGetVertexRange, DMNetworkGetEdgeRange, DMNetworkRegisterComponent
@*/
PetscErrorCode DMNetworkAddComponent(DM dm, PetscInt p,PetscInt componentkey,void* compvalue)
{
  DM_Network               *network = (DM_Network*)dm->data;
  DMNetworkComponent       *component = &network->component[componentkey];
  DMNetworkComponentHeader header = &network->header[p];
  DMNetworkComponentValue  cvalue = &network->cvalue[p];
  PetscErrorCode           ierr;
  
  PetscFunctionBegin;
  header->size[header->ndata] = component->size;
  ierr = PetscSectionAddDof(network->DataSection,p,component->size);CHKERRQ(ierr);
  header->key[header->ndata] = componentkey;
  if (header->ndata != 0) header->offset[header->ndata] = header->offset[header->ndata-1] + header->size[header->ndata-1]; 

  cvalue->data[header->ndata] = (void*)compvalue;
  header->ndata++;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkGetNumComponents"
/*@
  DMNetworkGetNumComponents - Get the number of components at a vertex/edge

  Not Collective 

  Input Parameters:
+ dm - The DMNetwork object
. p  - vertex/edge point

  Output Parameters:
. numcomponents - Number of components at the vertex/edge

  Level: intermediate

.seealso: DMNetworkRegisterComponent, DMNetworkAddComponent
@*/
PetscErrorCode DMNetworkGetNumComponents(DM dm,PetscInt p,PetscInt *numcomponents)
{
  PetscErrorCode ierr;
  PetscInt       offset;
  DM_Network     *network = (DM_Network*)dm->data;

  PetscFunctionBegin;
  ierr = PetscSectionGetOffset(network->DataSection,p,&offset);CHKERRQ(ierr);
  *numcomponents = ((DMNetworkComponentHeader)(network->componentdataarray+offset))->ndata;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkGetComponentTypeOffset"
/*@
  DMNetworkGetComponentTypeOffset - Gets the type along with the offset for indexing the 
                                    component value from the component data array

  Not Collective

  Input Parameters:
+ dm      - The DMNetwork object
. p       - vertex/edge point
- compnum - component number
	
  Output Parameters:
+ compkey - the key obtained when registering the component
- offset  - offset into the component data array associated with the vertex/edge point

  Notes:
  Typical usage:

  DMNetworkGetComponentDataArray(dm, &arr);
  DMNetworkGetVertex/EdgeRange(dm,&Start,&End);
  Loop over vertices or edges
    DMNetworkGetNumComponents(dm,v,&numcomps);
    Loop over numcomps
      DMNetworkGetComponentTypeOffset(dm,v,compnum,&key,&offset);
      compdata = (UserCompDataType)(arr+offset);
  
  Level: intermediate

.seealso: DMNetworkGetNumComponents, DMNetworkGetComponentDataArray, 
@*/
PetscErrorCode DMNetworkGetComponentTypeOffset(DM dm,PetscInt p, PetscInt compnum, PetscInt *compkey, PetscInt *offset)
{
  PetscErrorCode           ierr;
  PetscInt                 offsetp;
  DMNetworkComponentHeader header;
  DM_Network               *network = (DM_Network*)dm->data;

  PetscFunctionBegin;
  ierr = PetscSectionGetOffset(network->DataSection,p,&offsetp);CHKERRQ(ierr);
  header = (DMNetworkComponentHeader)(network->componentdataarray+offsetp);
  *compkey = header->key[compnum];
  *offset  = offsetp+network->dataheadersize+header->offset[compnum];
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkGetVariableOffset"
/*@
  DMNetworkGetVariableOffset - Get the offset for accessing the variable associated with the given vertex/edge from the local vector.

  Not Collective

  Input Parameters:
+ dm     - The DMNetwork object
- p      - the edge/vertex point

  Output Parameters:
. offset - the offset

  Level: intermediate

.seealso: DMNetworkGetVariableGlobalOffset, DMGetLocalVector
@*/
PetscErrorCode DMNetworkGetVariableOffset(DM dm,PetscInt p,PetscInt *offset)
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*)dm->data;

  PetscFunctionBegin;
  ierr = PetscSectionGetOffset(network->DofSection,p,offset);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkGetVariableGlobalOffset"
/*@
  DMNetworkGetVariableGlobalOffset - Get the global offset for the variable associated with the given vertex/edge from the global vector.

  Not Collective

  Input Parameters:
+ dm      - The DMNetwork object
- p       - the edge/vertex point

  Output Parameters:
. offsetg - the offset

  Level: intermediate

.seealso: DMNetworkGetVariableOffset, DMGetLocalVector
@*/
PetscErrorCode DMNetworkGetVariableGlobalOffset(DM dm,PetscInt p,PetscInt *offsetg)
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*)dm->data;

  PetscFunctionBegin;
  ierr = PetscSectionGetOffset(network->GlobalDofSection,p,offsetg);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkAddNumVariables"
/*@ 
  DMNetworkAddNumVariables - Add number of variables associated with a given point.

  Not Collective

  Input Parameters:
+ dm   - The DMNetworkObject
. p    - the vertex/edge point
- nvar - number of additional variables

  Level: intermediate

.seealso: DMNetworkSetNumVariables
@*/
PetscErrorCode DMNetworkAddNumVariables(DM dm,PetscInt p,PetscInt nvar)
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*)dm->data;

  PetscFunctionBegin;
  ierr = PetscSectionAddDof(network->DofSection,p,nvar);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkGetNumVariables"
/*@ 
  DMNetworkGetNumVariables - Gets number of variables for a vertex/edge point.

  Not Collective

  Input Parameters:
+ dm   - The DMNetworkObject
- p    - the vertex/edge point

  Output Parameters:
. nvar - number of variables

  Level: intermediate

.seealso: DMNetworkAddNumVariables, DMNetworkSddNumVariables
@*/
PetscErrorCode DMNetworkGetNumVariables(DM dm,PetscInt p,PetscInt *nvar)
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*)dm->data;

  PetscFunctionBegin;
  ierr = PetscSectionGetDof(network->DofSection,p,nvar);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkSetNumVariables"
/*@ 
  DMNetworkSetNumVariables - Sets number of variables for a vertex/edge point.

  Not Collective

  Input Parameters:
+ dm   - The DMNetworkObject
. p    - the vertex/edge point
- nvar - number of variables

  Level: intermediate

.seealso: DMNetworkAddNumVariables
@*/
PetscErrorCode DMNetworkSetNumVariables(DM dm,PetscInt p,PetscInt nvar)
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*)dm->data;

  PetscFunctionBegin;
  ierr = PetscSectionSetDof(network->DofSection,p,nvar);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

/* Sets up the array that holds the data for all components and its associated section. This
   function is called during DMSetUp() */
#undef __FUNCT__
#define __FUNCT__ "DMNetworkComponentSetUp"
PetscErrorCode DMNetworkComponentSetUp(DM dm)
{
  PetscErrorCode              ierr;
  DM_Network     *network = (DM_Network*)dm->data;
  PetscInt                    arr_size;
  PetscInt                    p,offset,offsetp;
  DMNetworkComponentHeader header;
  DMNetworkComponentValue  cvalue;
  DMNetworkComponentGenericDataType      *componentdataarray;
  PetscInt ncomp, i;

  PetscFunctionBegin;
  ierr = PetscSectionSetUp(network->DataSection);CHKERRQ(ierr);
  ierr = PetscSectionGetStorageSize(network->DataSection,&arr_size);CHKERRQ(ierr);
  ierr = PetscMalloc1(arr_size,&network->componentdataarray);CHKERRQ(ierr);
  componentdataarray = network->componentdataarray;
  for (p = network->pStart; p < network->pEnd; p++) {
    ierr = PetscSectionGetOffset(network->DataSection,p,&offsetp);CHKERRQ(ierr);
    /* Copy header */
    header = &network->header[p];
    ierr = PetscMemcpy(componentdataarray+offsetp,header,network->dataheadersize*sizeof(DMNetworkComponentGenericDataType));CHKERRQ(ierr);
    /* Copy data */
    cvalue = &network->cvalue[p];
    ncomp = header->ndata;
    for (i = 0; i < ncomp; i++) {
      offset = offsetp + network->dataheadersize + header->offset[i];
      ierr = PetscMemcpy(componentdataarray+offset,cvalue->data[i],header->size[i]*sizeof(DMNetworkComponentGenericDataType));CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

/* Sets up the section for dofs. This routine is called during DMSetUp() */
#undef __FUNCT__
#define __FUNCT__ "DMNetworkVariablesSetUp"
PetscErrorCode DMNetworkVariablesSetUp(DM dm)
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*)dm->data;

  PetscFunctionBegin;
  ierr = PetscSectionSetUp(network->DofSection);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkGetComponentDataArray"
/*@C
  DMNetworkGetComponentDataArray - Returns the component data array

  Not Collective

  Input Parameters:
. dm - The DMNetwork Object

  Output Parameters:
. componentdataarray - array that holds data for all components

  Level: intermediate

.seealso: DMNetworkGetComponentTypeOffset, DMNetworkGetNumComponents
@*/
PetscErrorCode DMNetworkGetComponentDataArray(DM dm,DMNetworkComponentGenericDataType **componentdataarray)
{
  DM_Network     *network = (DM_Network*)dm->data;

  PetscFunctionBegin;
  *componentdataarray = network->componentdataarray;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkDistribute"
/*@
  DMNetworkDistribute - Distributes the network and moves associated component data.

  Collective

  Input Parameter:
+ oldDM - the original DMNetwork object
- overlap - The overlap of partitions, 0 is the default

  Output Parameter:
. distDM - the distributed DMNetwork object

  Notes:
  This routine should be called only when using multiple processors.

  Distributes the network with <overlap>-overlapping partitioning of the edges.

  Level: intermediate

.seealso: DMNetworkCreate
@*/
PetscErrorCode DMNetworkDistribute(DM oldDM, PetscInt overlap,DM *distDM)
{
  PetscErrorCode ierr;
  DM_Network     *oldDMnetwork = (DM_Network*)oldDM->data;
  PetscSF        pointsf;
  DM             newDM;
  DM_Network     *newDMnetwork;

  PetscFunctionBegin;
  ierr = DMNetworkCreate(PetscObjectComm((PetscObject)oldDM),&newDM);CHKERRQ(ierr);
  newDMnetwork = (DM_Network*)newDM->data;
  newDMnetwork->dataheadersize = sizeof(struct _p_DMNetworkComponentHeader)/sizeof(DMNetworkComponentGenericDataType);
  /* Distribute plex dm and dof section */
  ierr = DMPlexDistribute(oldDMnetwork->plex,overlap,&pointsf,&newDMnetwork->plex);CHKERRQ(ierr);
  /* Distribute dof section */
  ierr = PetscSectionCreate(PetscObjectComm((PetscObject)oldDM),&newDMnetwork->DofSection);CHKERRQ(ierr);
  ierr = PetscSFDistributeSection(pointsf,oldDMnetwork->DofSection,NULL,newDMnetwork->DofSection);CHKERRQ(ierr);
  ierr = PetscSectionCreate(PetscObjectComm((PetscObject)oldDM),&newDMnetwork->DataSection);CHKERRQ(ierr);
  /* Distribute data and associated section */
  ierr = DMPlexDistributeData(newDMnetwork->plex,pointsf,oldDMnetwork->DataSection,MPIU_INT,(void*)oldDMnetwork->componentdataarray,newDMnetwork->DataSection,(void**)&newDMnetwork->componentdataarray);CHKERRQ(ierr);
  /* Destroy point SF */
  ierr = PetscSFDestroy(&pointsf);CHKERRQ(ierr);
  
  ierr = PetscSectionGetChart(newDMnetwork->DataSection,&newDMnetwork->pStart,&newDMnetwork->pEnd);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(newDMnetwork->plex,0, &newDMnetwork->eStart,&newDMnetwork->eEnd);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(newDMnetwork->plex,1,&newDMnetwork->vStart,&newDMnetwork->vEnd);CHKERRQ(ierr);
  newDMnetwork->nEdges = newDMnetwork->eEnd - newDMnetwork->eStart;
  newDMnetwork->nNodes = newDMnetwork->vEnd - newDMnetwork->vStart;
  newDMnetwork->NNodes = oldDMnetwork->NNodes;
  newDMnetwork->NEdges = oldDMnetwork->NEdges;
  /* Set Dof section as the default section for dm */
  ierr = DMSetDefaultSection(newDMnetwork->plex,newDMnetwork->DofSection);CHKERRQ(ierr);
  ierr = DMGetDefaultGlobalSection(newDMnetwork->plex,&newDMnetwork->GlobalDofSection);CHKERRQ(ierr);

  *distDM = newDM;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkGetSupportingEdges"
/*@C
  DMNetworkGetSupportingEdges - Return the supporting edges for this vertex point

  Not Collective

  Input Parameters:
+ dm - The DMNetwork object
- p  - the vertex point

  Output Paramters:
+ nedges - number of edges connected to this vertex point
- edges  - List of edge points

  Level: intermediate

  Fortran Notes:
  Since it returns an array, this routine is only available in Fortran 90, and you must
  include petsc.h90 in your code.

.seealso: DMNetworkCreate, DMNetworkGetConnectedNodes
@*/
PetscErrorCode DMNetworkGetSupportingEdges(DM dm,PetscInt vertex,PetscInt *nedges,const PetscInt *edges[])
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*)dm->data;

  PetscFunctionBegin;
  ierr = DMPlexGetSupportSize(network->plex,vertex,nedges);CHKERRQ(ierr);
  ierr = DMPlexGetSupport(network->plex,vertex,edges);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkGetConnectedNodes"
/*@C
  DMNetworkGetConnectedNodes - Return the connected vertices for this edge point

  Not Collective

  Input Parameters:
+ dm - The DMNetwork object
- p  - the edge point

  Output Paramters:
. vertices  - vertices connected to this edge

  Level: intermediate

  Fortran Notes:
  Since it returns an array, this routine is only available in Fortran 90, and you must
  include petsc.h90 in your code.

.seealso: DMNetworkCreate, DMNetworkGetSupportingEdges
@*/
PetscErrorCode DMNetworkGetConnectedNodes(DM dm,PetscInt edge,const PetscInt *vertices[])
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*)dm->data;

  PetscFunctionBegin;
  ierr = DMPlexGetCone(network->plex,edge,vertices);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkIsGhostVertex"
/*@
  DMNetworkIsGhostVertex - Returns TRUE if the vertex is a ghost vertex

  Not Collective

  Input Parameters:
+ dm - The DMNetwork object
. p  - the vertex point

  Output Parameter:
. isghost - TRUE if the vertex is a ghost point 

  Level: intermediate

.seealso: DMNetworkCreate, DMNetworkGetConnectedNodes, DMNetworkGetVertexRange
@*/
PetscErrorCode DMNetworkIsGhostVertex(DM dm,PetscInt p,PetscBool *isghost)
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*)dm->data;
  PetscInt       offsetg;
  PetscSection   sectiong;

  PetscFunctionBegin;
  *isghost = PETSC_FALSE;
  ierr = DMGetDefaultGlobalSection(network->plex,&sectiong);CHKERRQ(ierr);
  ierr = PetscSectionGetOffset(sectiong,p,&offsetg);CHKERRQ(ierr);
  if (offsetg < 0) *isghost = PETSC_TRUE;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMSetUp_Network"
PetscErrorCode DMSetUp_Network(DM dm)
{
  PetscErrorCode ierr;
  DM_Network     *network=(DM_Network*)dm->data;

  PetscFunctionBegin;
  ierr = DMNetworkComponentSetUp(dm);CHKERRQ(ierr);
  ierr = DMNetworkVariablesSetUp(dm);CHKERRQ(ierr);

  ierr = DMSetDefaultSection(network->plex,network->DofSection);CHKERRQ(ierr);
  ierr = DMGetDefaultGlobalSection(network->plex,&network->GlobalDofSection);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkHasJacobian"
/*@
    DMNetworkHasJacobian - Sets global flag for using user's sub Jacobian matrices
                            -- replaced by DMNetworkSetOption(network,userjacobian,PETSC_TURE)?

    Collective

    Input Parameters:
+   dm - The DMNetwork object
.   eflg - turn the option on (PETSC_TRUE) or off (PETSC_FALSE) if user provides Jacobian for edges
-   vflg - turn the option on (PETSC_TRUE) or off (PETSC_FALSE) if user provides Jacobian for vertices

    Level: intermediate

@*/
PetscErrorCode DMNetworkHasJacobian(DM dm,PetscBool eflg,PetscBool vflg)
{
  DM_Network     *network=(DM_Network*)dm->data;

  PetscFunctionBegin;
  network->userEdgeJacobian   = eflg;
  network->userVertexJacobian = vflg;
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkEdgeSetMatrix"
/*@
    DMNetworkEdgeSetMatrix - Sets user-provided Jacobian matrices for this edge to the network

    Not Collective

    Input Parameters:
+   dm - The DMNetwork object
.   p  - the edge point
-   J - array of Jacobian three submatrices for this edge point:
        J[0]: this edge; 
        J[1] and J[2]: connected vertices, obtained by calling DMNetworkGetConnectedNodes();

    Level: intermediate

.seealso: DMNetworkVertexSetMatrix
@*/
PetscErrorCode DMNetworkEdgeSetMatrix(DM dm,PetscInt p,Mat J[])
{
  PetscErrorCode ierr;
  DM_Network     *network=(DM_Network*)dm->data;

  PetscFunctionBegin;
  if (!network->userEdgeJacobian) SETERRQ(PetscObjectComm((PetscObject)dm),PETSC_ERR_ORDER,"Must call DMNetworkHasJacobian() collectively before calling DMNetworkEdgeSetMatrix");
  if (!network->Je) {
    ierr = PetscCalloc1(3*network->nEdges,&network->Je);CHKERRQ(ierr);
  }
  network->Je[3*p]   = J[0];
  network->Je[3*p+1] = J[1];
  network->Je[3*p+2] = J[2];
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMNetworkVertexSetMatrix"
/*@
    DMNetworkVetexSetMatrix - Sets user-provided Jacobian matrix for this vertex to the network

    Not Collective

    Input Parameters:
+   dm - The DMNetwork object
.   p  - the vertex point
-   J - array of Jacobian submatrices: 

    Level: intermediate

.seealso: DMNetworkEdgeSetMatrix
@*/
PetscErrorCode DMNetworkVertexSetMatrix(DM dm,PetscInt p,Mat J[])
{
  PetscErrorCode ierr;
  DM_Network     *network=(DM_Network*)dm->data;
  PetscInt       i,vStart,vEnd,*vptr,nedges;
  const PetscInt *edges;

  PetscFunctionBegin;
  if (!network->userVertexJacobian) SETERRQ(PetscObjectComm((PetscObject)dm),PETSC_ERR_ORDER,"Must call DMNetworkHasJacobian() collectively before calling DMNetworkVertexSetMatrix");

  ierr = DMNetworkGetVertexRange(dm,&vStart,&vEnd);CHKERRQ(ierr);
  if (!network->Jv) {
    /* count nvertex_total */
    PetscInt *vptr,v,nedges_total = 0;
    ierr = DMNetworkGetVertexRange(dm,&vStart,&vEnd);CHKERRQ(ierr);
    ierr = PetscMalloc1(vEnd-vStart+1,&vptr);CHKERRQ(ierr);
    i       = 1;
    vptr[0] = 0;
    for (v=vStart; v<vEnd; v++) {
      ierr = DMNetworkGetSupportingEdges(dm,v,&nedges,&edges);CHKERRQ(ierr);
      nedges_total += nedges;
      vptr[i] = vptr[i-1] + 2*nedges + 1; i++;
    }
    ierr = PetscCalloc1(2*nedges_total+network->nNodes,&network->Jv);CHKERRQ(ierr);
    network->Jvptr = vptr;
  }

  vptr=network->Jvptr;
  network->Jv[vptr[p-vStart]] = J[0];
  ierr = DMNetworkGetSupportingEdges(dm,p,&nedges,&edges);CHKERRQ(ierr);
  for (i=1; i<=2*nedges; i++) network->Jv[vptr[p-vStart]+i] = J[i];
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "MatSetDenseblock_private"
PetscErrorCode MatSetDenseblock_private(Mat *J,PetscInt nrows,PetscInt *rows,PetscInt ncols,PetscInt cstart)
{
  PetscErrorCode ierr;
  PetscInt       j,*cols;
  PetscScalar    *zeros;

  PetscFunctionBegin;
  ierr = PetscCalloc2(ncols,&cols,nrows*ncols,&zeros);CHKERRQ(ierr);
  for (j=0; j<ncols; j++) cols[j] = j+ cstart;
  ierr = MatSetValues(*J,nrows,rows,ncols,cols,zeros,INSERT_VALUES);CHKERRQ(ierr);
  ierr = PetscFree2(cols,zeros);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMCreateMatrix_Network"
PetscErrorCode DMCreateMatrix_Network(DM dm,Mat *J)
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*) dm->data;
  PetscInt       eStart,eEnd,vStart,vEnd,rstart,rend,row,row_e,nrows,localSize;
  PetscInt       cstart,ncols,col,j,e,v,*dnz,*onz,*dnzu,*onzu;
  const PetscInt *cols;
  PetscScalar    zero = 0.0;
  PetscBool      ghost; 
  Mat            Je,Jv;
  PetscSection   sectionGlobal;
  PetscInt       nedges;
  const PetscInt *edges;

  PetscFunctionBegin;
  if (!network->userEdgeJacobian && !network->userVertexJacobian) { /* user does not provide Jacobian blocks */
    ierr = DMCreateMatrix(network->plex,J);CHKERRQ(ierr);
    ierr = MatSetDM(*J,dm);CHKERRQ(ierr);
    PetscFunctionReturn(0);
  }

  ierr = MatCreate(PetscObjectComm((PetscObject)dm),J);CHKERRQ(ierr);
  ierr = DMGetDefaultGlobalSection(network->plex,&sectionGlobal);CHKERRQ(ierr);
  ierr = PetscSectionGetConstrainedStorageSize(sectionGlobal,&localSize);CHKERRQ(ierr); 
  ierr = MatSetSizes(*J,localSize,localSize,PETSC_DETERMINE,PETSC_DETERMINE);CHKERRQ(ierr);  

  ierr = MatSetType(*J,MATAIJ);CHKERRQ(ierr);
  ierr = MatSetFromOptions(*J);CHKERRQ(ierr);

  /* Preallocation - submatrix for an element (edge/vertex) is allocated as a dense block, see DMCreateMatrix_Plex() */
  ierr = PetscCalloc4(localSize,&dnz,localSize,&onz,localSize,&dnzu,localSize,&onzu);CHKERRQ(ierr);
  ierr = DMPlexPreallocateOperator(network->plex,1,dnz,onz,dnzu,onzu,*J,PETSC_FALSE);CHKERRQ(ierr);
  ierr = PetscFree4(dnz,onz,dnzu,onzu);CHKERRQ(ierr);

  /* Set matrix entries for edges */
  /*------------------------------*/
  ierr = DMNetworkGetEdgeRange(dm,&eStart,&eEnd);CHKERRQ(ierr);
  for (e=eStart; e<eEnd; e++) {
    /* Get row indices */
    ierr = DMNetworkGetVariableGlobalOffset(dm,e,&rstart);CHKERRQ(ierr);
    ierr = DMNetworkGetNumVariables(dm,e,&nrows);CHKERRQ(ierr); 
    
    PetscInt    rows[nrows];
    for (j=0; j<nrows; j++) rows[j] = j + rstart;

    /* Set matrix entries for conntected vertices */
    const PetscInt    *cone; 
    ierr = DMNetworkGetConnectedNodes(dm,e,&cone);CHKERRQ(ierr); 

    for (v=0; v<2; v++) {
      ierr = DMNetworkIsGhostVertex(dm,cone[v],&ghost);CHKERRQ(ierr);
      ierr = DMNetworkGetVariableGlobalOffset(dm,cone[v],&cstart);CHKERRQ(ierr);
      if (ghost) cstart = -(cstart + 1); /* Convert to actual global offset for ghost nodes */

      Je = network->Je[3*e+1+v];
      if (Je) { /* use user-provided Je */
        rend = rstart + nrows;
        for (row = rstart; row<rend; row++) {
          row_e = row - rstart;
          ierr = MatGetRow(Je,row_e,&ncols,&cols,NULL);CHKERRQ(ierr);
          for (j=0; j<ncols; j++) {
            col = cols[j] + cstart;
            ierr = MatSetValues(*J,1,&row,1,&col,&zero,INSERT_VALUES);CHKERRQ(ierr);
          }
        }
        ierr = MatRestoreRow(Je,row_e,&ncols,&cols,NULL);CHKERRQ(ierr);

      } else { /* use dense block */
        ierr = DMNetworkGetNumVariables(dm,cone[v],&ncols);CHKERRQ(ierr);
        ierr = MatSetDenseblock_private(J,nrows,rows,ncols,cstart);CHKERRQ(ierr);
      }
    }

    /* Set matrix entries for edge self */
    Je = network->Je[3*e];
    if (Je) { /* use user-provided Je */
      PetscInt M,N;
      ierr = MatGetSize(Je,&M,&N);CHKERRQ(ierr);
      if (nrows != M || nrows != N) SETERRQ3(PetscObjectComm((PetscObject)Je),PETSC_ERR_USER,"%D must equal %D and %D",nrows,M,N);
    
      rend = rstart + nrows;
      for (row=rstart; row<rend; row++) {
        row_e = row - rstart;
        ierr = MatGetRow(Je,row_e,&ncols,&cols,NULL);CHKERRQ(ierr);
        for (j=0; j<ncols; j++) {
          col = cols[j] + rstart;
          ierr = MatSetValues(*J,1,&row,1,&col,&zero,INSERT_VALUES);CHKERRQ(ierr);
        }
        ierr = MatRestoreRow(Je,row_e,&ncols,&cols,NULL);CHKERRQ(ierr);
      }
    } else { /* set a dense block */
      PetscInt cstart;
      ierr = DMNetworkGetVariableGlobalOffset(dm,e,&cstart);CHKERRQ(ierr);
      ierr = MatSetDenseblock_private(J,nrows,rows,nrows,cstart);CHKERRQ(ierr);
    }
  }

  /* Set matrix entries for vertices */
  /*---------------------------------*/
  ierr = DMNetworkGetVertexRange(dm,&vStart,&vEnd);CHKERRQ(ierr);
  for (v=vStart; v<vEnd; v++) {
    /* Get row indices */
    ierr = DMNetworkIsGhostVertex(dm,v,&ghost);CHKERRQ(ierr);
    ierr = DMNetworkGetVariableGlobalOffset(dm,v,&rstart);CHKERRQ(ierr);
    if (ghost) rstart = -(rstart + 1); /* Convert to actual global offset for ghost nodes */
    ierr = DMNetworkGetNumVariables(dm,v,&nrows);CHKERRQ(ierr);

    PetscInt    rows[nrows];
    for (j=0; j<nrows; j++) rows[j] = j + rstart;

    /* Get supporting edges and connected vertices */
    ierr = DMNetworkGetSupportingEdges(dm,v,&nedges,&edges);CHKERRQ(ierr);
   
    for (e=0; e<nedges; e++) {
      /* Supporting edges */
      ierr = DMNetworkGetVariableGlobalOffset(dm,edges[e],&cstart);CHKERRQ(ierr);
      ierr = DMNetworkGetNumVariables(dm,edges[e],&ncols);CHKERRQ(ierr); 

      PetscInt *vptr = network->Jvptr;
      Jv = network->Jv[vptr[v-vStart]+2*e+1];
      if (Jv) { /* use user-provided Je */
        PetscInt M,N;
        ierr = MatGetSize(Jv,&M,&N);CHKERRQ(ierr);
        if (nrows != M) SETERRQ2(PetscObjectComm((PetscObject)Jv),PETSC_ERR_USER,"%D must equal %D",nrows,M);
        if (ncols != N) SETERRQ2(PetscObjectComm((PetscObject)Jv),PETSC_ERR_USER,"%D must equal %D",ncols,N);

        ierr = DMNetworkGetVariableGlobalOffset(dm,edges[e],&cstart);CHKERRQ(ierr);
        rend = rstart + nrows;
        for (row=rstart; row<rend; row++) {
          row_e = row - rstart;
          ierr = MatGetRow(Jv,row_e,&ncols,&cols,NULL);CHKERRQ(ierr);
          for (j=0; j<ncols; j++) {
            col = cols[j] + cstart;
            ierr = MatSetValues(*J,1,&row,1,&col,&zero,INSERT_VALUES);CHKERRQ(ierr);
          }
          ierr = MatRestoreRow(Jv,row_e,&ncols,&cols,NULL);CHKERRQ(ierr);
        }
      } else { /* use dense block */
        ierr = MatSetDenseblock_private(J,nrows,rows,ncols,cstart);CHKERRQ(ierr);
      }

      /* Connected vertices */
      const PetscInt *cone;
      PetscInt       vc;
      ierr = DMNetworkGetConnectedNodes(dm,edges[e],&cone);CHKERRQ(ierr);
      vc = (v == cone[0]) ? cone[1]:cone[0];
   
      ierr = DMNetworkIsGhostVertex(dm,vc,&ghost);CHKERRQ(ierr);
      ierr = DMNetworkGetVariableGlobalOffset(dm,vc,&cstart);CHKERRQ(ierr);
      if (ghost) cstart = -(cstart + 1); /* Convert to actual global offset for ghost nodes */
      ierr = DMNetworkGetNumVariables(dm,vc,&ncols);CHKERRQ(ierr);
      ierr = MatSetDenseblock_private(J,nrows,rows,ncols,cstart);CHKERRQ(ierr);
    }

    /* Set matrix entries for vertex self */
    ierr = DMNetworkIsGhostVertex(dm,v,&ghost);CHKERRQ(ierr);
    if (!ghost) {
      PetscInt cstart;
      ierr = DMNetworkGetVariableGlobalOffset(dm,v,&cstart);CHKERRQ(ierr);
      ierr = MatSetDenseblock_private(J,nrows,rows,nrows,cstart);CHKERRQ(ierr);
    }
  }
  ierr = MatAssemblyBegin(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(*J,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
#if 0
  printf("\nMatrix J:\n");
  ierr = MatView(*J,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
#endif
  ierr = MatSetDM(*J,dm);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMDestroy_Network"
PetscErrorCode DMDestroy_Network(DM dm)
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*) dm->data;
  PetscInt       i;

  PetscFunctionBegin;
  if (--network->refct > 0) PetscFunctionReturn(0);
  if (network->Je) {
    for (i=0; i<3*network->nEdges; i++) {
      ierr = MatDestroy(&network->Je[i]);CHKERRQ(ierr);
    }
    ierr = PetscFree(network->Je);CHKERRQ(ierr);
  }
  if (network->Jv) {
    PetscInt       v,vStart,vEnd,nedges,*vptr=network->Jvptr;
    const PetscInt *edges;
    ierr = DMNetworkGetVertexRange(dm,&vStart,&vEnd);CHKERRQ(ierr);
    for (v=vStart; v<vEnd; v++) {
      ierr = MatDestroy(&network->Jv[vptr[v-vStart]]);CHKERRQ(ierr);
      ierr = DMNetworkGetSupportingEdges(dm,v,&nedges,&edges);CHKERRQ(ierr);
      for (i=0; i<2*nedges; i++) {
        ierr = MatDestroy(&network->Jv[vptr[v-vStart]+i+1]);CHKERRQ(ierr);
      }
    }
    ierr = PetscFree(network->Jvptr);CHKERRQ(ierr);
    ierr = PetscFree(network->Jv);CHKERRQ(ierr);
  }
  ierr = DMDestroy(&network->plex);CHKERRQ(ierr);
  network->edges = NULL;
  ierr = PetscSectionDestroy(&network->DataSection);CHKERRQ(ierr);
  ierr = PetscSectionDestroy(&network->DofSection);CHKERRQ(ierr);
 
  ierr = PetscFree(network->componentdataarray);CHKERRQ(ierr);
  ierr = PetscFree(network->cvalue);CHKERRQ(ierr);
  ierr = PetscFree(network->header);CHKERRQ(ierr);
  ierr = PetscFree(network);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__
#define __FUNCT__ "DMView_Network"
PetscErrorCode DMView_Network(DM dm, PetscViewer viewer)
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*) dm->data;

  PetscFunctionBegin;
  ierr = DMView(network->plex,viewer);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__ 
#define __FUNCT__ "DMGlobalToLocalBegin_Network"
PetscErrorCode DMGlobalToLocalBegin_Network(DM dm, Vec g, InsertMode mode, Vec l)
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*) dm->data;

  PetscFunctionBegin;
  ierr = DMGlobalToLocalBegin(network->plex,g,mode,l);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__ 
#define __FUNCT__ "DMGlobalToLocalEnd_Network"
PetscErrorCode DMGlobalToLocalEnd_Network(DM dm, Vec g, InsertMode mode, Vec l)
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*) dm->data;

  PetscFunctionBegin;
  ierr = DMGlobalToLocalEnd(network->plex,g,mode,l);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__ 
#define __FUNCT__ "DMLocalToGlobalBegin_Network"
PetscErrorCode DMLocalToGlobalBegin_Network(DM dm, Vec l, InsertMode mode, Vec g)
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*) dm->data;

  PetscFunctionBegin;
  ierr = DMLocalToGlobalBegin(network->plex,l,mode,g);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

#undef __FUNCT__ 
#define __FUNCT__ "DMLocalToGlobalEnd_Network"
PetscErrorCode DMLocalToGlobalEnd_Network(DM dm, Vec l, InsertMode mode, Vec g)
{
  PetscErrorCode ierr;
  DM_Network     *network = (DM_Network*) dm->data;

  PetscFunctionBegin;
  ierr = DMLocalToGlobalEnd(network->plex,l,mode,g);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}
