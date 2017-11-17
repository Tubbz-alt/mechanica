/*
 * MxMesh.cpp
 *
 *  Created on: Jan 31, 2017
 *      Author: andy
 */


#include "MxDebug.h"
#include <MxMesh.h>
#include <Magnum/Math/Math.h>
#include "MagnumExternal/Optional/optional.hpp"

#include <deque>
#include <limits>
#include <cmath>

int MxMesh::findVertex(const Magnum::Vector3& pos, double tolerance) {
    for (int i = 1; i < vertices.size(); ++i) {
        float dist = (vertices[i]->position - pos).dot();
        if (dist <= tolerance) {
            return i;
        }
    }
    return -1;
}

VertexPtr MxMesh::createVertex(const Magnum::Vector3& pos) {
	VertexPtr v = new MxVertex{0., 0., pos};
    vertices.push_back(v);
    assert(valid(v));
    return v;
}

CellPtr MxMesh::createCell(MxCellType *type) {
    CellPtr cell = new MxCell{type, this, nullptr};
    cells.push_back(cell);
    cell->ob_refcnt = cells.size() - 1;
    return cell;
}

void MxMesh::vertexAtributes(const std::vector<MxVertexAttribute>& attributes,
        uint vertexCount, uint stride, void* buffer) {
}


void MxMesh::dump(uint what) {
    for(int i = 0; i < vertices.size(); ++i) {
        std::cout << "[" << i << "]" << vertices[i]->position << std::endl;
    }
}

#include <random>

std::default_random_engine eng;

void MxMesh::jiggle() {

    std::uniform_real_distribution<float> distribution(-0.002,0.002);

    for (int i = 0; i < vertices.size(); ++i) {

        Vector3 test = vertices[i]->position + Vector3{distribution(eng), distribution(eng), distribution(eng)};

        if((test - initPos[i]).length() < 0.7) {
            vertices[i]->position  = test;
        }
    }
}

std::tuple<Magnum::Vector3, Magnum::Vector3> MxMesh::extents() {

    auto min = Vector3{std::numeric_limits<float>::max()};
    auto max = Vector3{std::numeric_limits<float>::min()};


    for(auto& v : vertices) {
        for(int i = 0; i < 3; ++i) {min[i] = (v->position[i] < min[i] ? v->position[i] : min[i]);}
        for(int i = 0; i < 3; ++i) {max[i] = (v->position[i] > max[i] ? v->position[i] : max[i]);}
    }

    return std::make_tuple(min, max);
}

TrianglePtr MxMesh::findTriangle(const std::array<VertexPtr, 3> &verts) {
    assert(valid(verts[0]));
    assert(valid(verts[1]));
    assert(valid(verts[2]));

    for (TrianglePtr tri : triangles) {
        if (tri->matchVertexIndices(verts) != 0) {
            return tri;
        }
    }

    return nullptr;
}






MxCellType universeCellType = {};

MxCellType *MxUniverseCell_Type = &universeCellType;

MxPartialTriangleType universePartialTriangleType = {};

MxPartialTriangleType *MxUniversePartialTriangle_Type =
        &universePartialTriangleType;

MxMesh::MxMesh()
{
    _rootCell = createCell();
    float range = edgeSplitStochasticAsymmetry / 2;
    uniformDist = std::uniform_real_distribution<float>(-range, range);
}



#if 0
/**
 * Configuration only valid for 5 cell arrangement, two cells on each face of triangle,
 * and one cell attached to each side.
 */
