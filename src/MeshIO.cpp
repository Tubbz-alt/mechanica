/*
 * MeshIO.cpp
 *
 *  Created on: Apr 5, 2018
 *      Author: andy
 */

#include "MeshIO.h"

#include <iostream>



#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include <unordered_map>

#include <OpenGL/glu.h>

GLUtesselator *tobj;

// AssImp
// Scene : Mesh*
// Mesh { vertices*, normals*, texCoord*, faces*}

/**
 *
 * * enumerate all the vertices, identify cell relationships
 * for each mesh
 *     for each vertex
 *         look in cache for vertex, if so, add mesh ptr to it. if not, insert
 *         vertex into dictionary, we share vertices.
 *
 * if every vertex has at most four incident cells, we're OK, can build a mesh.
 * create a real mesh vertex in the mesh, relate it to the cached vertices.
 *
 * for each mesh
 *     mesh faces should have the correct winding from assimp, normal facing out.
 *     create a new cell
 *
 *     for each face in mesh
 *        find an existing triangle in mesh, if found, make sure the empty partial
 *        triangle is correctly aligned for the current mesh.
 *        if not found, create a new triangle in mesh.
 *
 *     after all faces processed, go over all partial triangles in cell, connect
 *     neighbor pointers.
 *
 */





struct AiVecHasher
{
    std::size_t operator()(const aiVector3D &vec) const
    {
        using std::size_t;
        using std::hash;
        using std::string;

        // assume most vertices are around the same distance, shift x and
        // y to make hash func more unique.

        return ((hash<ai_real>()(100. * vec[0])
                ^ (hash<ai_real>()(10. * vec[1])
                        ^ (hash<ai_real>()(vec[2])))));
    }
};

struct ImpEdge {

    ImpEdge(aiMesh *msh, struct ImpVertex *v1, struct ImpVertex *v2) {
        aiMeshes.push_back(msh);
        verts[0] = v1;
        verts[1] = v2;
    }

    struct ImpVertex *verts[2];

    // the mesh (cells) that this vertex belongs to
    std::vector<aiMesh*> aiMeshes;
};

typedef std::vector<ImpEdge> EdgeVector;

typedef std::unordered_map<aiVector3D, ImpEdge*, AiVecHasher> EdgeMap;

struct ImpVertex {
    ImpVertex(aiMesh *msh) {
        aiMeshes.push_back(msh);
    }
    // the mesh (cells) that this vertex belongs to
    std::vector<aiMesh*> aiMeshes;

    bool containsMesh(aiMesh* msh) {
        return std::find(aiMeshes.begin(), aiMeshes.end(), msh) != aiMeshes.end();
    }

    ImpEdge *edgeForVertex(const aiVector3D &vert) {
        EdgeMap::iterator i = edges.find(vert);

        return i != edges.end() ? i->second : nullptr;
    }

    // the Mx vertex that we create in our mx mesh.
    MxVertex *vert = nullptr;

    EdgeMap edges;
};


struct ImpFace {
    bool equals(const aiFace *face) {

    }
};



/**
 * Hash:
 * A unary function object type that takes an object of type key type as argument and returns a unique value of type size_t based on it. This can either be a class implementing a function call operator or a pointer to a function (see constructor for an example). This defaults to hash<Key>, which returns a hash value with a probability of collision approaching 1.0/std::numeric_limits<size_t>::max().
 * The unordered_map object uses the hash values returned by this function to organize its elements internally, speeding up the process of locating individual elements.
 * Aliased as member type unordered_map::hasher.
 *
 * A hash function; this must be a class that overrides operator() and calculates
 * the hash value given an object of the key-type. One particularly
 * straight-forward way of doing this is to specialize the std::hash template
 * for your key-type.
 *
 * Pred:
 * A binary predicate that takes two arguments of the key type and returns a bool. The expression pred(a,b), where pred is an object of this type and a and b are key values, shall return true if a is to be considered equivalent to b. This can either be a class implementing a function call operator or a pointer to a function (see constructor for an example). This defaults to equal_to<Key>, which returns the same as applying the equal-to operator (a==b).
 * The unordered_map object uses this expression to determine whether two element keys are equivalent. No two elements in an unordered_map container can have keys that yield true using this predicate.
 * Aliased as member type unordered_map::key_equal.
 */

