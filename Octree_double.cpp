//
// Created by Hallison da Paz on 18/11/2016.
//

#include <fstream>
#include "Octree.h"
#include "Utils.h"


//using vecr;
// ----------------------------------------------------------------------------
std::unordered_map<std::string, int> Octree::leafvertexpool;
// ----------------------------------------------------------------------------
int classify_vertex(vecr vertex, vecr cam_origin, OctreeNode* root, DefaultMesh &mesh);
// ----------------------------------------------------------------------------

int Octree::no_intersections = 0;


bool OctreeNode::construct_or_update_children(unsigned int max_depth, const DefaultMesh &mesh)
{
    const Real childSize = this->size / 2;
    const int childHeight = this->depth + 1;
    bool hasChildren = false;
    for (int i = 0; i < NUM_CHILDREN; i++)
    {
        if (this->children[i] == nullptr){
            OctreeNode* child = new OctreeNode(NODE_INTERNAL,
                                               this->min + (CHILD_MIN_OFFSETS[i] * childSize),
                                               childSize,
                                               childHeight,
                                               this);
            if (this->drawInfo)
            {
                child->drawInfo = new OctreeDrawInfo();
                child->drawInfo->qef = child->drawInfo->qef + this->drawInfo->qef;
                child->drawInfo->averageNormal += this->drawInfo->averageNormal;
            }
            this->children[i] = Octree::BuildMeshHierarchy(child, max_depth, mesh);
        }
        else
        {
            this->children[i] = Octree::UpdateMeshHierarchy(this->children[i], max_depth, mesh);
        }
        hasChildren |= (this->children[i] != nullptr);
    }
    return hasChildren;
}

Octree::Octree(vecr min, Real size, unsigned int max_depth, DefaultMesh &mesh, vecr cam_origin)
{
    root = new OctreeNode(NODE_INTERNAL, min, size, 0);
    trace("building hierarchy");
    BuildMeshHierarchy(root, max_depth, mesh);
    trace("classifying vertices");
    classify_leaves_vertices(cam_origin, this->root, mesh);
}

OctreeNode *Octree::BuildMeshHierarchy(OctreeNode *node, unsigned int max_depth, const DefaultMesh &mesh)
{
    if (!node) return nullptr;

    select_inner_crossing_faces(node, mesh);
    if (node->innerEmpty() && node->crossingEmpty())
    {   //Empty space, no triangles crossing or inside this cell
        delete node;
        return nullptr;
    }

    if (/*(node->parent && node->parent->innerEmpty()) ||*/ node->depth == max_depth)
    {
        return construct_or_update_leaf(node, max_depth, mesh);
    }

    if (node->construct_or_update_children(max_depth, mesh))
    {
        return node;
    }

    delete node;
    return nullptr;
}

OctreeNode* Octree::UpdateMeshHierarchy(OctreeNode *node, unsigned int max_depth, const DefaultMesh &mesh)
{
    if (!node) return nullptr;
    //node->clean();
    select_inner_crossing_faces(node, mesh);
    if (node->innerEmpty() && node->crossingEmpty())
    {
        return node;
    }

    if (node->type == NODE_LEAF)
    {
        if (node->depth < max_depth /*&& !node->parent->innerEmpty()*/)
        {
            node->type = NODE_INTERNAL;
            node->construct_or_update_children(max_depth, mesh);
        }
        else {
            node = construct_or_update_leaf(node, max_depth, mesh, true);
        }
        return node;
    }

    node->construct_or_update_children(max_depth, mesh);
    return node;
}