HRESULT MxMesh::collapseHTriangleOld(MxTriangle& tri) {

    struct CellTriangles {
        MxCell *cell;
        // the pair of triangles attached to the original triangle that will be
        // removed and re-attached.
        std::array<MxTriangle*, 2> rtri;
        // the pair of triangles on the facets of the side cells that will be split,
        // and the removed triangles will be inserted between these. These two triangles
        // are on the right face of the cell, i.e. in the faces array below, the triangles
        // in faces[i].stri are between the faces[i].cell and faces[(i+1)%3].cell
        std::array<MxTriangle*, 2> stri;
    };

    // Get a pair of triangles attached to an edge. Order them such that
    // result[0] is the top triangle, one facing tri.cells[0], and
    // result[1] is the bottom triangle facing tri.cells[1]
    auto incidentCell = [this, tri](const std::array<VertexPtr, 2> &edge) -> std::optional<CellTriangles> {
        auto triangles = EdgeTriangles(this, tri, edge);
        int i = 0;
        CellTriangles result;

        for(MxTriangle &t : triangles) {

            switch(i) {
            case 0:
                continue;
            case 1:
                if(t.cells[0] == tri.cells[0] &&
                   t.cells[1] != tri.cells[0] &&
                   t.cells[1] != tri.cells[1]) {
                       result.cell = t.cells[1];
                       result.rtri[0] = &t;
                }
                else if(t.cells[0] == tri.cells[1] &&
                   t.cells[1] != tri.cells[1] &&
                   t.cells[1] != tri.cells[0]) {
                       result.cell = t.cells[1];
                       result.rtri[0] = &t;
                 }
                else if(t.cells[1] == tri.cells[0] &&
                   t.cells[0] != tri.cells[0] &&
                   t.cells[0] != tri.cells[1]) {
                       result.cell = t.cells[0];
                       result.rtri[0] = &t;
                }
                else if(t.cells[1] == tri.cells[1] &&
                   t.cells[0] != tri.cells[1] &&
                   t.cells[0] != tri.cells[0]) {
                       result.cell = t.cells[0];
                       result.rtri[0] = &t;
                 }
                else {
                    return {};
                }
                break;
            case 2:
                // make sure that the second triangle references the correct cells
                if((t.cells[0] == result.cell &&
                        (t.cells[1] == tri.cells[0] || t.cells[1] == tri.cells[1])) ||
                   (t.cells[1] == result.cell &&
                        (t.cells[0] == tri.cells[0] || t.cells[0] == tri.cells[1]))) {
                    result.rtri[1] = &t;
                    return {result};
                }
                else {
                    return {};
                }
                break;
            default:
                // gone past 2 triangles, not valid configuration
                return {};
            }
            i++;
        }
        return {};
    };




    // the 6 surrounding triangles. The 3 ones facing the top 0 cell are in the
    // faces[x][0] spot, and the bottom ones are in faces[x][1] spot.
    CellTriangles faces[3] = {0, 0, 0};

    // i = {0,1,2}, j = {1,2 0}
    for(int i = 0; i < 3; ++i)
    {
        std::optional<CellTriangles> t = incidentCell({tri.vertices[i], tri.vertices[(i+1)%3]});
        if(t) {
            faces[i] = *t;
        } else {
        		// TODO, raise an error here
            return false;
        }
    }


    // at this point, we've verified that we have 5 cells facing this triangle, we assume
    // that the initial candidate triangle has unique cells on each face.

    // two new vertices, positions adjusted
    MxVertex *verts[2] = {createVertex({0., 0., 0.}), createVertex({0., 0., 0.})};


    auto findSplitTriangles = [this, tri, verts](VertexPtr splitVert,
            CellPtr c1, CellPtr c2) -> std::array<TrianglePtr, 2> {
        struct TriPosn {TrianglePtr tri; std::array<Vector3*, 3> posns;};
        std::vector<TriPosn> triPosns;
    };

    // find the triangles in the side faces to split
    for(int i = 0; i < 3; ++i) {

    }

    auto findEdge = [this, tri](const MxTriangle &t) -> MxEdge {
        VertexPtr verts[2];
        int found = 0;

        for(int i = 0; i < 3 && found < 2; ++i) {
            for(int j = 0; j < 3 && found < 2; ++j) {
                if(t.vertices[i] == tri.vertices[j]) {
                    verts[found++] = t.vertices[i];
                }
            }
        }
        assert(found == 2);
        return MxEdge{verts[0], verts[1]};
    };

    // go through the triangles to be removed from the upper and lower faces, and
    // reconnected to the side faces. this loop reconnects the partial face indices
    // both the upper and lower triangle share the same edge with the center tri.
    for(int i = 0; i < 3; ++i) {
        MxEdge edge = findEdge(*faces[i].rtri[0]);
        triangleManifoldDisconnect(*faces[i].rtri[0], edge);
        triangleManifoldDisconnect(*faces[i].rtri[1], edge);
    }

    // go over all of the triangles attached to the existing three vertices of the
    // triangle to be removed. Re-attach these triangles to either the new top
    // vertex, or the new bottom vertex, depending on if the triangle is in the
    // top or bottom cell, i.e shares a face with the top or bottom cells.
    for(int i = 0; i < 3; ++i) {


        // set of vertices attached to the triangle vertex
        for (auto t : tri.vertices[i]->triangles) {
            // attach to new upper vertex
            if (t->cells[0] == tri.cells[0] || t->cells[1] == tri.cells[0]) {
                for(int j = 0; i < 3; ++j) {
                    if (t->vertices[j] == tri.vertices[i]) {
                        t->vertices[j] = verts[0];
                        break;
                    }
                }
            }
            // attach to lower vertex
            else {
                for(int j = 0; i < 3; ++j) {
                    if (t->vertices[j] == tri.vertices[i]) {
                        t->vertices[j] = verts[1];
                        break;
                    }
                }
            }
        }
    }

    // re-use the adjacent triangles, we 'flip' them sideways, and insert them into the
    // side faces.

    // done re-attaching adjacent triangles, now finish up by deleting
    // the triangle, and it's vertices.
    return true;
}
#endif

HRESULT MxMesh::collapseIEdge(const MxEdge& edge) {
	return 1;
}

void MxMesh::vertexReconnect(VertexPtr o, VertexPtr n) {
}

void MxMesh::triangleManifoldDisconnect(const MxTriangle& tri,
        const MxEdge& edge) {
}

FacetPtr MxMesh::findFacet(CellPtr a, CellPtr b) {
	for(auto facet : a->facets) {
		if (incident(facet, b)) {
			return facet;
		}
	}
	return nullptr;
}

FacetPtr MxMesh::createFacet(MxFacetType* type) {
    FacetPtr facet = new MxFacet{type, this, {{nullptr, nullptr}}};
    facets.push_back(facet);
    return facet;
}

HRESULT MxMesh::updateMeshToplogy() {
}

FacetPtr MxMesh::findFacet(const std::array<VertexPtr, 4>& verts) {
    for(FacetPtr facet : facets) {
        if(facet->triangles.size() != 2) continue;

        // each triangle has to be incident to two vertices.

        bool inc = true;
        for(TrianglePtr tri : facet->triangles) {
            int incCnt = 0;
            for(VertexPtr vert : verts) {
                if(incident(tri, vert)) {
                    incCnt += 1;
                }
            }
            inc &= (incCnt == 3);
        }
        if(inc) return facet;
    }
    return nullptr;
}

HRESULT MxMesh::deleteVertex(VertexPtr v) {
    longEdges.remove(v);
    shortEdges.remove(v);
    validateEnquedEdges();
    remove(vertices, v);
#ifndef NDEBUG
    for(TrianglePtr tri : triangles) {
        assert(!incident(tri, v));
    }
#endif
    delete v;
    return S_OK;
}

