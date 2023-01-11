/*
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

#ifndef VELOCITY_BLOCK_CONTAINER_H
#define VELOCITY_BLOCK_CONTAINER_H

#include <vector>
#include <stdio.h>

#include "common.h"
#include "unistd.h"

#ifdef DEBUG_VBC
#include <sstream>
#endif

#ifdef USE_CUDA
   #include "cuda_context.cuh"
   // Place block data and parameters inside splitvectors utilizing unified memory
   #include "src/splitvector/splitvec.h"
#endif

namespace vmesh {

#ifdef USE_CUDA
   static const double BLOCK_ALLOCATION_PADDING = 2.5;
   static const double BLOCK_ALLOCATION_FACTOR = 1.8;
#else
   static const double BLOCK_ALLOCATION_FACTOR = 1.1;
#endif

   template<typename LID>
   class VelocityBlockContainer {
    public:

      VelocityBlockContainer();
      LID capacity() const;
      size_t capacityInBytes() const;
      void clear();
      void copy(const LID& source,const LID& target);
      static double getBlockAllocationFactor();
      Realf* getData();
      const Realf* getData() const;
      Realf* getData(const LID& blockLID);
      const Realf* getData(const LID& blockLID) const;
      Real* getParameters();
      const Real* getParameters() const;
      Real* getParameters(const LID& blockLID);
      const Real* getParameters(const LID& blockLID) const;
      void pop();
      LID push_back();
      LID push_back(const uint32_t& N_blocks);
      bool recapacitate(const LID& capacity);
      bool setSize(const LID& newSize);
      LID size() const;
      size_t sizeInBytes() const;
      void swap(VelocityBlockContainer& vbc);

#ifdef USE_CUDA // for CUDA version

      void dev_Allocate(LID size);
      void dev_Allocate();
      void dev_prefetchHost();
      void dev_prefetchDevice();
      // Also add CUDA-capable version of vmesh when CUDA openhashmap is available
#endif

      #ifdef DEBUG_VBC
      const Realf& getData(const LID& blockLID,const unsigned int& cell) const;
      const Real& getParameters(const LID& blockLID,const unsigned int& i) const;
      void setData(const LID& blockLID,const unsigned int& cell,const Realf& value);
      #endif

    private:
      void exitInvalidLocalID(const LID& localID,const std::string& funcName) const;
      void resize();

      LID currentCapacity;
      LID numberOfBlocks;

#ifdef USE_CUDA
      split::SplitVector<Realf> block_data;
      split::SplitVector<Real> parameters;
#else
      std::vector<Realf,aligned_allocator<Realf,WID3> > block_data;
      std::vector<Real,aligned_allocator<Real,BlockParams::N_VELOCITY_BLOCK_PARAMS> > parameters;
#endif

   };

   template<typename LID> inline
   VelocityBlockContainer<LID>::VelocityBlockContainer() : currentCapacity {0}, numberOfBlocks {0} { }

   template<typename LID> inline
   LID VelocityBlockContainer<LID>::capacity() const {
      return currentCapacity;
   }

   template<typename LID> inline
   size_t VelocityBlockContainer<LID>::capacityInBytes() const {
      return (block_data.capacity())*sizeof(Realf) + parameters.capacity()*sizeof(Real);
   }

   /** Clears VelocityBlockContainer data and deallocates all memory
    * reserved for velocity blocks.*/
   template<typename LID> inline
   void VelocityBlockContainer<LID>::clear() {
#ifdef USE_CUDA
      split::SplitVector<Realf> dummy_data;
      split::SplitVector<Real> dummy_parameters;
#else
      std::vector<Realf,aligned_allocator<Realf,WID3> > dummy_data;
      std::vector<Real,aligned_allocator<Real,BlockParams::N_VELOCITY_BLOCK_PARAMS> > dummy_parameters;
#endif
      block_data.swap(dummy_data);
      parameters.swap(dummy_parameters);
      currentCapacity = 0;
      numberOfBlocks = 0;
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::copy(const LID& source,const LID& target) {
      #ifdef DEBUG_VBC
         bool ok = true;
         if (source >= numberOfBlocks) ok = false;
         if (source >= currentCapacity) ok = false;
         if (target >= numberOfBlocks) ok = false;
         if (target >= currentCapacity) ok = false;
         if (numberOfBlocks >= currentCapacity) ok = false;
         if (source != numberOfBlocks-1) ok = false;
         if (source*WID3+WID3-1 >= block_data.size()) ok = false;
         if (source*BlockParams::N_VELOCITY_BLOCK_PARAMS+BlockParams::N_VELOCITY_BLOCK_PARAMS-1 >= parameters.size()) ok = false;
         if (target*BlockParams::N_VELOCITY_BLOCK_PARAMS+BlockParams::N_VELOCITY_BLOCK_PARAMS-1 >= parameters.size()) ok = false;
         if (parameters.size()/BlockParams::N_VELOCITY_BLOCK_PARAMS != block_data.size()/WID3) ok = false;
         if (ok == false) {
            std::stringstream ss;
            ss << "VBC ERROR: invalid source LID=" << source << " in copy, target=" << target << " #blocks=" << numberOfBlocks << " capacity=" << currentCapacity << std::endl;
            ss << "or sizes are wrong, data.size()=" << block_data.size() << " parameters.size()=" << parameters.size() << std::endl;
            std::cerr << ss.str();
            sleep(1);
            exit(1);
         }
      #endif

      for (unsigned int i=0; i<WID3; ++i) block_data[target*WID3+i] = block_data[source*WID3+i];
      for (int i=0; i<BlockParams::N_VELOCITY_BLOCK_PARAMS; ++i) {
         parameters[target*BlockParams::N_VELOCITY_BLOCK_PARAMS+i] = parameters[source*BlockParams::N_VELOCITY_BLOCK_PARAMS+i];
      }
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::exitInvalidLocalID(const LID& localID,const std::string& funcName) const {
      int rank;
      MPI_Comm_rank(MPI_COMM_WORLD,&rank);

      #ifdef DEBUG_VBC
      std::stringstream ss;
      ss << "Process " << rank << ' ';
      ss << "Invalid localID " << localID << " used in function '" << funcName << "' max allowed value is " << numberOfBlocks << std::endl;
      std::cerr << ss.str();
      sleep(1);
      #endif
      exit(1);
   }

   template<typename LID> inline
   double VelocityBlockContainer<LID>::getBlockAllocationFactor() {
      return BLOCK_ALLOCATION_FACTOR;
   }

   template<typename LID> inline
   Realf* VelocityBlockContainer<LID>::getData() {
      return block_data.data();
   }

   template<typename LID> inline
   const Realf* VelocityBlockContainer<LID>::getData() const {
      return block_data.data();
   }

   template<typename LID> inline
   Realf* VelocityBlockContainer<LID>::getData(const LID& blockLID) {
      #ifdef DEBUG_VBC
         if (blockLID >= numberOfBlocks) exitInvalidLocalID(blockLID,"getData");
         if (blockLID >= block_data.size()/WID3) exitInvalidLocalID(blockLID,"const getData const");
      #endif
      return block_data.data() + blockLID*WID3;
   }

   template<typename LID> inline
   const Realf* VelocityBlockContainer<LID>::getData(const LID& blockLID) const {
      #ifdef DEBUG_VBC
         if (blockLID >= numberOfBlocks) exitInvalidLocalID(blockLID,"const getData const");
         if (blockLID >= block_data.size()/WID3) exitInvalidLocalID(blockLID,"const getData const");
      #endif
      return block_data.data() + blockLID*WID3;
   }

#ifdef USE_CUDA
   template<typename LID> inline
   void VelocityBlockContainer<LID>::dev_Allocate(LID size) {
      LID requirement = (size > numberOfBlocks) ? size : numberOfBlocks;
      if (block_data.capacity() > BLOCK_ALLOCATION_FACTOR * requirement) {
         return; // Still have enough buffer
      }

      if (numberOfBlocks > size) {
         std::cerr<<"Error in "<<__FILE__<<" line "<<__LINE__<<": attempting to allocate less than numberOfBlocks"<<std::endl;
         abort();
      }
      if (numberOfBlocks > BLOCK_ALLOCATION_FACTOR * size) {
         std::cerr<<"Warning in "<<__FILE__<<" line "<<__LINE__<<": attempting to allocate less than safety margins"<<std::endl;
      }
      block_data.reallocate(BLOCK_ALLOCATION_PADDING * size);
      parameters.reallocate(BLOCK_ALLOCATION_PADDING * size);
      return;
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::dev_Allocate() {
      if (block_data.capacity() > BLOCK_ALLOCATION_FACTOR * numberOfBlocks) {
         return; // Still have enough buffer
      }
      block_data.reallocate(BLOCK_ALLOCATION_PADDING * numberOfBlocks);
      parameters.reallocate(BLOCK_ALLOCATION_PADDING * numberOfBlocks);
      return;
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::dev_prefetchHost() {
      if (numberOfBlocks==0) return;
      block_data.optimizeCPU(cuda_getStream());
      parameters.optimizeCPU(cuda_getStream());
      return;
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::dev_prefetchDevice() {
      if (numberOfBlocks==0) return;
      block_data.optimizeGPU(cuda_getStream());
      parameters.optimizeGPU(cuda_getStream());
      return;
   }
#endif

   template<typename LID> inline
   Real* VelocityBlockContainer<LID>::getParameters() {
      return parameters.data();
   }

   template<typename LID> inline
   const Real* VelocityBlockContainer<LID>::getParameters() const {
      return parameters.data();
   }

   template<typename LID> inline
   Real* VelocityBlockContainer<LID>::getParameters(const LID& blockLID) {
      #ifdef DEBUG_VBC
         if (blockLID >= numberOfBlocks) exitInvalidLocalID(blockLID,"getParameters");
         if (blockLID >= parameters.size()/BlockParams::N_VELOCITY_BLOCK_PARAMS) exitInvalidLocalID(blockLID,"getParameters");
      #endif
      return parameters.data() + blockLID*BlockParams::N_VELOCITY_BLOCK_PARAMS;
   }

   template<typename LID> inline
   const Real* VelocityBlockContainer<LID>::getParameters(const LID& blockLID) const {
      #ifdef DEBUG_VBC
         if (blockLID >= numberOfBlocks) exitInvalidLocalID(blockLID,"const getParameters const");
         if (blockLID >= parameters.size()/BlockParams::N_VELOCITY_BLOCK_PARAMS) exitInvalidLocalID(blockLID,"getParameters");
      #endif
      return parameters.data() + blockLID*BlockParams::N_VELOCITY_BLOCK_PARAMS;
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::pop() {
      if (numberOfBlocks == 0) return;
      --numberOfBlocks;
   }

   template<typename LID> inline
   LID VelocityBlockContainer<LID>::push_back() {
      LID newIndex = numberOfBlocks;
      if (newIndex >= currentCapacity) {
#ifdef __CUDA_ARCH__
         std::cerr<<"Error in "<<__FILE__<<" line "<<__LINE__<<": attempting to resize splitvector from device code!"<<std::endl;
         abort();
#endif
         resize();
      }

      #ifdef DEBUG_VBC
      if (newIndex >= block_data.size()/WID3 || newIndex >= parameters.size()/BlockParams::N_VELOCITY_BLOCK_PARAMS) {
         std::stringstream ss;
         ss << "VBC ERROR in push_back, LID=" << newIndex << " for new block is out of bounds" << std::endl;
         ss << "\t data.size()=" << block_data.size()  << " parameters.size()=" << parameters.size() << std::endl;
         std::cerr << ss.str();
         sleep(1);
         exit(1);
      }
      #endif

      // Clear velocity block data to zero values
      for (size_t i=0; i<WID3; ++i) block_data[newIndex*WID3+i] = 0.0;
      for (size_t i=0; i<BlockParams::N_VELOCITY_BLOCK_PARAMS; ++i)
         parameters[newIndex*BlockParams::N_VELOCITY_BLOCK_PARAMS+i] = 0.0;

      ++numberOfBlocks;
      return newIndex;
   }

   template<typename LID> inline
   LID VelocityBlockContainer<LID>::push_back(const uint32_t& N_blocks) {
      const LID newIndex = numberOfBlocks;
      numberOfBlocks += N_blocks;
      if (numberOfBlocks > currentCapacity) {
#ifdef __CUDA_ARCH__
         std::cerr<<"Error in "<<__FILE__<<" line "<<__LINE__<<": attempting to resize splitvector from device code!"<<std::endl;
         abort();
#endif
         resize();
      }

      // Clear velocity block data to zero values
      for (size_t i=0; i<WID3*N_blocks; ++i) {
         block_data[newIndex*WID3+i] = 0.0;
      }
      for (size_t i=0; i<BlockParams::N_VELOCITY_BLOCK_PARAMS*N_blocks; ++i) {
         parameters[newIndex*BlockParams::N_VELOCITY_BLOCK_PARAMS+i] = 0.0;
      }

      return newIndex;
   }

   template<typename LID> inline
   bool VelocityBlockContainer<LID>::recapacitate(const LID& newCapacity) {
      if (newCapacity < numberOfBlocks) return false;
      {
#ifdef USE_CUDA
         split::SplitVector<Realf> dummy_data(newCapacity*WID3);
#else
         std::vector<Realf,aligned_allocator<Realf,WID3> > dummy_data(newCapacity*WID3);
#endif
         for (size_t i=0; i<numberOfBlocks*WID3; ++i) dummy_data[i] = block_data[i];
         dummy_data.swap(block_data);
      }
      {
#ifdef USE_CUDA
         split::SplitVector<Real> dummy_parameters(newCapacity*BlockParams::N_VELOCITY_BLOCK_PARAMS);
#else
         std::vector<Real,aligned_allocator<Real,BlockParams::N_VELOCITY_BLOCK_PARAMS> > dummy_parameters(newCapacity*BlockParams::N_VELOCITY_BLOCK_PARAMS);
#endif
         for (size_t i=0; i<numberOfBlocks*BlockParams::N_VELOCITY_BLOCK_PARAMS; ++i) dummy_parameters[i] = parameters[i];
         dummy_parameters.swap(parameters);
      }
      currentCapacity = newCapacity;
   return true;
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::resize() {
#ifdef __CUDA_ARCH__
         std::cerr<<"Error in "<<__FILE__<<" line "<<__LINE__<<": attempting to resize splitvector from device code!"<<std::endl;
         abort();
#endif
#ifdef USE_CUDA
      if (numberOfBlocks*BLOCK_ALLOCATION_FACTOR >= currentCapacity) {
         // Resize so that free space is block_allocation_chunk blocks,
         // and at least two in case of having zero blocks.
         // The order of velocity blocks is unaltered.
         currentCapacity = 2 + numberOfBlocks * BLOCK_ALLOCATION_PADDING;
         block_data.reallocate(currentCapacity*WID3);
         parameters.reallocate(currentCapacity*BlockParams::N_VELOCITY_BLOCK_PARAMS);
      }
#else
      if ((numberOfBlocks+1) >= currentCapacity) {
         // Resize so that free space is block_allocation_chunk blocks,
         // and at least two in case of having zero blocks.
         // The order of velocity blocks is unaltered.
         currentCapacity = 2 + numberOfBlocks * BLOCK_ALLOCATION_FACTOR;
         block_data.resize(currentCapacity*WID3);
         parameters.resize(currentCapacity*BlockParams::N_VELOCITY_BLOCK_PARAMS);
      }
#endif
   }

   template<typename LID> inline
   bool VelocityBlockContainer<LID>::setSize(const LID& newSize) {
      numberOfBlocks = newSize;
      if (newSize > currentCapacity) {
#ifdef __CUDA_ARCH__
         std::cerr<<"Error in "<<__FILE__<<" line "<<__LINE__<<": attempting to resize splitvector from device code!"<<std::endl;
         abort();
#endif
         resize();
      }
      return true;
   }

   /** Return the number of existing velocity blocks.
    * @return Number of existing velocity blocks.*/
   template<typename LID> inline
   LID VelocityBlockContainer<LID>::size() const {
      return numberOfBlocks;
   }

   template<typename LID> inline
   size_t VelocityBlockContainer<LID>::sizeInBytes() const {
      return block_data.size()*sizeof(Realf) + parameters.size()*sizeof(Real);
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::swap(VelocityBlockContainer& vbc) {
      block_data.swap(vbc.block_data);
      parameters.swap(vbc.parameters);

      LID dummy = currentCapacity;
      currentCapacity = vbc.currentCapacity;
      vbc.currentCapacity = dummy;

      dummy = numberOfBlocks;
      numberOfBlocks = vbc.numberOfBlocks;
      vbc.numberOfBlocks = dummy;
   }

   #ifdef DEBUG_VBC

   template<typename LID> inline
   const Realf& VelocityBlockContainer<LID>::getData(const LID& blockLID,const unsigned int& cell) const {
      bool ok = true;
      if (cell >= WID3) ok = false;
      if (blockLID >= numberOfBlocks) ok = false;
      if (blockLID*WID3+cell >= block_data.size()) ok = false;
      if (ok == false) {
         #ifdef DEBUG_VBC
         std::stringstream ss;
         ss << "VBC ERROR: out of bounds in getData, LID=" << blockLID << " cell=" << cell << " #blocks=" << numberOfBlocks << " data.size()=" << block_data.size() << std::endl;
         std::cerr << ss.str();
         sleep(1);
         #endif
         exit(1);
      }

      return block_data[blockLID*WID3+cell];
   }

   template<typename LID> inline
   const Real& VelocityBlockContainer<LID>::getParameters(const LID& blockLID,const unsigned int& cell) const {
      bool ok = true;
      if (cell >= BlockParams::N_VELOCITY_BLOCK_PARAMS) ok = false;
      if (blockLID >= numberOfBlocks) ok = false;
      if (blockLID*BlockParams::N_VELOCITY_BLOCK_PARAMS+cell >= parameters.size()) ok = false;
      if (ok == false) {
         #ifdef DEBUG_VBC
         std::stringstream ss;
         ss << "VBC ERROR: out of bounds in getParameters, LID=" << blockLID << " cell=" << cell << " #blocks=" << numberOfBlocks << " parameters.size()=" << parameters.size() << std::endl;
         std::cerr << ss.str();
         sleep(1);
         #endif
         exit(1);
      }

      return parameters[blockLID*BlockParams::N_VELOCITY_BLOCK_PARAMS+cell];
   }

   template<typename LID> inline
   void VelocityBlockContainer<LID>::setData(const LID& blockLID,const unsigned int& cell,const Realf& value) {
      bool ok = true;
      if (cell >= WID3) ok = false;
      if (blockLID >= numberOfBlocks) ok = false;
      if (blockLID*WID3+cell >= block_data.size()) ok = false;
      if (ok == false) {
         #ifdef DEBUG_VBC
         std::stringstream ss;
         ss << "VBC ERROR: out of bounds in setData, LID=" << blockLID << " cell=" << cell << " #blocks=" << numberOfBlocks << " data.size()=" << block_data.size() << std::endl;
         std::cerr << ss.str();
         sleep(1);
         #endif
         exit(1);
      }

      block_data[blockLID*WID3+cell] = value;
   }

   #endif

} // namespace block_cont

#endif
