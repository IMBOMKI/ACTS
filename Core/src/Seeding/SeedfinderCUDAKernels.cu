#include "Acts/Seeding/SeedfinderCUDAKernels.cuh"
#include "Acts/Utilities/Platforms/CUDA/CuUtils.cu"
#include "Acts/Seeding/IExperimentCuts.hpp"
#include "Acts/Seeding/SeedFilter.hpp"
#include "Acts/Seeding/SeedfinderConfig.hpp"
#include <cuda.h>
#include <cuda_runtime.h>
#include <iostream>

__global__ void cuSearchDoublet(const int* isBottom,  
				const float* rBvec, const float* zBvec, 
				const float* rMvec, const float* zMvec,
				const Acts::CuSeedfinderConfig* config,
				int* isCompatible
				//const float* deltaRMin,const float*deltaRMax,const float*cotThetaMax, 
				//const float* collisionRegionMin, const float* collisionRegionMax,
				);

__global__ void cuTransformCoordinates(const int* isBottom,
					const float* spM,
					const int* nSpB,
					const float* spBmat,
					float* circBmat);

__global__ void cuSearchTriplet(const float* spM,
				const int*   nSpB, const float* spBmat,
				const int*   nSpT, const float* spTmat,
				const float* circBmat,
				const float* circTmat,
				Acts::CuSeedfinderConfig* config
				//const float* maxScatteringAngle2, const float* sigmaScattering,
				//const float* minHelixDiameter2, const float* pT2perRadius,
				//const float* impactMax,
				);

namespace Acts{

  
  void SeedfinderCUDAKernels::searchDoublet(
			        dim3 grid, dim3 block,
				const int* isBottom,
				const float* rBvec, const float* zBvec, 
				const float* rMvec, const float* zMvec,
				//const float* deltaRMin,const float*deltaRMax,const float*cotThetaMax, 
				//const float* collisionRegionMin, const float* collisionRegionMax,
				const Acts::CuSeedfinderConfig* config,
				int* isCompatible  ){
    
  cuSearchDoublet<<< grid, block >>>( isBottom,
				      rBvec, zBvec, rMvec, zMvec,
				      //deltaRMin, deltaRMax, cotThetaMax, 
				      //collisionRegionMin, collisionRegionMax,
				      config,
				      isCompatible );
  gpuErrChk( cudaGetLastError() );
  }

  void SeedfinderCUDAKernels::transformCoordinates(
				   dim3 grid, dim3 block,
				   const int* isBottom, 
				   const float* spM,
				   const int*   nSpB,
				   const float* spBmat,
				   float* circBmat){
    
    cuTransformCoordinates<<< grid, block >>>(isBottom, spM, nSpB, spBmat, circBmat);
    gpuErrChk( cudaGetLastError() );  
  }

  void SeedfinderCUDAKernels::searchTriplet(
				dim3 grid, dim3 block,
				const float* spM,
				const int*   nSpB, const float* spBmat,
				const int*   nSpT, const float* spTmat,
				const float* circBmat,
				const float* circTmat,
				Acts::CuSeedfinderConfig* config
				// finder config
				//const float* maxScatteringAngle2, const float* sigmaScattering,
				//const float* minHelixDiameter2, const float* pT2perRadius,
				//const float* impactMax,
				// filter config
				//const float* deltaInvHelixDiameter,
				//const float* impactWeightFactor,
				//const float* deltaRMin,
				//const float* compatSeedWeight,
				//const size_t* compatSeedLimit,
				){
  
  cuSearchTriplet<<< grid, block, (3*sizeof(float)+sizeof(bool))*block.x >>>(
			       spM,
			       nSpB, spBmat,
			       nSpT, spTmat,				     
			       circBmat,circTmat,
			       config				     
			       //maxScatteringAngle2, sigmaScattering,
			       //minHelixDiameter2, pT2perRadius,
			       //impactMax,
			       );
  gpuErrChk( cudaGetLastError() );
  }
  
}

