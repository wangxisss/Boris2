#include "stdafx.h"
#include "MaterialsDataBase.h"

#include <iostream>

MaterialsDB::MaterialsDB(vector<PARAM_>& enabledParams) :
	dbEntry(enabledParams)
{
	//default database
	databaseName = GetDirectory() + default_databaseName;

	//if BorisMDB.txt doesn't exist, make it

	ifstream bdin;
	bdin.open(databaseName.c_str(), std::ios::in);

	if (!bdin.is_open()) {

		//file doesn't exist so make it
		make_mdb_file(databaseName);
	}
	else {

		//make sure the existing file parameters list contains all the entries found in dbEntry (e.g. the program might have new material parameters not found in the old database)
		fix_mdb_file();

		bdin.close();
	}

	//generic : nothing loaded
	dbEntryType = MESH_SUPERMESH;

	//make meshTypeHandles
	meshTypeHandles.push_back("supermesh", MESH_SUPERMESH);
	meshTypeHandles.push_back("ferromagnetic", MESH_FERROMAGNETIC);
	meshTypeHandles.push_back("dipole", MESH_DIPOLE);
	meshTypeHandles.push_back("metal", MESH_METAL);
	meshTypeHandles.push_back("insulator", MESH_INSULATOR);

	//load entries
	load_entries();
}

//-------------------------------------------------------------------- AUXILIARY

//make a fresh mdb file at databaseName, erasing any existing file
BError MaterialsDB::make_mdb_file(string newdatabaseName)
{
	//0. Name - this is the material name, and it is the handle used with addmaterial command
	//1. Formula - symbol formula for material (information only)
	//2. Type - the type of mesh this material applies to (e.g. ferromagnetic, metal, insulator, ...)
	//3. Description - brief description of entry, e.g. could indicate if this is applicable to a particular type of interface, such as Pt/Co, Ta/Co, or just bulk Co etc., or other relevant information so users know how they should use this entry
	//4. Contributor - name of contributor for this entry (leave blank for Anonymous)
	//5. State - is this a local, or an entry available in the official online data base. SHARED / LOCAL.
	//
	//Now follow all material parameters values. 
	//The columns are labelled using the handles in MeshParams::meshParams, i.e. same handles visible in the console when using the params command.
	//If the parameter has a unit then it is specified in brackets.
	//e.g. a column label would look like: "Ms (A/m)", or "damping", etc. - without the quotation marks
	//
	//6a. Parameter - the parameter values are entered numerically in the units specified by the column descriptor
	//6b. DOI - parameter values should have a valid literature reference; this is specified using a DOI, e.g. https://doi.org/10.1038/nphys3304
	//
	//7a. Parameter - //
	//7b. DOI - //
	//
	//etc. for all parameters

	BError error(__FUNCTION__);

	ofstream bdout;

	bdout.open(newdatabaseName.c_str(), std::ios::out);

	if (bdout.is_open()) {

		bdout << "Name" << "\t" << "Formula" << "\t" << "Type" << "\t" << "Description" << "\t" << "Contributor" << "\t" << "State";

		//write all possible material parameters handles

		for (int idx = 0; idx < dbEntry.get_num_meshparams(); idx++) {

			string handle = dbEntry.get_meshparam_handle(idx);
			string unit = dbEntry.get_meshparam_unit(idx);

			string Parameter = handle;
			if (unit.length()) Parameter += string(" (") + unit + string(")");

			bdout << "\t" << Parameter << "\tDOI";
		}

		bdout << std::endl;

		bdout.close();
	}
	else return error(BERROR_COULDNOTOPENFILE);

	error = load_entries();

	return error;
}

