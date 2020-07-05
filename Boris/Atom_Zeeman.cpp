#include "stdafx.h"
#include "Atom_Zeeman.h"

#if defined(MODULE_COMPILATION_ZEEMAN) && ATOMISTIC == 1

#include "Atom_Mesh.h"
#include "Atom_MeshParamsControl.h"

#include "SuperMesh.h"

#if COMPILECUDA == 1
#include "Atom_ZeemanCUDA.h"
#endif

Atom_Zeeman::Atom_Zeeman(Atom_Mesh *paMesh_) :
	Modules(),
	ZeemanBase(),
	ProgramStateNames(this, { VINFO(Ha), VINFO(H_equation) }, {})
{
	paMesh = paMesh_;

	pSMesh = paMesh->pSMesh;

	error_on_create = UpdateConfiguration(UPDATECONFIG_FORCEUPDATE);

	//-------------------------- Is CUDA currently enabled?

	//If cuda is enabled we also need to make the cuda module version
	if (paMesh->cudaEnabled) {

		if (!error_on_create) error_on_create = SwitchCUDAState(true);
	}
}

Atom_Zeeman::~Atom_Zeeman()
{
}

BError Atom_Zeeman::Initialize(void)
{
	BError error(CLASS_STR(Atom_Zeeman));

	initialized = true;

	non_empty_volume = paMesh->Get_NonEmpty_Magnetic_Volume();

	return error;
}

BError Atom_Zeeman::UpdateConfiguration(UPDATECONFIG_ cfgMessage)
{
	BError error(CLASS_STR(Zeeman));

	if (ucfg::check_cfgflags(cfgMessage, UPDATECONFIG_MESHCHANGE)) {

		Uninitialize();

		//update mesh dimensions in equation constants
		if (H_equation.is_set()) {

			DBL3 meshDim = paMesh->GetMeshDimensions();

			H_equation.set_constant("Lx", meshDim.x, false);
			H_equation.set_constant("Ly", meshDim.y, false);
			H_equation.set_constant("Lz", meshDim.z, false);
			H_equation.remake_equation();
		}

		Initialize();
	}

	//------------------------ CUDA UpdateConfiguration if set

#if COMPILECUDA == 1
	if (pModuleCUDA) {

		if (!error) error = pModuleCUDA->UpdateConfiguration(cfgMessage);
	}
#endif

	return error;
}

void Atom_Zeeman::UpdateConfiguration_Values(UPDATECONFIG_ cfgMessage)
{
	if (cfgMessage == UPDATECONFIG_TEQUATION_CONSTANTS) {

		UpdateTEquationUserConstants(false);
	}
	else if (cfgMessage == UPDATECONFIG_TEQUATION_CLEAR) {

		if (H_equation.is_set()) H_equation.clear();
	}

#if COMPILECUDA == 1
	if (pModuleCUDA) {

		pModuleCUDA->UpdateConfiguration_Values(cfgMessage);
	}
#endif
}

BError Atom_Zeeman::MakeCUDAModule(void)
{
	BError error(CLASS_STR(Atom_Zeeman));

#if COMPILECUDA == 1

		if (paMesh->paMeshCUDA) {

			//Note : it is posible pMeshCUDA has not been allocated yet, but this module has been created whilst cuda is switched on. This will happen when a new mesh is being made which adds this module by default.
			//In this case, after the mesh has been fully made, it will call SwitchCUDAState on the mesh, which in turn will call this SwitchCUDAState method; then pMeshCUDA will not be nullptr and we can make the cuda module version
			pModuleCUDA = new Atom_ZeemanCUDA(paMesh->paMeshCUDA, this);
			error = pModuleCUDA->Error_On_Create();
		}

#endif

	return error;
}

double Atom_Zeeman::UpdateField(void)
{
	/////////////////////////////////////////
	// Fixed set field
	/////////////////////////////////////////

	double energy = 0;

	if (!H_equation.is_set()) {

		if (IsZ(Ha.norm())) {

			this->energy = 0;
			return 0.0;
		}

#pragma omp parallel for reduction(+:energy)
		for (int idx = 0; idx < paMesh->n.dim(); idx++) {

			double cHA = paMesh->cHA;
			paMesh->update_parameters_mcoarse(idx, paMesh->cHA, cHA);

			paMesh->Heff1[idx] += cHA * Ha;

			energy -= MUB * paMesh->M1[idx] * MU0 * (cHA * Ha);
		}
	}

	/////////////////////////////////////////
	// Field set from user equation
	/////////////////////////////////////////

	else {

		double time = pSMesh->GetStageTime();
		double Temperature = paMesh->base_temperature;

#pragma omp parallel for reduction(+:energy)
		for (int j = 0; j < paMesh->n.y; j++) {
			for (int k = 0; k < paMesh->n.z; k++) {
				for (int i = 0; i < paMesh->n.x; i++) {

					int idx = i + j * paMesh->n.x + k * paMesh->n.x*paMesh->n.y;

					//on top of spatial dependence specified through an equation, also allow spatial dependence through the cHA parameter
					double cHA = paMesh->cHA;
					paMesh->update_parameters_mcoarse(idx, paMesh->cHA, cHA);

					DBL3 relpos = DBL3(i + 0.5, j + 0.5, k + 0.5) & paMesh->h;
					DBL3 H = H_equation.evaluate_vector(relpos.x, relpos.y, relpos.z, time);

					paMesh->Heff1[idx] += cHA * H;

					energy -= MUB * paMesh->M1[idx] * MU0 * (cHA * H);
				}
			}
		}
	}

	//convert to energy density and return
	if (non_empty_volume) this->energy = energy / non_empty_volume;
	else this->energy = 0.0;

	return this->energy;
}

//-------------------Energy density methods

