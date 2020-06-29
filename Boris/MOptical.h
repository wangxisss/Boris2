#pragma once

#include "BorisLib.h"
#include "Modules.h"

using namespace std;

class Mesh;

#ifdef MODULE_COMPILATION_MOPTICAL

//Magneto-optical module can only be used in a magnetic mesh

class MOptical :
	public Modules,
	public ProgramState<MOptical, tuple<>, tuple<>>
{

#if COMPILECUDA == 1
	friend class MOpticalCUDA;
#endif

private:

	//pointer to mesh object holding this effective field module
	Mesh *pMesh;

private:

public:

	MOptical(Mesh *pMesh_);
	~MOptical();

	//-------------------Implement ProgramState method

	void RepairObjectState(void) {}

	//-------------------Abstract base class method implementations

	void Uninitialize(void) { initialized = false; }

	BError Initialize(void);

	BError UpdateConfiguration(UPDATECONFIG_ cfgMessage);
	void UpdateConfiguration_Values(UPDATECONFIG_ cfgMessage);

	BError MakeCUDAModule(void);

	double UpdateField(void);

	//-------------------Energy density methods

	double GetEnergyDensity(Rect& avRect) { return 0.0; }

	//-------------------
};

#else

class MOptical :
	public Modules
{

private:

public:

	MOptical(Mesh *pMesh_) {}
	~MOptical() {}

	//-------------------Abstract base class method implementations

	void Uninitialize(void) {}

	BError Initialize(void) { return BError(); }

	BError UpdateConfiguration(UPDATECONFIG_ cfgMessage) { return BError(); }
	void UpdateConfiguration_Values(UPDATECONFIG_ cfgMessage) {}

	BError MakeCUDAModule(void) { return BError(); }

	double UpdateField(void) { return 0.0; }

	//-------------------Energy density methods

	double GetEnergyDensity(Rect& avRect) { return 0.0; }

	//-------------------
};

#endif
