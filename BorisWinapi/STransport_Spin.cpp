#include "stdafx.h"
#include "STransport.h"

#ifdef MODULE_TRANSPORT

#include "SuperMesh.h"

void STransport::solve_spin_transport_sor(void)
{
	//--------------

	DBL2 max_error = DBL2();

	iters_to_conv = 0;

	bool start_iters = true;

	//1. Solve V everywhere for current S until convergence criteria hit

	do {

		//get max error : the max change in V from one iteration to the next
		max_error = DBL2();

		//solve V in each mesh separately (1 iteration each) - but do not set CMBND cells yet
		for (int idx = 0; idx < (int)pTransport.size(); idx++) {

			DBL2 error;

			if (fixed_SOR_damping) error = pTransport[idx]->IterateSpinSolver_Charge_SOR(SOR_damping.i);
			else error = pTransport[idx]->IterateSpinSolver_Charge_aSOR(start_iters, s_errorMax);

			if (error.first > max_error.first) max_error.first = error.first;
			if (error.second > max_error.second) max_error.second = error.second;
		}

		//normalize error to maximum change
		max_error.first = (max_error.second > 0 ? max_error.first / max_error.second : max_error.first);

		start_iters = false;

		//now set CMBND cells for V
		set_cmbnd_spin_transport_V();

		iters_to_conv++;

	} while (max_error.first > errorMaxLaplace && iters_to_conv < maxLaplaceIterations);

	//2. update Jc in all meshes if a significant change occured
	for (int idx = 0; idx < (int)pTransport.size(); idx++) {

		pTransport[idx]->CalculateCurrentDensity();
	}

	//--------------

	//3. Solve S everywhere based on obtained Jc until convergence criteria hit

	s_iters_to_conv = 0;

	start_iters = true;

	do {

		aSOR_damping = DBL2();

		//get max error : the max change in |S| from one iteration to the next
		max_error = DBL2();

		//solve S in each mesh separately (1 iteration each) - but do not set CMBND cells yet
		for (int idx = 0; idx < (int)pTransport.size(); idx++) {

			DBL2 error;

			if (fixed_SOR_damping) error = pTransport[idx]->IterateSpinSolver_Spin_SOR(SOR_damping.j);
			else error = pTransport[idx]->IterateSpinSolver_Spin_aSOR(start_iters, s_errorMax);

			if (error.first > max_error.first) max_error.first = error.first;
			if (error.second > max_error.second) max_error.second = error.second;

			//store minimum and maximum damping values
			double damping = pS[idx]->aSOR_get_damping();
			if (aSOR_damping == DBL2()) aSOR_damping = DBL2(damping, damping);
			else aSOR_damping = DBL2(min(aSOR_damping.i, damping), max(aSOR_damping.j, damping));
		}

		//normalize error to maximum change
		max_error.first = (max_error.second > 0 ? max_error.first / max_error.second : max_error.first);

		start_iters = false;

		//now set CMBND cells for S
		set_cmbnd_spin_transport_S();

		s_iters_to_conv++;

	} while (max_error.first > s_errorMax && s_iters_to_conv < s_maxIterations);

	//store the current max error in the energy term so it can be read if requested
	energy = max_error.first;

	//always recalculate transport when using spin current solver - magnetization is bound to change, which will require updating s at least.
	recalculate_transport = true;
}

//-------------------CMBND computation methods

//------------ V (electrical potential)