typedef std::unordered_map<aiVector3D, ImpVertex, AiVecHasher> VectorMap;

/**
 * importer context.
 */
struct ImpCtx {

};

static ImpEdge *findImpEdge(VectorMap &vecMap, const aiMesh *aim, const aiVector3D &v1,
        const aiVector3D &v2) {

    VectorMap::iterator i1 = vecMap.find(v1);

    assert(i1 != vecMap.end());

    return i1->second.edgeForVertex(v2);
}

static ImpEdge *createImpEdge(VectorMap &vecMap, EdgeVector &edges, aiMesh *msh, const aiVector3D &v1,
        const aiVector3D &v2) {

    ImpVertex *vv1 = &vecMap.at(v1);
    ImpVertex *vv2 = &vecMap.at(v2);
    edges.emplace_back(msh, vv1, vv2);
    return &edges.back();
}

static void createTriangleForCell(MxMesh *mesh,
        const std::array<VertexPtr, 3>& verts, CellPtr cell) {
    TrianglePtr tri = mesh->findTriangle(verts);
    if(tri) {
        assert((tri->cells[0] != nullptr || tri->cells[1] != nullptr)
                && "found triangle that's connected on both sides");
    }
    else {
        tri = mesh->createTriangle(nullptr, verts);
    }

    assert(tri);
    assert(tri->cells[0] == nullptr || tri->cells[1] == nullptr);

    Vector3 meshNorm = Math::normal(verts[0]->position, verts[1]->position, verts[2]->position);
    float orientation = Math::dot(meshNorm, tri->normal);

    int cellIndx = orientation > 0 ? 0 : 1;
    cell->appendChild(&tri->partialTriangles[cellIndx]);
}

static void addUnclaimedPartialTrianglesToRoot(MxMesh *mesh)
{
    for(TrianglePtr tri : mesh->triangles) {
        assert(tri->cells[0]);
        if(!tri->cells[1]) {
            mesh->rootCell()->appendChild(&tri->partialTriangles[1]);
        }
    }
    assert(mesh->rootCell()->isValid());
}

