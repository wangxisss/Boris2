//Functions for working with files

#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <atlstr.h>
#include <strsafe.h>
#include <shellapi.h>

#include "Funcs_Conv.h"
#include "Funcs_Vectors.h"
#include "VEC.h"

#define FILEROWCHARS	5000	//maximum number of characters per input file row

///////////////////////////////////////////////////////////////////////////////
// GENERAL

//return termination including the dot. If no proper termination found then return empty string.
inline std::string GetFileTermination(const std::string& fileName)
{
	size_t found = fileName.find_last_of(".");

	if(found == std::string::npos) return "";

	return fileName.substr(found);
}

//Get current directory
inline std::string GetDirectory(void)
{
	TCHAR path[FILEROWCHARS];
	GetCurrentDirectory(FILEROWCHARS, path);
	std::string directory = std::string(CW2A(path)) + "\\";

	return directory;
}

//get directory from fileName, if any
inline std::string GetFilenameDirectory(const std::string& fileName)
{
	size_t found = fileName.find_last_of("\\/");

	if (found == std::string::npos) return "";
	else return fileName.substr(0, found + 1);
}

//return directory from fileName, if any, and also modify fileName by removing the directory
inline std::string ExtractFilenameDirectory(std::string& fileName)
{
	size_t found = fileName.find_last_of("\\/");

	if (found == std::string::npos) return "";
	else {

		std::string directory = fileName.substr(0, found + 1);
		fileName = fileName.substr(found + 1);
		return directory;
	}
}

//return termination from fileName, if any, and also modify fileName by removing the termination
inline std::string ExtractFilenameTermination(std::string& fileName)
{
	size_t found = fileName.find_last_of(".");

	if (found == std::string::npos) return "";
	else {

		std::string termination = fileName.substr(found);
		fileName = fileName.substr(0, found);
		return termination;
	}
}

//return all files sharing the given base (in specified directory) and termination. Return them ordered (including full path) by creation time
inline std::vector<std::string> GetFilesInDirectory(std::string directory, std::string baseFileName, std::string termination)
{
	std::vector<std::string> fileNames;
	std::vector<double> creationTimes;

	WIN32_FIND_DATA ffd;
	TCHAR szDir[MAX_PATH];
	HANDLE hFind = INVALID_HANDLE_VALUE;

	std::wstring wstr_directory = StringtoWideString(directory);

	StringCchCopy(szDir, MAX_PATH, wstr_directory.c_str());
	StringCchCat(szDir, MAX_PATH, TEXT("\\*"));

	// Find the first file in the directory.
	hFind = FindFirstFile(szDir, &ffd);

	if (INVALID_HANDLE_VALUE == hFind) return fileNames;

	// Get all the files in the directory.
	do {

		//only get file names, not directories
		if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {

			//creation time : form a double as uli.QuadPart
			ULARGE_INTEGER uli;
			uli.LowPart = ffd.ftCreationTime.dwLowDateTime;
			uli.HighPart = ffd.ftCreationTime.dwHighDateTime;

			//get the file name
			std::string fileName = WideStringtoString(ffd.cFileName);

			//if filename including termination is too short then skip it
			if (fileName.length() < termination.length()) continue;

			if (!baseFileName.length()) {

				if (!termination.length()) {

					//if no basefile name or termination is specified then just get the file
					fileNames.push_back(directory + fileName);
					creationTimes.push_back((double)uli.QuadPart);
				}
				else if (fileName.substr(fileName.length() - termination.length()).compare(termination) == 0) {

					//if no basefile name is specified but the termination is, then it must match that of the file (the ending of the file name)
					fileNames.push_back(directory + fileName);
					creationTimes.push_back((double)uli.QuadPart);
				}
			}
			else if (fileName.substr(0, baseFileName.length()).compare(baseFileName) == 0) {

				//if basefile name is specified then it must match that of the file (the beggining of the file name)
				if (!termination.length()) {

					fileNames.push_back(directory + fileName);
					creationTimes.push_back((double)uli.QuadPart);
				}
				else if (fileName.substr(fileName.length() - termination.length()).compare(termination) == 0) {

					fileNames.push_back(directory + fileName);
					creationTimes.push_back((double)uli.QuadPart);
				}
			}
		}
	} while (FindNextFile(hFind, &ffd) != 0);			//get next file in directory

														//finish
	FindClose(hFind);

	//sort by creation time order
	quicksort(creationTimes, fileNames);

	return fileNames;
}

///////////////////////////////////////////////////////////////////////////////
// READING