void STransport::set_cmbnd_spin_transport_V(void)
{	
	for (int idx1 = 0; idx1 < (int)CMBNDcontacts.size(); idx1++) {

		for (int idx2 = 0; idx2 < (int)CMBNDcontacts[idx1].size(); idx2++) {

			int idx_sec = CMBNDcontacts[idx1][idx2].mesh_idx.i;
			int idx_pri = CMBNDcontacts[idx1][idx2].mesh_idx.j;

			//use continuity of Jc and V across interface unless the interface is N-F type (normal metal - ferromagnetic) and the spin mixing conductance is not zero (i.e. continuous method disabled).

			//Is it an N-F contact?
			if ((pTransport[idx_pri]->pMesh->MComputation_Enabled() && !pTransport[idx_sec]->pMesh->Magnetisation_Enabled()) ||
				(pTransport[idx_sec]->pMesh->MComputation_Enabled() && !pTransport[idx_pri]->pMesh->Magnetisation_Enabled())) {

				//Yes we have an N-F contact. Is G interface enabled for this contact ? (remember top mesh sets G interface values)

				//if primary is top then we check GInterface in primary. If primary is bottom then we check GInterface in secondary as it must be top.
				if ((CMBNDcontacts[idx1][idx2].IsPrimaryTop() && pTransport[idx_pri]->pMesh->GInterface_Enabled()) ||
					(!CMBNDcontacts[idx1][idx2].IsPrimaryTop() && pTransport[idx_sec]->pMesh->GInterface_Enabled())) {

					//G interface method

					pV[idx_pri]->set_cmbnd_continuousflux<Transport, STransport>(
						*pV[idx_sec], CMBNDcontacts[idx1][idx2],
						&Transport::afunc_st_V_sec, &Transport::afunc_st_V_pri,
						&Transport::bfunc_st_V_sec, &Transport::bfunc_st_V_pri,
						&Transport::diff2_st_V_sec, &Transport::diff2_st_V_pri,
						&STransport::Afunc_V, &STransport::Bfunc_V,
						*pTransport[idx_sec], *pTransport[idx_pri], *this);

					//next contact
					continue;
				}
			}

			//continuous interface method - the G interface method check above didn't pass so this is what we have left

			pV[idx_pri]->set_cmbnd_continuous<Transport>(
				*pV[idx_sec], CMBNDcontacts[idx1][idx2],
				&Transport::afunc_st_V_sec, &Transport::afunc_st_V_pri,
				&Transport::bfunc_st_V_sec, &Transport::bfunc_st_V_pri,
				&Transport::diff2_st_V_sec, &Transport::diff2_st_V_pri,
				*pTransport[idx_sec], *pTransport[idx_pri]);
		}
	}
}

//------------ S (spin accumulation, or spin potential)

//calculate and set values at composite media boundaries for S
void STransport::set_cmbnd_spin_transport_S(void)
{	
	for (int idx1 = 0; idx1 < (int)CMBNDcontacts.size(); idx1++) {

		for (int idx2 = 0; idx2 < (int)CMBNDcontacts[idx1].size(); idx2++) {

			int idx_sec = CMBNDcontacts[idx1][idx2].mesh_idx.i;
			int idx_pri = CMBNDcontacts[idx1][idx2].mesh_idx.j;

			//use continuity of Js and S across interface unless the interface is N-F type (normal metal - ferromagnetic) and the spin mixing conductance is not zero (i.e. continuous method disabled).

			//Is it an N-F contact?
			if ((pTransport[idx_pri]->pMesh->MComputation_Enabled() && !pTransport[idx_sec]->pMesh->Magnetisation_Enabled()) ||
				(pTransport[idx_sec]->pMesh->MComputation_Enabled() && !pTransport[idx_pri]->pMesh->Magnetisation_Enabled())) {

				//Yes we have an N-F contact. Is G interface enabled for this contact ? (remember top mesh sets G interface values)

				//if primary is top then we check GInterface in primary. If primary is bottom then we check GInterface in secondary as it must be top.
				if ((CMBNDcontacts[idx1][idx2].IsPrimaryTop() && pTransport[idx_pri]->pMesh->GInterface_Enabled()) ||
					(!CMBNDcontacts[idx1][idx2].IsPrimaryTop() && pTransport[idx_sec]->pMesh->GInterface_Enabled())) {

					//G interface method

					if (pTransport[idx_pri]->pMesh->MComputation_Enabled()) {

						//interface conductance method with F being the primary mesh

						pS[idx_pri]->set_cmbnd_discontinuous<Transport, STransport, DBL33>(
							*pS[idx_sec], CMBNDcontacts[idx1][idx2],
							&Transport::afunc_st_S_sec, &Transport::afunc_st_S_pri,
							&Transport::bfunc_st_S_sec, &Transport::bfunc_st_S_pri,
							&Transport::diff2_st_S_sec, &Transport::diff2_st_S_pri,
							&STransport::Afunc_N_S, &STransport::Bfunc_N_S,
							&STransport::Afunc_F_S, &STransport::Bfunc_F_S,
							&Transport::cfunc_sec, &Transport::cfunc_pri,
							*pTransport[idx_sec], *pTransport[idx_pri], *this);

					}
					else {

						//interface conductance method with N being the primary mesh

						pS[idx_pri]->set_cmbnd_discontinuous<Transport, STransport, DBL33>(
							*pS[idx_sec], CMBNDcontacts[idx1][idx2],
							&Transport::afunc_st_S_sec, &Transport::afunc_st_S_pri,
							&Transport::bfunc_st_S_sec, &Transport::bfunc_st_S_pri,
							&Transport::diff2_st_S_sec, &Transport::diff2_st_S_pri,
							&STransport::Afunc_F_S, &STransport::Bfunc_F_S,
							&STransport::Afunc_N_S, &STransport::Bfunc_N_S,
							&Transport::cfunc_sec, &Transport::cfunc_pri,
							*pTransport[idx_sec], *pTransport[idx_pri], *this);
					}

					//next contact
					continue;
				}
			}

			//continuous interface method - the G interface method check above didn't pass so this is what we have left

			pS[idx_pri]->set_cmbnd_continuous<Transport>(
				*pS[idx_sec], CMBNDcontacts[idx1][idx2],
				&Transport::afunc_st_S_sec, &Transport::afunc_st_S_pri,
				&Transport::bfunc_st_S_sec, &Transport::bfunc_st_S_pri,
				&Transport::diff2_st_S_sec, &Transport::diff2_st_S_pri,
				*pTransport[idx_sec], *pTransport[idx_pri]);
			
		}
	}
}