double Atom_Zeeman::GetEnergyDensity(Rect& avRect)
{
#if COMPILECUDA == 1
	if (pModuleCUDA) return dynamic_cast<Atom_ZeemanCUDA*>(pModuleCUDA)->GetEnergyDensity(avRect);
#endif

	/////////////////////////////////////////
	// Fixed set field
	/////////////////////////////////////////

	double energy = 0;

	int num_points = 0;

	if (!H_equation.is_set()) {

		if (IsZ(Ha.norm())) {

			this->energy = 0;
			return 0.0;
		}

#pragma omp parallel for reduction(+:energy, num_points)
		for (int idx = 0; idx < paMesh->n.dim(); idx++) {

			//only average over values in given rectangle
			if (!avRect.contains(paMesh->M1.cellidx_to_position(idx))) continue;
			
			double cHA = paMesh->cHA;
			paMesh->update_parameters_mcoarse(idx, paMesh->cHA, cHA);

			energy -= MUB * paMesh->M1[idx] * MU0 * (cHA * Ha);
			num_points++;
		}
	}

	/////////////////////////////////////////
	// Field set from user equation
	/////////////////////////////////////////

	else {

		double time = pSMesh->GetStageTime();
		double Temperature = paMesh->base_temperature;

#pragma omp parallel for reduction(+:energy, num_points)
		for (int j = 0; j < paMesh->n.y; j++) {
			for (int k = 0; k < paMesh->n.z; k++) {
				for (int i = 0; i < paMesh->n.x; i++) {

					int idx = i + j * paMesh->n.x + k * paMesh->n.x*paMesh->n.y;

					//only average over values in given rectangle
					if (!avRect.contains(paMesh->M1.cellidx_to_position(idx))) continue;

					//on top of spatial dependence specified through an equation, also allow spatial dependence through the cHA parameter
					double cHA = paMesh->cHA;
					paMesh->update_parameters_mcoarse(idx, paMesh->cHA, cHA);

					DBL3 relpos = DBL3(i + 0.5, j + 0.5, k + 0.5) & paMesh->h;
					DBL3 H = H_equation.evaluate_vector(relpos.x, relpos.y, relpos.z, time);

					energy -= MUB * paMesh->M1[idx] * MU0 * (cHA * H);
					num_points++;
				}
			}
		}
	}

	//convert to energy density and return
	if (num_points) energy = energy / (num_points * paMesh->M1.h.dim());
	else energy = 0.0;

	return energy;
}

//----------------------------------------------- Others

void Atom_Zeeman::SetField(DBL3 Hxyz)
{
	//fixed field is being set - remove any equation settings
	if (H_equation.is_set()) H_equation.clear();

	Ha = Hxyz;

	//-------------------------- CUDA mirroring

#if COMPILECUDA == 1
	if (pModuleCUDA) dynamic_cast<Atom_ZeemanCUDA*>(pModuleCUDA)->SetField(Ha);
#endif
}

DBL3 Atom_Zeeman::GetField(void)
{
	if (H_equation.is_set()) {

		DBL3 meshDim = paMesh->GetMeshDimensions();

		return H_equation.evaluate_vector(meshDim.x / 2, meshDim.y / 2, meshDim.z / 2, pSMesh->GetStageTime());
	}
	else return Ha;
}

BError Atom_Zeeman::SetFieldEquation(string equation_string, int step)
{
	BError error(CLASS_STR(Atom_Zeeman));

	DBL3 meshDim = paMesh->GetMeshDimensions();

	//set equation if not already set, or this is the first step in a stage
	if (!H_equation.is_set() || step == 0) {

		//important to set user constants first : if these are required then the make_from_string call will fail. This may mean the user constants are not set when we expect them to.
		UpdateTEquationUserConstants(false);

		if (!H_equation.make_from_string(equation_string, { {"Lx", meshDim.x}, {"Ly", meshDim.y}, {"Lz", meshDim.z}, {"Tb", paMesh->base_temperature}, {"Ss", step} })) return error(BERROR_INCORRECTSTRING);
	}
	else {

		//equation set and not the first step : adjust Ss constant
		H_equation.set_constant("Ss", step);
		UpdateTEquationUserConstants(false);
	}

	//-------------------------- CUDA mirroring

#if COMPILECUDA == 1
	if (pModuleCUDA) error = dynamic_cast<Atom_ZeemanCUDA*>(pModuleCUDA)->SetFieldEquation(H_equation.get_vector_fspec());
#endif

	return error;
}

//Update TEquation object with user constants values
void Atom_Zeeman::UpdateTEquationUserConstants(bool makeCuda)
{
	if (paMesh->userConstants.size()) {

		vector<pair<string, double>> constants(paMesh->userConstants.size());
		for (int idx = 0; idx < paMesh->userConstants.size(); idx++) {

			constants[idx] = { paMesh->userConstants.get_key_from_index(idx), paMesh->userConstants[idx] };
		}

		H_equation.set_constants(constants);

		//-------------------------- CUDA mirroring

#if COMPILECUDA == 1
		if (pModuleCUDA && makeCuda) dynamic_cast<Atom_ZeemanCUDA*>(pModuleCUDA)->SetFieldEquation(H_equation.get_vector_fspec());
#endif
	}
}

//if base temperature changes we need to adjust Tb in H_equation if it's used.
void Atom_Zeeman::SetBaseTemperature(double Temperature)
{
	if (H_equation.is_set()) {

		H_equation.set_constant("Tb", Temperature);
	}

	//-------------------------- CUDA mirroring

#if COMPILECUDA == 1
	if (pModuleCUDA) dynamic_cast<Atom_ZeemanCUDA*>(pModuleCUDA)->SetFieldEquation(H_equation.get_vector_fspec());
#endif
}

#endif