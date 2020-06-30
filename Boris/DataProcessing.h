#pragma once

#include "BorisLib.h"
#include "SuperMesh.h"

#include "ErrorHandler.h"

#include <numeric>

#define MAX_ARRAYS	100

using namespace std;

class DPArrays {

private:

	//a number of data processing arrays (each a single column array). The length of each array is not fixed, but the number of available arrays is fixed at instantiation.
	vector< vector<double> > dpA;

	//maximum number of available arrays
	const int max_arrays;

private:

	//check if the array indexes are good (>= 0 and < max_arrays)
	template <typename ... Arrs>
	bool GoodArrays(Arrs ... dp_idx) const
	{
		vector<int> dp_indexes = make_vector(dp_idx...);

		return GoodArrays(dp_indexes);
	}

	//check if the array indexes are good (>= 0 and < max_arrays)
	bool GoodArrays(std::vector<int>& dp_indexes) const
	{
		for (int idx = 0; idx < dp_indexes.size(); idx++) {

			if (dp_indexes[idx] < 0 || dp_indexes[idx] >= max_arrays) return false;
		}

		return true;
	}

	//check if the array indexes are good and unique (>= 0 and < max_arrays and all different)
	template <typename ... Arrs>
	bool GoodArrays_Unique(Arrs ... dp_idx)
	{
		vector<int> dp_indexes = make_vector(dp_idx...);

		quicksort(dp_indexes);

		if (dp_indexes[0] < 0 || dp_indexes[0] >= max_arrays) return false;

		for (int idx = 1; idx < dp_indexes.size(); idx++) {

			if (dp_indexes[idx] < 0 || dp_indexes[idx] >= max_arrays || dp_indexes[idx] == dp_indexes[idx - 1]) return false;
		}

		return true;
	}

public:

	DPArrays(int _max_arrays);
	~DPArrays() {}

	//Get size methods
	int size(void) { return max_arrays; }

	//Indexing
	vector<double>& operator[](int arr_idx) { return dpA[arr_idx]; }

	//value setters
	void push_value(int arr_idx, double value) { dpA[arr_idx].push_back(value); }
	void push_value(int arr_idx, DBL3 value)
	{
		dpA[arr_idx].push_back(value.x);
		dpA[arr_idx + 1].push_back(value.y);
		dpA[arr_idx + 2].push_back(value.z);
	}

	void set_array(int arr_idx, vector<double>& data) { if (arr_idx < max_arrays) dpA[arr_idx] = data; }

	//
	//Various data processing methods accessible externally (corresponding to console commands for data processing)
	//	

	//--------------------- loading and saving

	void clear_all(void) 
	{ 
		for (int idx = 0; idx < dpA.size(); idx++) {

			dpA[idx].resize(0);
			dpA[idx].shrink_to_fit();
		}
	}
	
	//clear arr_num arrays starting at arr_idx
	void clear(int arr_idx, int arr_num = 1) 
	{ 
		if (GoodIdx(dpA.size() - 1, arr_idx + arr_num - 1)) {

			for (int idx = 0; idx < arr_num; idx++) {

				dpA[arr_idx + idx].resize(0);
				dpA[arr_idx + idx].shrink_to_fit();
			}
		}
	}

	void resize(int arr_idx, size_t newSize)
	{
		if (GoodIdx(dpA.size() - 1, arr_idx)) {

			dpA[arr_idx].resize(newSize);
			dpA[arr_idx].shrink_to_fit();
		}
	}

	//load a number of arrays from a file with given name : the file should contain columns of data
	BError load_arrays(string fileName, vector<int> all_indexes, int* prows_read);

	//save dp arrays with given indexes to file
	BError save_arrays(string fileName, vector<int> all_indexes);

	//--------------------- data extraction from meshes

	//Extract profile of physical quantity displayed on screen along the line specified with given start and end coordinates(unit m).
	//Place profile in given dp arrays: 4 consecutive dp arrays are used, first for distance along line, the next 3 for physical quantity so allow space for these starting at dparray_index.
	BError get_profile(DBL3 start, DBL3 end, SuperMesh *pSMesh, int arr_idx);

