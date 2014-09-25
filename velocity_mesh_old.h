#ifndef VELOCITY_MESH_OLD
#define VELOCITY_MESH_OLD

#include <stdint.h>
#include <vector>

namespace vmesh {

   template<typename GID,typename LID>
   class VelocityMesh {
    public:
      
      VelocityMesh();
      VelocityMesh(const VelocityMesh& vm);
      ~VelocityMesh();

//      LocalID  get(const GlobalID& globalID) const;
      static const GID* getBaseGridLength();
      static const Real* getBaseGridBlockSize();
      static const Real* getBaseGridCellSize();
      static bool getBlockCoordinates(const GID& globalID,Real coords[3]);
      static bool getBlockSize(const GID& globalID,Real size[3]);
      static bool getCellSize(const GID& globalID,Real size[3]);
//      void     getChildren(const GlobalID& globalID,std::vector<GlobalID>& children);
      static GID  getGlobalID(const Real& x,const Real& y,const Real& z);
      static GID  getGlobalID(const uint32_t& refLevel,const LID& i,const LID& j,const LID& k);
//      void     getNeighbors(const GlobalID& globalID,std::vector<GlobalID>& neighborIDs);
      static void getIndices(const GID& globalID,uint32_t& refLevel,LID& i,LID& j,LID& k);
      static GID getMaxVelocityBlocks();
      static const Real* getMeshMaxLimits();
      static const Real* getMeshMinLimits();
//      GlobalID getParent(const GlobalID& globalID);
//      void     getSiblingNeighbors(const GlobalID& globalID,std::vector<GlobalID>& nbrs);
//      void     getSiblings(const GlobalID& globalID,GlobalID siblings[8]);
//      void     getSiblings(const GlobalID& globalID,std::vector<GlobalID>& siblings);

      static bool initialize(Real meshLimits[6],LID gridLength[3],LID blockLength[3]);
      
//      bool     set(const GlobalID& globalID,const LocalID& localID);
//      size_t   size() const;

    private:
      VelocityMesh& operator=(const VelocityMesh& vm);

      static const LID error_velocity_block       = 0xFFFFFFFFu;
      static const GID error_velocity_block_index = 0xFFFFFFFFu;

      static LID max_velocity_blocks;                                           /**< Maximum valid block local ID.*/
      static LID blockLength[3];                                                /**< Number of cells in a block per coordinate.*/
      static Real blockSize[3];                                                 /**< Size of a block at base grid level.*/
      static Real cellSize[3];                                                  /**< Size of a cell in a block at base grid level.*/
      static Real gridSize[3];                                                  /**< Size of the grid.*/
      static GID gridLength[3];                                                 /**< Number of blocks in the grid.*/
      static Real meshMinLimits[3];                                             /**< Minimum coordinate values of the grid bounding box.*/
      static Real meshMaxLimits[3];                                             /**< Maximum coordinate values of the grid bounding box.*/
   };

   /** INITIALIZERS FOR STATIC MEMBER VARIABLES **/
   template<typename GID,typename LID> LID VelocityMesh<GID,LID>::max_velocity_blocks = 0;
   template<typename GID,typename LID> LID VelocityMesh<GID,LID>::blockLength[3] = {0,0,0};
   template<typename GID,typename LID> Real VelocityMesh<GID,LID>::blockSize[3] = {NAN,NAN,NAN};
   template<typename GID,typename LID> Real VelocityMesh<GID,LID>::cellSize[3] = {NAN,NAN,NAN};
   template<typename GID,typename LID> Real VelocityMesh<GID,LID>::gridSize[3] = {NAN,NAN,NAN};
   template<typename GID,typename LID> GID VelocityMesh<GID,LID>::gridLength[3] = {0,0,0};
   template<typename GID,typename LID> Real VelocityMesh<GID,LID>::meshMinLimits[3] = {NAN,NAN,NAN};
   template<typename GID,typename LID> Real VelocityMesh<GID,LID>::meshMaxLimits[3] = {NAN,NAN,NAN};
   
   /** DEFINITIONS OF TEMPLATE MEMBER FUNCTIONS **/
   
   template<typename GID,typename LID> inline
   const GID* VelocityMesh<GID,LID>::getBaseGridLength() {
      return gridLength;
   }

   template<typename GID,typename LID> inline
   const Real* VelocityMesh<GID,LID>::getBaseGridBlockSize() {
      return blockSize;
   }
   
   template<typename GID,typename LID> inline
   const Real* VelocityMesh<GID,LID>::getBaseGridCellSize() {
      return cellSize;
   }

   template<typename GID,typename LID> inline
   bool VelocityMesh<GID,LID>::getBlockCoordinates(const GID& globalID,Real coords[3]) {
      if (globalID == error_velocity_block) {
	 for (int i=0; i<3; ++i) coords[i] = std::numeric_limits<Real>::quiet_NaN();
	 return false;
      }
      if (globalID >= max_velocity_blocks) {
	 for (int i=0; i<3; ++i) coords[i] = std::numeric_limits<Real>::quiet_NaN();
	 return false;
      }
      
      uint32_t refLevel;
      LID indices[3];
      getIndices(globalID,refLevel,indices[0],indices[1],indices[2]);
      if (indices[0] == error_velocity_block_index) {
	 for (int i=0; i<3; ++i) coords[i] = std::numeric_limits<Real>::quiet_NaN();
	 return false;
      }

      coords[0] = meshMinLimits[0] + indices[0]*blockSize[0];
      coords[1] = meshMinLimits[1] + indices[1]*blockSize[1];
      coords[2] = meshMinLimits[2] + indices[2]*blockSize[2];
      return true;
   }

