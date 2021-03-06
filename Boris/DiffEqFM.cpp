#include "stdafx.h"
#include "DiffEqFM.h"

#ifdef MESH_COMPILATION_FERROMAGNETIC

#include "Mesh_Ferromagnetic.h"

DifferentialEquationFM::DifferentialEquationFM(FMesh *pMesh):
	DifferentialEquation(pMesh)
{
	error_on_create = UpdateConfiguration(UPDATECONFIG_FORCEUPDATE);
}

DifferentialEquationFM::~DifferentialEquationFM()
{
}

//---------------------------------------- OTHERS

//Restore magnetization after a failed step for adaptive time-step methods
void DifferentialEquationFM::Restoremagnetization(void)
{
#pragma omp parallel for
	for (int idx = 0; idx < pMesh->n.dim(); idx++)
		pMesh->M[idx] = sM1[idx];
}

//---------------------------------------- SET-UP METHODS

BError DifferentialEquationFM::AllocateMemory(void)
{
	BError error(CLASS_STR(DifferentialEquationFM));

	//first make sure everything not needed is cleared
	CleanupMemory();

	if (!sM1.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);

	switch (evalMethod) {

	case EVAL_EULER:
		break;

	case EVAL_TEULER:
	case EVAL_AHEUN:
		break;

	case EVAL_ABM:
		if (!sEval0.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval1.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		break;

	case EVAL_RK4:
		if (!sEval0.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval1.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval2.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		break;

	case EVAL_RK23:
		if (!sEval0.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval1.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval2.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		break;

	case EVAL_RKF:
		if (!sEval0.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval1.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval2.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval3.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval4.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		break;

	case EVAL_RKCK:
		if (!sEval0.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval1.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval2.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval3.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval4.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		break;

	case EVAL_RKDP:
		if (!sEval0.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval1.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval2.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval3.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval4.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!sEval5.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		break;

	case EVAL_SD:
		if (!sEval0.resize(pMesh->n)) return error(BERROR_OUTOFMEMORY_CRIT);
		break;
	}

	//For stochastic equations must also allocate memory for thermal VECs

	switch (setODE) {

	case ODE_SLLG:
	case ODE_SLLGSTT:
	case ODE_SLLGSA:
		if (!H_Thermal.resize(pMesh->h_s, pMesh->meshRect)) return error(BERROR_OUTOFMEMORY_CRIT);
		break;

	case ODE_SLLB:
	case ODE_SLLBSTT:
	case ODE_SLLBSA:
		if (!H_Thermal.resize(pMesh->h_s, pMesh->meshRect)) return error(BERROR_OUTOFMEMORY_CRIT);
		if (!Torque_Thermal.resize(pMesh->h_s, pMesh->meshRect)) return error(BERROR_OUTOFMEMORY_CRIT);
		break;
	}

	//----------------------- CUDA mirroring

#if COMPILECUDA == 1
	if (pmeshODECUDA) {

		if (!error) error = pmeshODECUDA->AllocateMemory();
	}
#endif

	return error;
}

void DifferentialEquationFM::CleanupMemory(void)
{
	//Only clear vectors not used for current evaluation method
	sM1.clear();

	if (evalMethod != EVAL_RK4 &&
		evalMethod != EVAL_ABM &&
		evalMethod != EVAL_RK23 &&
		evalMethod != EVAL_RKF &&
		evalMethod != EVAL_RKCK &&
		evalMethod != EVAL_RKDP &&
		evalMethod != EVAL_SD) {

		sEval0.clear();
	}

	if (evalMethod != EVAL_RK4 &&
		evalMethod != EVAL_ABM &&
		evalMethod != EVAL_RK23 &&
		evalMethod != EVAL_RKF &&
		evalMethod != EVAL_RKCK &&
		evalMethod != EVAL_RKDP) {

		sEval1.clear();
	}

	if (evalMethod != EVAL_RK4 &&
		evalMethod != EVAL_RK23 &&
		evalMethod != EVAL_RKF &&
		evalMethod != EVAL_RKCK &&
		evalMethod != EVAL_RKDP) {

		sEval2.clear();
	}

	if (evalMethod != EVAL_RK4 &&
		evalMethod != EVAL_RKF &&
		evalMethod != EVAL_RKCK &&
		evalMethod != EVAL_RKDP) {

		sEval3.clear();
		sEval4.clear();
	}

	if (evalMethod != EVAL_RKDP) {

		sEval5.clear();
	}

	//For thermal vecs only clear if not used for current set ODE
	if (setODE != ODE_SLLG &&
		setODE != ODE_SLLGSTT &&
		setODE != ODE_SLLB &&
		setODE != ODE_SLLBSTT &&
		setODE != ODE_SLLGSA &&
		setODE != ODE_SLLBSA) {

		H_Thermal.clear();
	}

	if (setODE != ODE_SLLB &&
		setODE != ODE_SLLBSTT &&
		setODE != ODE_SLLBSA) {

		Torque_Thermal.clear();
	}

	//----------------------- CUDA mirroring

#if COMPILECUDA == 1
	if (pmeshODECUDA) { pmeshODECUDA->CleanupMemory(); }
#endif
}


BError DifferentialEquationFM::UpdateConfiguration(UPDATECONFIG_ cfgMessage)
{
	BError error(CLASS_STR(DifferentialEquationFM));

	if (ucfg::check_cfgflags(cfgMessage, UPDATECONFIG_MESHCHANGE, UPDATECONFIG_ODE_SOLVER)) {

		if (pMesh->link_stochastic) {

			pMesh->h_s = pMesh->h;
			pMesh->n_s = pMesh->n;
		}
		else {

			pMesh->n_s = round(pMesh->meshRect / pMesh->h_s);
			if (pMesh->n_s.x == 0) pMesh->n_s.x = 1;
			if (pMesh->n_s.y == 0) pMesh->n_s.y = 1;
			if (pMesh->n_s.z == 0) pMesh->n_s.z = 1;
			pMesh->h_s = pMesh->meshRect / pMesh->n_s;
		}

		error = AllocateMemory();
	}

	if (ucfg::check_cfgflags(cfgMessage, UPDATECONFIG_ODE_MOVEMESH)) {

		if (!error) {

			//set skip cells flags for moving mesh if enabled
			if (moving_mesh) {

				Rect mesh_rect = pMesh->GetMeshRect();

				DBL3 end_size = mesh_rect.size() & DBL3(MOVEMESH_ENDRATIO, 1, 1);

				Rect end_rect_left = Rect(mesh_rect.s, mesh_rect.s + end_size);
				Rect end_rect_right = Rect(mesh_rect.e - end_size, mesh_rect.e);

				pMesh->M.set_skipcells(end_rect_left);
				pMesh->M.set_skipcells(end_rect_right);
			}
			else {

				pMesh->M.clear_skipcells();
			}
		}
	}

	//----------------------- CUDA mirroring

#if COMPILECUDA == 1
	if (pmeshODECUDA) {

		if (!error) error = pmeshODECUDA->UpdateConfiguration(cfgMessage);
	}
#endif

	return error;
}

//switch CUDA state on/off
BError DifferentialEquationFM::SwitchCUDAState(bool cudaState)
{
	BError error(CLASS_STR(DifferentialEquationFM));

#if COMPILECUDA == 1

	//are we switching to cuda?
	if (cudaState) {

		if (!pmeshODECUDA) {

			pmeshODECUDA = new DifferentialEquationFMCUDA(this);
			error = pmeshODECUDA->Error_On_Create();
		}
	}
	else {

		//cuda switched off so delete cuda module object
		if (pmeshODECUDA) delete pmeshODECUDA;
		pmeshODECUDA = nullptr;
	}

#endif

	return error;
}

//---------------------------------------- GETTERS

//return dM by dT - should only be used when evaluation sequence has ended (TimeStepSolved() == true)
DBL3 DifferentialEquationFM::dMdt(int idx)
{
	return (pMesh->M[idx] - sM1[idx]) / dT_last;
}

#endif