HRESULT MxMesh::deleteTriangle(TrianglePtr tri) {
    longEdges.remove(tri);
    shortEdges.remove(tri);
    validateEnquedEdges();
    remove(triangles, tri);
    delete tri;

    assert(!contains(triangles, tri));

#ifndef NDEBUG
    for(CellPtr cell : cells) {
        assert(!incident(cell, tri));
    }
    for(FacetPtr facet : facets) {
        assert(!contains(facet->triangles, tri));
    }
#endif
    return S_OK;
}

bool MxMesh::splitWedgeVertex(VertexPtr v0, VertexPtr nv0, VertexPtr nv1,
        MxCell* c0, MxCell* c1, MxTriangle* tri) {

    struct TriEdge {MxTriangle *t0; MxTriangle *t1; float d0; float d1; };
    std::deque<TriEdge> wedge;

    MxTriangle *startTri = 0;

    Vector3 v0_pos = v0->position;
    Vector3 nv0_pos = nv0->position;
    Vector3 nv1_pos = nv1->position;

    for(TrianglePtr t : v0->triangles()) {
        if (incident(t, c0) && incident(t, c1)) {
            startTri = t;
            break;
        }
    }

    if (startTri == 0) {
        return false;
    }

    auto triDistance = [this, &v0_pos, v0] (const MxTriangle *t) {
        Vector3 p1, p2;
        if(t->vertices[0] != v0) {
            p1 = t->vertices[1]->position;
            p2 = t->vertices[2]->position;
        }
        else if(t->vertices[1] != v0) {
            p1 = t->vertices[0]->position;
            p2 = t->vertices[2]->position;
        }
        else {
            p1 = t->vertices[0]->position;
            p2 = t->vertices[1]->position;
        }
        (p1 + p2) / 2.;
        return 0;
    };

    auto nextTri = [this, v0] (const MxTriangle *prev, const MxTriangle *current, MxCell *c) -> MxTriangle* {
        auto &p = current->partialTriangles[0];
        MxTriangle *n = p.neighbors[0]->triangle;
        if(n != prev && incident(n, v0) && incident(n, c)) {
            return n;
        }
        n = p.neighbors[1]->triangle;
        if(n != prev && incident(n, v0) && incident(n, c)) {
            return n;
        }
        n = p.neighbors[2]->triangle;
        if(n != prev && incident(n, v0) && incident(n, c)) {
            return n;
        }
        return nullptr;
    };



    // Grab all of the triangle pointers from the wedge, add them to a
    // container (wedge). Have to do squirrelly iteration with prev pointers
    // to keep track where to go next.
    MxTriangle *prev = nextTri(nullptr, startTri, c1);
    MxTriangle *next = nextTri(prev, startTri, c1);
    MxTriangle *curr = startTri;

    // iterate over the list backwards until we find the start of the fan.
    while(prev != nullptr) {
        wedge.push_front({prev, curr, 0, 0});
        MxTriangle *tmp = nextTri(curr, prev, c1);
        curr = prev;
        prev = tmp;
    }

    // grab the triangle before the wedge (not part of the wedge, but before),
    // don't check the opposite side faces the opposite cell
    wedge.push_front({nextTri(wedge.front().t1, wedge.front().t0, c0), wedge.front().t0, 0, 0});

    // iterate over the fan forward until we find the end of the fan.
    curr = startTri;
    while(next != nullptr) {
        wedge.push_back({curr, next, 0, 0});
        MxTriangle *tmp = nextTri(curr, next, c1);
        curr = next;
        next = tmp;
    }

    // grab the triangle after the wedge
    wedge.push_back({nextTri(wedge.back().t0, wedge.back().t1, c0), wedge.back().t1, 0, 0});

#ifndef NDEBUG
    for(int i = 0; i < wedge.size() - 1; ++i) {
        TriEdge &e = wedge[i];
        assert(adjacent(e.t0, e.t1));
        assert(wedge[i].t1 == wedge[i+1].t0);
    }
#endif

    // find the edge with the smallest distance difference to the
    // new vertex positions
    float min = std::numeric_limits<float>::max();

    // the triangles that will be split apart and the
    // new triangle inserted between them
    MxTriangle *left = nullptr, *right = nullptr;

    // shared far vertex, above triangles will remain
    // connected on the far side.
    VertexPtr v;

    for(auto &e : wedge) {
        prev = e.t0;
        next = e.t1;

        // get the shared far vertex
        if (prev->vertices[0] != v0 && incident(next, prev->vertices[0])) {
            v = prev->vertices[0];
        }
        else if (prev->vertices[1] != v0 && incident(next, prev->vertices[1])) {
            v = prev->vertices[1];
        }
        else {
            assert(prev->vertices[2] != v0 && incident(next, prev->vertices[2]));
            v = prev->vertices[2];
        }

        Vector3 pos = v->position;

        e.d0 = (nv0_pos - pos).length();
        e.d1 = (nv1_pos - pos).length();
        float d = std::abs(e.d0 - e.d1);
        if (d < min) {
            min = d;
            left = prev;
            right = next;
        }
    }

    assert(left && right);

    // we now have the left and right triangles, where the new triangle
    // will be inserted between.
    for(auto &e : wedge) {
        if(e.t0 == left && e.t1 == right) {

        }
        else {

        }
    }
    return false;
}


/**
 * Determine what kind of edge we have.
 */