MxMesh* MxMesh_FromFile(const char* fname, float density, MeshCellTypeHandler cellTypeHandler)
{
    Assimp::Importer imp;

    VectorMap vecMap;
    EdgeVector edges;

    uint flags = aiProcess_JoinIdenticalVertices |
            //aiProcess_Triangulate |
            aiProcess_RemoveComponent |
            aiProcess_FindDegenerates;

    imp.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
            aiComponent_NORMALS |
            aiComponent_TANGENTS_AND_BITANGENTS |
            aiComponent_COLORS |
            aiComponent_TEXCOORDS |
            aiComponent_BONEWEIGHTS |
            aiComponent_CAMERAS |
            aiComponent_LIGHTS |
            aiComponent_MATERIALS |
            aiComponent_TEXTURES |
            aiComponent_ANIMATIONS
            );

    // Reads the given file and returns its contents if successful.
    const aiScene *scene = imp.ReadFile (fname, flags);

    if(!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
    {
        std::cout << "ERROR::ASSIMP::" << imp.GetErrorString() << std::endl;
        return nullptr;
    }

    for(int i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh *mesh = scene->mMeshes[i];
        for(int j = 0; j < mesh->mNumVertices; ++j) {
            aiVector3D vec = mesh->mVertices[j];

            VectorMap::iterator i = vecMap.find(vec);

            if(i == vecMap.end()) {
                vecMap.emplace(vec, ImpVertex(mesh));
            } else if (!i->second.containsMesh(mesh)){
                if(i->second.aiMeshes.size() < 4) {
                    i->second.aiMeshes.push_back(mesh);
                }
                else {
                    // error, mesh has vertex with more than 4 cells
                    return nullptr;
                }
            }
        }
    }

    // now we've iterated over all vertices in the mesh, and checked to make sure that
    // no vertex is incident to more than 4 cells, safe to make new mesh now.

    // iterate over all the stored vertices in the dictionary, and make new vertices
    // in the mesh for these.

    MxMesh *mesh = new MxMesh();

    for(VectorMap::iterator i = vecMap.begin(); i != vecMap.end(); ++i) {
        const aiVector3D &pos = i->first;
        i->second.vert = mesh->createVertex({{pos.x, pos.y, pos.z}});

        std::cout << "created new vertex: " << i->second.vert << std::endl;
    }


    // the AI mesh has a set of 'faces', Each face is a set of indices. Each face
    // should be a triangle, as we tell assimp to fully triangulate the meshes.

    for(int i = 0; i < scene->mNumMeshes; ++i) {
        
        aiMesh *aim = scene->mMeshes[i];
        
        std::cout << "creating new cell \"" << aim->mName.C_Str() << "\"" << std::endl;

        CellPtr cell = mesh->createCell(cellTypeHandler(aim->mName.C_Str(), i));

        for(int j = 0; j < aim->mNumFaces; ++j) {
            aiFace *face = &aim->mFaces[j];

            assert(face->mNumIndices == 3);

            // enumerate all the faces
            for(int k = 0; k < face->mNumIndices; ++k) {
                aiVector3D &v1 = aim->mVertices[k];
                aiVector3D &v2 = aim->mVertices[(k+1)%face->mNumIndices];

                ImpEdge *edge = findImpEdge(vecMap, aim, v1, v2);

                if(edge == nullptr) {
                    edge = createImpEdge(vecMap, edges, aim, v1, v2);
                }
            }

            MxVertex *v0 = vecMap.at(aim->mVertices[face->mIndices[0]]).vert;
            MxVertex *v1 = vecMap.at(aim->mVertices[face->mIndices[1]]).vert;
            MxVertex *v2 = vecMap.at(aim->mVertices[face->mIndices[2]]).vert;
            
            std::cout << "creating new triangle: {" << v0 << ", " << v1 << ", " << v2 << "}" << std::endl;

            createTriangleForCell(mesh, {{v0, v1, v2}}, cell);
        }

        for(PTrianglePtr pt : cell->boundary) {
            float area = Magnum::Math::triangle_area(pt->triangle->vertices[0]->position,
                                                     pt->triangle->vertices[1]->position,
                                                     pt->triangle->vertices[2]->position);
            pt->mass = area * density;
        }

        assert(mesh->valid(cell));
        assert(cell->updateDerivedAttributes() == S_OK);
        assert(cell->isValid());
    }


    //addUnclaimedPartialTrianglesToRoot(mesh);

    return mesh;
}


/*
---------------------------------------------------------------------------
Open Asset Import Library (assimp)
---------------------------------------------------------------------------

Copyright (c) 2006-2018, assimp team



All rights reserved.

Redistribution and use of this software in source and binary forms,
with or without modification, are permitted provided that the following
conditions are met:

* Redistributions of source code must retain the above
  copyright notice, this list of conditions and the
  following disclaimer.

* Redistributions in binary form must reproduce the above
  copyright notice, this list of conditions and the
  following disclaimer in the documentation and/or other
  materials provided with the distribution.

* Neither the name of the assimp team, nor the names of its
  contributors may be used to endorse or promote products
  derived from this software without specific prior
  written permission of the assimp team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
---------------------------------------------------------------------------
*/

/** @file  TriangulateProcess.cpp
 *  @brief Implementation of the post processing step to split up
 *    all faces with more than three indices into triangles.
 *
 *
 *  The triangulation algorithm will handle concave or convex polygons.
 *  Self-intersecting or non-planar polygons are not rejected, but
 *  they're probably not triangulated correctly.
 *
 * DEBUG SWITCHES - do not enable any of them in release builds:
 *
 * AI_BUILD_TRIANGULATE_COLOR_FACE_WINDING
 *   - generates vertex colors to represent the face winding order.
 *     the first vertex of a polygon becomes red, the last blue.
 * AI_BUILD_TRIANGULATE_DEBUG_POLYS
 *   - dump all polygons and their triangulation sequences to
 *     a file
 */



