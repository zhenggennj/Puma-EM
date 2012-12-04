#include <fstream>
#include <iostream>
#include <string>
#include <complex>
#include <cmath>
#include <blitz/array.h>
#include <vector>
#include <algorithm>
#include <mpi.h>

using namespace std;

#include "cube.h"
#include "readWriteBlitzArrayFromFile.h"

Cube::Cube(const bool is_leaf,                           // 1 if cube is leaf
           const int level,                              // the level
           const double sideLength,                      // length of cube side
           const double bigCubeLowerCoord[3], // coordinates of level 0 cube
           const blitz::Array<double, 1>& r_c)                  // coordinates of center
{
  leaf = is_leaf;
  for (int i=0 ; i<3 ; ++i) rCenter[i] = r_c(i); // we must loop, since rCenter is an array

  // we compute the absolute cartesian coordinates and the cube number
  for (int i=0 ; i<3 ; ++i) absoluteCartesianCoord[i] = floor( (rCenter[i]-bigCubeLowerCoord[i])/sideLength );
  double maxNumberCubes1D = pow(2.0, level);
  number = static_cast<int>( absoluteCartesianCoord[0] * maxNumberCubes1D*maxNumberCubes1D + absoluteCartesianCoord[1] * maxNumberCubes1D + absoluteCartesianCoord[2] );

  // we compute the number of the father
  double cartesianCoordInFathers[3];
  for (int i=0; i<3; i++) cartesianCoordInFathers[i] = floor( (rCenter[i]-bigCubeLowerCoord[i])/(2.0*sideLength) );
  double maxNumberCubes1D_next_level = maxNumberCubes1D/2.0;
  fatherNumber =  static_cast<int>( cartesianCoordInFathers[0] * maxNumberCubes1D_next_level*maxNumberCubes1D_next_level + cartesianCoordInFathers[1] * maxNumberCubes1D_next_level + cartesianCoordInFathers[2] );
}

Cube::Cube(const Cube& sonCube,
           const int level,
           const double bigCubeLowerCoord[3],
           const double sideLength)
{
  leaf = 0; // since we construct from a son cube...
  number = sonCube.getFatherNumber();
  procNumber = sonCube.getProcNumber();
  sonsIndexes.push_back(sonCube.getIndex());
  double sonCartesianCoordInFathers[3];
  for (int i=0; i<3; i++) sonCartesianCoordInFathers[i] = floor( (sonCube.rCenter[i] - bigCubeLowerCoord[i]) / sideLength );
  for (int i=0; i<3; i++) rCenter[i] = bigCubeLowerCoord[i] + sonCartesianCoordInFathers[i] * sideLength + sideLength/2.0;
  // we compute the absolute cartesian coordinates
  for (int i=0 ; i<3 ; ++i) absoluteCartesianCoord[i] = floor( (rCenter[i]-bigCubeLowerCoord[i])/sideLength );

  // we compute the number of the father of _this_ cube
  // (i.e. grandfather of sonCube)
  double cartesianCoordInFathers[3];
  for (int i=0; i<3; i++) cartesianCoordInFathers[i] = floor( (rCenter[i]-bigCubeLowerCoord[i])/(2.0*sideLength) );
  double maxNumberCubes1D_next_level = pow(2.0, level-1);
  fatherNumber = static_cast<int>(cartesianCoordInFathers[0] * maxNumberCubes1D_next_level*maxNumberCubes1D_next_level + cartesianCoordInFathers[1] * maxNumberCubes1D_next_level + cartesianCoordInFathers[2]);
}