HRESULT MxMesh::collapseEdge(const MxEdge& edge) {

	// check if we have a manifold edge, most common kind of short edge
	if (edge.upperFacets().size() == 0 &&
	    edge.lowerFacets().size() == 0 &&
		edge.radialFacets().size() == 1) {
		return collapseManifoldEdge(edge);
	}

	std::cout << "only manifold edge collapse supported" << std::endl;
	return S_OK;

	if (edge.upperFacets().size() == 1 && edge.upperFacets().size() == 1) {

		// if any one of the facets only has one triangle, that means that it's
		// an H configuration, so collapse that triangle. Note, that means that
		// that operation will also collapse every other triangle that is adjacent
		// to that triangle.
		for(auto f : edge.radialFacets()) {
			if(f->triangles.size() == 1) {
				MxTriangle *t = f->triangles[0];
				return collapseHTriangle(t);
			}
		}

		// at this point, we know that the edge is not manifold
		// is incident to at at least two faces, that means that it's
		// an I configuration, so collapse that.
		return collapseIEdge(edge);
	}

	// could not collapse the edge
	return -1;
}


int test(const std::vector<std::string*> &stuff) {

    for(int i = 0; i < stuff.size(); ++i) {
        std::string *s = stuff[i];

        s->append("foo");
    }

    //stuff.push_back("");
    return 5;
}


static void testTriangle(const TrianglePtr tri) {
    assert(isfinite(tri->area) && tri->area > 0);
    assert(isfinite(tri->aspectRatio) && tri->aspectRatio > 0);
    assert(isfinite(tri->mass) && tri->mass > 0);
    assert(isfinite(tri->normal.length()) && tri->normal.length() > 0.9 && tri->normal.length() < 1.1);
    assert(tri->cells[0] && tri->cells[1]);
}

