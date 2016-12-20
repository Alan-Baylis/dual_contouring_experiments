#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include "Utils.h"
#include "old/octree.h"

#include "old/NormalsEstimator.h"
#include "Reconstruction.h"
#include "Contouring.h"
#include "Octree.h"


#include "json.hpp"

using namespace std;
//using glm::vecr;

using json = nlohmann::json;


// ----------------------------------------------------------------------------

Real compute_boundingbox(DefaultMesh &mesh, DefaultMesh::Point &bb_min);

const string MESH_FILES = "mesh_files";
const string CAM_POS = "cam_pos";

int main(int argc, char** argv)
{
    const int height = 6;

    string folder_name = "../";
    string inputfilename, outputfilename;
    std::cout <<"Input File Name" << endl;
    std::cin >> inputfilename;
    std::cout <<"Output File Name" << endl;
    std::cin >> outputfilename;

    std::ifstream descriptionStream(folder_name + inputfilename);
    json object_json;
    if (descriptionStream.is_open()) {
        descriptionStream >> object_json;
    }

    std::vector<std::string> filenames = object_json[MESH_FILES];
    std::vector<vecr> cameras;
    for (json::iterator j_it = object_json[CAM_POS].begin(); j_it != object_json[CAM_POS].end(); ++j_it) {
        json cam_json = j_it.value();
        std::cout << cam_json << std::endl;
        cameras.push_back(vecr(cam_json["x"], cam_json["y"], cam_json["z"]));
    }

    DefaultMesh myMesh;
    //OpenMesh::IO::read_mesh(myMesh, "../models/divided/vase_antonina.off");
    OpenMesh::IO::read_mesh(myMesh, "../models/taoju/mechanic.off");
    // compute bounding box
    DefaultMesh::Point bb_min;
    Real octreeSize = compute_boundingbox(myMesh, bb_min);
    std::cout << octreeSize << std::endl;
    OctreeNode* root = Fusion::octree_from_samples(openmesh_to_glm(bb_min)/* - vecr(0.1)*/, octreeSize * 1.1, height,
                                                   filenames, cameras);
    //root = Octree::SimplifyOctree(root, octreeSize/1000.0);

    VertexBuffer vertices;
    IndexBuffer indices;
    GenerateMeshFromOctree(root, vertices, indices);

    std::cout << Octree::no_intersections << std::endl;
    std::stringstream filepath;
    filepath << folder_name << outputfilename << height << ".ply";

    //write_OFF(filepath.str(), vertices, indices);
    write_Ply(filepath.str(), vertices, indices);


    /*root = Octree::SimplifyOctree(root, octreeSize/10000.0);
    GenerateMeshFromOctree(root, vertices, indices);

    std::stringstream filepath2;
    filepath2 << folder_name << outputfilename << height << "SIMP.off";
    write_OFF(filepath2.str(), vertices, indices);*/

    return EXIT_SUCCESS;
}

// ----------------------------------------------------------------------------

Real compute_boundingbox(DefaultMesh &mesh, DefaultMesh::Point &bb_min)
{
    auto v_it = mesh.vertices_begin();
    DefaultMesh::Point bb_max = mesh.point(*v_it);
    bb_min = bb_max;
    for (; v_it != mesh.vertices_end(); ++v_it)
    {
        bb_min.minimize(mesh.point(*v_it));
        bb_max.maximize(mesh.point(*v_it));
    }
    Real octreeSize = (bb_max - bb_min).max();
    std::cout << "Min: (" << bb_min[0] << ", " << bb_min[1] << ", " << bb_min[2] << ") " << "Size: " << octreeSize << std::endl;
    return octreeSize;
}

//dist = (int) octreeSize;

//OpenMesh::IO::read_mesh(myMesh, "../models/analytic/sphere_low2.off");
//Octree sphere_octree(openmesh_to_glm(bb_min) - vecr(0.1), octreeSize*1.1, height, myMesh);
/*std::vector<std::string> filenames = {"../models/analytic/sphere_lowPZ.off", //"../models/analytic/sphere_lowMZ.off",
                                      "../models/analytic/sphere_lowPX.off", "../models/analytic/sphere_lowMX.off",
                                      "../models/analytic/sphere_lowPY.off", "../models/analytic/sphere_lowMY.off"};*/

//std::vector<std::string> filenames = {"../models/divided/vase_antoninaPZ.off",
//                            "../models/divided/vase_antoninaPX.off", "../models/divided/vase_antoninaMX.off",
//                            "../models/divided/vase_antoninaPY.off", "../models/divided/vase_antoninaMY.off"};
//std::vector<std::string> filenames = {"../models/divided/trophy.off", /*"../models/divided/vase_holeMX.off"*/};
//std::vector<vecr> cameras = {vecr(0, 0, dist), /*vecr(0, 0, -dist),*/ vecr(dist, 0, 0), vecr(-dist, 0, 0), vecr(0, dist, 0), vecr(0, -dist, 0)};
//    std::vector<std::string> filenames = { "../models/analytic/cylinder_lowpoly.off"};