OctreeNode *Octree::construct_or_update_leaf(OctreeNode *leaf, unsigned int max_depth, const DefaultMesh &mesh,
                                             bool update) {
    if (!leaf)
    {
        std::cout << "Trying to construct a leaf in the middle" << std::endl;
        return nullptr;
    }

    // otherwise the voxel contains the surface, so find the edge intersections
    vecr averageNormal(0.f);
    svd::QefSolver qef;
    bool hasIntersection = false;
    //TODO: optimize computations to avoid redundant intersections (same edge from other cell)
    for (int i = 0; i < NUM_EDGES; ++i) //for each edge
    {
        const vecr p1 = leaf->get_vertex(edgevmap[i][0]);
        const vecr p2 = leaf->get_vertex(edgevmap[i][1]);

        vecr intersection;
        std::vector<vecr> intersection_points, normals;
        for (std::list<DefaultMesh::FaceHandle>::iterator face = leaf->crossingFaces.begin(); face != leaf->crossingFaces.end(); ++face)
        {
            auto fv_it = mesh.cfv_iter(*face);
            DefaultMesh::VertexHandle a = *fv_it;
            DefaultMesh::VertexHandle b = *(++fv_it);
            DefaultMesh::VertexHandle c = *(++fv_it);

            vecr face_vertices[3] = {openmesh_to_glm(mesh.point(a)), openmesh_to_glm(mesh.point(b)), openmesh_to_glm(mesh.point(c))};
            Vertex vertices[3] = { face_vertices[0], face_vertices[1], face_vertices[2]};
            if (moller_triangle_intersection(p1, p2, vertices, intersection)) {
                //keeps the intersection here
                if ((intersection_points.size() > 0) && (glm::distance(intersection, intersection_points[0]) < POINT_DISTANCE_THRESHOLD)){
                    continue;
                }
                /*if (mesh.is_boundary(*face)){
                    if (leaf->depth < max_depth)
                    {
                        if (leaf->construct_or_update_children(max_depth, mesh))
                        {
                            leaf->type = NODE_INTERNAL;
                            return leaf;
                        }
                    }
                    else
                    {
                        leaf->is_border = true;
                    }
                }*/
                intersection_points.push_back(intersection);

                Real u, v, w;
                barycentric(intersection, face_vertices[0], face_vertices[1], face_vertices[2], u, v, w);
                vecr normal_at_intersection = u * openmesh_to_glm(mesh.normal(a)) + v * openmesh_to_glm(mesh.normal(b)) + w * openmesh_to_glm(mesh.normal(c));
                normals.push_back(glm::normalize(normal_at_intersection));
                //normals.push_back(glm::normalize(openmesh_to_glm(mesh.normal(*face))));
            }
        }
        if (intersection_points.size() > 1) {
//            std::cout << intersection_points.size() << " Interseções na mesma aresta " << vecsigns[c1] << vecsigns[c2] << std::endl;
            if (leaf->depth < max_depth){
                //std::cout << intersection_points.size() << " Child Depth: " << leaf->depth+1 << " Child Size: " << leaf->size/2 << std:: endl;

                if(leaf->construct_or_update_children(max_depth, mesh))
                {
                    leaf->type = NODE_INTERNAL;
                    return leaf;
                }
               /* std::cout << "SERIAO????" << std::endl; //if it has an intersection why not the children?
                if (update){
                    return leaf;
                }
                delete leaf;
                return nullptr;*/
            }
            else{
                trace("reached maximum depth");
            }
        }
        // if we consider that an intersection happened, we'll consider only the first intersection for now
        if (intersection_points.size() > 0)
        {
            for (int j = 0; j < intersection_points.size(); ++j) {
                vecr &n = normals[j];
                vecr &v = intersection_points[j];
                qef.add(v.x, v.y, v.z, n.x, n.y, n.z);
                averageNormal += n;
            }

            hasIntersection = true;
        }
    }

    if (!hasIntersection && leaf->innerFaces.empty())
    {   // voxel is full inside or outside the volume
        if (update)
            return leaf;
        delete leaf;
        ++no_intersections;
        return nullptr;
    }

    /*std::unordered_map<int, bool> seenPoints;
    for (std::list<DefaultMesh::FaceHandle>::iterator face = leaf->innerFaces.begin(); face != leaf->innerFaces.end(); ++face) {
        auto fv_it = mesh.cfv_iter(*face);
        /*DefaultMesh::VertexHandle a = *fv_it;
        DefaultMesh::VertexHandle b = *(++fv_it);
        DefaultMesh::VertexHandle c = *(++fv_it);*/
        /*for (int i = 0; i < 3; ++i)
        {
            DefaultMesh::VertexHandle a = *(fv_it++);
            if (seenPoints.count(a.idx()) == 0)
            {
                seenPoints[a.idx()] = true;
                vecr pos = openmesh_to_glm(mesh.point(a));
                vecr normal = openmesh_to_glm(mesh.normal(a));
                qef.add(pos.x, pos.y, pos.z, normal.x, normal.y, normal.z);
                averageNormal += normal;
            }
        }
    }*/

    if (leaf->drawInfo == nullptr){
        leaf->drawInfo = new OctreeDrawInfo();
    }
    leaf->drawInfo->qef = leaf->drawInfo->qef + qef.getData();
    leaf->drawInfo->averageNormal += averageNormal;
    leaf->type = NODE_LEAF;
    for (int i = 0; i < NUM_CHILDREN; ++i) {
        std::string vertex_hash = hashvertex(leaf->get_vertex(i));
        if (leafvertexpool.count(vertex_hash) == 0)
            leafvertexpool[vertex_hash] = MATERIAL_UNKNOWN;
    }
    //return clean_node(leaf);
    std::cout << "Leaves: " << leafvertexpool.size() << std::endl;
    return leaf;
}

