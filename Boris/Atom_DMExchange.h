#pragma once

#include "BorisLib.h"
#include "Modules.h"



class Atom_Mesh;

#if defined(MODULE_COMPILATION_DMEXCHANGE) && ATOMISTIC == 1

//DM exchange for an atomistic simple cubic mesh

class Atom_DMExchange :
	public Modules,
	public ProgramState<Atom_DMExchange, std::tuple<>, std::tuple<>>
{
private:

	//pointer to mesh object holding this effective field module
	Atom_Mesh * paMesh;

	//divide energy by this to obtain energy density : this is the energy density in the entire mesh, which may not be rectangular.
	double non_empty_volume = 0.0;

public:

	Atom_DMExchange(Atom_Mesh *paMesh_);
	~Atom_DMExchange() {}

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

	//-------------------Energy methods

	//For simple cubic mesh spin_index coincides with index in M1
	double Get_Atomistic_Energy(int spin_index);
};

#else

class Atom_DMExchange :
	public Modules
{
private:

	//pointer to mesh object holding this effective field module
	Atom_Mesh * paMesh;

public:

	Atom_DMExchange(Atom_Mesh *paMesh_) {}
	~Atom_DMExchange() {}

	//-------------------Implement ProgramState method

	void RepairObjectState(void) {}

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

	//Compute exchange energy density and store it in displayVEC
	void Compute_Exchange(VEC<double>& displayVEC) {}
};


#endif
