#pragma once

#include "ofMain.h"

// Shape of the tubes made by makeStrokeMesh(). Lengths are in the polyline's
// own units, or multiples of radius where noted.
struct StrokeMeshSettings {
	float radius = 0.2;
	int sides = 8; // vertices around the tube
	int capRings = 3; // rings in each rounded end, counting the tip
	float fitTolerance = 0.25; // how far the smoothed curve may stray from the input, x radius
	float maxSegmentLength = 4.0; // longest straight run between rings, x radius
	float maxSegmentAngle = 15.0; // sharpest bend between rings, in degrees
	float endRadius = 0.35; // radius at the tips, as a fraction of radius
	float taperLength = 10.0; // distance from each end to full radius, x radius
};

// Builds a closed tube with rounded ends around a polyline.
//
// libigl fits a smooth cubic Bezier spline to the points (Schneider 1990),
// which removes hand jitter and lets rings be spaced by curvature instead of
// by input density. The rings are oriented with parallel-transported frames,
// so the tube doesn't twist. A single point gives a sphere.
//
// Triangles are wound counter-clockwise seen from outside, so back faces can
// be culled. Texture coordinates:
//   x: distance along the stroke in tube circumferences, so the uv space is
//      isotropic (1 in x is as long as once around the tube). It is 0 where
//      the body starts and negative on the first end cap.
//   y: 0 to 1 once around the tube. The seam vertices are doubled so y
//      doesn't wrap inside a triangle.
ofMesh makeStrokeMesh(const vector<glm::vec3> & polyline, const StrokeMeshSettings & settings = StrokeMeshSettings());
