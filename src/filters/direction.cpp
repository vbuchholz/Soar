#include <vector>
#include <string>
#include "sgnode.h"
#include "scene.h"
#include "filter.h"
#include "filter_table.h"
#include "common.h"

using namespace std;

/*
Calculates whether a bounding box is located to the left (-1), aligned
(0), or right (1) of another bounding box along the specified axis.
*/
bool direction(const sgnode *a, const sgnode *b, int axis, int comp) {
	int i, dir[3];
	vec3 amin, amax, bmin, bmax;

	a->get_bounds().get_vals(amin, amax);
	b->get_bounds().get_vals(bmin, bmax);
	
	/*
	 dir[i] = [-1, 0, 1] if a is [less than, overlapping,
	 greater than] b in that dimension.
	*/
	for(i = 0; i < 3; i++) {
		if (amax[i] <= bmin[i]) {
			dir[i] = -1;
		} else if (bmax[i] <= amin[i]) {
			dir[i] = 1;
		} else {
			dir[i] = 0;
		}
	}

	assert(0 <= axis && axis < 3);
	return comp == dir[axis];
}

bool size_comp(const sgnode *a, const sgnode *b) {
	int i, dir[3];
	vec3 amin, amax, bmin, bmax;

	a->get_bounds().get_vals(amin, amax);
	b->get_bounds().get_vals(bmin, bmax);
	float adiag = (amax-amin).norm();
	float bdiag = (bmax-bmin).norm();
	
	return (adiag*1.1) < bdiag;
}
bool linear_comp(const sgnode *a, const sgnode *b, const sgnode *c) {

	ptlist pa, pb, pc;
	a->get_bounds().get_points(pa);
	b->get_bounds().get_points(pb);
	c->get_bounds().get_points(pc);
	
	copy(pa.begin(), pa.end(), back_inserter(pc));
	float d = hull_distance(pb, pc);
	std::cout << d << std::endl;
	if (d < 0) {
		d = 0.;
		//d = -d;
	}
	//vec3 ca = a->get_centroid();
	//vec3 cb = b->get_centroid();
	//vec3 cc = c->get_centroid();;
	
	//float d = cc.line_dist(ca, cb);
	
	return (d < 0.001);
}

bool north_of(scene *scn, const vector<const sgnode*> &args) {
	assert(args.size() == 2);
	return direction(args[0], args[1], 1, 1);
}
bool smaller(scene *scn, const vector<const sgnode*> &args) {
	assert(args.size() == 2);
	return size_comp(args[0], args[1]);
}
bool linear(scene *scn, const vector<const sgnode*> &args) {
	assert(args.size() == 3);
	return linear_comp(args[0], args[1], args[2]);
}

bool south_of(scene *scn, const vector<const sgnode*> &args) {
	assert(args.size() == 2);
	return direction(args[0], args[1], 1, -1);
}

bool east_of(scene *scn, const vector<const sgnode*> &args) {
	assert(args.size() == 2);
	return direction(args[0], args[1], 0, 1);
}

bool west_of(scene *scn, const vector<const sgnode*> &args) {
	assert(args.size() == 2);
	return direction(args[0], args[1], 0, -1);
}

bool vertically_aligned(scene *scn, const vector<const sgnode*> &args) {
	assert(args.size() == 2);
	return direction(args[0], args[1], 0, 0);
}

bool horizontally_aligned(scene *scn, const vector<const sgnode*> &args) {
	assert(args.size() == 2);
	return direction(args[0], args[1], 1, 0);
}

bool planar_aligned(scene *scn, const vector<const sgnode*> &args) {
	assert(args.size() == 2);
	return direction(args[0], args[1], 2, 0);
}

bool above(scene *scn, const vector<const sgnode*> &args) {
	assert(args.size() == 2);
	return direction(args[0], args[1], 2, 1);
}

bool below(scene *scn, const vector<const sgnode*> &args) {
	assert(args.size() == 2);
	return direction(args[0], args[1], 2, -1);
}

/*
Filter version
*/

class direction_filter : public typed_map_filter<bool> {
public:
	direction_filter(Symbol *root, soar_interface *si, filter_input *input, int axis, int comp)
	: typed_map_filter<bool>(root, si, input), axis(axis), comp(comp) {}
	
	bool compute(const filter_params *p, bool adding, bool &res, bool &changed) {
		const sgnode *a, *b;
		
		if (!get_filter_param(this, p, "a", a)) {
			return false;
		}
		if (!get_filter_param(this, p, "b", b)) {
			return false;
		}
		
		bool newres = direction(a, b, axis, comp);
		changed = (newres != res);
		res = newres;
		return true;
	}
	virtual int getAxis() {
		return axis;
	}
	virtual int getComp() {
		return comp;
	}
private:
	int axis, comp;
};





class size_comp_filter : public typed_map_filter<bool> {
public:
	size_comp_filter(Symbol *root, soar_interface *si, filter_input *input)
	: typed_map_filter<bool>(root, si, input) {}
	
	bool compute(const filter_params *p, bool adding, bool &res, bool &changed) {
		const sgnode *a, *b;
		
		if (!get_filter_param(this, p, "a", a)) {
			return false;
		}
		if (!get_filter_param(this, p, "b", b)) {
			return false;
		}
		
		bool newres = size_comp(a, b);
		changed = (newres != res);
		res = newres;
		return true;
	}
};
class linear_comp_filter : public typed_map_filter<bool> {
public:
	linear_comp_filter(Symbol *root, soar_interface *si, filter_input *input)
	: typed_map_filter<bool>(root, si, input) {}
	
