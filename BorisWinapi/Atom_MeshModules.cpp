#include "stdafx.h"
#include "Atom_Mesh.h"

#include "SuperMesh.h"

//----------------------------------- MODULES CONTROL

BError Atom_Mesh::AddModule(MOD_ moduleID, bool force_add)
{
	BError error(__FUNCTION__);

	if (moduleID <= MOD_ERROR) return error(BERROR_INCORRECTNAME);

	//first make sure the module can be added to this mesh type
	if (!vector_contains(modules_for_meshtype[INT2(meshType, 0)], moduleID)) return error(BERROR_INCORRECTNAME);

	//if module is already set then don't add another one unless we specifically ask to do it.
	//only one module of each type allowed normally, but we may want to force adding of multiple modules of same type (force_add = true)
	if (!force_add && IsModuleSet(moduleID)) return error(BERROR_INCORRECTACTION_SILENT);

	//now set the module
	switch (moduleID) {

	case MOD_DEMAG:
		pMod.push_back(new Atom_Demag(this), MOD_DEMAG);
		break;

	case MOD_ZEEMAN:
		pMod.push_back(new Atom_Zeeman(this), MOD_ZEEMAN);
		break;

	case MOD_ATOM_EXCHANGE:
		pMod.push_back(new Atom_Exchange(this), MOD_ATOM_EXCHANGE);
		break;

	case MOD_ANIUNI:
		pMod.push_back(new Atom_Anisotropy_Uniaxial(this), MOD_ANIUNI);
		break;
	}

	//check the module was created correctly - if not, delete it
	error = pMod.back()->Error_On_Create();

	if (error) pMod.pop_back();
	else {

		//Delete any modules which are exclusive to moduleId
		for (int idx = 0; idx < (int)exclusiveModules[moduleID].size(); idx++) {

			MOD_ module = exclusiveModules[moduleID][idx];
			if (module == moduleID) continue;

			if (IsModuleSet(module)) { delete pMod[pMod.get_index_from_ID(module)]; pMod.erase(INT2(module, 0)); }
		}
	}

	return error;
}

//TO DO
//update MOD_TRANSPORT module only if set
void Atom_Mesh::UpdateTransportSolver(void)
{
	//if (IsModuleSet(MOD_TRANSPORT)) pMod(MOD_TRANSPORT)->UpdateField();
}

#if COMPILECUDA == 1
void Atom_Mesh::UpdateTransportSolverCUDA(void)
{
	//if (IsModuleSet(MOD_TRANSPORT)) pMod(MOD_TRANSPORT)->UpdateFieldCUDA();
}
#endif