#pragma once

#include "BorisLib.h"
#include "Modules.h"



class Mesh;

#ifdef MODULE_COMPILATION_EXCHANGE

#include "ExchangeBase.h"

//Exchange modules can only be used in a magnetic mesh

class Exch_6ngbr_Neu :
	public Modules,
	public ExchangeBase,
	public ProgramState<Exch_6ngbr_Neu, std::tuple<>, std::tuple<>>
{

private:

	//pointer to mesh object holding this effective field module
	Mesh *pMesh;

public:

	Exch_6ngbr_Neu(Mesh *pMesh_);
	~Exch_6ngbr_Neu();

	//-------------------Implement ProgramState method

	void RepairObjectState(void) {}

	//-------------------Abstract base class method implementations

	void Uninitialize(void) { initialized = false; }

	BError Initialize(void);

	BError UpdateConfiguration(UPDATECONFIG_ cfgMessage);
	void UpdateConfiguration_Values(UPDATECONFIG_ cfgMessage) {}

	BError MakeCUDAModule(void);

	double UpdateField(void);

	//-------------------Energy density methods

	double GetEnergyDensity(Rect& avRect);
	double GetEnergy_Max(Rect& rectangle);

	//Compute exchange energy density and store it in displayVEC
	void Compute_Exchange(VEC<double>& displayVEC);
};

#else

class Exch_6ngbr_Neu :
	public Modules
{

public:

	Exch_6ngbr_Neu(Mesh *pMesh_) {}
	~Exch_6ngbr_Neu() {}

	//-------------------Abstract base class method implementations

	void Uninitialize(void) {}

	BError Initialize(void) { return BError(); }

	BError UpdateConfiguration(UPDATECONFIG_ cfgMessage) { return BError(); }
	void UpdateConfiguration_Values(UPDATECONFIG_ cfgMessage) {}

	BError MakeCUDAModule(void) { return BError(); }

	double UpdateField(void) { return 0.0; }

	//-------------------Energy density methods

	double GetEnergyDensity(Rect& avRect) { return 0.0; }
	double GetEnergy_Max(Rect& rectangle) { return 0.0; }

	void Compute_Exchange(VEC<double>& displayVEC) {}
};

#endif