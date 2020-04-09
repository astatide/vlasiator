/*
 * This file is part of Vlasiator.
 * Copyright 2010-2020 Finnish Meteorological Institute
 *
 * For details of usage, see the COPYING file and read the "Rules of the Road"
 * at http://www.physics.helsinki.fi/vlasiator/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef IONOSPHERE_H
#define IONOSPHERE_H

#include <vector>
#include "../definitions.h"
#include "../readparameters.h"
#include "../spatial_cell.hpp"
#include "sysboundarycondition.h"
#include "../backgroundfield/fieldfunction.hpp"

using namespace projects;
using namespace std;

namespace SBC {

   struct IonosphereSpeciesParameters {
      Real rho;
      Real V0[3];
      Real T;
      Real fluffiness;
      uint nSpaceSamples;
      uint nVelocitySamples;
   };


   static const int MAX_TOUCHING_ELEMENTS = 6; // Maximum number of elements touching one node
   static const int MAX_DEPENDING_NODES = 10;   // Maximum number of depending nodes

   // Ionosphere finite element grid
   struct SphericalTriGrid {

      // One finite element, spanned between 3 nodes
      struct Element {
         int refLevel;
         std::array<uint32_t, 3> corners;                 // Node indices in the corners of this element
         //std::array<Real, 3> upmappedCentre = {0,0,0};    // Coordinates the cell barycentre maps to
         // List of fsgrid cells to couple to (and their strengths)

      };
      std::vector<Element> elements;

      // One grid node
      struct Node {
         // Elements touching this node
         uint numTouchingElements;
         std::array<uint32_t, MAX_TOUCHING_ELEMENTS> touchingElements;
   
         // List of nodes the current node depends on
         uint numDepNodes;
         std::array<uint32_t, MAX_DEPENDING_NODES> dependingNodes;
         std::array<Real, MAX_DEPENDING_NODES> dependingCoeffs;
         std::array<Real, MAX_DEPENDING_NODES> transposedCoeffs;

         std::array<Real, 3> x = {0,0,0}; // Coordinates of the node
         std::array<Real, 3> xMapped = {0,0,0}; // Coordinates mapped along fieldlines into simulation domain
         std::array<Real, MAX_DEPENDING_NODES> depCoeffs; // Dependency coefficients
         std::array<Real, MAX_DEPENDING_NODES> depCoeffsT; // Transposed ependency coefficient

         std::array<Real, N_IONOSPHERE_PARAMETERS> parameters = {0}; // Parameters carried by the node, see common.h
         std::vector<std::pair<std::array<int,3>, Real>> fsgridCellCoupling;
      };
      std::vector<Node> nodes;

      MPI_Comm communicator;             // The communicator internally used to solve
      int rank = -1;
      bool isCouplingToCells;             // True for any rank that actually couples to the outer simulation

      void offset_FAC();                  // Offset field aligned currents to get overall zero current
      void normalizeRadius(Node& n, Real R); // Scale all coordinates onto sphere with radius R
      void updateConnectivity();          // Re-link elements and nodes
      void initializeTetrahedron();       // Initialize grid as a base tetrahedron
      void initializeIcosahedron();       // Initialize grid as a base icosahedron
      int32_t findElementNeighbour(uint32_t e, int n1, int n2);
      void subdivideElement(uint32_t e);  // Subdivide mesh within element e
      void calculateFsgridCoupling(FsGrid< fsgrids::technical, 2> & technicalGrid, FieldFunction& dipole, Real radius);     // Link each element to fsgrid cells for coupling

      // Conjugate Gradient solver functions
      void addMatrixDependency(uint node1, uint node2, Real coeff, bool transposed=false); // Add matrix value for the solver
      void addAllMatrixDependencies(uint nodeIndex);
      void initSolver();                 // Initialize the CG solver
      Real Atimes(uint nodeIndex, int parameter, bool transpose=false); // Evaluate neighbour nodes' coupled parameter
      Real Asolve(uint nodeIndex, int parameter); // Evaluate own parameter value
      void solve();

      void mapDownFAC(
          FsGrid< std::array<Real, fsgrids::dperb::N_DPERB>, 2> dPerBGrid,
          FsGrid< std::array<Real, fsgrids::bgbfield::N_BGB>, 2> & BgBGrid);  // Map field-aligned currents down from the simulation boundary onto this grid

      // Returns the surface area of one element on the sphere
      Real elementArea(uint32_t elementIndex) {
         const std::array<Real, 3>& a = nodes[elements[elementIndex].corners[0]].x;
         const std::array<Real, 3>& b = nodes[elements[elementIndex].corners[1]].x;
         const std::array<Real, 3>& c = nodes[elements[elementIndex].corners[2]].x;

         // Two edges e1 = b-c,  e2 = c-a
         std::array<Real, 3> e1{b[0]-c[0], b[1]-c[1],b[2]-c[2]};
         std::array<Real, 3> e2{c[0]-a[0], c[1]-a[1],c[2]-a[2]};
         // Area vector A = cross(e1 e2)
         std::array<Real, 3> area{ e1[1]*e2[2] - e1[2]*e2[1],
                                   e1[2]*e2[0] - e1[0]*e2[2],
                                   e1[0]*e2[1] - e1[1]*e2[0]};
         
         return 0.5 * sqrt( area[0]*area[0] + area[1]*area[1] + area[2]*area[2] );
      }

      // Returns the projected surface area of one element, mapped up along the magnetic field to
      // the simulation boundary. If one of the nodes maps nowhere, returns 0.
      Real mappedElementArea(uint32_t elementIndex) {
         const std::array<Real, 3>& a = nodes[elements[elementIndex].corners[0]].xMapped;
         const std::array<Real, 3>& b = nodes[elements[elementIndex].corners[1]].xMapped;
         const std::array<Real, 3>& c = nodes[elements[elementIndex].corners[2]].xMapped;

         // Check if any node maps to zero
         if( sqrt( a[0]*a[0] + a[1]*a[1] + a[2]*a[2] ) == 0 ||
               sqrt( b[0]*b[0] + b[1]*b[1] + b[2]*b[2] ) == 0 ||
               sqrt( c[0]*c[0] + c[1]*c[1] + c[2]*c[2] ) == 0) {

            return 0;
         }

         // Two edges e1 = b-c,  e2 = c-a
         std::array<Real, 3> e1{b[0]-c[0], b[1]-c[1],b[2]-c[2]};
         std::array<Real, 3> e2{c[0]-a[0], c[1]-a[1],c[2]-a[2]};
         // Area vector A = cross(e1 e2)
         std::array<Real, 3> area{ e1[1]*e2[2] - e1[2]*e2[1],
                                   e1[2]*e2[0] - e1[0]*e2[2],
                                   e1[0]*e2[1] - e1[1]*e2[0]};
         
         return 0.5 * sqrt( area[0]*area[0] + area[1]*area[1] + area[2]*area[2] );
      }

      Real nodeNeighbourArea(uint32_t nodeIndex) { // Summed area of all touching elements

         Node& n = nodes[nodeIndex];
         Real area=0;

         for(uint i=0; i<n.numTouchingElements; i++) {
            area += elementArea(n.touchingElements[i]);
         }
         return area;
      }

      std::array<Real,3> computeGradT(const std::array<Real, 3>& a, const std::array<Real, 3>& b, const std::array<Real, 3>& c);
      std::array<Real, 9> sigmaAverage(uint elementIndex);
      double elementIntegral(uint elementIndex, int i, int j, bool transpose = false);

   };

   extern SphericalTriGrid ionosphereGrid;

   /*!\brief Ionosphere is a class applying ionospheric boundary conditions.
    * 
    * Ionosphere is a class handling cells tagged as sysboundarytype::IONOSPHERE by this system boundary condition. It applies ionospheric boundary conditions.
    * 
    * These consist in:
    * - Do nothing for the distribution (keep the initial state constant in time);
    * - Keep only the normal perturbed B component and null out the other perturbed components (perfect conductor behavior);
    * - Null out the electric fields.
    */
   class Ionosphere: public SysBoundaryCondition {
   public:
      Ionosphere();
      virtual ~Ionosphere();
      
      static void addParameters();
      virtual void getParameters();
      
      virtual bool initSysBoundary(
         creal& t,
         Project &project
      );
      virtual bool assignSysBoundary(dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
                                     FsGrid< fsgrids::technical, 2> & technicalGrid);
      virtual bool applyInitialState(
         const dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
         FsGrid< std::array<Real, fsgrids::bfield::N_BFIELD>, 2> & perBGrid,
         Project &project
      );
      virtual Real fieldSolverBoundaryCondMagneticField(
         FsGrid< std::array<Real, fsgrids::bfield::N_BFIELD>, 2> & bGrid,
         FsGrid< fsgrids::technical, 2> & technicalGrid,
         cint i,
         cint j,
         cint k,
         creal& dt,
         cuint& component
      );
      virtual void fieldSolverBoundaryCondElectricField(
         FsGrid< std::array<Real, fsgrids::efield::N_EFIELD>, 2> & EGrid,
         cint i,
         cint j,
         cint k,
         cuint component
      );
      virtual void fieldSolverBoundaryCondHallElectricField(
         FsGrid< std::array<Real, fsgrids::ehall::N_EHALL>, 2> & EHallGrid,
         cint i,
         cint j,
         cint k,
         cuint component
      );
      virtual void fieldSolverBoundaryCondGradPeElectricField(
         FsGrid< std::array<Real, fsgrids::egradpe::N_EGRADPE>, 2> & EGradPeGrid,
         cint i,
         cint j,
         cint k,
         cuint component
      );
      virtual void fieldSolverBoundaryCondDerivatives(
         FsGrid< std::array<Real, fsgrids::dperb::N_DPERB>, 2> & dPerBGrid,
         FsGrid< std::array<Real, fsgrids::dmoments::N_DMOMENTS>, 2> & dMomentsGrid,
         cint i,
         cint j,
         cint k,
         cuint& RKCase,
         cuint& component
      );
      virtual void fieldSolverBoundaryCondBVOLDerivatives(
         FsGrid< std::array<Real, fsgrids::volfields::N_VOL>, 2> & volGrid,
         cint i,
         cint j,
         cint k,
         cuint& component
      );
      virtual void vlasovBoundaryCondition(
         const dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
         const CellID& cellID,
         const uint popID,
         const bool calculate_V_moments
      );
      
      virtual std::string getName() const;
      virtual uint getIndex() const;
      static Real innerRadius; /*!< Radius of the ionosphere model */
      static int solverMaxIterations; /*!< Maximum iterations of CG solver per timestep */
      
   protected:
      void generateTemplateCell(Project &project);
      void setCellFromTemplate(SpatialCell* cell,const uint popID);
      
      Real shiftedMaxwellianDistribution(const uint popID,creal& vx, creal& vy, creal& vz);
      
      vector<vmesh::GlobalID> findBlocksToInitialize(
         SpatialCell& cell,const uint popID
      );
      
      std::array<Real, 3> fieldSolverGetNormalDirection(
         FsGrid< fsgrids::technical, 2> & technicalGrid,
         cint i,
         cint j,
         cint k
      );
      
      Real center[3]; /*!< Coordinates of the centre of the ionosphere. */
      Real radius; /*!< Radius of the inner simulation boundary */
      uint geometry; /*!< Geometry of the ionosphere, 0: inf-norm (diamond), 1: 1-norm (square), 2: 2-norm (circle, DEFAULT), 3: polar-plane cylinder with line dipole. */

      std::vector<IonosphereSpeciesParameters> speciesParams;
      Real T;
      Real rho;
      Real VX0;
      Real VY0;
      Real VZ0;

      std::string baseShape; // Basic mesh shape (icosahedron / tetrahedron)
      // Boundaries of refinement latitude bands
      std::vector<Real> refineMinLatitudes;
      std::vector<Real> refineMaxLatitudes;
      
      uint nSpaceSamples;
      uint nVelocitySamples;
      
      spatial_cell::SpatialCell templateCell;
   };
}

#endif