static int collapseStr = 0;
HRESULT MxMesh::collapseManifoldEdge(const MxEdge& e) {

    HRESULT res;

    collapseStr++;

    TrianglePtr t1 = nullptr, t2 = nullptr;

    std::vector<TrianglePtr> edgeTri = e.radialTriangles();

    assert(edgeTri.size() == 2);

    t1 = edgeTri[0]; t2 = edgeTri[1];

    VertexPtr c = nullptr, d = nullptr;

#ifndef NDEBUG
    auto cells = t1->cells;
    assert(cells == t2->cells);

    if(!cells[0]->isRoot()) assert(cells[0]->manifold());
    if(!cells[1]->isRoot()) assert(cells[1]->manifold());
#endif

    for(VertexPtr v : t1->vertices) {
        if(v != e.a && v != e.b) {
            c = v;
        }
        if(v->facets().size() != 1) {
        //    return mx_error(E_FAIL, "vertex belongs to more than one facet");
        }
    }

    for(VertexPtr v : t2->vertices) {
        if(v != e.a && v != e.b) {
            d = v;
        }
        if(v->facets().size() != 1) {
        //    return mx_error(E_FAIL, "vertex belongs to more than one facet");
        }
    }

    assert(c && d);

    auto t3 = EdgeTriangles(t1, t1->adjacentEdgeIndex(e.a, c));
    auto t4 = EdgeTriangles(t1, t1->adjacentEdgeIndex(e.b, c));
    auto t5 = EdgeTriangles(t2, t2->adjacentEdgeIndex(e.a, d));
    auto t6 = EdgeTriangles(t2, t2->adjacentEdgeIndex(e.b, d));

    // new center postion
    Magnum::Vector3 pos = (e.a->position + e.b->position) / 2;

    // all of the triangles attached to edge endpoints a and b will have thier corner
    // that is attached to the edge endpoints moved to the new center. Need to
    // check all of these triangles and make sure that we do not invert any triangles,
    // or cause any triangles to become colinear (zero area).


    // is it safe to move this triangle
    auto safeTriangleMove = [pos, t1, t2](const TrianglePtr tri, const VertexPtr vert) -> HRESULT {

        if((tri == t1) || (tri == t2)) {
            return S_OK;
        }

        Vector3 before = normal(tri->vertices[0]->position,
                tri->vertices[1]->position,
                tri->vertices[2]->position);

        Vector3 pos0 = (tri->vertices[0] == vert) ? pos : tri->vertices[0]->position;
        Vector3 pos1 = (tri->vertices[1] == vert) ? pos : tri->vertices[1]->position;
        Vector3 pos2 = (tri->vertices[2] == vert) ? pos : tri->vertices[2]->position;

        Vector3 after = normal(pos0, pos1, pos2);

        if(Magnum::Math::dot(before, after) <= 0) {
            return mx_error(E_FAIL, "can't perform edge collapse, triangle will become inverted");
        }

        if(Magnum::Math::triangle_area(pos0, pos1, pos2) < 0.001) {
            return mx_error(E_FAIL, "can't perform edge collapse, triangle area becomes too small or colinear");
        }

        return S_OK;
    };

    for(TrianglePtr tri : e.a->triangles()) {
        if((res = safeTriangleMove(tri, e.a)) != S_OK) {
            return res;
        }
    }

    for(TrianglePtr tri : e.b->triangles()) {
        if((res = safeTriangleMove(tri, e.b)) != S_OK) {
            return res;
        }
    }

    // is this configuration topologically safe to reconnect. This check that the top
    // and bottom triangle neighbors are not themselves connected.
    auto safeTopology = [](const TrianglePtr tri, const Edge& edge1, const Edge& edge2) -> HRESULT {
        for(int i = 0; i < 2; ++i) {
            if(!tri->cells[i]->isRoot()) {

                PTrianglePtr p1 = nullptr, p2 = nullptr;

                for(int j = 0; j < 3; ++j) {
                    PTrianglePtr pn = tri->partialTriangles[i].neighbors[j];
                    if(incident(pn, edge1)) {
                        p1 = pn;
                        continue;
                    }
                    if(incident(pn, edge2)) {
                        p2 = pn;
                        continue;
                    }
                }

                assert(p1 && p2);
                assert(p1 != p2);
                assert(p1 != &tri->partialTriangles[i]);
                assert(p2 != &tri->partialTriangles[i]);
                assert(adjacent(p1, &tri->partialTriangles[i]));
                assert(adjacent(p2, &tri->partialTriangles[i]));

                if (adjacent(p1, p2)) {
                    return mx_error(E_FAIL, "can't perform edge collapse, not topologically invariant");
                }
            }
        }
        return S_OK;
    };

    // make sure with topologically safe
    if((res = safeTopology(t1, {{e.a, c}}, {{e.b, c}})) != S_OK) return res;
    if((res = safeTopology(t2, {{e.a, d}}, {{e.b, d}})) != S_OK) return res;


    float leftUpperArea = Magnum::Math::triangle_area(e.a->position, c->position, pos);
    float leftLowerArea = Magnum::Math::triangle_area(e.a->position, d->position, pos);
    float rightUpperArea = Magnum::Math::triangle_area(e.b->position, c->position, pos);
    float rightLowerArea = Magnum::Math::triangle_area(e.b->position, d->position, pos);

    // need to calculate area here, because the area in the triangle has not been updated yet.
    float upperArea = Magnum::Math::triangle_area(e.a->position, e.b->position, c->position);
    float lowerArea = Magnum::Math::triangle_area(e.a->position, e.b->position, d->position);

    assert(leftLowerArea > 0 && leftUpperArea > 0 && rightLowerArea > 0 && rightUpperArea > 0);

    auto moveMaterial = [](const EdgeTriangles& leftTri,
                              const EdgeTriangles& rightTri,
                              float leftFraction, float rightFraction,
                              const TrianglePtr src) -> void {
        assert(abs(1.0 - (leftFraction + rightFraction)) < 0.01);
        int leftCount = 0;
        int rightCount = 0;

        for(auto i = leftTri.begin(); i != leftTri.end(); ++i) {
            leftCount += 1;
        }

        for(auto i = rightTri.begin(); i != rightTri.end(); ++i) {
            rightCount += 1;
        }

        assert(leftCount > 0 && rightCount > 0);

        for(TrianglePtr tri : leftTri) {
            tri->mass += (leftFraction * src->mass) / leftCount;
        }

        for(TrianglePtr tri : rightTri) {
            tri->mass += (rightFraction * src->mass) / rightCount;
        }
    };

    moveMaterial(t3, t4, leftUpperArea/upperArea, rightUpperArea/upperArea, t1);
    moveMaterial(t5, t6, leftLowerArea/lowerArea, rightLowerArea/lowerArea, t2);

    // Remove the partial triangles pointers from the given triangle,
    // and it's two neighboring partial on the given two edges,
    // and reconnect the two outer neighboring partial triangles with each other.
    // do this for both sides of the triangle.
    auto reconnectPartialTriangles = [](TrianglePtr tri, const Edge& edge1, const Edge& edge2) {
        for(int i = 0; i < 2; ++i) {
            if(!tri->cells[i]->isRoot()) {

                PTrianglePtr p1 = nullptr, p2 = nullptr;

                assert(incident(tri, edge1));
                assert(incident(tri, edge2));
                assert(incident(&tri->partialTriangles[i], edge1));
                assert(incident(&tri->partialTriangles[i], edge2));

                for(int j = 0; j < 3; ++j) {
                    assert(tri->partialTriangles[i].neighbors[j]);

                    PTrianglePtr pn = tri->partialTriangles[i].neighbors[j];
                    if(incident(pn, edge1)) {
                        p1 = pn;
                        continue;
                    }
                    if(incident(pn, edge2)) {
                        p2 = pn;
                        continue;
                    }
                }

                assert(p1 && p2);
                assert(p1 != p2);
                assert(p1 != &tri->partialTriangles[i]);
                assert(p2 != &tri->partialTriangles[i]);
                assert(adjacent(p1, &tri->partialTriangles[i]));
                assert(adjacent(p2, &tri->partialTriangles[i]));

                assert(!adjacent(p1, p2));

                disconnect(p1, &tri->partialTriangles[i]);
                disconnect(p2, &tri->partialTriangles[i]);
                connect(p1, p2);
                assert(tri->partialTriangles[i].unboundNeighborCount() == 2);
            }
        }
    };

#ifndef NDEBUG
    if(!cells[0]->isRoot()) assert(cells[0]->manifold());
    if(!cells[1]->isRoot()) assert(cells[1]->manifold());

    for(TrianglePtr tri : e.a->triangles()) {
        assert(tri->cells[0] && tri->cells[1]);
    }
    for(TrianglePtr tri : e.b->triangles()) {
        assert(tri->cells[0] && tri->cells[1]);
    }
#endif

    // disconnect the partial triangle pointers.
    reconnectPartialTriangles(t1, {{e.a, c}}, {{e.b, c}});
    reconnectPartialTriangles(t2, {{e.a, d}}, {{e.b, d}});

    VertexPtr vsrc = nullptr, vdest = nullptr;

    if(e.a->triangles().size() >= e.b->triangles().size()) {
        vsrc = e.b;
        vdest = e.a;
    } else {
        vsrc = e.a;
        vdest = e.b;
    }

    // the destination vertex where all the other triangles get moved to,
    // set this to the new center pos.
    vdest->position = pos;

    vdest->removeTriangle(t1);
    vdest->removeTriangle(t2);
    vsrc->removeTriangle(t1);
    vsrc->removeTriangle(t2);

    std::vector<TrianglePtr> srcTriangles = vsrc->triangles();

    for(int i = 0; i < vdest->triangles().size(); ++i) {
        TrianglePtr tri = vdest->triangles()[i];
        testTriangle(tri);
    }

    for(int i = 0; i < vsrc->triangles().size(); ++i) {
        TrianglePtr tri = vsrc->triangles()[i];
        testTriangle(tri);
    }

    for(TrianglePtr tri : srcTriangles) {
        if(tri == t1 || tri == t2) continue;

        testTriangle(tri);

        disconnect(tri, vsrc);
        connect(tri, vdest);

        assert(tri->cells[0] && tri->cells[1]);

        tri->positionsChanged();

        testTriangle(tri);

        for(int i = 0; i < 2; ++i) {
            tri->cells[i]->topologyChanged();
        }
    }

    disconnect(t1, e.a);
    disconnect(t1, e.b);
    disconnect(t2, e.a);
    disconnect(t2, e.b);
    disconnect(t1, c);
    disconnect(t2, d);

    assert(t1->vertices[0] == nullptr && t1->vertices[1] == nullptr && t1->vertices[2] == nullptr);
    assert(t2->vertices[0] == nullptr && t2->vertices[1] == nullptr && t2->vertices[2] == nullptr);

    deleteVertex(vsrc);

#ifndef NDEBUG
    for(TrianglePtr tri : triangles) {
        if(tri == t1 || tri == t2) continue;
        assert(!incident(tri, vsrc));
    }

    for(int i = 0; i < vdest->triangles().size(); ++i) {
        TrianglePtr tri = vdest->triangles()[i];
        assert(tri != t1 && tri != t2);
        assert(tri->isValid());
    }
#endif

    for(int i = 0; i < 2; ++i) {
        CellPtr cell = t1->cells[i];
        cell->removeChild(t1);
        cell->topologyChanged();
        cell = t2->cells[i];
        cell->removeChild(t2);
        cell->topologyChanged();
    }

    deleteTriangle(t1);
    deleteTriangle(t2);

#ifndef NDEBUG
    if(!cells[0]->isRoot()) assert(cells[0]->manifold());
    if(!cells[1]->isRoot()) assert(cells[1]->manifold());
    validate();
#endif

    return S_OK;
}



