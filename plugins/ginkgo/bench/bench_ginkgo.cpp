/*
 * Copyright 2020 IFPEN-CEA
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arccore/message_passing_mpi/StandaloneMpiMessagePassingMng.h>

#include <alien/move/AlienMoveSemantic.h>
#include <alien/move/handlers/scalar/VectorWriter.h>

#include <alien/ginkgo/backend.h>
#include <alien/ginkgo/options.h>

// tmp read from mtx
#include <fstream>

std::vector<double> readFromMtx(const std::string& vec_filename)
{
  // read file
  auto stream = std::ifstream(vec_filename);
  if (!stream) {
    std::cerr << "readFromMatrixMarket : Unable to read file";
    return {};
  }

  // get nb values
  std::string line;
  int nbvalues = 0;

  while (std::getline(stream, line)) {

    if ('%' == line[0]) {
      // skip comment
      continue;
    }
    else {
      //first line is vector size, then done with banner
      std::stringstream ss;
      ss << line;
      ss >> nbvalues;
      break;
    }
  }

  // read values into std::vector
  std::vector<double> values(nbvalues);
  int cpt = 0;
  while (std::getline(stream, line)) {
    if ('%' == line[0]) {
      continue;
    }

    double value;
    std::stringstream ss;
    ss << line;
    ss >> value;
    values[cpt] = value;
    cpt++;
  }
  return values;
}

int test(const Alien::Ginkgo::OptionTypes::eSolver& solv, const Alien::Ginkgo::OptionTypes::ePreconditioner& prec, const std::string& mat_filename, const std::string& vec_filename)
{
  auto* pm = Arccore::MessagePassing::Mpi::StandaloneMpiMessagePassingMng::create(MPI_COMM_WORLD);
  auto* tm = Arccore::arccoreCreateDefaultTraceMng();

  Alien::setTraceMng(tm);
  Alien::setVerbosityLevel(Alien::Verbosity::Debug);

  auto A = Alien::Move::readFromMatrixMarket(pm, mat_filename);

  /**
	 *  Vecteur xe (ones)
	 *********************/

  tm->info() << "* xe = 1";
  auto xe = Alien::Move::VectorData(A.distribution().colDistribution());
  {
    Alien::Move::LocalVectorWriter v_build(std::move(xe));
    for (int i = 0; i < v_build.size(); i++) {
      v_build[i] = 1.0;
    }
    xe = v_build.release();
  }

  tm->info() << "=> Vector Distribution : " << xe.distribution();

  /**
	 *  Vecteur b
	 *************/

  Alien::Move::VectorData b(A.rowSpace(), A.distribution().rowDistribution());
  Alien::Ginkgo::LinearAlgebra algebra;

  if (vec_filename != "") { // Read vector data from mtx file and write it into Alien Vector
    tm->info() << "Read vector file : " << vec_filename;
    std::vector<double> values = readFromMtx(vec_filename);
    int vec_size = values.size();

    Alien::Move::VectorWriter writer(std::move(b));
    for (int i = 0; i < vec_size; i++) {
      writer[i] = values[i];
    }
    b = writer.release();
  }
  else { // Init b with A * xe
    tm->info() << "* b = A * xe";
    algebra.mult(A, xe, b);
  }

  /**
	 *  Calcul x, tq : Ax = b 
	 ********************************************/
  tm->info() << "* Calcul de x, tel que  :  A x = b";
  Alien::Move::VectorData x(A.colSpace(), A.distribution().rowDistribution());

  /** Add initial guess for x ?? */

  Alien::Ginkgo::Options options;
  options.numIterationsMax(500);
  options.stopCriteriaValue(1e-9);
  options.preconditioner(prec); // Jacobi, NoPC
  options.solver(solv); //CG, GMRES, BICG, BICGSTAB
  auto solver = Alien::Ginkgo::LinearSolver(options);
  solver.solve(A, b, x);

  /**
	 *  Calcul du résidu ||Ax - b|| ~ 0
	 ********************************************/
  {
    tm->info() << "* r = Ax - b";

    Alien::Move::VectorData r(A.rowSpace(), A.distribution().rowDistribution());
    algebra.mult(A, x, r);

    tm->info() << "r -= b";
    algebra.axpy(-1., b, r);

    auto norm = algebra.norm2(r);
    auto norm_b = algebra.norm2(b);

    tm->info() << " => ||r|| = " << norm << " ; ||r||/||b|| = " << norm / norm_b;
  }

  tm->info() << " ";
  tm->info() << "... example finished !!!";

  return 0;
}

int main(int argc, char** argv)
{

  MPI_Init(&argc, &argv);

  if (argc < 4) {
    std::cerr << "Usage : ./bench_ginkgo [solver] [preconditioner] [matrix] [vector] \n"
              << "  - solver : (CG|GMRES|BICG|BICGSTAB) \n"
              << "  - preconditioner : (Jacobi|NoPC) \n"
              << "  - MTX matrix file \n"
              << "  - optional MTX vector file \n";
    return -1;
  }

  // Read the solver
  Alien::Ginkgo::OptionTypes::eSolver solver;
  if (std::string(argv[1]) == "CG") {
    solver = Alien::Ginkgo::OptionTypes::CG;
  }
  else if (std::string(argv[1]) == "GMRES") {
    solver = Alien::Ginkgo::OptionTypes::GMRES;
  }
  else if (std::string(argv[1]) == "BICG") {
    solver = Alien::Ginkgo::OptionTypes::BICG;
  }
  else if (std::string(argv[1]) == "BICGSTAB") {
    solver = Alien::Ginkgo::OptionTypes::BICGSTAB;
  }
  else {
    std::cerr << "Unrecognized solver : " << argv[1] << "\n"
              << "  - solver list : (CG|GMRES|BICG|BICGSTAB) \n";
    return -1;
  }

  // Read the preconditioner
  Alien::Ginkgo::OptionTypes::ePreconditioner prec;
  if (std::string(argv[2]) == "Jacobi") {
    prec = Alien::Ginkgo::OptionTypes::Jacobi;
  }
  else if (std::string(argv[2]) == "NoPC") {
    prec = Alien::Ginkgo::OptionTypes::NoPC;
  }
  else {
    std::cerr << "Unrecognized preconditioner : " << argv[2] << "\n"
              << "  - preconditioner list : (Jacobi|NoPC) \n";
    return -1;
  }

  // Read Matrix file
  std::string matrix_file;
  // Read matrix file
  if (argv[3]) {
    matrix_file = std::string(argv[3]);
  }
  else {
    std::cerr << "Matrix File is needed for this bench.";
    return -1;
  }

  // Read optional Vector file
  std::string vec_file = "";
  if (argv[4]) {
    vec_file = std::string(argv[4]);
  }

  auto ret = 0;
  try {
    ret = test(solver, prec, matrix_file, vec_file);
    // ret = test("matrix_first.mtx", "vector_first.mtx");
    // ret = test("mesh1em6.mtx");
    //  ret = test("msc00726.mtx");
  }
  catch (const Arccore::Exception& ex) {
    std::cerr << "Exception: " << ex << '\n';
    ret = 3;
  }
  catch (const std::exception& ex) {
    std::cerr << "** A standard exception occured: " << ex.what() << ".\n";
    ret = 2;
  }
  catch (...) {
    std::cerr << "** An unknown exception has occured...\n";
    ret = 1;
  }

  MPI_Finalize();
  return ret;
}