//From given file extract row by row numerical data fields separated by separator, and with number of fields in each row matching ncols (if specified).
//If ncols specified then number of data columns in file must match. Return numbers of rows read.
template <typename VType, std::enable_if_t<is_vector<VType>::value>* = nullptr>
int ReadBlockData(std::string filename, std::string separator, VType &data, int ncols = 0)
{
	//stored element type
	using SType = typename contained_type<VType>::type;

	int rowsRead = 0;

	std::ifstream bdin;
	bdin.open(filename.c_str(), std::ios::in);

	if(bdin.is_open()) {

		char line[FILEROWCHARS];
		std::vector<std::string> stringsVector;
		data.resize(0);

		while(bdin.getline(line, FILEROWCHARS)) {

			if(std::string(line).length()) {

				stringsVector.resize(0);
				stringsVector = split(std::string(line), separator);

				if(stringsVector.size() && (stringsVector.size() == ncols || !ncols)) {

					for(int i = 0; i < stringsVector.size(); i++) {

						double dval = ToNum(stringsVector[i]);

						data.push_back((SType)dval);
					}

					rowsRead++;
				}
			}
		}
	}
	else return 0;

	bdin.close();
	return rowsRead;
}

//From given file read specified data columns only and load them in data_cols. Return maximum number of rows read.
template <typename ... PType, typename VType, std::enable_if_t<is_vector<VType>::value>* = nullptr>
int ReadDataColumns(std::string filename, std::string separator, std::vector< VType > &data_cols, std::vector<int> cols)
{
	//stored element type
	using SType = typename contained_type<VType>::type;

	data_cols.resize(0);
	data_cols.resize(cols.size());

	std::ifstream bdin;
	bdin.open(filename.c_str(), std::ios::in);

	if (bdin.is_open()) {

		char line[FILEROWCHARS];
		std::vector<std::string> stringsVector;

		while (bdin.getline(line, FILEROWCHARS)) {

			//check this is a correct line : non-empty and, apart from separators, must contain only alphanumeric characters, e, -, +, .
			if (has_numbers_only(std::string(line), separator)) {

				stringsVector.resize(0);
				stringsVector = split(std::string(line), separator);

				for (int idx = 0; idx < (int)cols.size(); idx++) {

					if (cols[idx] < stringsVector.size()) {

						data_cols[idx].push_back((SType)ToNum(stringsVector[cols[idx]]));
					}
				}
			}
		}
	}
	else return 0;

	bdin.close();
	return (int)data_cols[0].size();
}

//From given file read specified data columns only and load them in data_cols. Return maximum number of rows read.
template <typename ... PType, typename VType, std::enable_if_t<is_vector<VType>::value>* = nullptr>
int ReadDataColumns(std::string filename, std::string separator, std::vector< VType > &data_cols, int col_idx, PType ... col_idxs)
{
	std::vector<int> cols = make_vector(col_idx, col_idxs...);

	return ReadDataColumns(filename, separator, data_cols, cols);
}

//From given file read specified data columns separated using separator, and rows separated using newline. 
//Rows may have different sizes.
//Return number of rows read.
inline int ReadData(std::string filename, std::string separator, std::vector< std::vector<std::string> > &data)
{
	std::ifstream bdin;
	bdin.open(filename.c_str(), std::ios::in);

	if (bdin.is_open()) {

		data.clear();

		char line[FILEROWCHARS];
		std::vector<std::string> row;

		while (bdin.getline(line, FILEROWCHARS)) {

			row = split(std::string(line), separator);
			data.push_back(row);
		}
	}
	else return 0;

	bdin.close();
	return (int)data.size();
}

//load vector quantity from file where it's stored in binary
template <typename VType, std::enable_if_t<is_vector<VType>::value>* = nullptr>
int ReadBinaryVector(std::string filename, VType &data)
{
	std::ifstream bdin;
	bdin.open(filename.c_str(), std::ios::in + std::ios::binary);

	int elements_read = 0;

	if (bdin.is_open()) {

		for (auto it = data.begin(); it != data.end(); ++it, elements_read++) {

			using SType = typename contained_type<VType>::type;

			SType value = *it;

			bdin.read(reinterpret_cast<char*>(&value), sizeof(SType));

			*it = value;
		}

		bdin.close();
	}

	return elements_read;
}

///////////////////////////////////////////////////////////////////////////////
// WRITING

inline bool SaveTextToFile(std::string filename, std::string text) 
{
	bool success = false;

	std::ofstream bdout;

	bdout.open(filename.c_str(), std::ios::out);

	if(bdout.is_open()) {

		bdout << text;
		bdout.close();

		success = true;
	}

	return success;
}

//Save data by arranging it with specified number of columns - this method is for a vector type (e.g. std::vector, VEC, VEC_VC - the important thing here it has to be indexable and have a size method)
template <typename VType, std::enable_if_t<is_vector<VType>::value>* = nullptr>
bool SaveBlockData(std::string filename, char separator, VType &data, int ncols) 
{
	std::ofstream bdout;
	bdout.open(filename.c_str(), std::ios::out);

	if(bdout.is_open()) {

		for(int idx = 0; idx < (int)data.size(); idx++) {

			bdout << data[idx];

			if(idx != (int)data.size() - 1) {

				if(idx % ncols < ncols - 1) bdout << separator;
				else bdout << "\n";
			}
		}
	}
	else return false;

	bdout.close();

	return true;
}