__global__ void cuSearchDoublet(const int* isBottom,
				const float* rBvec, const float* zBvec, 
				const float* rMvec, const float* zMvec,
				const Acts::CuSeedfinderConfig* config,				
				int* isCompatible 
				//const float* deltaRMin,const float*deltaRMax,const float*cotThetaMax, 
				//const float* collisionRegionMin, const float* collisionRegionMax,
				){
  
  int globalId = threadIdx.x+blockDim.x*blockIdx.x;
  
  float rB = rBvec[threadIdx.x];
  float zB = zBvec[threadIdx.x];
  float rM = rMvec[blockIdx.x];
  float zM = zMvec[blockIdx.x];  

  isCompatible[globalId] = true;
  
  // Doublet search for bottom hits
  if (*isBottom == true){

    float deltaR = rM - rB;

    if (deltaR > config->deltaRMax){
      isCompatible[globalId] = false;
    }

    if (deltaR < config->deltaRMin){
      isCompatible[globalId] = false;
    }

    float cotTheta = (zM - zB)/deltaR;
    if (fabs(cotTheta) > config->cotThetaMax){
      isCompatible[globalId] = false;
    }

    float zOrigin = zM - rM*cotTheta;
    if (zOrigin < config->collisionRegionMin || zOrigin > config->collisionRegionMax){
      isCompatible[globalId] = false;
    }
  }

  // Doublet search for top hits
  else if (*isBottom == false){

    float deltaR = rB - rM;

    if (deltaR < config->deltaRMin){
      isCompatible[globalId] = false;
    }

    if (deltaR > config->deltaRMax){
      isCompatible[globalId] = false;
    }

    if (isCompatible[globalId] == true){
      float cotTheta = (zB - zM)/deltaR;
      if (fabs(cotTheta) > config->cotThetaMax){
	isCompatible[globalId] = false;
      }
      
      float zOrigin = zM - rM*cotTheta;
      if (zOrigin < config->collisionRegionMin || zOrigin > config->collisionRegionMax){
	isCompatible[globalId] = false;
      }
    }
  }
}


__global__ void cuTransformCoordinates(const int* isBottom,
				       const float* spM,
				       const int* nSpB,
				       const float* spBmat,
				       float* circBmat){

  int globalId = threadIdx.x+blockDim.x*blockIdx.x;
  
  float xM = spM[0];
  float yM = spM[1];
  float zM = spM[2];
  float rM = spM[3];
  float varianceRM = spM[4];
  float varianceZM = spM[5];
  float cosPhiM = xM / rM;
  float sinPhiM = yM / rM;
    
  float xB = spBmat[globalId+(*nSpB)*0];
  float yB = spBmat[globalId+(*nSpB)*1];
  float zB = spBmat[globalId+(*nSpB)*2];
  float rB = spBmat[globalId+(*nSpB)*3];
  float varianceRB = spBmat[globalId+(*nSpB)*4];
  float varianceZB = spBmat[globalId+(*nSpB)*5];
  
  float deltaX = xB - xM;
  float deltaY = yB - yM;
  float deltaZ = zB - zM;
  
  // calculate projection fraction of spM->sp vector pointing in same
  // direction as
  // vector origin->spM (x) and projection fraction of spM->sp vector pointing
  // orthogonal to origin->spM (y)
  float x = deltaX * cosPhiM + deltaY * sinPhiM;
  float y = deltaY * cosPhiM - deltaX * sinPhiM;
  // 1/(length of M -> SP)
  float iDeltaR2 = 1. / (deltaX * deltaX + deltaY * deltaY);
  float iDeltaR = std::sqrt(iDeltaR2);

  int bottomFactor = 1 * (int(!(*isBottom))) - 1 * (int(*isBottom));
  // cot_theta = (deltaZ/deltaR)
  float cot_theta = deltaZ * iDeltaR * bottomFactor;
  // VERY frequent (SP^3) access

  // location on z-axis of this SP-duplet
  float Zo = zM - rM * cot_theta;
  
  // transformation of circle equation (x,y) into linear equation (u,v)
  // x^2 + y^2 - 2x_0*x - 2y_0*y = 0
  // is transformed into
  // 1 - 2x_0*u - 2y_0*v = 0
  // using the following m_U and m_V
  // (u = A + B*v); A and B are created later on  
  float U  = x*iDeltaR2;
  float V  = y*iDeltaR2;
  // error term for sp-pair without correlation of middle space point  
  float Er = ((varianceZM + varianceZB) +
	      (cot_theta * cot_theta) * (varianceRM + varianceRB)) * iDeltaR2;  
  
  circBmat[globalId+(*nSpB)*0] = Zo;
  circBmat[globalId+(*nSpB)*1] = cot_theta;
  circBmat[globalId+(*nSpB)*2] = iDeltaR;
  circBmat[globalId+(*nSpB)*3] = Er;
  circBmat[globalId+(*nSpB)*4] = U;
  circBmat[globalId+(*nSpB)*5] = V; 
  
}