void Octree::classify_leaves_vertices(vecr cam_origin, OctreeNode* node, DefaultMesh &mesh)
{
    //trace("classify leaves vertices");
    if (node == nullptr) return;

    if (node->type == NODE_LEAF)
    {
        if (node->drawInfo == nullptr){
            node->drawInfo = new OctreeDrawInfo();
        }
        int corners = 0;
        for (int i = 0; i < NUM_CHILDREN; ++i)
        {
            vecr cell_vertex = node->get_vertex(i);
            std::string vertex_hash = hashvertex(cell_vertex);
            if (leafvertexpool[vertex_hash] == MATERIAL_UNKNOWN)
            {
                int sign = classify_vertex(cam_origin, cell_vertex, this->root, mesh);
                leafvertexpool[vertex_hash] = sign;
            }
            corners |= (leafvertexpool[vertex_hash] << i);
        }
        node->drawInfo->corners |= corners;
    }
    else
    {
        for (int i = 0; i < NUM_CHILDREN; ++i) {
            classify_leaves_vertices(cam_origin, node->children[i], mesh);
        }
    }
}

// Intersect ray R(t) = p + t*d against AABB a. When intersecting,
// return intersection distance tmin and point q of intersection
bool intersectRayBox(vecr origin, vecr dest, const vecr min, const Real size, Real &intersection_distance, vecr &intersection) {
    intersection_distance = 0.0f; // set to -FLT_MAX to get first hit on line
    Real tmax = FLT_MAX; // set to max distance ray can travel (for segment)
    // For all three slabs
    vecr dir = glm::normalize(dest - origin);
    vecr max = min + vecr(size);
    for (int i = 0; i < 3; i++)
    {
        if (std::abs(dir[i]) < EPSILON)
        {
            // Ray is parallel to slab. No hit if origin not within slab
            if (origin[i] < min[i] || origin[i] > max[i])
                return 0;
        }
        else
        {
            // Compute intersection t value of ray with near and far plane of slab
            Real ood = 1.0f / dir[i];
            Real t1 = (min[i] - origin[i]) * ood;
            Real t2 = (max[i] - origin[i]) * ood;
            // Make t1 be intersection with near plane, t2 with far plane
            if (t1 > t2)
            {
                Real aux = t1;
                t1 = t2;
                t2 = aux;
            }
            // Compute the intersection of slab intersection intervals
            if (t1 > intersection_distance) intersection_distance = t1;
            if (t2 > tmax) tmax = t2;
            // Exit with no collision as soon as slab intersection becomes empty
            if (intersection_distance > tmax)
                return false;
        }
    }
    // Ray intersects all 3 slabs. Return point (q) and intersection t value (intersection_distance)
    intersection = origin + dir * intersection_distance;
    return true;
}


int ray_faces_intersection(const vecr origin, const vecr dest, DefaultMesh &mesh,
                           std::list<DefaultMesh::FaceHandle> &facelist, std::unordered_map<int, bool> &visited_triangles)
{
    int num_intersections = 0;
    std::vector<vecr> intersections;
    for (std::list<DefaultMesh::FaceHandle>::iterator face = facelist.begin(); face != facelist.end(); ++face){
        if (visited_triangles.count(face->idx()) == 0){
            auto fv_it = mesh.cfv_iter(*face);
            DefaultMesh::VertexHandle a = *fv_it;
            DefaultMesh::VertexHandle b = *(++fv_it);
            DefaultMesh::VertexHandle c = *(++fv_it);

            vecr face_vertices[3] = {openmesh_to_glm(mesh.point(a)), openmesh_to_glm(mesh.point(b)), openmesh_to_glm(mesh.point(c))};
            Vertex vertices[3] = { face_vertices[0], face_vertices[1], face_vertices[2]};

            vecr intersection;

            if (moller_triangle_intersection(origin, dest, vertices, intersection)) {
                //keeps the intersection here
                intersections.push_back(intersection);
                ++num_intersections;
            }
            visited_triangles[face->idx()] = true;
        }
    }
    return num_intersections;
}

