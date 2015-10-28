/* ---------------------------------------------------------------------
 * Copyright (C) 2014 David Wells
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
 * This program is based on step-26 of the deal.ii library.
 *
 * Author: David Wells, Virginia Tech, 2014
 *         David Wells, Rensselaer Polytechnic Institute, 2015
 */

#include <deal.II/lac/vector.h>
#include <deal.II/lac/full_matrix.h>

#include <deal.II/bundled/boost/math/special_functions/round.hpp>

#include <memory>
#include <utility>
#include <vector>

#include <deal.II-pod/h5/h5.h>
#include <deal.II-pod/ode/ode.h>
#include <deal.II-pod/ns/filter.h>
#include <deal.II-pod/ns/ns.h>
#include <deal.II-pod/pod/pod.h>

#include "parameters.h"
#include "rk_factory.h"


namespace NavierStokes
{
  using namespace dealii;
  using namespace POD;

  template<int dim>
  class ROM
  {
  public:
    ROM(const POD::NavierStokes::Parameters &parameters);
    void run();

  private:
    void setup_reduced_system();
    void time_iterate();

    POD::NavierStokes::Parameters    parameters;


    FullMatrix<double>               mass_matrix;
    FullMatrix<double>               boundary_matrix;
    FullMatrix<double>               laplace_matrix;
    FullMatrix<double>               linear_operator;
    FullMatrix<double>               joint_convection;
    std::vector<FullMatrix<double>>  nonlinear_operator;
    Vector<double>                   mean_contribution_vector;

    unsigned int                     n_pod_dofs;

    Vector<double>                   solution;

    double                           time;
    unsigned int                     timestep_number;
  };


  template<int dim>
  ROM<dim>::ROM(const POD::NavierStokes::Parameters &parameters)
    :
    parameters(parameters),
    time {parameters.initial_time},
    timestep_number {0}
  {}


  template<int dim>
  {
      {
          {
          }

      }

    // The joint convection matrix is necessary for the L2 Projection model (all
    // terms resulting from the nonlinearity must be filtered)
    joint_convection.reinit(n_pod_dofs, n_pod_dofs);
    joint_convection.add(-1.0, convection_matrix_0);
    joint_convection.add(-1.0, convection_matrix_1);

    linear_operator.reinit(n_pod_dofs, n_pod_dofs);
    linear_operator.add(-1.0/parameters.reynolds_n, laplace_matrix);
    linear_operator.add(1.0/parameters.reynolds_n, boundary_matrix);
    linear_operator.add(-1.0, convection_matrix_0);
    linear_operator.add(-1.0, convection_matrix_1);
    std::cout << "assembled all affine terms." << std::endl;
  }


  template<int dim>
  void ROM<dim>::time_iterate()
  {
    Vector<double> old_solution(solution);
    Vector<double> output_solution(solution);

    std::string outname;
    std::unique_ptr<ODE::RungeKuttaBase> rk_method {new ODE::RungeKutta4()};
    std::tie(outname, rk_method) = POD::NavierStokes::rk_factory
      (boundary_matrix, joint_convection, laplace_matrix,
       linear_operator, mean_contribution_vector, mass_matrix,
       nonlinear_operator, parameters);

    // Annoyingly, there is no way to access the filter burried inside
    // rk_method at this point, so we must build another filter regardless of
    // which filter model we actually use.
    POD::NavierStokes::AD::LavrentievFilter ad_filter
      (mass_matrix, laplace_matrix, boundary_matrix, parameters.filter_radius,
       parameters.noise_multiplier, parameters.lavrentiev_parameter);

    // Filter the initial condition, if appropriate
    if (parameters.filter_model == POD::FilterModel::ADLavrentiev)
      {
        ad_filter.apply(old_solution, solution);
      }

    int n_save_steps = boost::math::iround
      ((parameters.final_time - parameters.initial_time)/parameters.time_step)
      /parameters.output_interval;
    FullMatrix<double> solutions(n_save_steps + 1, n_pod_dofs);
    unsigned int output_n = 0;
    if (parameters.save_plot_pictures)
      {
        auto prefix = outname.substr(0, outname.size() - 3) + "-";
        pod_output.reinit(dof_handler, mean_vector, pod_vectors, prefix);
      }
    while (time < parameters.final_time)
      {
        old_solution = solution;
        rk_method->step(parameters.time_step, old_solution, solution);

        if (timestep_number % parameters.output_interval == 0)
          {
            if (parameters.filter_model == POD::FilterModel::ADLavrentiev)
              {
                ad_filter.apply_inverse(output_solution, solution);
              }
            else
              {
                output_solution = solution;
              }

            for (unsigned int i = 0; i < n_pod_dofs; ++i)
              {
                solutions(output_n, i) = output_solution(i);
              }
            ++output_n;

            if (parameters.save_plot_pictures
                and time >= parameters.output_plot_time_start
                and time <= parameters.output_plot_time_stop)
              {
                pod_output.save_solution(output_solution, time, timestep_number);
              }
          }
        ++timestep_number;
        time += parameters.time_step;
      }

    H5::save_full_matrix(outname, solutions);
  }


  template<int dim>
  void ROM<dim>::run()
  {
    setup_reduced_system();
    time_iterate();
  }
}


int main(int argc, char **argv)
{
  try
    {
      using namespace dealii;
      using namespace NavierStokes;
      Utilities::MPI::MPI_InitFinalize mpi_initialization(argc, argv, 1);
      {
        deallog.depth_console(0);

        POD::NavierStokes::Parameters parameters;
        parameters.read_data("parameter-file.prm");
        ROM<3> nse_solver(parameters);
        nse_solver.run();
      }
    }
  catch (std::exception &exc)
    {
      std::cerr << std::endl << std::endl
                << "----------------------------------------------------"
                << std::endl;
      std::cerr << "Exception on processing: " << std::endl << exc.what()
                << std::endl << "Aborting!" << std::endl
                << "----------------------------------------------------"
                << std::endl;

      return 1;
    }
  catch (...)
    {
      std::cerr << std::endl << std::endl
                << "----------------------------------------------------"
                << std::endl;
      std::cerr << "Unknown exception!" << std::endl << "Aborting!"
                << std::endl
                << "----------------------------------------------------"
                << std::endl;
      return 1;
    }

  return 0;
}
