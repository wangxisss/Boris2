#include "stdafx.h"
#include "Atom_Mesh_Cubic.h"

#ifdef MESH_COMPILATION_ATOM_CUBIC

//Take a Monte Carlo step in this atomistic mesh
void Atom_Mesh_Cubic::Iterate_MonteCarlo(double acceptance_rate)
{
	//Not applicable at zero temperature
	if (IsZ(base_temperature)) return;

	if (mc_constrain) {

		if (mc_parallel) Iterate_MonteCarlo_Parallel_Constrained();
		else Iterate_MonteCarlo_Serial_Constrained();
	}
	else {

		if (mc_parallel) Iterate_MonteCarlo_Parallel_Classic();
		else Iterate_MonteCarlo_Serial_Classic();
	}

	///////////////////////////////////////////////////////////////
	// ADAPTIVE CONE ANGLE

	if (acceptance_rate < 0 || acceptance_rate > 1) acceptance_rate = MONTECARLO_TARGETACCEPTANCE;

	//adaptive cone angle - nice and simple, efficient for all temperatures; much better than MCM fixed cone angle variants, or cone angle set using a formula.
	if (mc_acceptance_rate < acceptance_rate) {

		//acceptance probability too low : decrease cone angle
		mc_cone_angledeg -= MONTECARLO_CONEANGLEDEG_DELTA;
		if (mc_cone_angledeg < MONTECARLO_CONEANGLEDEG_MIN) mc_cone_angledeg = MONTECARLO_CONEANGLEDEG_MIN;
	}
	else {

		//acceptance probability too high : increase cone angle
		mc_cone_angledeg += MONTECARLO_CONEANGLEDEG_DELTA;
		if (mc_cone_angledeg > MONTECARLO_CONEANGLEDEG_MAX) mc_cone_angledeg = MONTECARLO_CONEANGLEDEG_MAX;
	}
}

#if COMPILECUDA == 1
//Take a Monte Carlo step in this atomistic mesh
void Atom_Mesh_Cubic::Iterate_MonteCarloCUDA(double acceptance_rate)
{ 
	if (paMeshCUDA && mc_parallel) {

		if (mc_constrain) mc_acceptance_rate = reinterpret_cast<Atom_Mesh_CubicCUDA*>(paMeshCUDA)->Iterate_MonteCarloCUDA_Constrained(mc_cone_angledeg);
		else mc_acceptance_rate = reinterpret_cast<Atom_Mesh_CubicCUDA*>(paMeshCUDA)->Iterate_MonteCarloCUDA_Classic(mc_cone_angledeg);
	}

	///////////////////////////////////////////////////////////////
	// ADAPTIVE CONE ANGLE

	if (acceptance_rate < 0 || acceptance_rate > 1) acceptance_rate = MONTECARLO_TARGETACCEPTANCE;

	//adaptive cone angle - nice and simple, efficient for all temperatures; much better than MCM fixed cone angle variants, or cone angle set using a formula.
	if (mc_acceptance_rate < acceptance_rate) {

		//acceptance probability too low : decrease cone angle
		mc_cone_angledeg -= MONTECARLO_CONEANGLEDEG_DELTA;
		if (mc_cone_angledeg < MONTECARLO_CONEANGLEDEG_MIN) mc_cone_angledeg = MONTECARLO_CONEANGLEDEG_MIN;
	}
	else {

		//acceptance probability too high : increase cone angle
		mc_cone_angledeg += MONTECARLO_CONEANGLEDEG_DELTA;
		if (mc_cone_angledeg > MONTECARLO_CONEANGLEDEG_MAX) mc_cone_angledeg = MONTECARLO_CONEANGLEDEG_MAX;
	}
}
#endif

