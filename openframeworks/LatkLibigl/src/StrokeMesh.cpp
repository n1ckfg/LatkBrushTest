#include "StrokeMesh.h"

// openFrameworks' PI and TWO_PI macros clash with libigl (see the ofxLibigl
// README), and so does the B0 baud rate macro from <termios.h>, which
// openFrameworks includes on Linux.
#pragma push_macro("PI")
#pragma push_macro("TWO_PI")
#pragma push_macro("B0")
#undef PI
#undef TWO_PI
#undef B0
#include <igl/bezier.h>
#include <igl/cumsum.h>
#include <igl/fit_cubic_bezier.h>
#include <igl/rotation_matrix_from_directions.h>
#pragma pop_macro("B0")
#pragma pop_macro("TWO_PI")
#pragma pop_macro("PI")

namespace {

using Vec3 = Eigen::Vector3d;

// A point on the tube's centre line.
struct Station {
	Vec3 position;
	Vec3 tangent;
};

double angleBetween(const Vec3 & a, const Vec3 & b) {
	return std::atan2(a.cross(b).norm(), a.dot(b));
}

// Fits a smooth spline to the points and samples it, with more stations where
// it bends and fewer where it runs straight.
vector<Station> sampleSpine(const Eigen::MatrixXd & points, const StrokeMeshSettings & settings) {
	vector<Station> spine;
	if (points.rows() == 1) {
		spine.push_back({points.row(0).transpose(), Vec3::UnitZ()});
		return spine;
	}

	const double tolerance = settings.fitTolerance * settings.radius;
	vector<Eigen::MatrixXd> cubics;
	igl::fit_cubic_bezier(points, tolerance * tolerance, cubics);

	const double maxLength = settings.maxSegmentLength * settings.radius;
	const double maxAngle = ofDegToRad(settings.maxSegmentAngle);
	for (size_t c = 0; c < cubics.size(); c++) {
		const Eigen::MatrixXd & controls = cubics[c];
		// The derivative of a cubic Bezier is a quadratic Bezier (the hodograph).
		Eigen::MatrixXd hodograph(3, 3);
		for (int i = 0; i < 3; i++) hodograph.row(i) = 3.0 * (controls.row(i + 1) - controls.row(i));

		// Arc length is close to the mean of the chord and the control polygon.
		double polygon = 0;
		for (int i = 0; i < 3; i++) polygon += (controls.row(i + 1) - controls.row(i)).norm();
		const double length = 0.5 * (polygon + (controls.row(3) - controls.row(0)).norm());
		// The tangent turns no further than the hodograph's control polygon.
		const double turn = angleBetween(hodograph.row(0).transpose(), hodograph.row(1).transpose())
			+ angleBetween(hodograph.row(1).transpose(), hodograph.row(2).transpose());
		const int segments = std::max({1, (int)std::ceil(length / maxLength), (int)std::ceil(turn / maxAngle)});

		// Each cubic starts where the last one ended, so skip its first sample.
		const int first = c == 0 ? 0 : 1;
		const Eigen::VectorXd t = Eigen::VectorXd::LinSpaced(segments + 1, 0, 1).tail(segments + 1 - first);
		Eigen::MatrixXd positions, derivatives;
		igl::bezier(controls, t, positions);
		igl::bezier(hodograph, t, derivatives);
		for (int i = 0; i < t.size(); i++) {
			spine.push_back({positions.row(i).transpose(), derivatives.row(i).transpose()});
		}
	}

	// The derivative can vanish where a control point sits on an end point;
	// estimate those tangents from the neighbouring stations instead.
	const double minSpeed = 1e-9 * settings.radius;
	for (size_t i = 0; i < spine.size(); i++) {
		Vec3 & tangent = spine[i].tangent;
		if (tangent.norm() <= minSpeed) {
			tangent = spine[std::min(i + 1, spine.size() - 1)].position - spine[i > 0 ? i - 1 : 0].position;
		}
		tangent = tangent.norm() > minSpeed ? tangent.normalized() : Vec3::UnitZ();
	}
	return spine;
}

}