#include <memory>

//#define AI_BUILD_TRIANGULATE_COLOR_FACE_WINDING
//#define AI_BUILD_TRIANGULATE_DEBUG_POLYS

#define POLY_GRID_Y 40
#define POLY_GRID_X 70
#define POLY_GRID_XPAD 20
#define POLY_OUTPUT_FILE "assimp_polygons_debug.txt"


// -------------------------------------------------------------------------------
/** Compute the signed area of a triangle.
 *  The function accepts an unconstrained template parameter for use with
 *  both aiVector3D and aiVector2D, but generally ignores the third coordinate.*/
template <typename T>
inline double GetArea2D(const T& v1, const T& v2, const T& v3)
{
    return 0.5 * (v1.x * ((double)v3.y - v2.y) + v2.x * ((double)v1.y - v3.y) + v3.x * ((double)v2.y - v1.y));
}

// -------------------------------------------------------------------------------
/** Test if a given point p2 is on the left side of the line formed by p0-p1.
 *  The function accepts an unconstrained template parameter for use with
 *  both aiVector3D and aiVector2D, but generally ignores the third coordinate.*/
template <typename T>
inline bool OnLeftSideOfLine2D(const T& p0, const T& p1,const T& p2)
{
    return GetArea2D(p0,p2,p1) > 0;
}


// -------------------------------------------------------------------------------
/** Compute the normal of an arbitrary polygon in R3.
 *
 *  The code is based on Newell's formula, that is a polygons normal is the ratio
 *  of its area when projected onto the three coordinate axes.
 *
 *  @param out Receives the output normal
 *  @param num Number of input vertices
 *  @param x X data source. x[ofs_x*n] is the n'th element.
 *  @param y Y data source. y[ofs_y*n] is the y'th element
 *  @param z Z data source. z[ofs_z*n] is the z'th element
 *
 *  @note The data arrays must have storage for at least num+2 elements. Using
 *  this method is much faster than the 'other' NewellNormal()
 */
template <int ofs_x, int ofs_y, int ofs_z, typename TReal>
inline void NewellNormal (aiVector3t<TReal>& out, int num, TReal* x, TReal* y, TReal* z)
{
    // Duplicate the first two vertices at the end
    x[(num+0)*ofs_x] = x[0];
    x[(num+1)*ofs_x] = x[ofs_x];

    y[(num+0)*ofs_y] = y[0];
    y[(num+1)*ofs_y] = y[ofs_y];

    z[(num+0)*ofs_z] = z[0];
    z[(num+1)*ofs_z] = z[ofs_z];

    TReal sum_xy = 0.0, sum_yz = 0.0, sum_zx = 0.0;

    TReal *xptr = x +ofs_x, *xlow = x, *xhigh = x + ofs_x*2;
    TReal *yptr = y +ofs_y, *ylow = y, *yhigh = y + ofs_y*2;
    TReal *zptr = z +ofs_z, *zlow = z, *zhigh = z + ofs_z*2;

    for (int tmp=0; tmp < num; tmp++) {
        sum_xy += (*xptr) * ( (*yhigh) - (*ylow) );
        sum_yz += (*yptr) * ( (*zhigh) - (*zlow) );
        sum_zx += (*zptr) * ( (*xhigh) - (*xlow) );

        xptr  += ofs_x;
        xlow  += ofs_x;
        xhigh += ofs_x;

        yptr  += ofs_y;
        ylow  += ofs_y;
        yhigh += ofs_y;

        zptr  += ofs_z;
        zlow  += ofs_z;
        zhigh += ofs_z;
    }
    out = aiVector3t<TReal>(sum_yz,sum_zx,sum_xy);
}


