#include "TransportCUDA.h"

#if COMPILECUDA == 1

#ifdef MODULE_TRANSPORT

#include "MeshCUDA.h"

#include "BorisCUDALib.cuh"

//-------------------Calculation Methods

void TransportCUDA::IterateChargeSolver_aSOR(bool start_iters, cuReal conv_error, cu_obj<cuReal>& max_error, cu_obj<cuReal>& max_value)
{
	pMeshCUDA->V()->IteratePoisson_aSOR(pMeshCUDA->n_e.dim(), (TransportCUDA_V_Funcs&)poisson_V, start_iters, conv_error, max_error, max_value);
}

void TransportCUDA::IterateChargeSolver_SOR(cu_obj<cuReal>& damping, cu_obj<cuReal>& max_error, cu_obj<cuReal>& max_value)
{
	pMeshCUDA->V()->IteratePoisson_SOR(pMeshCUDA->n_e.dim(), (TransportCUDA_V_Funcs&)poisson_V, damping, max_error, max_value);
}

#endif

#endif