//make sure the mdb file has the correct entries for parameters as obtained from dbEntry
BError MaterialsDB::fix_mdb_file(void)
{
	BError error(__FUNCTION__);

	//////////////////////////////////////////

	//first read all entries

	error = load_entries();
	if (error) return error;

	bool entries_modified = false;

	//////////////////////////////////////////

	//the list of parameter handles as found in dbEntry, in same order
	vector<string> parameters_handles;

	for (int idx = 0; idx < dbEntry.get_num_meshparams(); idx++) {

		string handle = dbEntry.get_meshparam_handle(idx);
		string unit = dbEntry.get_meshparam_unit(idx);

		if (unit.length()) handle += string(" (") + unit + string(")");

		parameters_handles.push_back(handle);
	}

	//////////////////////////////////////////
	
	{
		//using first row, check all entries against the parameters vector - first row contains the parameters handles
		//if a database entry is not found in the parameters vector then delete it

		if (!entries.size() || entries[0].size() <= params_start_idx) return error(BERROR_INCORRECTARRAYS);

		vector<int> erase_columns;

		for (int idx = params_start_idx; idx < entries[0].size(); idx += 2) {

			//if parameter entry not found in parameters vector then mark this column for deletion
			if (!vector_contains(parameters_handles, entries[0][idx])) erase_columns.push_back(idx);
		}

		if (erase_columns.size()) {

			entries_modified = true;

			for (int i = 0; i < entries.size(); i++) {

				int deleted_columns = 0;

				for (int idx = 0; idx < erase_columns.size(); idx++) {

					//the column index to erase
					int erase_idx = erase_columns[idx] - deleted_columns;

					if (erase_idx + 1 < entries[i].size()) {

						//erase 2 columns as for each parameter there's also a DOI column
						entries[i].erase(entries[i].begin() + erase_columns[idx] - deleted_columns, entries[i].begin() + erase_columns[idx] - deleted_columns + 2);

						//adjust next column index to erase
						deleted_columns += 2;
					}
				}
			}
		}
	}
	
	//////////////////////////////////////////
	
	{
		//now check parameters to see if there are any not present in the database parameters
		//if any found, then insert columns in the entries with parameter values set as "N/A"

		vector<string> insert_columns;

		for (int idx = 0; idx < parameters_handles.size(); idx++) {

			//if database doesn't contain parameter handle then mark it for insertion
			if (!vector_contains(entries[0], parameters_handles[idx])) insert_columns.push_back(parameters_handles[idx]);
		}

		int num_columns = entries[0].size();

		if (insert_columns.size()) {

			entries_modified = true;

			//first insert new handles
			for (int idx = 0; idx < insert_columns.size(); idx++) {

				//after each parameter column there is a DOI column
				entries[0].push_back(insert_columns[idx]);
				entries[0].push_back("DOI");
			}

			for (int i = 1; i < entries.size(); i++) {

				//now insert N/A entries - row must have exactly the same number of entries as the first
				if (entries[i].size() == num_columns) {

					for (int idx = 0; idx < insert_columns.size(); idx++) {

						entries[i].push_back("N/A");
						entries[i].push_back("N/A");
					}
				}
			}
		}
	}
	
	if (entries_modified) {

		error = store_entries();
	}
	
	return error;
}

//load entries from currently selected database
BError MaterialsDB::load_entries(void)
{
	BError error(__FUNCTION__);

	if (!ReadData(databaseName, "\t", entries)) return error(BERROR_COULDNOTLOADFILE);

	//for multicomponent values, e.g. Gi, Gmix, some external editors like to put quotation marks!
	//get rid of them

	for (int i = 0; i < entries.size(); i++) {
		for (int j = 0; j < entries[i].size(); j++) {

			//also enforce character limit
			entries[i][j] = trim(entries[i][j].substr(0, FIELD_CHAR_LIMIT), "\"");
		}
	}

	return error;
}

//store entries in currently selected database
BError MaterialsDB::store_entries(void)
{
	BError error(__FUNCTION__);

	if (!SaveData(databaseName, "\t", entries)) return error(BERROR_COULDNOTSAVEFILE);

	return error;
}

//find row index in entries for given materialName
int MaterialsDB::find_entry_idx(string materialName)
{
	int material_row_idx = -1;

	for (int i = 1; i < entries.size(); i++) {

		if (entries[i].size() && entries[i][0] == materialName) {

			material_row_idx = i;
			break;
		}
	}

	return material_row_idx;
}

//replace certain characters with an ascii signalling code before sending the string with POST / GET requests
void MaterialsDB::encode_characters_ascii(string& message)
{
	//+ -> <asc>43</asc>

	replaceall(message, string("+"), string("<asc>43</asc>"));
}

//-------------------------------------------------------------------- COMMAND HANDLERS

//handles the materialsdatabase command - switches the currently selected database
BError MaterialsDB::SwitchDataBase(string newdatabaseName)
{
	BError error(__FUNCTION__);

	ifstream bdin;
	bdin.open(newdatabaseName.c_str(), std::ios::in);

	if (!bdin.is_open()) {

		//file doesn't exist so make it
		error = make_mdb_file(newdatabaseName);
	}
	else {

		bdin.close();
	}

	if (!error) {

		databaseName = newdatabaseName;

		//make sure the existing file parameters list contains all the entries found in dbEntry (e.g. the program might have new material parameters not found in the old database)
		error = fix_mdb_file();
	}

	return error;
}