	//calculate topological charge in M and given rect, using equation Q = Integral(m.(dm/dx x dm/dy) dxdy) / 4PI
	BError get_topological_charge(VEC_VC<DBL3>& M, double x, double y, double radius, double* pQ);

	//count skyrmions in M and given rect, using equation Q = Integral(|m.(dm/dx x dm/dy)| dxdy) / 4PI
	BError count_skyrmions(VEC_VC<DBL3>& M, double x, double y, double radius, double* pQ);

	//calculate histogram for |M| using given parameters
	BError calculate_histogram(VEC_VC<DBL3>& M, int dp_x, int dp_y, double bin, double min, double max);

	//--------------------- dp array manipulation

	//append data in dp_new at the end of dp_original
	BError append_array(int dp_original, int dp_new);

	//Pick elements from dp_in using the skip value (1 by default) and set them in dp_out; e.g. with skip = 2 every 3rd data point is picked; skip = 1 picks every other point.
	BError rarefy(int dp_in, int dp_out, int skip);

	//From  dp_in extract a number of points (length) starting at start_index and place them in dp_out
	BError extract(int dp_in, int dp_out, int start_index, int length);

	//From dp_index erase a number of points (length) starting at start_index
	BError erase(int dp_index, int start_index, int length);

	//--------------------- data generation

	//generate a sequence of points in dp_idx from start_value using increment
	BError generate_sequence(int dp_idx, double start_value, double increment, int points);

	//--------------------- testing

	BError dump_tdep(SuperMesh *pSMesh, string meshName, string paramName, double max_temperature, int dp_arr);

	//--------------------- simple algebraic operations

	//single source versions
	BError add(int dp_source, int dp_dest, double value);
	BError subtract(int dp_source, int dp_dest, double value);
	BError multiply(int dp_source, int dp_dest, double value);
	BError divide(int dp_source, int dp_dest, double value);
	BError dotproduct(int dp_x, int dp_y, int dp_z, DBL3 u, int dp_out);

	//multiple sources versions
	BError add_dparr(int dp_x1, int dp_x2, int dp_dest);
	BError subtract_dparr(int dp_x1, int dp_x2, int dp_dest);
	BError multiply_dparr(int dp_x1, int dp_x2, int dp_dest);
	BError divide_dparr(int dp_x1, int dp_x2, int dp_dest);
	BError dotproduct_dparr(int dp_x1, int dp_x2, double *pvalue);

	//--------------------- simple data handling

	BError get_min_max(int dp_source, DBL2* pmin_max_values, INT2* pmin_max_indexes = nullptr);

	BError get_mean(int dp_source, DBL2* pmean_err);

	//get amplitude value (max - min)/2 every pointsPeriod
	BError get_amplitude(int dp_source, int pointsPeriod, double* pamplitude);
	
	//--------------------- algorithms

	//unweighted linear regression : get gradient and intercept together with uncertainties
	//dp_z can be used to perform multiple linear regressions, where each batch of points for linear regression is identified by having the same value in dp_z
	//if dp_z is used then place output in 5 dp arrays starting at dp_out as: 0) unique dp_z value, 1) gradient, 2) gradient error, 3) intercept, 4) intercept error
	BError linreg(int dp_x, int dp_y, int dp_z, int dp_out, DBL2* pgradient, DBL2* pintercept);

	//get coercivity field values for up and down sweeps
	BError get_coercivity(int dp_x, int dp_y, DBL3 *pHc_up, DBL3 *pHc_dn);

	//get remanence for up and down sweeps
	BError get_remanence(int dp_x, int dp_y, double *pMr_up, double *pMr_dn);

	//For a hysteresis loop with only one branch continue it by constructing the other direction branch (invert both x and y data and add it in continuation)
	BError complete_hysteresis(int dp_x, int dp_y);

	//--------------------- histograms
	
