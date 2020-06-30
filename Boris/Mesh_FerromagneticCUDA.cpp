#include "stdafx.h"
#include "Mesh_FerromagneticCUDA.h"

#if COMPILECUDA == 1
#ifdef MESH_COMPILATION_FERROMAGNETIC

#include "Mesh_Ferromagnetic.h"

FMeshCUDA::FMeshCUDA(FMesh* pMesh) :
	MeshCUDA(pMesh)
{
	pFMesh = pMesh;
}

FMeshCUDA::~FMeshCUDA()
{

}

//----------------------------------- IMPORTANT CONTROL METHODS

//call when a configuration change has occurred - some objects might need to be updated accordingly
BError FMeshCUDA::UpdateConfiguration(UPDATECONFIG_ cfgMessage)
{
	BError error(CLASS_STR(FMeshCUDA));

	if (ucfg::check_cfgflags(cfgMessage, UPDATECONFIG_MESHCHANGE)) {

		//resize arrays held in Mesh - if changing mesh to new size then use resize : this maps values from old mesh to new mesh size (keeping magnitudes). Otherwise assign magnetization along x in whole mesh.
		if (M()->size_cpu().dim()) {

			if (!M()->resize((cuReal3)h, (cuRect)meshRect)) return error(BERROR_OUTOFGPUMEMORY_CRIT);
		}
		else if (!M()->assign((cuReal3)h, (cuRect)meshRect, cuReal3(-(Ms()->get_current_cpu()), 0, 0))) return error(BERROR_OUTOFGPUMEMORY_CRIT);

		if (!Heff()->assign((cuReal3)h, (cuRect)meshRect, cuReal3())) return error(BERROR_OUTOFGPUMEMORY_CRIT);
	}

	return error;
}

cuBReal FMeshCUDA::CheckMoveMesh(bool antisymmetric, double threshold)
{
	//move mesh algorithm applied to systems containing domain walls in order to simulate domain wall movement.
	//In this case the magnetisation at the ends of the mesh must be fixed (magnetisation evolver not applied to the ends).
	//Find the average magnetisation projected along the absolute direction of the magnetisation at the ends.
	//For a domain wall at the centre this value should be close to zero. If it exceeds a threshold then move mesh by a cellsize along +x or -x.

	if (!pFMesh->move_mesh_trigger) return 0;

	//the ends should not be completely empty, and must have a constant magnetisation direction
	int cells_fixed = (int)ceil_epsilon((double)n.x * MOVEMESH_ENDRATIO);

	cuReal3 M_end = M()->average(n.dim(), cuBox(0, 0, 0, cells_fixed, n.y, n.z));
	cuReal3 M_av_left = M()->average(n.dim(), cuBox(cells_fixed, 0, 0, n.x / 2, n.y, n.z));
	cuReal3 M_av_right = M()->average(n.dim(), cuBox(n.x / 2, 0, 0, n.x - cells_fixed, n.y, n.z));

	cuBReal direction = (2 * cuBReal(antisymmetric) - 1);

	cuReal3 M_av = M_av_right + direction * M_av_left;

	if (GetMagnitude(M_end) * GetMagnitude(M_av)) {

		double Mratio = (M_end * M_av) / (GetMagnitude(M_end) * GetMagnitude(M_av));

		if (Mratio > threshold) return -h.x * direction;
		if (Mratio < -threshold) return h.x * direction;
	}

	return 0;
}

//----------------------------------- ENABLED MESH PROPERTIES CHECKERS

	//get exchange_couple_to_meshes status flag from the cpu version
bool FMeshCUDA::GetMeshExchangeCoupling(void)
{
	return pFMesh->GetMeshExchangeCoupling();
}

#endif
#endif