static int ctr = 0;

/**
 * go around the ring of the edge, and split every incident triangle on
 * that edge. Creates a new vertex at the midpoint of this edge.
 */
HRESULT MxMesh::splitEdge(const MxEdge& e) {
    auto triangles = e.radialTriangles();

    assert(triangles.size() >= 2);

#ifndef NDEBUG
    std::vector<TrianglePtr> newTriangles;
#endif

    ctr += 1;

    // new vertex at the center of this edge
    Vector3 center = (e.a->position + e.b->position) / 2.;
    center = center + (e.a->position - e.b->position) * uniformDist(randEngine);
    VertexPtr vert = createVertex(center);

    TrianglePtr firstNewTri = nullptr;
    TrianglePtr prevNewTri = nullptr;

    for(uint i = 0; i < triangles.size(); ++i)
    {
        TrianglePtr tri = triangles[i];
        std::cout << "tri[" << i << "], cell[0]:" << tri->cells[0] << ", cell[1]:" << tri->cells[1] << std::endl;
    }

    for(uint i = 0; i < triangles.size(); ++i)
    {
        TrianglePtr tri = triangles[i];

        #ifndef NDEBUG
        float originalArea = tri->area;
        #endif

        // find the outside tri vertex
        VertexPtr outer = nullptr;
        for(uint i = 0; i < 3; ++i) {
            if(tri->vertices[i] !=  e.b && tri->vertices[i] != e.a ) {
                outer = tri->vertices[i];
                break;
            }
        }
        assert(outer);

        // copy of old triangle vertices, replace the bottom (a) vertex
        // here with the new center vertex
        auto vertices = tri->vertices;
        for(uint i = 0; i < 3; ++i) {
            if(vertices[i] == e.a) {
                vertices[i] = vert;
                break;
            }
        }

        // the original triangle has three vertices in the order
        // {a, outer, b}, in CCW winding order. The new vertices in the same
        // winding order are {vert, outer, b}. Because we ensure the same winding
        // we can set up the cell pointers and partial triangles in the same
        // order.
        TrianglePtr nt = createTriangle((MxTriangleType*)tri->ob_type, vertices);
        nt->cells = tri->cells;
        nt->partialTriangles[0].ob_type = tri->partialTriangles[0].ob_type;
        nt->partialTriangles[1].ob_type = tri->partialTriangles[1].ob_type;

#ifndef NDEBUG
        newTriangles.push_back(nt);
#endif

        // make damned sure the winding is correct and the new triangle points
        // in the same direction as the existing one
        assert(Math::dot(nt->normal, tri->normal) >= 0);


        if(tri->facet) {
            tri->facet->appendChild(nt);
        }

        // remove the b vertex from the old triangle, and replace it with the
        // new center vertex
        disconnect(tri, e.b);
        connect(tri, vert);

        tri->positionsChanged();

        // make damned sure the winding is correct and the new triangle points
        // in the same direction as the existing one, but now with the
        // replaced b vertex
        assert(Math::dot(nt->normal, tri->normal) >= 0);

        // make sure at most 1% difference in new total area and original area.
        assert(std::abs(nt->area + tri->area - originalArea) < (1.0 / originalArea));

        // makes sure that new and old tri share an edge.
        assert(adjacent(tri, nt));

        // removes the e.b - outer edge connection connection from the old
        // triangle and replaces it with the new triangle,
        // manually add the partial triangles to the cell
        for(uint i = 0; i < 2; ++i) {
            if(tri->cells[i] != rootCell()) {

                assert(tri->partialTriangles[i].unboundNeighborCount() == 0);
                assert(nt->partialTriangles[i].unboundNeighborCount() == 3);
                reconnect(&tri->partialTriangles[i], &nt->partialTriangles[i], {{e.b, outer}});
                assert(tri->partialTriangles[i].unboundNeighborCount() == 1);
                assert(nt->partialTriangles[i].unboundNeighborCount() == 2);
                connect(&tri->partialTriangles[i], &nt->partialTriangles[i]);
                assert(tri->partialTriangles[i].unboundNeighborCount() == 0);
                assert(nt->partialTriangles[i].unboundNeighborCount() == 1);
                tri->cells[i]->boundary.push_back(&nt->partialTriangles[i]);
                if(tri->cells[i]->renderer) {
                    tri->cells[i]->renderer->invalidate();
                }
            }
        }

        assert(incident(nt, {{e.b, outer}}));
        assert(!incident(tri, {{e.b, outer}}));


        // split the mass according to area
        nt->mass = nt->area / (nt->area + tri->area) * tri->mass;
        tri->mass = tri->area / (nt->area + tri->area) * tri->mass;

        if(i == 0) {
            firstNewTri = nt;
            prevNewTri = nt;
        } else {
            #ifndef NDEBUG
            if (ctr >= 109 && triangles.size() >= 4) {
                std::cout << "boom" << std::endl;
            }
            #endif
            connect(nt, prevNewTri);
            prevNewTri = nt;
        }
    }

    // connect the first and last new triangles. If this is a
    // manifold edge, only 2 new triangles, which already got
    // connected above.
    if(triangles.size() > 2) {
        if(triangles.size() >= 4) {
            std::cout << "root cell: " << rootCell() << std::endl;
            std::cout << "boom" << std::endl;
        }
        connect(firstNewTri, prevNewTri);
    }



#ifndef NDEBUG
    for(uint t = 0; t < newTriangles.size(); ++t) {
        TrianglePtr nt = newTriangles[t];
        assert(nt->isValid());
        for(uint i = 0; i < 2; ++i) {
            if(nt->cells[i] != rootCell()) {
                assert(nt->partialTriangles[i].unboundNeighborCount() == 0);
            }
        }
    }
    for(uint t = 0; t < triangles.size(); ++t) {
        TrianglePtr tri = triangles[t];
        for(uint i = 0; i < 2; ++i) {
            if(tri->cells[i] != rootCell()) {
                assert(tri->partialTriangles[i].unboundNeighborCount() == 0);
            }
        }
    }
    for(uint t = 0; t < triangles.size(); ++t) {
        TrianglePtr tri = triangles[t];
        for(uint i = 0; i < 2; ++i) {
            if(tri->cells[i] != rootCell()) {
                assert(tri->cells[i]->manifold());
            }
        }
    }

    validate();
#endif

    return S_OK;
}

