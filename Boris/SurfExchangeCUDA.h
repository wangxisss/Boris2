#pragma once

#include "Boris_Enums_Defs.h"
#if COMPILECUDA == 1

#ifdef MODULE_COMPILATION_SURFEXCHANGE

#include "BorisCUDALib.h"
#include "ModulesCUDA.h"

class MeshCUDA;
class SurfExchange;
class ManagedMeshCUDA;

class SurfExchangeCUDA :
	public ModulesCUDA
{

private:

	//pointer to CUDA version of mesh object holding the effective field module holding this CUDA module
	MeshCUDA* pMeshCUDA;

	//pointer to cpu version of SurfExchange
	SurfExchange* pSurfExch;

	//cu arrays with pointers to other meshes in surface exchange coupling with the mesh holding this module, top and bottom, (ferromagnetic)
	cu_arr<ManagedMeshCUDA> pMeshFM_Bot;
	cu_arr<ManagedMeshCUDA> pMeshFM_Top;

	//cu arrays with pointers to other meshes in surface exchange coupling with the mesh holding this module, top and bottom, (two-sublattice model meshes)
	cu_arr<ManagedMeshCUDA> pMeshAFM_Bot;
	cu_arr<ManagedMeshCUDA> pMeshAFM_Top;

	//cu arrays with pointers to diamagnetic meshes in surface exchange coupling with the mesh holding this module
	cu_arr<ManagedMeshCUDA> pMeshDia_Bot;
	cu_arr<ManagedMeshCUDA> pMeshDia_Top;

	//coupled cells in gpu memory
	cu_obj<int> coupled_cells;

public:

	SurfExchangeCUDA(MeshCUDA* pMeshCUDA_, SurfExchange* pSurfExch_);
	~SurfExchangeCUDA();

	//-------------------Abstract base class method implementations

	void Uninitialize(void) { initialized = false; }

	BError Initialize(void);

	BError UpdateConfiguration(UPDATECONFIG_ cfgMessage);
	void UpdateConfiguration_Values(UPDATECONFIG_ cfgMessage) {}

	void UpdateField(void);

	//-------------------

};

#else

class SurfExchangeCUDA
{
};

#endif

#endif