int ray_mesh_intersection(vecr cam_origin, vecr vertex, OctreeNode* root, DefaultMesh &mesh, std::unordered_map<int, bool> &visited_triangles)
{
    if (root == nullptr){
        return 0;
    }
    int num_intersections = 0;
    vecr intersection;
    Real t;
    if (intersectRayBox(cam_origin, vertex, root->min, root->size, t, intersection)){
        if (root->type == NODE_LEAF)
        {
            num_intersections += ray_faces_intersection(cam_origin, vertex, mesh, root/*->meshInfo*/->crossingFaces, visited_triangles);
            num_intersections += ray_faces_intersection(cam_origin, vertex, mesh, root/*->meshInfo*/->innerFaces, visited_triangles);
        }
        else
        {
            for (int i = 0; i < NUM_CHILDREN; ++i)
            {
                num_intersections += ray_mesh_intersection(cam_origin, vertex, root->children[i], mesh, visited_triangles);
            }
        }
    }
    return num_intersections;
}

int classify_vertex(vecr cam_origin, vecr vertex, OctreeNode* root, DefaultMesh &mesh)
{
    std::unordered_map<int, bool> visited_triangles;
    int num_intersections = ray_mesh_intersection(cam_origin, vertex, root, mesh, visited_triangles);
    if (num_intersections > 0)
    {
        return MATERIAL_SOLID;
    }
    return MATERIAL_AIR;
}

// -------------------------------------------------------------------------------

OctreeNode* Octree::SimplifyOctree(OctreeNode* node, const Real threshold)
{
    if (!node)
    {
        //std::cout << "Empty node" << std::endl;
        return nullptr;
    }

    if (node->type != NODE_INTERNAL)
    {
        // can't simplify!
        return node;
    }
    //std::cout << "Simplifying at level: " << node->depth << std::endl;
    svd::QefSolver qef;

    int signs[8] = { MATERIAL_UNKNOWN, MATERIAL_UNKNOWN, MATERIAL_UNKNOWN, MATERIAL_UNKNOWN,
                     MATERIAL_UNKNOWN, MATERIAL_UNKNOWN, MATERIAL_UNKNOWN, MATERIAL_UNKNOWN };
    int midsign = MATERIAL_UNKNOWN;
    int edgeCount = 0;
    bool isCollapsible = true;

    for (int i = 0; i < 8; ++i)
    {
        node->children[i] = SimplifyOctree(node->children[i], threshold);
        if (node->children[i])
        {
            OctreeNode* child = node->children[i];
            if (child->type == NODE_INTERNAL)
            {
                isCollapsible = false;
            }
            else
            {
                qef.add(child->drawInfo->qef);

                midsign = (child->drawInfo->corners >> (7 - i)) & 1;
                signs[i] = (child->drawInfo->corners >> i) & 1;

                edgeCount++;
            }
        }
    }

    if (!isCollapsible)
    {
        // at least one child is an internal node, can't collapse
        return node;
    }

    svd::Vec3 qefPosition;
    qef.solve(qefPosition, QEF_ERROR, QEF_SWEEPS, QEF_ERROR);
    Real error = qef.getError();

    // convert to glm vecr for ease of use
    vecr position(qefPosition.x, qefPosition.y, qefPosition.z);

    // at this point the masspoint will actually be a sum, so divide to make it the average
    if (error > threshold)
    {
        // this collapse breaches the threshold
        return node;
    }

    if ((position.x < node->min.x) || (position.x > (node->min.x + node->size)) ||
        (position.y < node->min.y) || (position.y > (node->min.y + node->size)) ||
        (position.z < node->min.z) || (position.z > (node->min.z + node->size)))
    {
        const auto& mp = qef.getMassPoint();
        position = vecr(mp.x, mp.y, mp.z);
    }

    // change the node from an internal node to a 'pseudo leaf' node
    OctreeDrawInfo* drawInfo = new OctreeDrawInfo;

    for (int i = 0; i < 8; i++)
    {
        if (signs[i] == MATERIAL_UNKNOWN)
        {
            // Undetermined, use centre sign instead
            drawInfo->corners |= (midsign << i);
        }
        else
        {
            drawInfo->corners |= (signs[i] << i);
        }
    }

    drawInfo->averageNormal = vecr(0.f);
    for (int i = 0; i < 8; ++i)
    {
        if (node->children[i])
        {
            OctreeNode* child = node->children[i];
            if (child->type != NODE_INTERNAL)
            {
                drawInfo->averageNormal += child->drawInfo->averageNormal;
            }
        }
    }

    drawInfo->averageNormal = glm::normalize(drawInfo->averageNormal);
    drawInfo->position = position;
    drawInfo->qef = qef.getData();

    for (int i = 0; i < 8; i++)
    {
        delete node->children[i];
        node->children[i] = nullptr;
    }

    node->type = NODE_PSEUDO;
    node->drawInfo = drawInfo;

    return node;
}