HRESULT MxMesh::collapseHTriangle(TrianglePtr tri) {

	// perpendicular facets
	FacetPtr perpFacets[3] = {nullptr};

	// identify the triangle radial facets, these are the facets that are
	// the side facets. We identify the side facets, as those that are
	// incident to a triangle vertex, but not incident to any other
	// triangle vertices. Each triangle vertex should have exactly one
	// Indecent facet that is not indecent to any other triangle vertices
	// for this to be a valid H configuration.
	for(int i = 0; i < 3; ++i) {
		for(FacetPtr p : tri->vertices[i]->facets()) {

			bool found = false;

			if(p == tri->facet) {
				continue;
			}

			for(FacetPtr x : tri->vertices[(i+1)%3]->facets()) {
				if (x == p) {
					found = true;
					break;
				}
			}

			if (!found) {
				for(FacetPtr x : tri->vertices[(i+2)%3]->facets()) {
					if (x == p) {
						found = true;
						break;
					}
				}
			}

			// we did not find this facet is incident to the other two vertices.
			// need to check that that we have not already found a perpendicular facet
			// for this triangle vertex.
			if (!found) {
				if(perpFacets[i] == nullptr) {
					perpFacets[i] = p;
				} else {
					// not a valid H configuration,
					// TODO set some sort of error conditions
					return -1;
				}
			}
		}

		// make sure we've found a perpendicular facet
		assert(perpFacets[i]);
	}

	VertexPtr v0 = collapseCellSingularFacet(tri->cells[0], tri->facet);
	VertexPtr v1 = collapseCellSingularFacet(tri->cells[1], tri->facet);

	TrianglePtr perpTriangles[3];

	for(int i = 0; i < 3; ++i) {
		perpTriangles[i] = splitFacetBoundaryVertex(perpFacets[i], tri->vertices[i], v0, v1);
	}

	return 0;
}



TrianglePtr MxMesh::splitFacetBoundaryVertex(FacetPtr face, VertexPtr v,
		VertexPtr v0, VertexPtr v1) {
	return nullptr;
}

VertexPtr MxMesh::collapseCellSingularFacet(CellPtr cell, FacetPtr facet) {
	return nullptr;
}

void MxMesh::triangleVertexReconnect(MxTriangle& tri, VertexPtr o,
		VertexPtr n) {
}

bool MxMesh::valid(TrianglePtr p) {

	if(std::find(triangles.begin(), triangles.end(), p) == triangles.end()) {
		return false;
	}


	return
		p == p->partialTriangles[0].triangle &&
		p == p->partialTriangles[1].triangle &&
		valid(p->vertices[0]) &&
		valid(p->vertices[1]) &&
		valid(p->vertices[2]);
}

bool MxMesh::valid(CellPtr c) {
	if(std::find(cells.begin(), cells.end(), c) == cells.end()) {
		return false;
	}

	for(PTrianglePtr p : c->boundary) {
		if(!valid(p)) {
			return false;
		}
	}

	return true;
}

