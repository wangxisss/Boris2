#pragma once

#include "VEC.h"

//--------------------------------------------VEC SHAPE MASKS

/////////////////////////////////////////////////////////////////////////////////////////

//auxiliary function for generating shapes, where the shape is defined in shape_method
//shape_method takes two parameters: position of point under consideration and dimensions of shape; if point is within shape then return true
//generate shape at centre_pos with given rotation
//make given number of repetitions, with displacements between repetitions
//shape set using the indicated method (or, not, xor) and default_value
template <typename VType>
void VEC<VType>::shape_setter(std::function<bool(DBL3, DBL3)>& shape_method, MeshShape shape, VType default_value)
{
	if (!(shape.repetitions > INT3())) return;
	if (!(shape.dimensions > DBL3())) return;

	//limit number of repetitions if exceeding mesh size, otherwise they're wasted (and user might have entered something not sensible)
	if (shape.repetitions.x * shape.displacements.x > rect.length()) shape.repetitions.x = round(rect.length() / shape.displacements.x);
	if (shape.repetitions.y * shape.displacements.y > rect.width()) shape.repetitions.y = round(rect.width() / shape.displacements.y);
	if (shape.repetitions.z * shape.displacements.z > rect.height()) shape.repetitions.z = round(rect.height() / shape.displacements.z);

	//make a vector with centre positions for multiple shapes in an array
	std::vector<DBL3> centre_pos_vec;
	if (!malloc_vector(centre_pos_vec, shape.repetitions.dim())) return;

	for (int i = 0; i < shape.repetitions.i; i++) {
		for (int j = 0; j < shape.repetitions.j; j++) {
			for (int k = 0; k < shape.repetitions.k; k++) {

				int idx = i + j * shape.repetitions.i + k * shape.repetitions.i * shape.repetitions.j;
				centre_pos_vec[idx] = (shape.displacements & DBL3(i, j, k)) + shape.centre_pos;
			}
		}
	}

#pragma omp parallel for
	for (int j = 0; j < n.y; j++) {
		for (int k = 0; k < n.z; k++) {
			for (int i = 0; i < n.x; i++) {

				int idx = i + j * n.x + k * n.x*n.y;

				DBL3 position = cellidx_to_position(INT3(i, j, k));

				for (int obidx = 0; obidx < centre_pos_vec.size(); obidx++) {

					if (shape_method(rotate_object_yxz(position - centre_pos_vec[obidx], -shape.rotation.x, -shape.rotation.y, -shape.rotation.z), shape.dimensions)) {

						switch (shape.method) {

							//add shape
						case MSHAPEMETHOD_ADD:
							quantity[idx] = default_value;
							break;

							//subtract shape
						case MSHAPEMETHOD_SUB:
							quantity[idx] = VType();
							break;

							//add shape but delete overlaps
						case MSHAPEMETHOD_XOR:
							if (is_empty(idx)) {

								quantity[idx] = default_value;
							}
							else {

								quantity[idx] = VType();
							}
							break;

							//only replace existing parts
						case MSHAPEMETHOD_AND:
							if (is_not_empty(idx)) {

								quantity[idx] = default_value;
							}
							break;

						default:
							//do nothing
							break;
						}
					}
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////

//Disk with dimensions (x, y diameters, thickness), centre position relative to mesh, rotation angles, number of repetitions along x, y, z (1, 1, 1 for no repetitions), displacement values if repetitions used
template <typename VType>
std::function<bool(DBL3, DBL3)> VEC<VType>::shape_disk(MeshShape shape, VType default_value, bool setshape)
{
	std::function<bool(DBL3, DBL3)> disk = [](DBL3 centre_distance, DBL3 dimensions) -> bool {

		return (pow(centre_distance.x / (dimensions.x / 2), 2) + pow(centre_distance.y / (dimensions.y / 2), 2) < 1.0 &&
			abs(centre_distance.z) < dimensions.z / 2);
	};

	if (setshape) shape_setter(disk, shape, default_value);

	return disk;
}

//rectangle
template <typename VType>
std::function<bool(DBL3, DBL3)> VEC<VType>::shape_rect(MeshShape shape, VType default_value, bool setshape)
{
	std::function<bool(DBL3, DBL3)> rectangle = [](DBL3 centre_distance, DBL3 dimensions) -> bool {

		return (abs(centre_distance.x) < dimensions.x / 2 && abs(centre_distance.y) < dimensions.y / 2 && abs(centre_distance.z) < dimensions.z / 2);
	};

	if (setshape) shape_setter(rectangle, shape, default_value);

	return rectangle;
}

//triangle
template <typename VType>
std::function<bool(DBL3, DBL3)> VEC<VType>::shape_triangle(MeshShape shape, VType default_value, bool setshape)
{
	std::function<bool(DBL3, DBL3)> triangle = [](DBL3 centre_distance, DBL3 dimensions) -> bool {

		DBL2 s_lhs = DBL2(-dimensions.x / 2, -dimensions.y / 2);
		DBL2 s_rhs = DBL2(dimensions.x / 2, -dimensions.y / 2);
		DBL2 e = DBL2(0, dimensions.y / 2);
		DBL2 p = DBL2(centre_distance.x, centre_distance.y);

		return (
			point_on_rhs_of_line(s_lhs, e, p) && !point_on_rhs_of_line(s_rhs, e, p) &&
			abs(centre_distance.x) < dimensions.x / 2 && abs(centre_distance.y) < dimensions.y / 2 && abs(centre_distance.z) < dimensions.z / 2);
	};

	if (setshape) shape_setter(triangle, shape, default_value);

	return triangle;
}

//prolate ellipsoid
template <typename VType>
std::function<bool(DBL3, DBL3)> VEC<VType>::shape_ellipsoid(MeshShape shape, VType default_value, bool setshape)
{
	std::function<bool(DBL3, DBL3)> ellipsoid = [](DBL3 centre_distance, DBL3 dimensions) -> bool {

		return (pow(centre_distance.x / (dimensions.x / 2), 2) + pow(centre_distance.y / (dimensions.y / 2), 2) + pow(centre_distance.z / (dimensions.z / 2), 2) < 1.0);
	};

	if (setshape) shape_setter(ellipsoid, shape, default_value);

	return ellipsoid;
}

//pyramid
template <typename VType>
std::function<bool(DBL3, DBL3)> VEC<VType>::shape_pyramid(MeshShape shape, VType default_value, bool setshape)
{
	std::function<bool(DBL3, DBL3)> pyramid = [](DBL3 centre_distance, DBL3 dimensions) -> bool {

		DBL3 s_lhs_bt = DBL3(-dimensions.x / 2, -dimensions.y / 2, -dimensions.z / 2);
		DBL3 s_lhs_up = DBL3(-dimensions.x / 2, +dimensions.y / 2, -dimensions.z / 2);
		DBL3 s_rhs_bt = DBL3(+dimensions.x / 2, -dimensions.y / 2, -dimensions.z / 2);
		DBL3 s_rhs_up = DBL3(+dimensions.x / 2, +dimensions.y / 2, -dimensions.z / 2);
		DBL3 e = DBL3(0, 0, +dimensions.z / 2);
		DBL3 p = centre_distance;

		return (
			point_on_rhs_of_plane(s_lhs_bt, s_lhs_up, e, p) && point_on_rhs_of_plane(s_lhs_up, s_rhs_up, e, p) &&
			point_on_rhs_of_plane(s_rhs_up, s_rhs_bt, e, p) && point_on_rhs_of_plane(s_rhs_bt, s_lhs_bt, e, p) &&
			abs(centre_distance.x) < dimensions.x / 2 && abs(centre_distance.y) < dimensions.y / 2 && abs(centre_distance.z) < dimensions.z / 2);
	};

	if (setshape) shape_setter(pyramid, shape, default_value);

	return pyramid;
}

//tetrahedron
template <typename VType>
std::function<bool(DBL3, DBL3)> VEC<VType>::shape_tetrahedron(MeshShape shape, VType default_value, bool setshape)
{
	std::function<bool(DBL3, DBL3)> tetrahedron = [](DBL3 centre_distance, DBL3 dimensions) -> bool {

		DBL3 s_lhs_bt = DBL3(-dimensions.x / 2, -dimensions.y / 2, -dimensions.z / 2);
		DBL3 s_rhs_bt = DBL3(+dimensions.x / 2, -dimensions.y / 2, -dimensions.z / 2);
		DBL3 s_up = DBL3(0, +dimensions.y / 2, -dimensions.z / 2);
		DBL3 e = DBL3(0, 0, +dimensions.z / 2);
		DBL3 p = centre_distance;

		return (
			point_on_rhs_of_plane(s_lhs_bt, s_up, e, p) && point_on_rhs_of_plane(s_up, s_rhs_bt, e, p) && point_on_rhs_of_plane(s_rhs_bt, s_lhs_bt, e, p) &&
			abs(centre_distance.x) < dimensions.x / 2 && abs(centre_distance.y) < dimensions.y / 2 && abs(centre_distance.z) < dimensions.z / 2);
	};

	if (setshape) shape_setter(tetrahedron, shape, default_value);

	return tetrahedron;
}

//cone
template <typename VType>
std::function<bool(DBL3, DBL3)> VEC<VType>::shape_cone(MeshShape shape, VType default_value, bool setshape)
{
	std::function<bool(DBL3, DBL3)> cone = [](DBL3 centre_distance, DBL3 dimensions) -> bool {

		double rx = (dimensions.x / 2) * (0.5 - centre_distance.z / dimensions.z);
		double ry = (dimensions.y / 2) * (0.5 - centre_distance.z / dimensions.z);

		return (
			abs(centre_distance.z) < dimensions.z / 2 && 
			(sqrt(pow(centre_distance.x / rx, 2) + pow(centre_distance.y / ry, 2)) < 1.0));
	};

	if (setshape) shape_setter(cone, shape, default_value);

	return cone;
}

//torus
template <typename VType>
std::function<bool(DBL3, DBL3)> VEC<VType>::shape_torus(MeshShape shape, VType default_value, bool setshape)
{
	std::function<bool(DBL3, DBL3)> torus = [](DBL3 centre_distance, DBL3 dimensions) -> bool {

		double r = dimensions.z / 2;
		double Rx = dimensions.x / 2 - r;
		double Ry = dimensions.y / 2 - r;

		//find point on torus inner circle which is closest to point p = centre_distance
		//using projection in xy plane with z = 0 coordinate (centre of shape), this point is on a radial direction.
		DBL3 p = centre_distance;
		DBL2 p_xy = DBL2(p.x, p.y);
		DBL2 p_xy_polar = Cartesian_to_Polar(p_xy);

		double theta_rad = p_xy_polar.j * PI / 180;
		double length = sqrt(1 / (pow(cos(theta_rad) / Rx, 2) + pow(sin(theta_rad) / Ry, 2)));

		DBL2 s_xy = Polar_to_Cartesian(DBL2(length, p_xy_polar.j));
		DBL3 s = DBL3(s_xy.x, s_xy.y, 0);

		return ((s - p).norm() < r);
	};

	if (setshape) shape_setter(torus, shape, default_value);

	return torus;
}

/////////////////////////////////////////////////////////////////////////////////////////

//set composite shape: differs slightly from single elementary shape setter: this adds the composite shape into the mesh, so any subtractive shapes are only subtracted when forming the composite shape, not subtracted from the mesh
template <typename VType>
void VEC<VType>::shape_setter(std::vector<std::function<bool(DBL3, DBL3)>> shape_methods, std::vector<MeshShape> shapes, VType default_value)
{
	//make checks and adjustments on shapes vector
	for (int idx = 0; idx < shapes.size(); idx++) {

		if (!(shapes[idx].repetitions > INT3())) return;
		if (!(shapes[idx].dimensions > DBL3())) return;

		//limit number of repetitions if exceeding mesh size, otherwise they're wasted (and user might have entered something not sensible)
		if (shapes[idx].repetitions.x * shapes[idx].displacements.x > rect.length()) shapes[idx].repetitions.x = round(rect.length() / shapes[idx].displacements.x);
		if (shapes[idx].repetitions.y * shapes[idx].displacements.y > rect.width()) shapes[idx].repetitions.y = round(rect.width() / shapes[idx].displacements.y);
		if (shapes[idx].repetitions.z * shapes[idx].displacements.z > rect.height()) shapes[idx].repetitions.z = round(rect.height() / shapes[idx].displacements.z);
	}

	//make a vector with centre positions for multiple shapes in an array, for each shape in shapes

	std::vector<std::vector<DBL3>> centre_pos_vecs(shapes.size());

	for (int shape_idx = 0; shape_idx < shapes.size(); shape_idx++) {

		if (!malloc_vector(centre_pos_vecs[shape_idx], shapes[shape_idx].repetitions.dim())) return;

		for (int i = 0; i < shapes[shape_idx].repetitions.i; i++) {
			for (int j = 0; j < shapes[shape_idx].repetitions.j; j++) {
				for (int k = 0; k < shapes[shape_idx].repetitions.k; k++) {

					int idx = i + j * shapes[shape_idx].repetitions.i + k * shapes[shape_idx].repetitions.i * shapes[shape_idx].repetitions.j;
					centre_pos_vecs[shape_idx][idx] = (shapes[shape_idx].displacements & DBL3(i, j, k)) + shapes[shape_idx].centre_pos;
				}
			}
		}
	}

#pragma omp parallel for
	for (int j = 0; j < n.y; j++) {
		for (int k = 0; k < n.z; k++) {
			for (int i = 0; i < n.x; i++) {

				int idx = i + j * n.x + k * n.x*n.y;

				DBL3 position = cellidx_to_position(INT3(i, j, k));

				bool set_value = false;

				//check composite shape
				for (int shape_idx = 0; shape_idx < shapes.size(); shape_idx++) {

					//check each object in this shape (e.g. might be repeated as an array)
					for (int obidx = 0; obidx < centre_pos_vecs[shape_idx].size(); obidx++) {

						if (shape_methods[shape_idx](rotate_object_yxz(position - centre_pos_vecs[shape_idx][obidx], -shapes[shape_idx].rotation.x, -shapes[shape_idx].rotation.y, -shapes[shape_idx].rotation.z), shapes[shape_idx].dimensions)) {

							switch (shapes[shape_idx].method) {

							case MSHAPEMETHOD_ADD:
								set_value = true;
								break;

							case MSHAPEMETHOD_SUB:
								set_value = false;
								break;

							case MSHAPEMETHOD_XOR:
								set_value = !set_value;
								break;

							default:
							case MSHAPEMETHOD_AND:
								set_value &= is_not_empty(idx);
								break;
							}
						}
					}
				}

				if (set_value) {

					quantity[idx] = default_value;
				}
			}
		}
	}
}

//set a composite shape using combination of the above elementary shapes
template <typename VType>
void VEC<VType>::shape_set(std::vector<MeshShape> shapes, VType default_value)
{
	std::vector<std::function<bool(DBL3, DBL3)>> shape_methods;

	for (int idx = 0; idx < shapes.size(); idx++) {

		if (shapes[idx].id == MSHAPE_DISK) {

			shape_methods.push_back(shape_disk(shapes[idx], default_value, false));
		}
		else if (shapes[idx].id == MSHAPE_RECT) {

			shape_methods.push_back(shape_rect(shapes[idx], default_value, false));
		}
		else if (shapes[idx].id == MSHAPE_TRIANGLE) {

			shape_methods.push_back(shape_triangle(shapes[idx], default_value, false));
		}
		else if (shapes[idx].id == MSHAPE_ELLIPSOID) {

			shape_methods.push_back(shape_ellipsoid(shapes[idx], default_value, false));
		}
		else if (shapes[idx].id == MSHAPE_PYRAMID) {

			shape_methods.push_back(shape_pyramid(shapes[idx], default_value, false));
		}
		else if (shapes[idx].id == MSHAPE_TETRAHEDRON) {

			shape_methods.push_back(shape_tetrahedron(shapes[idx], default_value, false));
		}
		else if (shapes[idx].id == MSHAPE_CONE) {

			shape_methods.push_back(shape_cone(shapes[idx], default_value, false));
		}
		else if (shapes[idx].id == MSHAPE_TORUS) {

			shape_methods.push_back(shape_torus(shapes[idx], default_value, false));
		}
	}

	shape_setter(shape_methods, shapes, default_value);
}

/////////////////////////////////////////////////////////////////////////////////////////

//similar to shape_setter, but sets value in composite shape where both the mesh and composite shapes are not empty
template <typename VType>
void VEC<VType>::shape_valuesetter(std::vector<std::function<bool(DBL3, DBL3)>> shape_methods, std::vector<MeshShape> shapes, VType value)
{
	//make checks and adjustements on shapes vector
	for (int idx = 0; idx < shapes.size(); idx++) {

		if (!(shapes[idx].repetitions > INT3())) return;
		if (!(shapes[idx].dimensions > DBL3())) return;

		//limit number of repetitions if exceeding mesh size, otherwise they're wasted (and user might have entered something not sensible)
		if (shapes[idx].repetitions.x * shapes[idx].displacements.x > rect.length()) shapes[idx].repetitions.x = round(rect.length() / shapes[idx].displacements.x);
		if (shapes[idx].repetitions.y * shapes[idx].displacements.y > rect.width()) shapes[idx].repetitions.y = round(rect.width() / shapes[idx].displacements.y);
		if (shapes[idx].repetitions.z * shapes[idx].displacements.z > rect.height()) shapes[idx].repetitions.z = round(rect.height() / shapes[idx].displacements.z);
	}

	//make a vector with centre positions for multiple shapes in an array, for each shape in shapes

	std::vector<std::vector<DBL3>> centre_pos_vecs(shapes.size());

	for (int shape_idx = 0; shape_idx < shapes.size(); shape_idx++) {

		if (!malloc_vector(centre_pos_vecs[shape_idx], shapes[shape_idx].repetitions.dim())) return;

		for (int i = 0; i < shapes[shape_idx].repetitions.i; i++) {
			for (int j = 0; j < shapes[shape_idx].repetitions.j; j++) {
				for (int k = 0; k < shapes[shape_idx].repetitions.k; k++) {

					int idx = i + j * shapes[shape_idx].repetitions.i + k * shapes[shape_idx].repetitions.i * shapes[shape_idx].repetitions.j;
					centre_pos_vecs[shape_idx][idx] = (shapes[shape_idx].displacements & DBL3(i, j, k)) + shapes[shape_idx].centre_pos;
				}
			}
		}
	}

#pragma omp parallel for
	for (int j = 0; j < n.y; j++) {
		for (int k = 0; k < n.z; k++) {
			for (int i = 0; i < n.x; i++) {

				int idx = i + j * n.x + k * n.x*n.y;

				DBL3 position = cellidx_to_position(INT3(i, j, k));

				bool set_value = false;

				//check composite shape
				for (int shape_idx = 0; shape_idx < shapes.size(); shape_idx++) {

					//check each object in this shape (e.g. might be repeated as an array)
					for (int obidx = 0; obidx < centre_pos_vecs[shape_idx].size(); obidx++) {

						if (shape_methods[shape_idx](rotate_object_yxz(position - centre_pos_vecs[shape_idx][obidx], -shapes[shape_idx].rotation.x, -shapes[shape_idx].rotation.y, -shapes[shape_idx].rotation.z), shapes[shape_idx].dimensions)) {

							switch (shapes[shape_idx].method) {

							case MSHAPEMETHOD_ADD:
								set_value = true;
								break;

							case MSHAPEMETHOD_SUB:
								set_value = false;
								break;

							case MSHAPEMETHOD_XOR:
								set_value = !set_value;
								break;

							default:
							case MSHAPEMETHOD_AND:
								//do nothing
								break;
							}
						}
					}
				}

				if (set_value) quantity[idx] = value;
			}
		}
	}
}

//set a composite shape using combination of the above elementary shapes but:
//only set value in non-empty parts of mesh, which are also non-empty parts of shape 
template <typename VType>
void VEC<VType>::shape_setvalue(std::vector<MeshShape> shapes, VType value)
{
	std::vector<std::function<bool(DBL3, DBL3)>> shape_methods;

	for (int idx = 0; idx < shapes.size(); idx++) {

		if (shapes[idx].id == MSHAPE_DISK) {

			shape_methods.push_back(shape_disk(shapes[idx], value, false));
		}
		else if (shapes[idx].id == MSHAPE_RECT) {

			shape_methods.push_back(shape_rect(shapes[idx], value, false));
		}
		else if (shapes[idx].id == MSHAPE_TRIANGLE) {

			shape_methods.push_back(shape_triangle(shapes[idx], value, false));
		}
		else if (shapes[idx].id == MSHAPE_ELLIPSOID) {

			shape_methods.push_back(shape_ellipsoid(shapes[idx], value, false));
		}
		else if (shapes[idx].id == MSHAPE_PYRAMID) {

			shape_methods.push_back(shape_pyramid(shapes[idx], value, false));
		}
		else if (shapes[idx].id == MSHAPE_TETRAHEDRON) {

			shape_methods.push_back(shape_tetrahedron(shapes[idx], value, false));
		}
		else if (shapes[idx].id == MSHAPE_CONE) {

			shape_methods.push_back(shape_cone(shapes[idx], value, false));
		}
		else if (shapes[idx].id == MSHAPE_TORUS) {

			shape_methods.push_back(shape_torus(shapes[idx], value, false));
		}
	}

	shape_valuesetter(shape_methods, shapes, value);
}