//attempt to load entry from current database for material with given name
//set in *pmeshType the type of material loaded, e.g. ferromagnetic, metal, insulator, etc. - see enum MESH_ in MeshDefs.h for possible values
//If succesful return true so the caller can create a mesh of the required type
//The actual mesh parameter values will then be available here in dbEntry
BError MaterialsDB::LoadMaterial(string materialName, int* pmeshType)
{
	BError error(__FUNCTION__);

	//search for material name

	int material_row_idx = find_entry_idx(materialName);

	if (material_row_idx >= 1) {

		//found it. Load in dbEntry and set *pmeshType

		if (pmeshType) {

			//Type is at index 2 - set *pmeshType from it

			if (2 < entries[material_row_idx].size()) {

				if (meshTypeHandles.has_value(entries[material_row_idx][2])) {

					*pmeshType = (int)meshTypeHandles.get_ID_from_value(entries[material_row_idx][2]);
				}
				else return error(BERROR_INCORRECTCONFIG);
			}
			else return error(BERROR_INCORRECTARRAYS);
		}

		//now try to load parameter values

		for (int idx = 0; idx < dbEntry.get_num_meshparams(); idx++) {

			//for each parameter in dbEntry search the entries vector then set value from string
			string handle = dbEntry.get_meshparam_handle(idx);
			string unit = dbEntry.get_meshparam_unit(idx);

			string Parameter = handle;
			if (unit.length()) Parameter += string(" (") + unit + string(")");

			int column_idx = search_vector(entries[0], Parameter);

			if (column_idx >= 0 && column_idx < entries[material_row_idx].size()) {

				//found it
				string value_string = entries[material_row_idx][column_idx];

				//set value if available
				if (value_string != "N/A") {

					PARAM_ paramID = (PARAM_)dbEntry.get_meshparam_id(idx);
					dbEntry.set_meshparam_value(paramID, value_string);
				}
			}
			else return error(BERROR_INCORRECTARRAYS);
		}
	}
	else return error(BERROR_INCORRECTNAME);

	return error;
}

//copy parameters from current dbEntry to copy_to_this - e.g. a newly created mesh with addmaterial
//can also be used with the setmaterial command, which overwrites mesh parameter values in an already created mesh
void MaterialsDB::copy_parameters(MeshParams& copy_to_this)
{
	//dbEntry has all required parameters - just copy them to copy_to_this : this is base for a mesh

	copy_to_this.copy_parameters(dbEntry);
}

//add new entry in currently selected data base - use with addmdbentry command
BError MaterialsDB::AddMDBEntry(string materialName, MeshParams& meshParamsValues, MESH_ meshType)
{
	BError error(__FUNCTION__);

	if (!entries.size()) return error(BERROR_INCORRECTARRAYS);

	vector<string> new_entry;
	
	//search mdb for any entries with materialName - overwrite it if found, but keep fields which are normally manually edited

	bool entries_modified = false;

	int existing_entry_idx = find_entry_idx(materialName);

	//////////////////////////////////////////

	//the list of parameter handles and ids as found in dbEntry, in same order
	vector<string> parameters_handles;
	vector<PARAM_> parameters_ids;

	for (int idx = 0; idx < dbEntry.get_num_meshparams(); idx++) {

		string handle = dbEntry.get_meshparam_handle(idx);
		string unit = dbEntry.get_meshparam_unit(idx);

		if (unit.length()) handle += string(" (") + unit + string(")");

		parameters_handles.push_back(handle);
		parameters_ids.push_back((PARAM_)dbEntry.get_meshparam_id(idx));
	}

	//////////////////////////////////////////

	//1. Name
	new_entry.push_back(materialName);
	
	//2. Formula
	if (existing_entry_idx >= 1 && 1 < entries[existing_entry_idx].size()) new_entry.push_back(entries[existing_entry_idx][1]);
	else new_entry.push_back("edit this");

	//3. Type
	new_entry.push_back(meshTypeHandles(meshType));

	//4. Description
	if (existing_entry_idx >= 1 && 3 < entries[existing_entry_idx].size()) new_entry.push_back(entries[existing_entry_idx][3]);
	else new_entry.push_back("edit this");

	//5. Contributor
	if (existing_entry_idx >= 1 && 4 < entries[existing_entry_idx].size()) new_entry.push_back(entries[existing_entry_idx][4]);
	else new_entry.push_back("N/A");

	//6. State
	new_entry.push_back("LOCAL");

	//7a. Parameter
	//7b. DOI

	//build the parameters list in the order found in the mdb file
	for (int idx = params_start_idx; idx < entries[0].size(); idx += 2) {

		//entry handle
		string handle = entries[0][idx];

		string value = "N/A";

		//get value string
		int idx_param = search_vector(parameters_handles, handle);

		//get value from meshParamsValues if available
		if (idx_param >= 0 && meshParamsValues.contains_param(parameters_ids[idx_param])) {

			value = meshParamsValues.get_meshparam_value_sci(parameters_ids[idx_param]);
		}

		//overwrite existing entry or make new one
		if (existing_entry_idx >= 1 && idx + 1 < entries[existing_entry_idx].size()) {

			new_entry.push_back(value);
			new_entry.push_back(entries[existing_entry_idx][idx + 1]);
		}
		else {

			new_entry.push_back(value);
			new_entry.push_back("N/A");
		}
	}

	if (existing_entry_idx >= 1) {

		entries[existing_entry_idx] = new_entry;
	}
	else {

		entries.push_back(new_entry);
	}

	error = store_entries();
		
	return error;
}