	//From input x - y data build a histogram of number of times x - y data crosses a given line (up or down). 
	//The line varies between minimum and maximum of y data in given number of steps. 
	//Output the line values in dp_lev with corresponding number of crossings in dp_counts.
	BError crossings_histogram(int dp_x, int dp_y, int dp_lev, int dp_counts, int steps);

	//From input x-y data build a histogram of average frequency the x-y data crosses a given line (up and down, separated). 
	//The line varies between minimum and maximum of y data in given number of steps (100 by default). 
	//Output the line values in dp_level with corresponding crossgins frequencies in dp_freq_up and dp_freq_dn.
	BError crossings_frequencies(int dp_x, int dp_y, int dp_lev, int dp_freq_up, int dp_freq_dn, int steps);

	//From input x-y data build a histogram of average frequency of peaks in the x-y data in bands given by the number of steps. 
	//The bands vary between minimum and maximum of y data in given number of steps (100 by default). 
	//Output the line values in dp_level with corresponding peak frequencies in dp_freq.
	BError peaks_frequencies(int dp_x, int dp_y, int dp_lev, int dp_freq, int steps);

	//--------------------- curve fitting

	//fit f(x) = y0 + S dH / (4(x-H0)^2 + dH^2). Return fitting parameters with their standard deviations.
	BError fit_lorentz(int dp_x, int dp_y, DBL2 *pS, DBL2 *pH0, DBL2 *pdH, DBL2 *py0);

	//fit Mz(x) = Ms * cos(2*arctan(sinh(R/w)/sinh((x-x0)/w))). Return fitting parameters with their standard deviations.
	BError fit_skyrmion(int dp_x, int dp_y, DBL2 *pR, DBL2 *px0, DBL2 *pMs, DBL2 *pw);

	//fit STT function
	BError fit_stt(VEC<DBL3>& T, VEC_VC<DBL3>& M, VEC_VC<DBL3>& J, Rect rectangle, DBL2* pP, DBL2 *pbeta, double *pRsq);

	//fit STT function using a stencil to obtain spatial dependence of P parameter (adiabatic == true) or nonadiabaticity parameter (adiabatic == false)
	BError fit_stt_variation(VEC<DBL3>& T, VEC_VC<DBL3>& M, VEC_VC<DBL3>& J, VEC<double>& output, bool adiabatic, double absolute_error_threshold = 0.1, double Rsq_threshold = 0.9, double Torque_ratio_threshold = 0.1, int stencil_size = 3);

	//fit SOT function
	BError fit_sot(VEC<DBL3>& T, VEC_VC<DBL3>& M, VEC_VC<DBL3>& J, Rect rectangle, DBL2* pSHAeff, DBL2 *pflST, double *pRsq);

	//fit simultaneously STT and SOT when the self-consistent interfacial spin torque contains both
	BError fit_sotstt(VEC<DBL3>& T, VEC_VC<DBL3>& M, VEC_VC<DBL3>& J_hm, VEC_VC<DBL3>& J_fm, Rect rectangle, DBL2* pSHAeff, DBL2 *pflST, DBL2* pP, DBL2 *pbeta, double *pRsq);

	//--------------------- data processing
	
	//Replace repeated points from using linear interpolation: if two adjacent sets of repeated points found, replace repeats between the mid-points of the sets
	BError replace_repeats(int dp_in, int dp_out);

	//Subtract the first point (the offset) from all the points in dp_in
	BError remove_offset(int dp_in, int dp_out);

	//smooth data in dp_in using nearest-neighbor averaging with given window size, and place result in dp_out (must be different)
	BError adjacent_averaging(int dp_in, int dp_out, int window_size);

	//extract monotonic sequence form input arrays and place it in output arrays. Primary array (dp_in_pri) decides ordering and dependent array (dp_in_dep) must have same size
	BError extract_monotonic(int dp_in_pri, int dp_in_dep, int dp_out_pri, int dp_out_dep);

	//convert from Cartesian to polar (as r, theta)
	BError Cartesian_to_Polar(int dp_in_x, int dp_in_y, int dp_out_x, int dp_out_y);
};