   template<typename GID,typename LID> inline
   bool VelocityMesh<GID,LID>::getBlockSize(const GID& globalID,Real size[3]) {
      size[0] = blockSize[0];
      size[1] = blockSize[1];
      size[2] = blockSize[2];
      return true;
   }
   
   template<typename GID,typename LID> inline
   bool VelocityMesh<GID,LID>::getCellSize(const GID& globalID,Real size[3]) {
      size[0] = cellSize[0];
      size[1] = cellSize[1];
      size[2] = cellSize[2];
      return true;
   }
   
   template<typename GID,typename LID> inline
   GID VelocityMesh<GID,LID>::getGlobalID(const uint32_t& refLevel,const LID& i,const LID& j,const LID& k) {
      if (i >= gridLength[0] || j >= gridLength[1] || k >= gridLength[2]) {
	 return error_velocity_block;
      }
      return i + j*gridLength[0] + k*gridLength[0]*gridLength[1];
   }
   
   template<typename GID,typename LID> inline
   GID VelocityMesh<GID,LID>::getGlobalID(const Real& x,const Real& y,const Real& z) {
      if (x < meshMinLimits[0] || x >= meshMaxLimits[0] || y < meshMinLimits[1] || y >= meshMaxLimits[1] || z < meshMinLimits[2] || z >= meshMaxLimits[2]) {
	 return error_velocity_block;
      }
      
      const LID indices[3] = {
	 static_cast<LID>(floor((x - meshMinLimits[0]) / blockSize[0])),
	 static_cast<LID>(floor((y - meshMinLimits[1]) / blockSize[1])),
	 static_cast<LID>(floor((z - meshMinLimits[2]) / blockSize[2]))
      };

      return getGlobalID(0,indices[0],indices[1],indices[2]);
   }

   template<typename GID,typename LID> inline
   void VelocityMesh<GID,LID>::getIndices(const GID& globalID,uint32_t& refLevel,LID& i,LID& j,LID& k) {
      refLevel = 0;
      if (globalID >= max_velocity_blocks) {
	 i = j = k = error_velocity_block_index;
      } else {
	 i = globalID % gridLength[0];
	 j = (globalID / gridLength[0]) % gridLength[1];
	 k = globalID / (gridLength[0] * gridLength[1]);
      }
   }
   
   template<typename GID,typename LID> inline
   GID VelocityMesh<GID,LID>::getMaxVelocityBlocks() {
      return max_velocity_blocks;
   }
   
   template<typename GID,typename LID> inline
   const Real* VelocityMesh<GID,LID>::getMeshMaxLimits() {
      return meshMaxLimits;
   }
   
   template<typename GID,typename LID> inline
   const Real* VelocityMesh<GID,LID>::getMeshMinLimits() {
      return meshMinLimits;
   }
   
   template<typename GID,typename LID> inline
   bool VelocityMesh<GID,LID>::initialize(Real meshLimits[6],LID gridLength[3],LID blockLength[3]) {
      meshMinLimits[0] = meshLimits[0];
      meshMinLimits[1] = meshLimits[2];
      meshMinLimits[2] = meshLimits[4];
      meshMaxLimits[0] = meshLimits[1];
      meshMaxLimits[1] = meshLimits[3];
      meshMaxLimits[2] = meshLimits[5];

      VelocityMesh<GID,LID>::gridLength[0] = gridLength[0];
      VelocityMesh<GID,LID>::gridLength[1] = gridLength[1];
      VelocityMesh<GID,LID>::gridLength[2] = gridLength[2];
      
      VelocityMesh<GID,LID>::blockLength[0] = blockLength[0];
      VelocityMesh<GID,LID>::blockLength[1] = blockLength[1];
      VelocityMesh<GID,LID>::blockLength[2] = blockLength[2];
      
      // Calculate derived mesh parameters:
      gridSize[0] = meshMaxLimits[0] - meshMinLimits[0];
      gridSize[1] = meshMaxLimits[1] - meshMinLimits[1];
      gridSize[2] = meshMaxLimits[2] - meshMinLimits[2];
      
      VelocityMesh<GID,LID>::blockSize[0] = gridSize[0] / gridLength[0];
      VelocityMesh<GID,LID>::blockSize[1] = gridSize[1] / gridLength[1];
      VelocityMesh<GID,LID>::blockSize[2] = gridSize[2] / gridLength[2];

      cellSize[0] = blockSize[0] / blockLength[0];
      cellSize[1] = blockSize[1] / blockLength[1];
      cellSize[2] = blockSize[2] / blockLength[2];

      max_velocity_blocks = gridLength[0]*gridLength[1]*gridLength[2];
      return true;
   }

   template<typename GID,typename LID> inline
   VelocityMesh<GID,LID>::VelocityMesh() { }
   
   template<typename GID,typename LID> inline
   VelocityMesh<GID,LID>::~VelocityMesh() { }
   
} // namespace vmesh

#endif