void Cube::computeGaussLocatedArguments(const blitz::Array<int, 1>& local_RWG_numbers,
                                        const blitz::Array<int, 1>& local_RWG_Numbers_CFIE_OK,
                                        const blitz::Array<float, 2>& local_RWGNumbers_trianglesCoord,
                                        const int startIndex_in_localArrays,
                                        const int NRWG,
                                        const int N_Gauss)
{
  RWG_numbers.resize(NRWG);
  RWG_numbers_CFIE_OK.resize(NRWG);
  for (int j=0 ; j<NRWG ; ++j) RWG_numbers[j] = local_RWG_numbers(startIndex_in_localArrays + j);
  for (int j=0 ; j<NRWG ; ++j) RWG_numbers_CFIE_OK[j] = local_RWG_Numbers_CFIE_OK(startIndex_in_localArrays + j);
  
  GaussLocatedWeightedRWG.resize(NRWG, 2*N_Gauss);
  GaussLocatedWeighted_nHat_X_RWG.resize(NRWG, 2*N_Gauss);
  GaussLocatedExpArg.resize(NRWG, 2*N_Gauss);
  double sum_weigths;
  const double *xi, *eta, *weigths;
  IT_points (xi, eta, weigths, sum_weigths, N_Gauss);

  for (int j=0 ; j<NRWG ; ++j) {
    double r[3], r0[3], r1[3], r2[3], n_hat[3], r1_r0[3], r2_r0[3], r2_r1[3];
    const double *r_p;
    for (int halfBasisCounter = 0 ; halfBasisCounter < 2 ; ++halfBasisCounter) {
      if (halfBasisCounter==0) {
        for (int i=0; i<3; i++) {
          r0[i] = local_RWGNumbers_trianglesCoord(startIndex_in_localArrays + j, i);
          r1[i] = local_RWGNumbers_trianglesCoord(startIndex_in_localArrays + j, i+3);
          r2[i] = local_RWGNumbers_trianglesCoord(startIndex_in_localArrays + j, i+6);
          r1_r0[i] = r1[i] - r0[i];
          r2_r0[i] = r2[i] - r0[i];
          r2_r1[i] = r2[i] - r1[i];
        }
      }
      else {
        for (int i=0; i<3; i++) {
          r0[i] = local_RWGNumbers_trianglesCoord(startIndex_in_localArrays + j, i+9);
          r1[i] = local_RWGNumbers_trianglesCoord(startIndex_in_localArrays + j, i+6);
          r2[i] = local_RWGNumbers_trianglesCoord(startIndex_in_localArrays + j, i+3);
          r1_r0[i] = r1[i] - r0[i];
          r2_r0[i] = r2[i] - r0[i];
          r2_r1[i] = r2[i] - r1[i];
        }
      }
      r_p = r0;
      n_hat[0] = r1_r0[1]*r2_r0[2] - r1_r0[2]*r2_r0[1];
      n_hat[1] = r1_r0[2]*r2_r0[0] - r1_r0[0]*r2_r0[2];
      n_hat[2] = r1_r0[0]*r2_r0[1] - r1_r0[1]*r2_r0[0];
      const double Area = sqrt(n_hat[0]*n_hat[0] + n_hat[1]*n_hat[1] + n_hat[2]*n_hat[2])/2.0;
      for (int i=0; i<3; i++) n_hat[i] *= 1.0/(2.0*Area);
      double l_p = sqrt(r2_r1[0]*r2_r1[0] + r2_r1[1]*r2_r1[1] + r2_r1[2]*r2_r1[2]);
      const double sign_edge_p_tmp[2] = {1.0, -1.0};
      const double sign_edge_p = sign_edge_p_tmp[halfBasisCounter];
      for (int i=0 ; i<N_Gauss ; ++i) {
        r[0] = r0[0] * xi[i] + r1[0] * eta[i] + r2[0] * (1-xi[i]-eta[i]);
        r[1] = r0[1] * xi[i] + r1[1] * eta[i] + r2[1] * (1-xi[i]-eta[i]);
        r[2] = r0[2] * xi[i] + r1[2] * eta[i] + r2[2] * (1-xi[i]-eta[i]);
        const double r_rp[3] = {r[0]-r_p[0], r[1]-r_p[1], r[2]-r_p[2]};
        double n_hat_X_r_rp[3];
        n_hat_X_r_rp[0] = n_hat[1]*r_rp[2] - n_hat[2]*r_rp[1];
        n_hat_X_r_rp[1] = n_hat[2]*r_rp[0] - n_hat[0]*r_rp[2];
        n_hat_X_r_rp[2] = n_hat[0]*r_rp[1] - n_hat[1]*r_rp[0];
        const double temp(sign_edge_p * l_p/2.0/sum_weigths * weigths[i]);
        GaussLocatedWeightedRWG(j, i + halfBasisCounter*N_Gauss)[0] = temp * r_rp[0];
        GaussLocatedWeightedRWG(j, i + halfBasisCounter*N_Gauss)[1] = temp * r_rp[1];
        GaussLocatedWeightedRWG(j, i + halfBasisCounter*N_Gauss)[2] = temp * r_rp[2];
        GaussLocatedWeighted_nHat_X_RWG(j, i + halfBasisCounter*N_Gauss)[0] = temp * n_hat_X_r_rp[0];
        GaussLocatedWeighted_nHat_X_RWG(j, i + halfBasisCounter*N_Gauss)[1] = temp * n_hat_X_r_rp[1];
        GaussLocatedWeighted_nHat_X_RWG(j, i + halfBasisCounter*N_Gauss)[2] = temp * n_hat_X_r_rp[2];
        GaussLocatedExpArg(j, i + halfBasisCounter*N_Gauss)[0] = r[0]-rCenter[0];
        GaussLocatedExpArg(j, i + halfBasisCounter*N_Gauss)[1] = r[1]-rCenter[1];
        GaussLocatedExpArg(j, i + halfBasisCounter*N_Gauss)[2] = r[2]-rCenter[2];
      }
    }
  }
}