//------------

//Calculate interface spin accumulation torque (in magnetic meshes for NF interfaces with G interface conductance set)
void STransport::CalculateSAInterfaceField(void)
{
	//calculate and add Ts values in Ts_interf VECs
	for (int idx1 = 0; idx1 < (int)CMBNDcontacts.size(); idx1++) {

		for (int idx2 = 0; idx2 < (int)CMBNDcontacts[idx1].size(); idx2++) {

			int idx_sec = CMBNDcontacts[idx1][idx2].mesh_idx.i;
			int idx_pri = CMBNDcontacts[idx1][idx2].mesh_idx.j;

			pTransport[idx_pri]->CalculateSAInterfaceField(pTransport[idx_sec], CMBNDcontacts[idx1][idx2]);
		}
	}
}

//Calculate interface spin accumulation torque only in mesh with matching transport module
VEC<DBL3>& STransport::GetInterfacialSpinTorque(Transport* pMeshTrans)
{
	if (!pMeshTrans->PrepareDisplayVEC(pMeshTrans->pMesh->h))
		return pMeshTrans->displayVEC;

	//calculate interfacial spin torque in displayVEC from all contacts with matching mesh
	for (int idx1 = 0; idx1 < (int)CMBNDcontacts.size(); idx1++) {

		for (int idx2 = 0; idx2 < (int)CMBNDcontacts[idx1].size(); idx2++) {

			int idx_sec = CMBNDcontacts[idx1][idx2].mesh_idx.i;
			int idx_pri = CMBNDcontacts[idx1][idx2].mesh_idx.j;

			if (pTransport[idx_pri] == pMeshTrans)
				pTransport[idx_pri]->CalculateDisplaySAInterfaceTorque(pTransport[idx_sec], CMBNDcontacts[idx1][idx2]);
		}
	}

	return pMeshTrans->displayVEC;
}

#if COMPILECUDA == 1
//return interfacial spin torque in given mesh with matching transport module
cu_obj<cuVEC<cuReal3>>& STransport::GetInterfacialSpinTorqueCUDA(Transport* pMeshTrans)
{
	return reinterpret_cast<STransportCUDA*>(pModuleCUDA)->GetInterfacialSpinTorque(reinterpret_cast<TransportCUDA*>(pMeshTrans->pModuleCUDA));
}
#endif

#endif