// -------------------------------------------------------------------------------
/** Test if a given point is inside a given triangle in R2.
 * The function accepts an unconstrained template parameter for use with
 *  both aiVector3D and aiVector2D, but generally ignores the third coordinate.*/
template <typename T>
inline bool PointInTriangle2D(const T& p0, const T& p1,const T& p2, const T& pp)
{
    // Point in triangle test using baryzentric coordinates
    const aiVector2D v0 = p1 - p0;
    const aiVector2D v1 = p2 - p0;
    const aiVector2D v2 = pp - p0;

    double dot00 = v0 * v0;
    double dot01 = v0 * v1;
    double dot02 = v0 * v2;
    double dot11 = v1 * v1;
    double dot12 = v1 * v2;

    const double invDenom = 1 / (dot00 * dot11 - dot01 * dot01);
    dot11 = (dot11 * dot02 - dot01 * dot12) * invDenom;
    dot00 = (dot00 * dot12 - dot01 * dot02) * invDenom;

    return (dot11 > 0) && (dot00 > 0) && (dot11 + dot00 < 1);
}


/**
 * Copied directly from AssImp.
 */
// Triangulates the given mesh.
bool TriangulateMesh( aiMesh* pMesh)
{
    // Now we have aiMesh::mPrimitiveTypes, so this is only here for test cases
    if (!pMesh->mPrimitiveTypes)    {
        bool bNeed = false;

        for( unsigned int a = 0; a < pMesh->mNumFaces; a++) {
            const aiFace& face = pMesh->mFaces[a];

            if( face.mNumIndices != 3)  {
                bNeed = true;
            }
        }
        if (!bNeed)
            return false;
    }
    else if (!(pMesh->mPrimitiveTypes & aiPrimitiveType_POLYGON)) {
        return false;
    }

    // Find out how many output faces we'll get
    unsigned int numOut = 0, max_out = 0;
    bool get_normals = true;
    for( unsigned int a = 0; a < pMesh->mNumFaces; a++) {
        aiFace& face = pMesh->mFaces[a];
        if (face.mNumIndices <= 4) {
            get_normals = false;
        }
        if( face.mNumIndices <= 3) {
            numOut++;

        }
        else {
            numOut += face.mNumIndices-2;
            max_out = std::max(max_out,face.mNumIndices);
        }
    }

    // Just another check whether aiMesh::mPrimitiveTypes is correct
    assert(numOut != pMesh->mNumFaces);

    aiVector3D* nor_out = NULL;

    // if we don't have normals yet, but expect them to be a cheap side
    // product of triangulation anyway, allocate storage for them.
    if (!pMesh->mNormals && get_normals) {
        // XXX need a mechanism to inform the GenVertexNormals process to treat these normals as preprocessed per-face normals
    //  nor_out = pMesh->mNormals = new aiVector3D[pMesh->mNumVertices];
    }

    // the output mesh will contain triangles, but no polys anymore
    pMesh->mPrimitiveTypes |= aiPrimitiveType_TRIANGLE;
    pMesh->mPrimitiveTypes &= ~aiPrimitiveType_POLYGON;

    aiFace* out = new aiFace[numOut](), *curOut = out;
    std::vector<aiVector3D> temp_verts3d(max_out+2); /* temporary storage for vertices */
    std::vector<aiVector2D> temp_verts(max_out+2);

    // Apply vertex colors to represent the face winding?
#ifdef AI_BUILD_TRIANGULATE_COLOR_FACE_WINDING
    if (!pMesh->mColors[0])
        pMesh->mColors[0] = new aiColor4D[pMesh->mNumVertices];
    else
        new(pMesh->mColors[0]) aiColor4D[pMesh->mNumVertices];

    aiColor4D* clr = pMesh->mColors[0];
#endif

#ifdef AI_BUILD_TRIANGULATE_DEBUG_POLYS
    FILE* fout = fopen(POLY_OUTPUT_FILE,"a");
#endif

    const aiVector3D* verts = pMesh->mVertices;

    // use std::unique_ptr to avoid slow std::vector<bool> specialiations
    std::unique_ptr<bool[]> done(new bool[max_out]);
    for( unsigned int a = 0; a < pMesh->mNumFaces; a++) {
        aiFace& face = pMesh->mFaces[a];

        unsigned int* idx = face.mIndices;
        int num = (int)face.mNumIndices, ear = 0, tmp, prev = num-1, next = 0, max = num;

        // Apply vertex colors to represent the face winding?
#ifdef AI_BUILD_TRIANGULATE_COLOR_FACE_WINDING
        for (unsigned int i = 0; i < face.mNumIndices; ++i) {
            aiColor4D& c = clr[idx[i]];
            c.r = (i+1) / (float)max;
            c.b = 1.f - c.r;
        }
#endif

        aiFace* const last_face = curOut;

        // if it's a simple point,line or triangle: just copy it
        if( face.mNumIndices <= 3)
        {
            aiFace& nface = *curOut++;
            nface.mNumIndices = face.mNumIndices;
            nface.mIndices    = face.mIndices;

            face.mIndices = NULL;
            continue;
        }
        // optimized code for quadrilaterals
        else if ( face.mNumIndices == 4) {

            // quads can have at maximum one concave vertex. Determine
            // this vertex (if it exists) and start tri-fanning from
            // it.
            unsigned int start_vertex = 0;
            for (unsigned int i = 0; i < 4; ++i) {
                const aiVector3D& v0 = verts[face.mIndices[(i+3) % 4]];
                const aiVector3D& v1 = verts[face.mIndices[(i+2) % 4]];
                const aiVector3D& v2 = verts[face.mIndices[(i+1) % 4]];

                const aiVector3D& v = verts[face.mIndices[i]];

                aiVector3D left = (v0-v);
                aiVector3D diag = (v1-v);
                aiVector3D right = (v2-v);

                left.Normalize();
                diag.Normalize();
                right.Normalize();

                const float angle = std::acos(left*diag) + std::acos(right*diag);
                if (angle > AI_MATH_PI_F) {
                    // this is the concave point
                    start_vertex = i;
                    break;
                }
            }

            const unsigned int temp[] = {face.mIndices[0], face.mIndices[1], face.mIndices[2], face.mIndices[3]};

            aiFace& nface = *curOut++;
            nface.mNumIndices = 3;
            nface.mIndices = face.mIndices;

            nface.mIndices[0] = temp[start_vertex];
            nface.mIndices[1] = temp[(start_vertex + 1) % 4];
            nface.mIndices[2] = temp[(start_vertex + 2) % 4];

            aiFace& sface = *curOut++;
            sface.mNumIndices = 3;
            sface.mIndices = new unsigned int[3];

            sface.mIndices[0] = temp[start_vertex];
            sface.mIndices[1] = temp[(start_vertex + 2) % 4];
            sface.mIndices[2] = temp[(start_vertex + 3) % 4];

            // prevent double deletion of the indices field
            face.mIndices = NULL;
            continue;
        }
        else
        {
            // A polygon with more than 3 vertices can be either concave or convex.
            // Usually everything we're getting is convex and we could easily
            // triangulate by tri-fanning. However, LightWave is probably the only
            // modeling suite to make extensive use of highly concave, monster polygons ...
            // so we need to apply the full 'ear cutting' algorithm to get it right.

            // RERQUIREMENT: polygon is expected to be simple and *nearly* planar.
            // We project it onto a plane to get a 2d triangle.

            // Collect all vertices of of the polygon.
            for (tmp = 0; tmp < max; ++tmp) {
                temp_verts3d[tmp] = verts[idx[tmp]];
            }

            // Get newell normal of the polygon. Store it for future use if it's a polygon-only mesh
            aiVector3D n;
            NewellNormal<3,3,3>(n,max,&temp_verts3d.front().x,&temp_verts3d.front().y,&temp_verts3d.front().z);
            if (nor_out) {
                 for (tmp = 0; tmp < max; ++tmp)
                     nor_out[idx[tmp]] = n;
            }

            // Select largest normal coordinate to ignore for projection
            const float ax = (n.x>0 ? n.x : -n.x);
            const float ay = (n.y>0 ? n.y : -n.y);
            const float az = (n.z>0 ? n.z : -n.z);

            unsigned int ac = 0, bc = 1; /* no z coord. projection to xy */
            float inv = n.z;
            if (ax > ay) {
                if (ax > az) { /* no x coord. projection to yz */
                    ac = 1; bc = 2;
                    inv = n.x;
                }
            }
            else if (ay > az) { /* no y coord. projection to zy */
                ac = 2; bc = 0;
                inv = n.y;
            }

            // Swap projection axes to take the negated projection vector into account
            if (inv < 0.f) {
                std::swap(ac,bc);
            }

            for (tmp =0; tmp < max; ++tmp) {
                temp_verts[tmp].x = verts[idx[tmp]][ac];
                temp_verts[tmp].y = verts[idx[tmp]][bc];
                done[tmp] = false;
            }

#ifdef AI_BUILD_TRIANGULATE_DEBUG_POLYS
            // plot the plane onto which we mapped the polygon to a 2D ASCII pic
            aiVector2D bmin,bmax;
            ArrayBounds(&temp_verts[0],max,bmin,bmax);

            char grid[POLY_GRID_Y][POLY_GRID_X+POLY_GRID_XPAD];
            std::fill_n((char*)grid,POLY_GRID_Y*(POLY_GRID_X+POLY_GRID_XPAD),' ');

            for (int i =0; i < max; ++i) {
                const aiVector2D& v = (temp_verts[i] - bmin) / (bmax-bmin);
                const size_t x = static_cast<size_t>(v.x*(POLY_GRID_X-1)), y = static_cast<size_t>(v.y*(POLY_GRID_Y-1));
                char* loc = grid[y]+x;
                if (grid[y][x] != ' ') {
                    for(;*loc != ' '; ++loc);
                    *loc++ = '_';
                }
                *(loc+::ai_snprintf(loc, POLY_GRID_XPAD,"%i",i)) = ' ';
            }


            for(size_t y = 0; y < POLY_GRID_Y; ++y) {
                grid[y][POLY_GRID_X+POLY_GRID_XPAD-1] = '\0';
                fprintf(fout,"%s\n",grid[y]);
            }

            fprintf(fout,"\ntriangulation sequence: ");
#endif

            //
            // FIXME: currently this is the slow O(kn) variant with a worst case
            // complexity of O(n^2) (I think). Can be done in O(n).
            while (num > 3) {

                // Find the next ear of the polygon
                int num_found = 0;
                for (ear = next;;prev = ear,ear = next) {

                    // break after we looped two times without a positive match
                    for (next=ear+1;done[(next>=max?next=0:next)];++next);
                    if (next < ear) {
                        if (++num_found == 2) {
                            break;
                        }
                    }
                    const aiVector2D* pnt1 = &temp_verts[ear],
                        *pnt0 = &temp_verts[prev],
                        *pnt2 = &temp_verts[next];

                    // Must be a convex point. Assuming ccw winding, it must be on the right of the line between p-1 and p+1.
                    if (OnLeftSideOfLine2D(*pnt0,*pnt2,*pnt1)) {
                        continue;
                    }

                    // and no other point may be contained in this triangle
                    for ( tmp = 0; tmp < max; ++tmp) {

                        // We need to compare the actual values because it's possible that multiple indexes in
                        // the polygon are referring to the same position. concave_polygon.obj is a sample
                        //
                        // FIXME: Use 'epsiloned' comparisons instead? Due to numeric inaccuracies in
                        // PointInTriangle() I'm guessing that it's actually possible to construct
                        // input data that would cause us to end up with no ears. The problem is,
                        // which epsilon? If we chose a too large value, we'd get wrong results
                        const aiVector2D& vtmp = temp_verts[tmp];
                        if ( vtmp != *pnt1 && vtmp != *pnt2 && vtmp != *pnt0 && PointInTriangle2D(*pnt0,*pnt1,*pnt2,vtmp)) {
                            break;
                        }
                    }
                    if (tmp != max) {
                        continue;
                    }

                    // this vertex is an ear
                    break;
                }
                if (num_found == 2) {

                    // Due to the 'two ear theorem', every simple polygon with more than three points must
                    // have 2 'ears'. Here's definitely something wrong ... but we don't give up yet.
                    //

                    // Instead we're continuing with the standard tri-fanning algorithm which we'd
                    // use if we had only convex polygons. That's life.
                    // DefaultLogger::get()->error("Failed to triangulate polygon (no ear found). Probably not a simple polygon?");

#ifdef AI_BUILD_TRIANGULATE_DEBUG_POLYS
                    fprintf(fout,"critical error here, no ear found! ");
#endif
                    num = 0;
                    break;

                    curOut -= (max-num); /* undo all previous work */
                    for (tmp = 0; tmp < max-2; ++tmp) {
                        aiFace& nface = *curOut++;

                        nface.mNumIndices = 3;
                        if (!nface.mIndices)
                            nface.mIndices = new unsigned int[3];

                        nface.mIndices[0] = 0;
                        nface.mIndices[1] = tmp+1;
                        nface.mIndices[2] = tmp+2;

                    }
                    num = 0;
                    break;
                }

                aiFace& nface = *curOut++;
                nface.mNumIndices = 3;

                if (!nface.mIndices) {
                    nface.mIndices = new unsigned int[3];
                }

                // setup indices for the new triangle ...
                nface.mIndices[0] = prev;
                nface.mIndices[1] = ear;
                nface.mIndices[2] = next;

                // exclude the ear from most further processing
                done[ear] = true;
                --num;
            }
            if (num > 0) {
                // We have three indices forming the last 'ear' remaining. Collect them.
                aiFace& nface = *curOut++;
                nface.mNumIndices = 3;
                if (!nface.mIndices) {
                    nface.mIndices = new unsigned int[3];
                }

                for (tmp = 0; done[tmp]; ++tmp);
                nface.mIndices[0] = tmp;

                for (++tmp; done[tmp]; ++tmp);
                nface.mIndices[1] = tmp;

                for (++tmp; done[tmp]; ++tmp);
                nface.mIndices[2] = tmp;

            }
        }

#ifdef AI_BUILD_TRIANGULATE_DEBUG_POLYS

        for(aiFace* f = last_face; f != curOut; ++f) {
            unsigned int* i = f->mIndices;
            fprintf(fout," (%i %i %i)",i[0],i[1],i[2]);
        }

        fprintf(fout,"\n*********************************************************************\n");
        fflush(fout);

#endif

        for(aiFace* f = last_face; f != curOut; ) {
            unsigned int* i = f->mIndices;

            //  drop dumb 0-area triangles
            if (std::fabs(GetArea2D(temp_verts[i[0]],temp_verts[i[1]],temp_verts[i[2]])) < 1e-5f) {

                //DefaultLogger::get()->debug("Dropping triangle with area 0");
                --curOut;

                delete[] f->mIndices;
                f->mIndices = NULL;

                for(aiFace* ff = f; ff != curOut; ++ff) {
                    ff->mNumIndices = (ff+1)->mNumIndices;
                    ff->mIndices = (ff+1)->mIndices;
                    (ff+1)->mIndices = NULL;
                }
                continue;
            }

            i[0] = idx[i[0]];
            i[1] = idx[i[1]];
            i[2] = idx[i[2]];
            ++f;
        }

        delete[] face.mIndices;
        face.mIndices = NULL;
    }

#ifdef AI_BUILD_TRIANGULATE_DEBUG_POLYS
    fclose(fout);
#endif

    // kill the old faces
    delete [] pMesh->mFaces;

    // ... and store the new ones
    pMesh->mFaces    = out;
    pMesh->mNumFaces = (unsigned int)(curOut-out); /* not necessarily equal to numOut */
    return true;
}