void Cube::copyCube(const Cube& cubeToCopy) // copy member function
{
  leaf = cubeToCopy.getLeaf();
  number = cubeToCopy.getNumber();
  index = cubeToCopy.getIndex();
  oldIndex = cubeToCopy.getOldIndex();
  procNumber = cubeToCopy.getProcNumber();
  fatherNumber = cubeToCopy.getFatherNumber();
  fatherProcNumber = cubeToCopy.getFatherProcNumber();
  fatherIndex = cubeToCopy.getFatherIndex();
  sonsIndexes = cubeToCopy.getSonsIndexes();
  sonsProcNumbers = cubeToCopy.getSonsProcNumbers();
  neighborsIndexes.resize(cubeToCopy.neighborsIndexes.size());
  neighborsIndexes = cubeToCopy.neighborsIndexes;
  localAlphaTransParticipantsIndexes.resize(cubeToCopy.localAlphaTransParticipantsIndexes.size());
  localAlphaTransParticipantsIndexes = cubeToCopy.localAlphaTransParticipantsIndexes;
  nonLocalAlphaTransParticipantsIndexes.resize(cubeToCopy.nonLocalAlphaTransParticipantsIndexes.size());
  nonLocalAlphaTransParticipantsIndexes = cubeToCopy.nonLocalAlphaTransParticipantsIndexes;
  for (int i=0; i<3; i++) rCenter[i] = cubeToCopy.rCenter[i];
  for (int i=0; i<3; i++) absoluteCartesianCoord[i] = cubeToCopy.absoluteCartesianCoord[i];
  RWG_numbers.resize(cubeToCopy.RWG_numbers.size());
  RWG_numbers = cubeToCopy.RWG_numbers;
  RWG_numbers_CFIE_OK.resize(cubeToCopy.RWG_numbers_CFIE_OK.size());
  RWG_numbers_CFIE_OK = cubeToCopy.RWG_numbers_CFIE_OK;
  const int M = cubeToCopy.GaussLocatedWeightedRWG.extent(0), N = cubeToCopy.GaussLocatedWeightedRWG.extent(1);
  GaussLocatedWeightedRWG.resize(M, N);
  GaussLocatedWeighted_nHat_X_RWG.resize(M, N);
  GaussLocatedExpArg.resize(M, N);
  for (int i=0 ; i<M ; i++) {
    for (int j=0 ; j<N ; j++) {
      GaussLocatedWeightedRWG(i, j)[0] = cubeToCopy.GaussLocatedWeightedRWG(i, j)[0];
      GaussLocatedWeightedRWG(i, j)[1] = cubeToCopy.GaussLocatedWeightedRWG(i, j)[1];
      GaussLocatedWeightedRWG(i, j)[2] = cubeToCopy.GaussLocatedWeightedRWG(i, j)[2];
      GaussLocatedWeighted_nHat_X_RWG(i, j)[0] = cubeToCopy.GaussLocatedWeighted_nHat_X_RWG(i, j)[0];
      GaussLocatedWeighted_nHat_X_RWG(i, j)[1] = cubeToCopy.GaussLocatedWeighted_nHat_X_RWG(i, j)[1];
      GaussLocatedWeighted_nHat_X_RWG(i, j)[2] = cubeToCopy.GaussLocatedWeighted_nHat_X_RWG(i, j)[2];
      GaussLocatedExpArg(i, j)[0] = cubeToCopy.GaussLocatedExpArg(i, j)[0];
      GaussLocatedExpArg(i, j)[1] = cubeToCopy.GaussLocatedExpArg(i, j)[1];
      GaussLocatedExpArg(i, j)[2] = cubeToCopy.GaussLocatedExpArg(i, j)[2];
    }
  }
}

Cube::Cube(const Cube& cubeToCopy) // copy constructor
{
  copyCube(cubeToCopy);
}

Cube& Cube::operator=(const Cube& cubeToCopy) { // copy assignment
  copyCube(cubeToCopy);
  return *this;
}

Cube::~Cube() {
  sonsIndexes.clear();
  sonsProcNumbers.clear();
  neighborsIndexes.clear();
  localAlphaTransParticipantsIndexes.clear();
  nonLocalAlphaTransParticipantsIndexes.clear();
  RWG_numbers.clear();
  RWG_numbers_CFIE_OK.clear();
  GaussLocatedWeightedRWG.free();
  GaussLocatedWeighted_nHat_X_RWG.free();
  GaussLocatedExpArg.free();
}

void Cube::addSon(const Cube& sonCube)
{
  if (number == sonCube.getFatherNumber()) sonsIndexes.push_back(sonCube.getIndex());
  else {
    cout << "ERROR: no Son added because (number != sonCube.getFatherNumber())" << endl;
    exit(1);
  }
}


bool Cube::operator== (const Cube & right) const {
  if ( this->getFatherNumber() == right.getFatherNumber() ) return 1;
  else return 0;
}

bool Cube::operator< (const Cube & right) const {
  if ( this->getFatherNumber() < right.getFatherNumber() ) return 1;
  else return 0;
}