//Save VEC to file arranged as : x labels columns, y labels rows. Save all planes consecutively.
template <typename VType>
bool SaveVEC(std::string filename, char separator, VEC<VType>& vec)
{
	std::ofstream bdout;
	bdout.open(filename.c_str(), std::ios::out);

	if (bdout.is_open()) {

		for (int k = 0; k < vec.n.k; k++) {
			for (int j = 0; j < vec.n.j; j++) {
				for (int i = 0; i < vec.n.i; i++) {

					int idx = i + j * vec.n.x + k * vec.n.x * vec.n.y;

					bdout << vec[idx];

					if (i != vec.n.i - 1) bdout << separator;
				}

				//finished row - separate planes with extra newline
				bdout << "\n";
			}

			//finished plane
			if (k != vec.n.k - 1) bdout << "\n";
		}
	}
	else return false;

	bdout.close();

	return true;
}

//Save specified columns from the 2D vector
template <typename VType, std::enable_if_t<is_vector<VType>::value>* = nullptr>
bool SaveDataColumns(std::string filename, char separator, std::vector< VType > &data_cols, std::vector<int> col_idx)
{
	auto is_valid_row = [](std::vector< VType > &data_cols, std::vector<int>& col_idx, int row_idx) -> bool {

		bool valid = false;

		for (int idx = 0; idx < (int)col_idx.size(); idx++) {

			valid |= (row_idx < data_cols[col_idx[idx]].size());
		}

		//the row index is valid for at least one column in data_cols
		return valid;
	};

	std::ofstream bdout;
	bdout.open(filename.c_str(), std::ios::out);

	if (bdout.is_open()) {

		int row_idx = 0;

		while (is_valid_row(data_cols, col_idx, row_idx)) {

			//at least another row left to save
			//save this row column by column as specified in col_idx
			for (int idx = 0; idx < (int)col_idx.size(); idx++) {

				int index = col_idx[idx];

				if (index < (int)data_cols.size()) {

					if (row_idx < (int)data_cols[index].size()) {

						//number available at this row column position
						bdout << data_cols[index][row_idx];
					}
					else {

						//no number available at this row column position : save a space instead
						bdout << " ";
					}
				}

				if (idx != (int)col_idx.size() - 1) {

					if (index < (int)data_cols.size()) bdout << separator;	//more columns to save
				}
				else bdout << "\n";											//no more columns : newline for next one
			}

			row_idx++;
		}
	}
	else return false;

	bdout.close();

	return true;
}

//Save all columns from the 2D vector
template <typename VType, std::enable_if_t<is_vector<VType>::value>* = nullptr>
bool SaveDataColumns(std::string filename, char separator, std::vector< VType > &data_cols)
{
	vector<int> col_idx;

	for (int idx = 0; idx < (int)data_cols.size(); idx++)
		col_idx.push_back(idx);

	//save all columns in data_cols
	return SaveDataColumns(filename, separator, data_cols, col_idx);
}

//Save vector as a single column
template <typename VType, std::enable_if_t<is_vector<VType>::value>* = nullptr>
bool SaveDataColumn(std::string filename, VType &data)
{
	return SaveBlockData(filename, ' ', data, 1);
}

//In given file write specified data columns separated using separator, and rows separated using newline. 
//Rows may have different sizes.
inline bool SaveData(std::string filename, std::string separator, std::vector< std::vector<std::string> > &data)
{
	std::ofstream bdout;
	bdout.open(filename.c_str(), std::ios::out);

	if (bdout.is_open()) {

		for (int i = 0; i < data.size(); i++) {

			bdout << combine(data[i], separator) << std::endl;
		}
	}
	else return false;

	bdout.close();
	return true;
}

//save vector quantity to file in binary
template <typename VType, std::enable_if_t<is_vector<VType>::value>* = nullptr>
int WriteBinaryVector(std::string filename, VType &data)
{
	std::ofstream bdout;
	bdout.open(filename.c_str(), std::ios::out + std::ios::binary);

	int elements_written = 0;

	if (bdout.is_open()) {

		for (auto it = data.begin(); it != data.end(); ++it, elements_written++) {

			using SType = typename contained_type<VType>::type;

			SType value = *it;

			bdout.write(reinterpret_cast<char*>(&value), sizeof(SType));
		}

		bdout.close();
	}

	return elements_written;
}

//open a file programatically : the file name includes a path
inline void open_file(std::string fileName)
{
	std::string command = "open";

	ShellExecute(GetDesktopWindow(), StringtoWCHARPointer(command), StringtoWCHARPointer(fileName), NULL, NULL, SW_SHOWNORMAL);
}