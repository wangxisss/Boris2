#pragma once

#include "Boris_Enums_Defs.h"
#if COMPILECUDA == 1

#ifdef MODULE_SDEMAG

#include "ModulesCUDA.h"

#include "ConvolutionCUDA.h"
#include "DemagKernelCUDA.h"
#include "DemagKernelCollectionCUDA.h"
#include "SDemagCUDA_KernelCollection.h"

#include "SDemagCUDA_Demag.h"

class SuperMesh;
class SDemag;
class SDemagCUDA_Demag;
class ManagedMeshCUDA;

class SDemagCUDA :
	public ModulesCUDA,
	public ConvolutionCUDA<DemagKernelCUDA>
{

	friend SDemagCUDA_Demag;

private:

	//pointer to CUDA version of mesh object holding the effective field module holding this CUDA module
	SuperMesh* pSMesh;

	//the SDemag module (cpu version) holding this CUDA version
	SDemag* pSDemag;

	//--------

	//super-mesh magnetization values used for computing demag field on the super-mesh
	cu_obj<cuVEC<cuReal3>> sm_Vals;

	//--------

	//collection of all SDemagCUDA_Demag modules in individual ferromagnetic meshes
	vector<SDemagCUDA_Demag*> pSDemagCUDA_Demag;

	//collect FFT input spaces : after Forward FFT the ffts of M from the individual meshes will be found here
	//These are used as inputs to kernel multiplications. Same order as pSDemag_Demag.
	std::vector<cu_arr<cuComplex>*> FFT_Spaces_x_Input, FFT_Spaces_y_Input, FFT_Spaces_z_Input;

	//Output FFT spaces, but collected not in a std::vector but in a cu_arr so we can pass them to a __global__
	cu_arr<cuComplex*> FFT_Spaces_x_Output, FFT_Spaces_y_Output, FFT_Spaces_z_Output;

	//Kernels used in multi-layered convolution sorted by kernel, i.e. each kernel held in KerTypeCollectionCUDA is unique, and may have a number of input and output spaces for which it is used.
	//TESTING ONLY - SLOWER THAN MULTIPLE INPUTS VERSION SO NOT IN CURRENT USE
	vector<KerTypeCollectionCUDA*> Kernels;

	//collection of rectangles of meshes, same ordering as for pSDemag_Demag and FFT_Spaces, used in multi-layered convolution
	//these are not necessarily the rectangles of the input M meshes, but are the rectangles of the transfer meshes (M -> transfer -> convolution)
	//for instance in 3D mode, all rectangles in multi-layered convolution must have same size
	//in 2D mode the rectangles can differ in thickness but must have the same xy size
	//thus in 3D mode find largest one and extend all the other rectangles to match (if possible try to have them overlapping in xy-plane projections so we can use kernel symmetries)
	//in 2D mode find largest xy dimension and extend all xy dimensions -> again try to overlap their xy-plane projections
	vector<cuRect> Rect_collection;

	//demag kernels used for multilayered convolution, one collection per mesh/SDemag_Demag module. Don't recalculate redundant kernels in the collection.
	vector<DemagKernelCollectionCUDA*> kernel_collection;

public:

	SDemagCUDA(SuperMesh* pSMesh_, SDemag* pSDemag_);
	~SDemagCUDA();

	//-------------------Abstract base class method implementations

	void Uninitialize(void) { initialized = false; }

	void UninitializeAll(void);

	BError Initialize(void);

	BError UpdateConfiguration(UPDATECONFIG_ cfgMessage = UPDATECONFIG_GENERIC);

	void UpdateField(void);

	//-------------------

	cu_obj<cuVEC<cuReal3>>& GetDemagField(void) { return sm_Vals; }
};

#else

class SDemagCUDA
{
};

#endif

#endif