//Take a Monte Carlo step in this atomistic mesh : these functions implement the actual algorithms
void Atom_Mesh_Cubic::Iterate_MonteCarlo_Serial_Classic(void)
{
	//number of moves in this step : one per each spin
	int num_moves = M1.get_nonempty_cells();

	//number of cells in this mesh
	unsigned N = n.dim();

	//make sure indices array has correct memory allocated
	if (mc_indices.size() != N) if (!malloc_vector(mc_indices, N)) return;

	//recalculate it
	mc_acceptance_rate = 0.0;

	//reset indices array so we can shuffle them
#pragma omp parallel for
	for (int idx = 0; idx < N; idx++)
		mc_indices[idx] = idx;

	///////////////////////////////////////////////////////////////
	// SERIAL MONTE-CARLO METROPOLIS

	for (int idx = N - 1; idx >= 0; idx--) {

		//pick spin index in a random order, but guarantee each spin is given the chance to move exactly once per MCM step.
		//Note this approach is more efficient than picking spins completely at random, where each spin is moved once per step only on average (tests I've ran show this approach requires more steps to thermalize).
		//Moreover the Metropolis et al. paper also moves each particle exactly once (if accepted) per step. The random picking approach is a little easier to write and requires less memory but overall is not as good.
		//Technical note: prefer using the method below to shuffle spins rather than the std::shuffle algorithm as the latter is slower.
		int rand_idx = floor(prng.rand() * (idx + 1));
		//idx < N check to be sure - it is theoretically possible idx = N can be generated but the probability is small.
		if (rand_idx >= N) rand_idx = N - 1;
		int spin_idx = mc_indices[rand_idx];
		mc_indices[rand_idx] = mc_indices[idx];

		//only consider non-empty cells
		if (M1.is_not_empty(spin_idx)) {

			//Picked spin is M1[spin_idx]
			DBL3 M1_old = M1[spin_idx];

			//obtain rotated spin in a cone around the picked spin
			double theta_rot = prng.rand() * mc_cone_angledeg * PI / 180.0;
			double phi_rot = prng.rand() * 2 * PI;
			//Move spin in cone with uniform random probability distribution. This approach only requires 2 random numbers to be generated. 
			//Also using a Gaussian distribution to move spin around the initial spin is less efficient, requiring more steps to thermalize.
			DBL3 M1_new = relrotate_polar(M1_old, theta_rot, phi_rot);

			//Find energy for current spin by summing all contributing modules energies at given spin only
			double old_energy = 0.0, new_energy = 0.0;
			for (int mod_idx = 0; mod_idx < pMod.size(); mod_idx++) {

				old_energy += pMod[mod_idx]->Get_Atomistic_Energy(spin_idx);
			}

			//Set new spin; M1_new has same length as M1[spin_idx], but reset length anyway to avoid floating point error creep
			M1[spin_idx] = M1_new.normalized() * M1_old.norm();
			for (int mod_idx = 0; mod_idx < pMod.size(); mod_idx++) {

				new_energy += pMod[mod_idx]->Get_Atomistic_Energy(spin_idx);
			}

			//Compute acceptance probability
			double P_accept = exp(-(new_energy - old_energy) / (BOLTZMANN * base_temperature));

			//uniform random number between 0 and 1
			double P = prng.rand();

			if (P > P_accept) {

				//reject move
				M1[spin_idx] = M1_old;
			}
			else mc_acceptance_rate += 1.0 / num_moves;
		}
	}
}