__global__ void cuSearchTriplet(const float* spM,
				const int*   nSpB, const float* spBmat,
				const int*   nSpT, const float* spTmat,
				const float* circBmat,
				const float* circTmat,
				Acts::CuSeedfinderConfig* config
				//const float* maxScatteringAngle2, const float* sigmaScattering,
				//const float* minHelixDiameter2,    const float* pT2perRadius,
				//const float* impactMax,
				){
  __shared__ extern bool  isPassed[];
  __shared__ extern float rT[];
  __shared__ extern float curvatures[];
  __shared__ extern float impactParameters[];
  
  int threadId = threadIdx.x;
  int blockId  = blockIdx.x;

  rT[threadIdx.x] = spTmat[threadId+(*nSpT)*3];
  
  float rM = spM[3];
  float zM = spM[2];
  float varianceRM = spM[4];
  float varianceZM = spM[5];

  float spB[6];
  spB[0] = spBmat[blockId+(*nSpB)*0];
  spB[1] = spBmat[blockId+(*nSpB)*1];
  spB[2] = spBmat[blockId+(*nSpB)*2];
  spB[3] = spBmat[blockId+(*nSpB)*3];
  spB[4] = spBmat[blockId+(*nSpB)*4];
  spB[5] = spBmat[blockId+(*nSpB)*5];
  
  float Zob        = circBmat[blockId+(*nSpB)*0];
  float cotThetaB  = circBmat[blockId+(*nSpB)*1];
  float iDeltaRB   = circBmat[blockId+(*nSpB)*2];
  float ErB        = circBmat[blockId+(*nSpB)*3];
  float Ub         = circBmat[blockId+(*nSpB)*4];
  float Vb         = circBmat[blockId+(*nSpB)*5];
  float iSinTheta2 = (1. + cotThetaB * cotThetaB);
  float scatteringInRegion2 = config->maxScatteringAngle2 * iSinTheta2;
  scatteringInRegion2 *= config->sigmaScattering * config->sigmaScattering;

  float Zot        = circTmat[blockId+(*nSpT)*0];
  float cotThetaT  = circTmat[blockId+(*nSpT)*1];
  float iDeltaRT   = circTmat[blockId+(*nSpT)*2];
  float ErT        = circTmat[blockId+(*nSpT)*3];
  float Ut         = circTmat[blockId+(*nSpT)*4];
  float Vt         = circTmat[blockId+(*nSpT)*5];

  // add errors of spB-spM and spM-spT pairs and add the correlation term
  // for errors on spM
  float error2 = ErT + ErB +
    2 * (cotThetaT * cotThetaT * varianceRM + varianceZM) * iDeltaRB * iDeltaRT;
  
  float deltaCotTheta = cotThetaB - cotThetaT;
  float deltaCotTheta2 = deltaCotTheta * deltaCotTheta;

  float error;
  float dCotThetaMinusError2;

  isPassed[threadId] = true;
  
  // if the error is larger than the difference in theta, no need to
  // compare with scattering
  if (deltaCotTheta2 - error2 > 0) {
    deltaCotTheta = std::fabs(deltaCotTheta);
    // if deltaTheta larger than the scattering for the lower pT cut, skip
    error = std::sqrt(error2);
    dCotThetaMinusError2 =
      deltaCotTheta2 + error2 - 2 * deltaCotTheta * error;
    // avoid taking root of scatteringInRegion
    // if left side of ">" is positive, both sides of unequality can be
    // squared
    // (scattering is always positive)
    
    if (dCotThetaMinusError2 > scatteringInRegion2) {
      isPassed[threadId] = false;
    }
  }

  // protects against division by 0
  float dU = Ut - Ub;
  if (dU == 0.) {
    isPassed[threadId] = false;
  }

  // A and B are evaluated as a function of the circumference parameters
  // x_0 and y_0
  float A = (Vt - Vb) / dU;
  float S2 = 1. + A * A;
  float B = Vb - A * Ub;
  float B2 = B * B;
  // sqrt(S2)/B = 2 * helixradius
  // calculated radius must not be smaller than minimum radius
  if (S2 < B2 * config->minHelixDiameter2) {
    isPassed[threadId] = false;
  }
  
  // 1/helixradius: (B/sqrt(S2))/2 (we leave everything squared)
  float iHelixDiameter2 = B2 / S2;
  // calculate scattering for p(T) calculated from seed curvature
  float pT2scatter = 4 * iHelixDiameter2 * config->pT2perRadius;
  // TODO: include upper pT limit for scatter calc
  // convert p(T) to p scaling by sin^2(theta) AND scale by 1/sin^4(theta)
  // from rad to deltaCotTheta
  float p2scatter = pT2scatter * iSinTheta2;
  // if deltaTheta larger than allowed scattering for calculated pT, skip
  if ((deltaCotTheta2 - error2 > 0) &&
      (dCotThetaMinusError2 >
       p2scatter * config->sigmaScattering * config->sigmaScattering)) {
    isPassed[threadId] = false;
  }
  // A and B allow calculation of impact params in U/V plane with linear
  // function
  // (in contrast to having to solve a quadratic function in x/y plane)
  impactParameters[threadId] = std::fabs((A - B * rM) * rM);
  curvatures[threadId] = B / std::sqrt(S2);
  
  if (impactParameters[threadId] > config->impactMax){
    isPassed[threadId] = false;
  }  


  //config->seedFilter.filterSeeds_2SpFixed();
  config->seedFilter.filterSeeds_2SpFixed(&threadId, spM, spB, nSpT, spTmat,
  					  isPassed, curvatures, impactParameters, &Zob);
}