bool MxMesh::valid(VertexPtr v) {
    return std::find(vertices.begin(), vertices.end(), v) != vertices.end();
}

bool MxMesh::valid(PTrianglePtr p) {
	return p && valid(p->triangle);
}

MxMesh::~MxMesh() {
	for(auto c : cells) {
		delete c;
	}
	for(auto p : vertices) {
		delete p;
	}
	for(auto t : triangles) {
		delete t;
	}
}

HRESULT MxMesh::positionsChanged() {
    HRESULT result = E_FAIL;

    for(VertexPtr v : vertices) {
        v->mass = 0;
        v->area = 0;
    }

    for(TrianglePtr tri : triangles) {
        if((result = tri->positionsChanged() != S_OK)) {
            return result;
        }

        // TODO: should we have explicit edges??? Save on compute time.
        for(int i = 0; i < 3; ++i) {
            float d = (tri->vertices[i]->position - tri->vertices[(i+1)%3]->position).length();

            if (d <= shortCutoff) {
                enqueueShortEdge(tri->vertices[i], tri->vertices[(i+1)%3]);
            }

            if (d >= longCutoff) {
                enqueueLongEdge(tri->vertices[i], tri->vertices[(i+1)%3]);
            }
        }
    }

    if((result = processOffendingEdges()) != S_OK) {
        return result;
    }

    for(FacetPtr facet : facets) {
        if((result = facet->positionsChanged() != S_OK)) {
            return result;
        }
    }

    for(CellPtr cell : cells) {
        if((result = cell->positionsChanged() != S_OK)) {
            return result;
        }
    }

	return S_OK;
}

HRESULT MxMesh::processOffendingEdges() {
    HRESULT result;
    while(!longEdges.empty()) {
        MxEdge edge = longEdges.top();
        longEdges.pop();
        shortEdges.remove(edge.a);
        shortEdges.remove(edge.b);
        std::cout << "trying edge split, len: " << edge.length() << std::endl;
        if(edge.length() >= longCutoff) {
            result = splitEdge(edge);
        }

    }

    while(!shortEdges.empty()) {
        MxEdge edge = shortEdges.top();
        shortEdges.pop();
        longEdges.remove(edge.a);
        longEdges.remove(edge.b);
        std::cout << "trying edge collapse edge, len: " << edge.length() << std::endl;
        if(edge.length() < shortCutoff) {
            result = collapseEdge(edge);
        }
    }
    return S_OK;
}

TrianglePtr MxMesh::createTriangle(MxTriangleType* type,
		const std::array<VertexPtr, 3>& verts) {

	TrianglePtr tri = new MxTriangle{type, verts};
	triangles.push_back(tri);

    assert(valid(tri));

    return tri;
}

HRESULT MxMesh::flipManifoldEdge(const MxEdge& e) {

    return E_NOTIMPL;
}

HRESULT MxMesh::collapseManifoldEdgeTriangles(const MxEdge& e) {

    return E_NOTIMPL;

}

bool MxMesh::isCollapseManifoldEdgeValid(const MxEdge& e) const {
    std::set<VertexPtr> v1_link = e.a->link();
    std::set<VertexPtr> v2_link = e.b->link();
    std::set<VertexPtr> e_link = e.link();
    std::vector<VertexPtr> vertexIntersection;

    std::set_intersection(v1_link.begin(), v1_link.end(),
                          v2_link.begin(), v2_link.end(),
                          std::back_inserter(vertexIntersection));

    if(e_link.size() != vertexIntersection.size()) {
        return false;
    }

    for(VertexPtr v : e_link) {
        if(!contains(vertexIntersection, v)) {
            return false;
        }
    }

    return true;
}

bool MxMesh::validateVertex(const VertexPtr v) {
    assert(contains(vertices, v));
    for(TrianglePtr tri : v->triangles()) {
        assert(incident(tri, v));
        assert(contains(triangles, tri));
        assert(tri->cells[0] && tri->cells[1]);
    }
    return true;
}

bool MxMesh::validateVertices() {
    for(int i = 0; i < vertices.size(); ++i) {
        validateVertex(vertices[i]);
    }
    return true;
}

bool MxMesh::validateTriangles() {
    for(int i = 0; i < triangles.size(); ++i) {
        validateTriangle(triangles[i]);
    }
    return true;
}

bool MxMesh::validateTriangle(const TrianglePtr tri) {
    assert(tri->isValid());

    for(int i = 0; i < 3; ++i) {
        validateVertex(tri->vertices[i]);
        assert(contains(tri->vertices[i]->triangles(), tri));
    }
    return true;
}

bool MxMesh::validate() {
    return true;
    validateTriangles();
    validateVertices();
    return true;
}

HRESULT MxMesh::enqueueShortEdge(const VertexPtr a, const VertexPtr b) {
    validateEdge(a, b);
    this->shortEdges.push({{a, b}});
    return S_OK;
}

HRESULT MxMesh::enqueueLongEdge(const VertexPtr a, const VertexPtr b) {
    validateEdge(a, b);
    longEdges.push({{a, b}});
    return S_OK;
}

bool MxMesh::validateEdge(const VertexPtr a, const VertexPtr b) {
    MxEdge e(a, b);
    return true;
}

bool MxMesh::validateEnquedEdges() {
    for(auto i = shortEdges.begin(); i != shortEdges.end(); ++i) {
        const Edge& e = *i;
        MxEdge(e[0], e[1]);
    }

   for(auto i = longEdges.begin(); i != longEdges.end(); ++i) {
        const Edge& e = *i;
        MxEdge(e[0], e[1]);
    }
   return true;
}