void Atom_Mesh_Cubic::Iterate_MonteCarlo_Serial_Constrained(void)
{
	//number of moves in this step : one per each spin
	int num_moves = M1.get_nonempty_cells();

	//number of cells in this mesh
	unsigned N = n.dim();

	//make sure indices array has correct memory allocated
	if (mc_indices.size() != N) if (!malloc_vector(mc_indices, N)) return;

	//recalculate it
	mc_acceptance_rate = 0.0;

	//Total moment along CMC direction
	double cmc_M = 0.0;

	//reset indices array so we can shuffle them
#pragma omp parallel for reduction(+:cmc_M)
	for (int idx = 0; idx < N; idx++) {

		mc_indices[idx] = idx;
		if (M1.is_not_empty(idx)) cmc_M += M1[idx] * cmc_n;
	}

	///////////////////////////////////////////////////////////////
	// SERIAL CONSTRAINED MONTE-CARLO METROPOLIS

	// Following Asselin et al., PRB 82, 054415 (2010) but with some differences: 
	// 1) adaptive cone angle for target acceptance of 0.5
	// 2) move spins in a cone using uniform pdf polar and azimuthal angles
	// 3) pick spin pairs exactly once in a random order per CMC step

	for (int idx = N - 1; idx >= 0; idx -= 2) {

		if (idx == 0) break;

		int rand_idx1 = floor(prng.rand() * (idx + 1));
		//idx < N check to be sure - it is theoretically possible idx = N can be generated but the probability is small.
		if (rand_idx1 >= N) rand_idx1 = N - 1;
		int spin_idx1 = mc_indices[rand_idx1];
		mc_indices[rand_idx1] = mc_indices[idx];

		int rand_idx2 = floor(prng.rand() * idx);
		int spin_idx2 = mc_indices[rand_idx2];
		mc_indices[rand_idx2] = mc_indices[idx - 1];

		if (M1.is_not_empty(spin_idx1) && M1.is_not_empty(spin_idx2)) {

			//Picked spins are M1[spin_idx1], M1[spin_idx2]
			DBL3 M_old1 = M1[spin_idx1];
			DBL3 M_old2 = M1[spin_idx2];

			//rotate to a system with x axis along cmc_n : easier to apply algorithm formulas this way
			//NOTE : don't rotate x axis towards cmc_n, but rotate cmc_n towards x axis, i.e. use invrotate_polar, not rotate_polar
			DBL3 Mrot_old1 = invrotate_polar(M_old1, cmc_n);
			DBL3 Mrot_old2 = invrotate_polar(M_old2, cmc_n);

			//obtain rotated spin in a cone around the first picked spin
			double theta_rot = prng.rand() * mc_cone_angledeg * PI / 180.0;
			double phi_rot = prng.rand() * 2 * PI;
			DBL3 Mrot_new1 = relrotate_polar(Mrot_old1, theta_rot, phi_rot);

			//adjust second spin to keep required total moment direction
			DBL3 Mrot_new2 = DBL3(0.0, Mrot_old2.y + Mrot_old1.y - Mrot_new1.y, Mrot_old2.z + Mrot_old1.z - Mrot_new1.z);
			double sq2 = Mrot_new2.y * Mrot_new2.y + Mrot_new2.z * Mrot_new2.z;
			double sqnorm = M_old2.norm()*M_old2.norm();

			if (sq2 < sqnorm) {

				Mrot_new2.x = get_sign(Mrot_old2.x) * sqrt(sqnorm - sq2);

				//Obtain new spins in original coordinate system, i.e. rotate back
				DBL3 M_new1 = rotate_polar(Mrot_new1, cmc_n);
				DBL3 M_new2 = rotate_polar(Mrot_new2, cmc_n);
				M_new1 = M_new1.normalized() * M_old1.norm();
				M_new2 = M_new2.normalized() * M_old2.norm();

				//Find energy for current spin by summing all contributing modules energies at given spins only
				double old_energy = 0.0, new_energy = 0.0;
				for (int mod_idx = 0; mod_idx < pMod.size(); mod_idx++) {

					old_energy += pMod[mod_idx]->Get_Atomistic_Energy(spin_idx1);
					old_energy += pMod[mod_idx]->Get_Atomistic_Energy(spin_idx2);
				}

				//Set new spins
				M1[spin_idx1] = M_new1;
				M1[spin_idx2] = M_new2;

				//Find new energy
				for (int mod_idx = 0; mod_idx < pMod.size(); mod_idx++) {

					new_energy += pMod[mod_idx]->Get_Atomistic_Energy(spin_idx1);
					new_energy += pMod[mod_idx]->Get_Atomistic_Energy(spin_idx2);
				}

				double cmc_M_new = cmc_M + Mrot_new1.x + Mrot_new2.x - Mrot_old1.x - Mrot_old2.x;

				if (cmc_M_new > 0.0) {

					//Compute acceptance probability
					double P_accept = (cmc_M_new / cmc_M) * (cmc_M_new / cmc_M) * (abs(Mrot_old2.x) / abs(Mrot_new2.x)) * exp(-(new_energy - old_energy) / (BOLTZMANN * base_temperature));

					//uniform random number between 0 and 1
					double P = prng.rand();

					if (P <= P_accept) {

						//move accepted (x2 since we moved 2 spins)
						mc_acceptance_rate += 2.0 / num_moves;

						//turns out this line isn't really necessary (either cmc_M ~= cmc_M_new at low temperatures, or cmc_M varies about a steady average value so the effect of just using cmc_M computed at the start of the step averages out)
						//For the serial algorithm keep it, but for the parallel algorithm keeping this line is very problematic (there are solutions, e.g. atomic writes, but at the cost of performance).
						cmc_M = cmc_M_new;

						//next iteration, don't fall through
						continue;
					}
				}
			}

			//fell through: reject move
			M1[spin_idx1] = M_old1;
			M1[spin_idx2] = M_old2;

			continue;
		}
	}
}