//delete entry in currently selected data base
BError MaterialsDB::DelMDBEntry(string materialName)
{
	BError error(__FUNCTION__);

	int existing_entry_idx = find_entry_idx(materialName);

	if (existing_entry_idx >= 1) {

		error = store_entries();
	}
	else error(BERROR_INCORRECTNAME);

	return error;
}

//reload the currently selected database
BError MaterialsDB::RefreshMDB(void)
{
	BError error(__FUNCTION__);

	error = load_entries();

	return error;
}

//From currently selected data base, request the named material is added to the main online shared materials data base. Handles the requestmdbsync command.
//If successfully sent, the entry will be checked by a moderator, and if correct will become available for all users
//If successfully sent, returnMessage is empty, otherwise it contains a message detailing why the sync request couldn't be sent (e.g. incorrectly formatted entry / missing parameters etc. - too specific to set this as a BError entry)
//BError contains general info, e.g. couldn't connect etc. (or no error code if everything fine)
BError MaterialsDB::RequestMDBSync(string materialName, string domain_name, string mdb_entry_handler, string emailContact, string *presponseMessage)
{
	BError error(__FUNCTION__);

	error = load_entries();
	if (error) return error;

	int existing_entry_idx = find_entry_idx(materialName);
	if (existing_entry_idx < 1) return error(BERROR_INCORRECTNAME);

	//////////////////////////////////////////

	//build string to send : entries in given row separated by tabs
	string new_entry_string = combine(subvec(entries[existing_entry_idx], 0, params_start_idx), "\t");

	new_entry_string += string("\t") + emailContact;

	//parameter values are sent as:
	//handle (unit); value; doi;
	
	for (int idx = params_start_idx; idx < entries[0].size(); idx += 2) {

		//entry handle
		string handle_withunit = entries[0][idx];

		//remove unit since the column names in the sql database only contain the parameter handle, not augmented by units
		string handle = trimblock(handle_withunit, " (", ")");

		if (idx + 1 < entries[existing_entry_idx].size()) {

			string value = entries[existing_entry_idx][idx];
			string doi = entries[existing_entry_idx][idx + 1];

			//we want the doi to be entered as https://doi.org/...
			//if the start bit is missing then add it
			if (doi != "N/A" && doi.find(string("https://doi.org/")) == string::npos) {

				//does not contain https://doi.org/

				if (doi.find(string("doi.org/")) == string::npos) {

					//does not contain doi.org/ either
					doi = string("https://doi.org/") + doi;
				}
				else {

					//contains doi.org/ already
					doi = string("https://") + doi;
				}
			}

			new_entry_string += string("\t") + handle + string(";") + value + string(";") + doi + string(";");
		}
		else return error(BERROR_INCORRECTARRAYS);
	}
	
	//need a terminating tab as php script uses this
	new_entry_string += string("\t");

	///////////////////////////////////////////////

	//now send string to handler
	HTTP httpComms(domain_name);

	//certain characters are not sent properly with POST request, e.g. + which gets replaced by a space
	//deal with this by sending it as an ascii code # in a sequence <asc>#</asc>
	//the receiving php script will know to replace this back with the required sign

	encode_characters_ascii(new_entry_string);

	if (httpComms.is_available() && httpComms.http_post(mdb_entry_handler, string("newentry=") + new_entry_string, *presponseMessage)) {

		//new entry was sent, *preturnMessage contains a message from the server which can be displayed in the console.
		if (*presponseMessage != string("Server message: Entry received.")) {

			return error(BERROR_INCORRECTACTION_SILENT);
		}
	}
	else {

		//connection failure
		error(BERROR_COULDNOTCONNECT);
	}

	return error;
}

