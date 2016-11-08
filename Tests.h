//
// Created by Hallison da Paz on 11/09/2016.
//

#ifndef DUAL_CONTOURING_EXPERIMENTS_TESTS_H
#define DUAL_CONTOURING_EXPERIMENTS_TESTS_H

#include <unordered_map>
#include "octree.h"

class Tests {
public:
    static bool validate_cells_signs(OctreeNode *node, std::unordered_map<std::string, int> &vertexpool, int &num_incorrect);
    static bool validate_vertices_map(std::unordered_map<std::string, int> &vertexpool);

    static void reconstruct_pieces(std::string input_folder, std::string basename, int num_pieces, int height=9, bool should_simplify = false);
};


#endif //DUAL_CONTOURING_EXPERIMENTS_TESTS_H