void Atom_Mesh_Cubic::Iterate_MonteCarlo_Parallel_Classic(void)
{
	//number of moves in this step : one per each spin
	int num_moves = M1.get_nonempty_cells();

	//number of cells in this mesh
	unsigned N = n.dim();

	//recalculate it
	mc_acceptance_rate = 0.0;

	///////////////////////////////////////////////////////////////
	// PARALLEL MONTE-CARLO METROPOLIS

	//red-black : two passes will be taken
	int rb = 0;
	while (rb < 2) {

		double acceptance_rate = 0.0;

#pragma omp parallel for reduction(+:acceptance_rate)
		for (int idx_jk = 0; idx_jk < M1.n.y * M1.n.z; idx_jk++) {

			int j = idx_jk % M1.n.y;
			int k = (idx_jk / M1.n.y) % M1.n.z;

			//red_nudge = true for odd rows and even planes or for even rows and odd planes - have to keep index on the checkerboard pattern
			bool red_nudge = (((j % 2) == 1 && (k % 2) == 0) || (((j % 2) == 0 && (k % 2) == 1)));

			//For red pass (first) i starts from red_nudge. For black pass (second) i starts from !red_nudge.
			for (int i = (1 - rb) * red_nudge + rb * (!red_nudge); i < M1.n.x; i += 2) {

				int spin_idx = i + j * M1.n.x + k * M1.n.x*M1.n.y;

				//only consider non-empty cells
				if (M1.is_not_empty(spin_idx)) {

					//Picked spin is M1[spin_idx]
					DBL3 M1_old = M1[spin_idx];

					//obtain rotated spin in a cone around the picked spin
					double theta_rot = prng.rand() * mc_cone_angledeg * PI / 180.0;
					double phi_rot = prng.rand() * 2 * PI;
					//Move spin in cone with uniform random probability distribution. This approach only requires 2 random numbers to be generated. 
					//Also using a Gaussian distribution to move spin around the initial spin is less efficient, requiring more steps to thermalize.
					DBL3 M1_new = relrotate_polar(M1_old, theta_rot, phi_rot);

					//Find energy for current spin by summing all contributing modules energies at given spin only
					double old_energy = 0.0, new_energy = 0.0;
					for (int mod_idx = 0; mod_idx < pMod.size(); mod_idx++) {

						old_energy += pMod[mod_idx]->Get_Atomistic_Energy(spin_idx);
					}

					//Set new spin; M1_new has same length as M1[spin_idx], but reset length anyway to avoid floating point error creep
					M1[spin_idx] = M1_new.normalized() * M1_old.norm();
					for (int mod_idx = 0; mod_idx < pMod.size(); mod_idx++) {

						new_energy += pMod[mod_idx]->Get_Atomistic_Energy(spin_idx);
					}

					//Compute acceptance probability
					double P_accept = exp(-(new_energy - old_energy) / (BOLTZMANN * base_temperature));

					//uniform random number between 0 and 1
					double P = prng.rand();

					if (P > P_accept) {

						//reject move
						M1[spin_idx] = M1_old;
					}
					else acceptance_rate += 1.0 / num_moves;
				}
			}
		}

		mc_acceptance_rate += acceptance_rate;
		rb++;
	}
}