ofMesh makeStrokeMesh(const vector<glm::vec3> & polyline, const StrokeMeshSettings & settings) {
	ofMesh mesh;
	mesh.setMode(OF_PRIMITIVE_TRIANGLES);

	// Repeated points carry no shape and upset the spline fit's tangent estimates.
	const double minGap = 1e-3 * settings.radius;
	Eigen::MatrixXd points(polyline.size(), 3);
	int count = 0;
	for (auto & p : polyline) {
		const Eigen::RowVector3d q(p.x, p.y, p.z);
		if (!q.allFinite() || (count > 0 && (q - points.row(count - 1)).norm() <= minGap)) continue;
		points.row(count++) = q;
	}
	if (count == 0) return mesh;
	points.conservativeResize(count, 3);

	const vector<Station> spine = sampleSpine(points, settings);
	const int n = spine.size();

	// Rotation-minimising frames: carry the first normal down the spine by the
	// rotation between successive tangents.
	vector<Vec3> normals(n);
	{
		const Vec3 & t = spine[0].tangent;
		Vec3 axis = Vec3::Zero();
		int k;
		t.cwiseAbs().minCoeff(&k);
		axis[k] = 1;
		normals[0] = t.cross(axis).normalized();
	}
	for (int i = 1; i < n; i++) {
		const Vec3 & a = spine[i - 1].tangent;
		const Vec3 & b = spine[i].tangent;
		Vec3 normal = normals[i - 1];
		// The rotation is ill-defined for (anti)parallel tangents, but there the
		// previous normal is still perpendicular to the new tangent.
		if (std::abs(a.dot(b)) < 1 - 1e-9) normal = igl::rotation_matrix_from_directions(a, b) * normal;
		// Re-orthogonalise so rounding errors don't build up.
		normals[i] = (normal - normal.dot(b) * b).normalized();
	}

	// Arc length at each station.
	Eigen::VectorXd segmentLengths(n - 1);
	for (int i = 0; i + 1 < n; i++) segmentLengths(i) = (spine[i + 1].position - spine[i].position).norm();
	Eigen::VectorXd arcLength;
	igl::cumsum(segmentLengths, 1, true, arcLength);
	const double length = arcLength(n - 1);

	// The radius eases from endRadius at the tips to full size taperLength in.
	// Short strokes never reach full size, like a quick dab of a brush.
	const double radius = settings.radius;
	const double taperLength = settings.taperLength * radius;
	auto ease = [&](double x, double & slope) {
		if (taperLength <= 0) {
			slope = 0;
			return 1.0;
		}
		const double y = ofClamp(x / taperLength, 0, 1);
		slope = 6 * y * (1 - y) / taperLength;
		return y * y * (3 - 2 * y);
	};
	// Returns the radius at arc length x, and its slope in slope.
	auto profile = [&](double x, double & slope) {
		double inSlope, outSlope;
		const double in = ease(x, inSlope);
		const double out = ease(length - x, outSlope);
		const double scale = radius * (1 - settings.endRadius);
		slope = scale * (inSlope * out - in * outSlope);
		return radius * settings.endRadius + scale * in * out;
	};

	const int sides = std::max(3, settings.sides);
	const int columns = sides + 1;
	const int capRings = std::max(1, settings.capRings);
	const int rings = n + 2 * capRings;
	mesh.getVertices().reserve(rings * columns);
	mesh.getNormals().reserve(rings * columns);
	mesh.getTexCoords().reserve(rings * columns);
	mesh.getIndices().reserve(6 * sides * (rings - 2));

	vector<double> cosines(columns), sines(columns);
	for (int j = 0; j < columns; j++) {
		cosines[j] = std::cos(TWO_PI * j / sides);
		sines[j] = std::sin(TWO_PI * j / sides);
	}
	const double uScale = 1.0 / (TWO_PI * radius);

	// Adds a ring of vertices around center. Each vertex normal is
	// (around * outward + along * tangent), normalised.
	auto addRing = [&](const Vec3 & center, const Vec3 & tangent, const Vec3 & normal, double ringRadius, double around, double along, double u) {
		const Vec3 binormal = tangent.cross(normal);
		for (int j = 0; j < columns; j++) {
			const Vec3 outward = cosines[j] * normal + sines[j] * binormal;
			const Vec3 position = center + ringRadius * outward;
			const Vec3 vertexNormal = (around * outward + along * tangent).normalized();
			mesh.addVertex(glm::vec3(position.x(), position.y(), position.z()));
			mesh.addNormal(glm::vec3(vertexNormal.x(), vertexNormal.y(), vertexNormal.z()));
			mesh.addTexCoord(glm::vec2(u * uScale, float(j) / sides));
		}
	};

	// Rounded end caps are half-ellipsoids, sampled evenly by angle from the
	// tip (phi = 0) to where they meet the body. Their u follows the surface.
	double slope;
	const Station & start = spine.front();
	const double startRadius = profile(0, slope);
	for (int k = 0; k < capRings; k++) {
		const double phi = HALF_PI * k / capRings;
		addRing(start.position - start.tangent * startRadius * std::cos(phi), start.tangent, normals.front(),
			startRadius * std::sin(phi), std::sin(phi), -std::cos(phi), -startRadius * (HALF_PI - phi));
	}

	// The normal of a tube whose radius changes leans back by the slope.
	for (int i = 0; i < n; i++) {
		const double ringRadius = profile(arcLength(i), slope);
		addRing(spine[i].position, spine[i].tangent, normals[i], ringRadius, 1, -slope, arcLength(i));
	}

	const Station & end = spine.back();
	const double endRadius = profile(length, slope);
	for (int k = capRings - 1; k >= 0; k--) {
		const double phi = HALF_PI * k / capRings;
		addRing(end.position + end.tangent * endRadius * std::cos(phi), end.tangent, normals.back(),
			endRadius * std::sin(phi), std::sin(phi), std::cos(phi), length + endRadius * (HALF_PI - phi));
	}

	// Quads between neighbouring rings. The first and last rings are the tips,
	// where one triangle of each quad collapses to a line, so skip those.
	for (int i = 0; i + 1 < rings; i++) {
		for (int j = 0; j < sides; j++) {
			const ofIndexType a = i * columns + j;
			const ofIndexType b = a + 1;
			const ofIndexType c = a + columns;
			const ofIndexType d = c + 1;
			if (i > 0) mesh.addTriangle(a, b, c);
			if (i + 2 < rings) mesh.addTriangle(b, d, c);
		}
	}

	return mesh;
}