//check if there are any new online materials data base entries - should only be called once at the start of the program to limit the potential for spamming the server
//the server should also have a built-in timer to reject requests from users if sent too often from same ip
BError MaterialsDB::CheckMDBLastUpdate(string *presponseMessage)
{
	BError error(__FUNCTION__);

	return error;
}

//Update BorisMDB.txt database with any new entries available in the online version. Handles the updatemdb command. 
BError MaterialsDB::UpdateMDB(string domain_name, string mdb_handler, string* presponseMessage)
{
	BError error(__FUNCTION__);

	error = SwitchDataBase(default_databaseName);
	if (error) return error;

	//////////////////////////////////////////

	//the list of handles in the order found in the entries vector
	vector<string> parameters_handles;

	for (int idx = params_start_idx; idx < entries[0].size(); idx+=2) {

		string handle = entries[0][idx];
		//remove units
		handle = trimblock(handle, " (", ")");

		parameters_handles.push_back(handle);
	}

	//////////////////////////////////////////

	HTTP httpComms(domain_name);

	string responseMessage;

	if (httpComms.is_available() && httpComms.http_post(mdb_handler, "", responseMessage, 16 * 1024 * 1024)) {

		//new entry was sent, *preturnMessage contains a message from the server which can be displayed in the console.

		vector<string> rows = split(responseMessage, "\n");
		
		if (rows.size() <= 1) {

			*presponseMessage = responseMessage;
			return error(BERROR_INCORRECTACTION_SILENT);
		}

		//get entries in fields
		vector<vector<string>> fields;
		fields.resize(rows.size());

		for (int idx = 0; idx < fields.size(); idx++) {

			vector<string> row = split(rows[idx], "\t");
			fields[idx] = row;
		}

		//first delete all rows from local entries which have status set to SHARED
		//it's possible one or more materials have been deleted, so don't keep them in the local database
		for (int i = 0; i < entries.size(); i++) {

			if (entries[i].size() < params_start_idx) return error(BERROR_INCORRECTARRAYS);

			if (entries[i][params_start_idx - 1] == "SHARED") {

				entries.erase(entries.begin() + i);
				i--;
			}
		}

		//go through all materials in fields
		for (int i = 1; i < fields.size(); i++) {

			if (fields[i].size() < params_start_idx) return error(BERROR_INCORRECTARRAYS);

			//overwrite existing entry, or make a new one
			int entry_idx = find_entry_idx(fields[i][0]);

			if (entry_idx < 0) {

				entries.push_back(entries.back());
				entry_idx = entries.size() - 1;
			}
			
			//now write fields 0 to params_start_idx first
			for (int j = 0; j < params_start_idx - 1; j++) {

				entries[entry_idx][j] = fields[i][j];
			}

			//"State" column is always just before the parameters start
			entries[entry_idx][params_start_idx - 1] = string("SHARED");

			//now write parameters and DOI
			for (int j = params_start_idx - 1; j < fields[i].size(); j++) {

				if (fields[0].size() <= j) return error(BERROR_INCORRECTARRAYS);

				int idx_param_entry = params_start_idx + (j - params_start_idx + 1) * 2;

				//entry is of the form value [DOI]
				string entry = fields[i][j];

				string value = trimblock(entry, " [", "]");
				string doi = get_contained_substring(entry, " [", "]");

				//find idx where we should place these in entries
				string handle = fields[0][j];

				int column_idx = search_vector(parameters_handles, handle);

				if (column_idx < 0 || entries[entry_idx].size() <= params_start_idx + column_idx * 2 + 1) {

					error = load_entries();
					return error(BERROR_INCORRECTARRAYS);
				}

				entries[entry_idx][params_start_idx + column_idx * 2] = value;
				entries[entry_idx][params_start_idx + column_idx * 2 + 1] = doi;
			}
		}

		error = store_entries();
	}
	else {

		//connection failure
		error(BERROR_COULDNOTCONNECT);
	}

	return error;
}