void Atom_Mesh_Cubic::Iterate_MonteCarlo_Parallel_Constrained(void)
{
	//number of moves in this step : one per each spin
	int num_moves = M1.get_nonempty_cells();

	//number of cells in this mesh
	unsigned N = n.dim();

	//number of red and black squares
	int num_reds = (n.z / 2) * (n.x * n.y / 2) + (n.z - (n.z / 2)) * (n.x * n.y / 2 + (n.x * n.y) % 2);
	int num_blacks = (n.z / 2) * (n.x * n.y / 2 + (n.x * n.y) % 2) + (n.z - (n.z / 2)) * (n.x * n.y / 2);

	//make sure indices array has correct memory allocated
	if (mc_indices_red.size() != num_reds) if (!malloc_vector(mc_indices_red, num_reds)) return;
	if (mc_indices_black.size() != num_blacks) if (!malloc_vector(mc_indices_black, num_blacks)) return;

	//recalculate it
	mc_acceptance_rate = 0.0;

	//Total moment along CMC direction
	double cmc_M = 0.0;

	//calculate cmc_M
#pragma omp parallel for reduction(+:cmc_M)
	for (int idx = 0; idx < N; idx++) {

		if (M1.is_not_empty(idx)) cmc_M += M1[idx] * cmc_n;
	}

	///////////////////////////////////////////////////////////////
	// PARALLEL CONSTRAINED MONTE-CARLO METROPOLIS

	// Following Asselin et al., PRB 82, 054415 (2010) but with some differences: 
	// 1) adaptive cone angle for target acceptance of 0.5
	// 2) move spins in a cone using uniform pdf polar and azimuthal angles
	// 3) pick spin pairs exactly once in a random order per CMC step, and pairs picked both from red or from black squares, not mixed.
	// 4) do not update cmc_M after every move, but only calculate cmc_M at the start of every step (this is problematic for parallel algorithm but turns out not necessary - see comments for the serial version where we do update after every move)

	//red-black : two passes will be taken : first generate red and black indices so we can shuffle them
	int rb = 0;
	while (rb < 2) {

#pragma omp parallel for
		for (int idx_jk = 0; idx_jk < M1.n.y * M1.n.z; idx_jk++) {

			int j = idx_jk % M1.n.y;
			int k = (idx_jk / M1.n.y) % M1.n.z;

			//red_nudge = true for odd rows and even planes or for even rows and odd planes - have to keep index on the checkerboard pattern
			bool red_nudge = (((j % 2) == 1 && (k % 2) == 0) || (((j % 2) == 0 && (k % 2) == 1)));

			//For red pass (first) i starts from red_nudge. For black pass (second) i starts from !red_nudge.
			for (int i = (1 - rb) * red_nudge + rb * (!red_nudge); i < M1.n.x; i += 2) {

				int spin_idx = i + j * M1.n.x + k * M1.n.x*M1.n.y;

				if (rb == 0) {

					//Form red squares index (0, 1, 2 ...) from total index
					int even_rows = 0, odd_rows;

					if (k % 2 == 0) {

						even_rows = (j / 2) * (n.x / 2);
						odd_rows = (j - (j / 2)) * (n.x / 2 + n.x % 2);
					}
					else {

						even_rows = (j / 2) * (n.x / 2 + n.x % 2);
						odd_rows = (j - (j / 2)) * (n.x / 2);
					}

					int even_planes = (k / 2) * (n.x * n.y / 2);
					int odd_planes = (k - (k / 2)) * (n.x * n.y / 2 + (n.x * n.y) % 2);

					int idx_red = (i / 2) + (even_rows + odd_rows) + (even_planes + odd_planes);

					mc_indices_red[idx_red] = spin_idx;
				}
				else {

					//Form black squares index (0, 1, 2 ...) from total index
					int even_rows = 0, odd_rows;

					if (k % 2 == 0) {

						even_rows = (j / 2) * (n.x / 2);
						odd_rows = (j - (j / 2)) * (n.x / 2 + n.x % 2);
					}
					else {

						even_rows = (j / 2) * (n.x / 2 + n.x % 2);
						odd_rows = (j - (j / 2)) * (n.x / 2);
					}

					int even_planes = (k / 2) * (n.x * n.y / 2);
					int odd_planes = (k - (k / 2)) * (n.x * n.y / 2 + (n.x * n.y) % 2);

					int idx_black = (i / 2) + (even_rows + odd_rows) + (even_planes + odd_planes);

					mc_indices_black[idx_black] = spin_idx;
				}
			}
		}

		rb++;
	}

	//pre-shuffle red and black indices so paired spins are not always the same
	//Serial Fisher-Yates shuffling algorithm. 
	//TO DO : Parallel shuffling is possible but a bit of a pain - probably best to use a bijective hash function to generate random permutations but need to look into this carefully.
	//The serial shuffling algorithm is not so bad as this is a minor relative cost of the overall Monte-Carlo algorithm, but need a parallel one for CUDA.
	rb = 0;
	while (rb < 2) {

		int num = (rb == 0 ? num_reds : num_blacks);
		for (int idx = num - 1; idx >= 0; idx--) {

			int rand_idx = floor(prng.rand() * (idx + 1));
			//rand_idx < num check to be sure - it is theoretically possible rand_idx = num can be generated but the probability is small.
			if (rand_idx >= num) continue;
			
			if (rb == 0) {

				std::swap(mc_indices_red[idx], mc_indices_red[rand_idx]);
			}
			else {

				std::swap(mc_indices_black[idx], mc_indices_black[rand_idx]);
			}
		}

		rb++;
	}

	//Now run the algorithm in parallel using two passes
	rb = 0;
	while (rb < 2) {

		double acceptance_rate = 0.0;
		int num = (rb == 0 ? num_reds : num_blacks);

#pragma omp parallel for reduction(+:acceptance_rate)
		for (int idx = num - 1; idx >= 0; idx -= 2) {

			if (idx == 0) continue;

			//use pre-shuffled spins : each will be picked exactly once if even number; if odd, then one spin (random) will be left untouched.
			int spin_idx1, spin_idx2;

			if (rb == 0) {
				
				spin_idx1 = mc_indices_red[idx];
				spin_idx2 = mc_indices_red[idx - 1];
			}
			else {

				spin_idx1 = mc_indices_black[idx];
				spin_idx2 = mc_indices_black[idx - 1];
			}

			//If there are empty cells then make sure to only pair non-empty ones
			if (M1.is_not_empty(spin_idx1) && M1.is_not_empty(spin_idx2)) {

				//Picked spins are M1[spin_idx1], M1[spin_idx2]
				DBL3 M_old1 = M1[spin_idx1];
				DBL3 M_old2 = M1[spin_idx2];

				//rotate to a system with x axis along cmc_n : easier to apply algorithm formulas this way
				//NOTE : don't rotate x axis towards cmc_n, but rotate cmc_n towards x axis, i.e. use invrotate_polar, not rotate_polar
				DBL3 Mrot_old1 = invrotate_polar(M_old1, cmc_n);
				DBL3 Mrot_old2 = invrotate_polar(M_old2, cmc_n);

				//obtain rotated spin in a cone around the first picked spin
				double theta_rot = prng.rand() * mc_cone_angledeg * PI / 180.0;
				double phi_rot = prng.rand() * 2 * PI;
				DBL3 Mrot_new1 = relrotate_polar(Mrot_old1, theta_rot, phi_rot);

				//adjust second spin to keep required total moment direction
				DBL3 Mrot_new2 = DBL3(0.0, Mrot_old2.y + Mrot_old1.y - Mrot_new1.y, Mrot_old2.z + Mrot_old1.z - Mrot_new1.z);
				double sq2 = Mrot_new2.y * Mrot_new2.y + Mrot_new2.z * Mrot_new2.z;
				double sqnorm = M_old2.norm()*M_old2.norm();

				if (sq2 <= sqnorm) {

					Mrot_new2.x = get_sign(Mrot_old2.x) * sqrt(sqnorm - sq2);

					//Obtain new spins in original coordinate system, i.e. rotate back
					DBL3 M_new1 = rotate_polar(Mrot_new1, cmc_n);
					DBL3 M_new2 = rotate_polar(Mrot_new2, cmc_n);
					M_new1 = M_new1.normalized() * M_old1.norm();
					M_new2 = M_new2.normalized() * M_old2.norm();

					//Find energy for current spin by summing all contributing modules energies at given spins only
					double old_energy = 0.0, new_energy = 0.0;
					for (int mod_idx = 0; mod_idx < pMod.size(); mod_idx++) {

						old_energy += pMod[mod_idx]->Get_Atomistic_Energy(spin_idx1);
						old_energy += pMod[mod_idx]->Get_Atomistic_Energy(spin_idx2);
					}

					//Set new spins
					M1[spin_idx1] = M_new1;
					M1[spin_idx2] = M_new2;

					//Find new energy
					for (int mod_idx = 0; mod_idx < pMod.size(); mod_idx++) {

						new_energy += pMod[mod_idx]->Get_Atomistic_Energy(spin_idx1);
						new_energy += pMod[mod_idx]->Get_Atomistic_Energy(spin_idx2);
					}

					double cmc_M_new = cmc_M + Mrot_new1.x + Mrot_new2.x - Mrot_old1.x - Mrot_old2.x;

					if (cmc_M_new > 0.0) {

						//Compute acceptance probability
						double P_accept = (cmc_M_new / cmc_M) * (cmc_M_new / cmc_M) * (abs(Mrot_old2.x) / abs(Mrot_new2.x)) * exp(-(new_energy - old_energy) / (BOLTZMANN * base_temperature));

						//uniform random number between 0 and 1
						double P = prng.rand();

						if (P <= P_accept) {

							//move accepted (x2 since we moved 2 spins)
							acceptance_rate += 2.0 / num_moves;

							//next iteration, don't fall through
							continue;
						}
					}
				}

				//fell through: reject move
				M1[spin_idx1] = M_old1;
				M1[spin_idx2] = M_old2;

				continue;
			}
		}

		mc_acceptance_rate += acceptance_rate;
		rb++;
	}
}

#endif