	bool compute(const filter_params *p, bool adding, bool &res, bool &changed) {
		const sgnode *a, *b, *c;
		
		if (!get_filter_param(this, p, "a", a)) {
			return false;
		}
		if (!get_filter_param(this, p, "b", b)) {
			return false;
		}
		if (!get_filter_param(this, p, "c", c)) {
			return false;
		}
		
		bool newres = linear_comp(a, b, c);
		changed = (newres != res);
		res = newres;
		return true;
	}
};

filter *make_north_of(Symbol *root, soar_interface *si, scene *scn, filter_input *input) {
	return new direction_filter(root, si, input, 1, 1);
}
filter *make_smaller(Symbol *root, soar_interface *si, scene *scn, filter_input *input) {
	return new size_comp_filter(root, si, input);
}
filter *make_linear(Symbol *root, soar_interface *si, scene *scn, filter_input *input) {
	return new linear_comp_filter(root, si, input);
}

filter *make_south_of(Symbol *root, soar_interface *si, scene *scn, filter_input *input) {
	return new direction_filter(root, si, input, 1, -1);
}

filter *make_east_of(Symbol *root, soar_interface *si, scene *scn, filter_input *input) {
	return new direction_filter(root, si, input, 0, 1);
}

filter *make_west_of(Symbol *root, soar_interface *si, scene *scn, filter_input *input) {
	return new direction_filter(root, si, input, 0, -1);
}

filter *make_vertically_aligned(Symbol *root, soar_interface *si, scene *scn, filter_input *input) {
	return new direction_filter(root, si, input, 0, 0);
}

filter *make_horizontally_aligned(Symbol *root, soar_interface *si, scene *scn, filter_input *input) {
	return new direction_filter(root, si, input, 1, 0);
}

filter *make_planar_aligned(Symbol *root, soar_interface *si, scene *scn, filter_input *input) {
	return new direction_filter(root, si, input, 2, 0);
}

filter *make_above(Symbol *root, soar_interface *si, scene *scn, filter_input *input) {
	return new direction_filter(root, si, input, 2, 1);
}

filter *make_below(Symbol *root, soar_interface *si, scene *scn, filter_input *input) {
	return new direction_filter(root, si, input, 2, -1);
}


filter_table_entry smaller_fill_entry() {
	filter_table_entry e;
	e.name = "smaller-than";
	e.parameters.push_back("a");
	e.parameters.push_back("b");
	e.ordered = true;
	e.allow_repeat = false;
	e.create = &make_smaller;
	e.calc = &smaller;
	return e;
}
filter_table_entry linear_fill_entry() {
	filter_table_entry e;
	e.name = "linear-with";
	e.parameters.push_back("a");
	e.parameters.push_back("b");
	e.parameters.push_back("c");
	e.ordered = true;
	e.allow_repeat = false;
	e.create = &make_linear;
	e.calc = &linear;
	return e;
}

filter_table_entry north_of_fill_entry() {
	filter_table_entry e;
	e.name = "y-greater-than";
	e.parameters.push_back("a");
	e.parameters.push_back("b");
	e.ordered = true;
	e.allow_repeat = false;
	e.create = &make_north_of;
	e.calc = &north_of;
	return e;
}

filter_table_entry south_of_fill_entry() {
	filter_table_entry e;
	e.name = "y-less-than";
	e.parameters.push_back("a");
	e.parameters.push_back("b");
	e.ordered = true;
	e.allow_repeat = false;
	e.create = &make_south_of;
	e.calc = &south_of;
	return e;
}

filter_table_entry east_of_fill_entry() {
	filter_table_entry e;
	e.name = "x-greater-than";
	e.parameters.push_back("a");
	e.parameters.push_back("b");
	e.ordered = true;
	e.allow_repeat = false;
	e.create = &make_east_of;
	e.calc = &east_of;
	return e;
}

filter_table_entry west_of_fill_entry() {
	filter_table_entry e;
	e.name = "x-less-than";
	e.parameters.push_back("a");
	e.parameters.push_back("b");
	e.ordered = true;
	e.allow_repeat = false;
	e.create = &make_west_of;
	e.calc = &west_of;
	return e;
}

filter_table_entry horizontally_aligned_fill_entry() {
	filter_table_entry e;
	e.name = "y-aligned";
	e.parameters.push_back("a");
	e.parameters.push_back("b");
	e.ordered = false;
	e.allow_repeat = false;
	e.create = &make_horizontally_aligned;
	e.calc = &horizontally_aligned;
	return e;
}

filter_table_entry vertically_aligned_fill_entry() {
	filter_table_entry e;
	e.name = "x-aligned";
	e.parameters.push_back("a");
	e.parameters.push_back("b");
	e.ordered = false;
	e.allow_repeat = false;
	e.create = &make_vertically_aligned;
	e.calc = &vertically_aligned;
	return e;
}

filter_table_entry planar_aligned_fill_entry() {
	filter_table_entry e;
	e.name = "z-aligned";
	e.parameters.push_back("a");
	e.parameters.push_back("b");
	e.ordered = false;
	e.allow_repeat = false;
	e.create = &make_planar_aligned;
	e.calc = &planar_aligned;
	return e;
}

filter_table_entry above_fill_entry() {
	filter_table_entry e;
	e.name = "z-greater-than";
	e.parameters.push_back("a");
	e.parameters.push_back("b");
	e.ordered = true;
	e.allow_repeat = false;
	e.create = &make_above;
	e.calc = &above;
	return e;
}

filter_table_entry below_fill_entry() {
	filter_table_entry e;
	e.name = "z-less-than";
	e.parameters.push_back("a");
	e.parameters.push_back("b");
	e.ordered = true;
	e.allow_repeat = false;
	e.create = &make_below;
	e.calc = &below;
	return e;
}

