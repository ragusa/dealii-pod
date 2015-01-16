/* ---------------------------------------------------------------------
 * $Id: project.cc $
 *
 * Copyright (C) 2015 David Wells
 *
 * This file is NOT part of the deal.II library.
 *
 * This file is free software; you can use it, redistribute it, and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 * The full text of the license can be found in the file LICENSE at
 * the top level of the deal.II distribution.
 *
 * ---------------------------------------------------------------------
 *
 * Author: David Wells, Virginia Tech, 2015
 */
#include <deal.II/base/quadrature_lib.h>

#include <deal.II/dofs/dof_handler.h>
#include <deal.II/dofs/dof_tools.h>

#include <deal.II/grid/tria.h>

#include <deal.II/lac/block_vector.h>
#include <deal.II/lac/vector.h>
#include <deal.II/lac/full_matrix.h>
#include <deal.II/lac/compressed_sparsity_pattern.h>
#include <deal.II/lac/sparse_matrix.h>

#include <deal.II/numerics/matrix_tools.h>

#include <algorithm>
#include <glob.h>
#include <iostream>
#include <vector>

#include "../h5/h5.h"
#include "../pod/pod.h"

constexpr int dim {3};
constexpr bool renumber {false};

using namespace dealii;
int main(int argc, char **argv)
  {
    Utilities::MPI::MPI_InitFinalize mpi_initialization(argc, argv, 1);

    const FE_Q<dim> fe(2);
    const QGauss<dim> quad((3*fe.degree + 2)/2);

    std::vector<BlockVector<double>> pod_vectors;
    BlockVector<double> mean_vector;
    Triangulation<dim> triangulation;
    DoFHandler<dim> dof_handler;

    POD::create_dof_handler_from_triangulation_file
      ("triangulation.txt", renumber, fe, dof_handler, triangulation);
    POD::load_pod_basis("pod-vector-0*h5", "mean-vector.h5", mean_vector, pod_vectors);
    const unsigned int n_pod_vectors = pod_vectors.size();

    SparsityPattern sparsity_pattern;
    {
      CompressedSparsityPattern c_sparsity(dof_handler.n_dofs());
      DoFTools::make_sparsity_pattern(dof_handler, c_sparsity);
      sparsity_pattern.copy_from(c_sparsity);
    }
    SparseMatrix<double> full_mass_matrix(sparsity_pattern);
    MatrixCreator::create_mass_matrix(dof_handler, quad, full_mass_matrix);

    glob_t glob_result;
    const std::string snapshot_glob("snapshot-0*h5");

    glob(snapshot_glob.c_str(), GLOB_TILDE, nullptr, &glob_result);
    const unsigned int n_snapshots = glob_result.gl_pathc;
    std::cout << "n_snapshots is " << n_snapshots << std::endl;
    std::vector<std::string> glob_results;
    for (unsigned int snapshot_n = 0; snapshot_n < glob_result.gl_pathc; ++snapshot_n)
      {
        glob_results.push_back(std::string(glob_result.gl_pathv[snapshot_n]));
      }
    globfree(&glob_result);
    std::sort(glob_results.begin(), glob_results.end());

    FullMatrix<double> pod_coefficients_matrix(n_snapshots, n_pod_vectors);
    unsigned int snapshot_n = 0;
    for (auto &file_name : glob_results)
      {
        BlockVector<double> snapshot;
        H5::load_block_vector(file_name, snapshot);
        snapshot -= mean_vector;

        Vector<double> temp(pod_vectors.at(0).block(0).size());
        for (unsigned int dim_n = 0; dim_n < dim; ++dim_n)
          {
            full_mass_matrix.vmult(temp, snapshot.block(dim_n));
            for (unsigned int pod_vector_n = 0; pod_vector_n < pod_vectors.size();
                 ++pod_vector_n)
              {
                pod_coefficients_matrix(snapshot_n, pod_vector_n) +=
                  temp * pod_vectors.at(pod_vector_n).block(dim_n);
              }
          }
        ++snapshot_n;
      }
    std::string out_file_name("projected-pod-coefficients.h5");
    H5::save_full_matrix(out_file_name, pod_coefficients_matrix);
  }
