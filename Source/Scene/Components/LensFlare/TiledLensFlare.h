#pragma once

////////////////////////////////////////////////////////////////////////////////
//  Headers
////////////////////////////////////////////////////////////////////////////////

#include "PCH.h"
#include "Common.h"
#include "PhysicalCamera.h"

namespace TiledLensFlare
{
	////////////////////////////////////////////////////////////////////////////////
	/** Component and display name. */
	static constexpr const char* COMPONENT_NAME = "TiledLensFlare";
	static constexpr const char* DISPLAY_NAME = "Tiled Lens Flare";
	static constexpr const char* CATEGORY = "LensFlare";

	////////////////////////////////////////////////////////////////////////////////
	/** Maximum number of various properties. */
	static const size_t MAX_LENSES = 64;
	static const size_t MAX_CHANNELS = 3;

	////////////////////////////////////////////////////////////////////////////////
	struct CommonParameters
	{
		// The various methods available to determine ghost attributes for rendering
		meta_enum(PhysicalPupilSizeMethod, int, EntrancePupil, ParaxialTracedPupil);

		// The method used to determine the physical pupil size
		PhysicalPupilSizeMethod m_physicalPupilSizeMethod;

		// List of ghosts to ignore for each optical system
		std::unordered_map<std::string, std::vector<int>> m_ignoredGhosts;
	};

	////////////////////////////////////////////////////////////////////////////////
	struct RenderGhostsParameters
	{
		// The various methods available to determine ghost attributes for rendering
		meta_enum(GhostAttribsMethod, int, NoGhostAttribs, RayTracedGhostAttribs, ParaxialGhostAttribs);
		meta_enum(ParaxialBoundsSizeMethod, int, CurrentAngle, ZeroAngle, MaxCurrentZero, AverageCurrentZero);
		meta_enum(ProjectionMethod, int, SensorGrid, PupilGrid);
		meta_enum(ShadingMethod, int, Shaded, Uncolored, Unshaded, PupilCoords, SensorCoords, UVCoords, RelativeRadius, ApertureDist, ClipFactor, Validity);
		meta_enum(OverlayMethod, int, DisableOverlay, TractabilityOverlay, ValidityOverlay, SensorBoundsOverlay);
		meta_enum(GridMethod, int, FixedGrid, AdaptiveGrid);
		meta_enum(RayCountMethod, int, FixedCount, DynamicSensorSizeBased);
		meta_enum(RenderMethod, int, Direct, Tiled);
		meta_enum(RaytraceMethod, int, Analytical, PolynomialFullFit, PolynomialPartialFit);
		meta_enum(IntensityMethod, int, DynamicIntensity, FixedIntensity);

		// How to determine the ghost rendering attributes
		GhostAttribsMethod m_ghostAttribsMethod;
		ProjectionMethod m_projectionMethod;
		ShadingMethod m_shadingMethod;
		OverlayMethod m_overlayMethod;
		GridMethod m_gridMethod;
		RayCountMethod m_rayCountMethod;
		RenderMethod m_renderMethod;
		RaytraceMethod m_raytraceMethod;
		ParaxialBoundsSizeMethod m_paraxialBoundsSizeMethod;
		IntensityMethod m_intensityMethod;

		// Bounding method slacks to apply
		float m_paraxialBoundsSlack;

		// Whether invalid rays should be clipped or not
		bool m_clipRays;

		// Whether invalid pixels should be clipped or not
		bool m_clipPixels;

		// Whether wireframe mode should be enabled or not
		bool m_wireframe;

		// Number of rays used when adaptive ray counts are off
		int m_rayCount;

		// Dynamic ray count parameters
		float m_rayCountReductionScale;
		float m_rayCountReductionPower;

		// First and last ghosts to render
		int m_firstGhost;
		int m_numGhost;
		int m_numWavelengths;

		// Maximum angle of incidence
		float m_maxAngle;

		// Intensity scale factor
		float m_intensityScale;

		// Target intensity for dynamic scaling
		float m_targetIntensity;

		// Relative radius and iris mask clip factors
		float m_radiusClipping;
		float m_irisClipping;
		float m_intensityClipping;
		float m_sizeClipping;
		float m_refractionClipping;
		bool m_clipSensor;

		// Film stretch factor
		float m_filmStretch;

		// Resolution scale factor to apply to the intermediate buffer
		float m_resolutionScaling;

		// Gaussian blur sigma for the final rendering
		float m_blurSigma;

		// Size of a tile
		int m_tileSize;

		// Size of a coarse tile
		int m_coarseTileSize;

		// Max number of quads allowed
		int m_maxQuads;

		// Max elements in a tile
		int m_maxTileEntries;

		// Max elements in a coarse tile
		int m_maxCoarseTileEntries;

		// How many quad merge steps to perform
		int m_numQuadMergeSteps;

		// Quad merging edge threshold
		float m_quadMergeEdgeThreshold;

		// Size of a group for baking polynomial terms
		int m_bakePolynomialsGroupSize;

		// Size of a group for tracing rays
		int m_traceRaysGroupSize;

		// Size of a group for splatting quads
		int m_splatTrianglesGroupSize;

		// Whether statistics should be read back or not
		bool m_readBackStatistics;

		// Fixed angle and rotation parameters
		bool m_lockAngle = false;
		bool m_lockRotation = false;
		float m_fixedAngle = 0.0f;
		float m_fixedRotation = 0.0f;

		// ---- Private members
		std::vector<glm::vec2> m_blurKernelLinear;
		std::vector<glm::vec2> m_blurKernelDiscrete;
	};

	////////////////////////////////////////////////////////////////////////////////
	struct PrecomputeGhostsParameters
	{
		// Number of rays used
		int m_rayCount;

		// Angle of incidence sampling
		float m_maxAngle;
		float m_angleStep;

		// Number of light wavelengths
		int m_numChannels;

		// First and number of ghosts to process
		int m_firstGhost;
		int m_numGhost;

		// Clip factors
		float m_radiusClipping;
		float m_irisClipping;
		float m_intensityClipping;
		float m_refractionClipping;

		// How much the pupil should be enlarged
		float m_pupilExpansion;

		// Extra slack to add to the resulting bounds
		float m_slackAbsolute;
		float m_slackPercentage;
		float m_slackRays;

		// How many times to try to refine the search
		int m_refinementSteps;

		// Whether the sensor should be used for clipping or not
		bool m_clipSensor;

		// Whether we should terminate on the first invalid incidence angle or not
		bool m_terminateOnFirstInvalid;

		// Whether we want to compute the ghost attribs or not
		bool m_computeGhostAttribs;

		// Whether we want to save the ghost attribs or not
		bool m_saveGhostAttribs;

		// Whether we want to also save the resulting ghost geometry or not
		bool m_saveFullGhostGeometry;

		// Whether we want to also save the resulting filtered ghost geometry or not
		bool m_saveValidGhostGeometry;

		// Whether we want to also save the resulting filtered ghost geometry or not
		bool m_saveBoundedGhostGeometry;
	};

	////////////////////////////////////////////////////////////////////////////////
	struct PolynomialFitParameters
	{
		meta_enum(FitMethod, int, SimulatedAnnealing, PolynomialRegression);
		meta_enum(DenseFitLinearMethod, int, SkipLinear, BDCSVD, JacobiSVD, PartialPivLU, FullPivLU, ColPivHouseholderQr, FullPivHouseholderQr, LLT, LDLT, CompleteOrthogonalDecomposition);
		meta_enum(DenseFitNonlinearMethod, int, SkipNonLinear, LevMar, HybridNonlinearSolver, GradientDescent, NAG, RMSProp, Adam, Nadam);
		meta_enum(NonlinearFitFrequency, int, EveryIteration, AfterOptimization, Never);
		meta_enum(PartialErrorCollapseMethod, int, PCM_Min, PCM_Max, PCM_MAE, PCM_MSE, PCM_RMSE);
		meta_enum(DebugLevel, int, NoDebug, LightDebug, DetailedDebug);

		// Polynomial regression parameters
		struct GhostGeometryParameters
		{
			// Number of rays used
			int m_rayCount;

			// Number of incident angles
			int m_numAngles;

			// Maximum angle of incidence
			float m_maxAngle;

			// Number of incident angles
			int m_numRotations;

			// Maximum angle of incidence
			float m_maxRotation;

			// Number of light wavelengths
			int m_numChannels;

			// First and number of ghosts to process
			int m_firstGhost;
			int m_numGhost;

			// Per-ghost clip parameters
			struct GhostClipping
			{
				float m_sizeClipping;
				float m_intensityClipping;
				bool m_clipSensor;
			};

			// Per-ray clip parameters
			struct GeometryClipping
			{
				float m_radiusClipping;
				float m_irisClipping;
				float m_intensityClipping;
				float m_refractionClipping;
				bool m_clipSensor;
			};

			// Clip factors
			GhostClipping m_ghostClipping;
			GeometryClipping m_geometryClipping;
			GeometryClipping m_boundsClipping;

			// Amount of valid samples in the subsampled dataset
			float m_validSampleRatio;

			// Whether geometry of neighboring incident angles should be shared or not
			bool m_shareNeighboringGeometries;

			// Whether the fitting data should be smoothened or not
			bool m_smoothenData;

			// Whether zero-weighted variables should be included or not
			bool m_includeZeroWeightedDataset;
			bool m_includeZeroWeightedFit;
		} m_ghostGeometryParameters;

		// The number of sparse polynomial terms
		int m_numSparseTerms;

		// Polynomial fitting method
		FitMethod m_fitMethod;

		// Dense coefficient fitting method (linear)
		DenseFitLinearMethod m_denseFitLinearMethod;

		// Dense coefficient fitting method (nonlinear)
		DenseFitNonlinearMethod m_denseFitNonlinearMethod;

		// Nonlinear fitting frequency
		NonlinearFitFrequency m_nonlinearFitFrequency;

		// Partial fit error collapse method
		PartialErrorCollapseMethod m_partialErrorCollapseMethod;

		// Whether the full fit polynomials should be partialized
		bool m_bakeInvariants;

		// Whether per-ghost term counts should be used or not
		bool m_useDynamicTermCount;

		// Whether we should use groupshared memory or not
		bool m_useGroupSharedMemory;

		// How much extra space to add around rotated bounds
		float m_rotatedBoundsSlackAbsolute;
		float m_rotatedBoundsSlackPercentage;

		// The highest possible polynomial degree
		int m_maxTermDegree;

		// Whether individual entries should be evaluated in parallel or not
		bool m_evaluateEntriesInParallel;

		// Minimum and maximum extent of the coefficient values
		glm::vec2 m_coefficientLimits;

		// Per-variable parameters for the output variables
		struct OutputVariableParams
		{
			// Weighting for interior values
			float m_interiorWeight;

			// Weighting for boundary values
			float m_boundaryWeight;

			// Weighting for exterior values
			float m_exteriorWeight;

			// Penalty for boundary-violating values
			float m_boundaryPenalty;
		};

		// Exterior weight for the various terms
		std::unordered_map<std::string, OutputVariableParams> m_outputVariableParams;

		// Simulated annealing parameters
		struct AnnealingParams
		{
			// Number of iterations
			int m_numIterations;

			// Number of samples to use from the wholte dataset
			int m_numSamples;

			// Term swap annealing factor
			float m_swapAnnealingRate;

			// Expontential acceptance rate offset
			float m_acceptanceOffset;

			// Per-variable parameters for the output variables
			struct OutputVariableParams
			{

				// When to stop with the search early
				float m_stopErrorThreshold;
			};

			// Exterior weight for the various terms
			std::unordered_map<std::string, OutputVariableParams> m_outputVariableParams;
		} m_annealingParams;

		// Polynomial regression parameters
		struct PolynomialRegressionParams
		{
			// Number of samples to use from the wholte dataset
			int m_numSamples;

			// How deep the recursion should go
			int m_recursionDepth;

			// Whether we should always include the maximum term numbers or not
			bool m_fixedTermNumber;

			// When to start looking for simplifications
			int m_simplificationThreshold;

			// Per-variable parameters for the output variables
			struct OutputVariableParams
			{
				// Minimum error decrease for expanding
				float m_expansionErrorThreshold;

				// Minimum error decrease for expanding
				float m_expansionErrorDecrease;

				// Maximum error increase for simplying
				float m_simplificationErrorIncrease;

				// Minimum error decrease before increasing depth
				float m_stopErrorDecreaseThreshold;

				// When to stop with the search early
				float m_stopErrorThreshold;
			};

			// Exterior weight for the various terms
			std::unordered_map<std::string, OutputVariableParams> m_outputVariableParams;

		} m_polynomialRegressionParams;

		// Whether the computation should be debugged or not
		DebugLevel m_debugComputation;
	};

	////////////////////////////////////////////////////////////////////////////////
	struct PrecomputeGhostAttribs
	{
		glm::vec2 m_pupilMin;
		glm::vec2 m_pupilMax;
		glm::vec2 m_sensorMin;
		glm::vec2 m_sensorMax;

		float m_avgIntensity;

		size_t m_ghostID;
		size_t m_channelID;
		size_t m_angleID;
		float m_theta;
	};

	////////////////////////////////////////////////////////////////////////////////
	/** Represents a single monomial of a polinomial. */
	template<size_t N>
	struct MonomialN
	{
		enum { NUM_TERMS = N };

		float m_coefficient;
		int m_degrees[NUM_TERMS];

		MonomialN() :
			m_coefficient(0.0f)
		{
			std::fill_n(m_degrees, NUM_TERMS, 0);
		}

		template<size_t M>
		MonomialN(int degrees[M]) :
			MonomialN()
		{
			std::copy(degrees, degrees + std::min(N, M), m_degrees);
		}

		MonomialN(std::initializer_list<int> degrees) :
			MonomialN()
		{
			std::copy(degrees.begin(), degrees.end(), m_degrees);
		}

		MonomialN<N> increaseDegree(const size_t id, const int delta) const
		{
			MonomialN<N> result = *this;
			result.m_degrees[id] += delta;
			return result;
		}

		bool sameDegrees(MonomialN<N> const& other) const
		{
			for (size_t i = 0; i < NUM_TERMS; ++i)
				if (m_degrees[i] != other.m_degrees[i])
					return false;
			return true;
		}

		bool isAnyDegreeNegative() const
		{
			for (size_t i = 0; i < NUM_TERMS; ++i)
				if (m_degrees[i] < 0)
					return true;
			return false;
		}

		int maxDegree() const
		{
			return *std::max_element(m_degrees, m_degrees + NUM_TERMS);
		}
	};

	// Represents a single polynomial for a single variable
	template<size_t N>
	struct PolynomialN : public std::vector<MonomialN<N>>
	{
		using std::vector<MonomialN<N>>::vector;

		float minAbsCoefficient() const
		{
			float result = FLT_MAX;
			for (size_t i = 0; i < this->size(); ++i)
				result = std::min(result, std::abs(this->at(i).m_coefficient));
			return result;
		}

		float maxAbsCoefficient() const
		{
			float result = 0.0f;
			for (size_t i = 0; i < this->size(); ++i)
				result = std::max(result, std::abs(this->at(i).m_coefficient));
			return result;
		}

		int maxDegree() const
		{
			int result = 0;
			for (size_t i = 0; i < this->size(); ++i)
				result = std::max(result, std::abs(this->at(i).maxDegree()));
			return result;
		}
	};

	////////////////////////////////////////////////////////////////////////////////
	template<size_t N>
	std::ostream& operator<<(std::ostream& ostream, MonomialN<N> const& monomial)
	{
		ostream << "{ ";
		ostream << monomial.m_coefficient << ", ";
		ostream << "( ";
		for (size_t i = 0; i < N; ++i)
		{
			if (i > 0) ostream << ", ";
			ostream << monomial.m_degrees[i];
		}
		ostream << " )";
		ostream << " }";
		return ostream;
	}

	////////////////////////////////////////////////////////////////////////////////
	template<size_t N>
	std::ostream& operator<<(std::ostream& ostream, PolynomialN<N> const& polynomial)
	{
		ostream << "Polynomial{ ";
		ostream << polynomial.size() << ": ";
		for (size_t i = 0; i < polynomial.size(); ++i)
		{
			if (i > 0) ostream << ", ";
			ostream << polynomial[i];
		}
		ostream << " }";
		return ostream;
	}

	////////////////////////////////////////////////////////////////////////////////
	/** A directional light component. */
	struct TiledLensFlareComponent
	{
		// Common lens flare parameters
		CommonParameters m_commonParameters;

		// Ghost rendering parameters
		RenderGhostsParameters m_renderGhostsParameters;

		// Ghost attribute pre-computation parameters
		PrecomputeGhostsParameters m_precomputeGhostsParameters;

		// Fit settings for polynomial optics
		PolynomialFitParameters m_polynomialFitParameters;

		// Attributes for the camera used
		PhysicalCamera::PhysicalCameraAttributes m_camera;

		// ---- Private members

		// All the physical camera patents in use.
		std::unordered_map<std::string, PhysicalCamera::PhysicalCameraAttributes> m_cameraPresets;

		// Resulting vector
		std::vector<PhysicalCamera::GhostIndices> m_ghostIndices;

		// Ghost transfer matrices for matrix lens flare
		std::vector<std::vector<std::vector<glm::mat2>>> m_transferMatrices;

		// Ghost attributes
		std::unordered_map<std::string, PrecomputeGhostAttribs> m_precomputedGhostAttribs;

		// Number of polynomial terms for full fit
		std::vector<size_t> m_numPolynomialTermsFullFit;

		// Number of polynomial terms for full fit
		std::vector<size_t> m_numPolynomialTermsPartialFit;
	};

	////////////////////////////////////////////////////////////////////////////////
	void initObject(Scene::Scene& scene, Scene::Object& object);

	////////////////////////////////////////////////////////////////////////////////
	void releaseObject(Scene::Scene& scene, Scene::Object& object);

	////////////////////////////////////////////////////////////////////////////////
	void updateObject(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* object);

	////////////////////////////////////////////////////////////////////////////////
	void generateGui(Scene::Scene& scene, Scene::Object* guiSettings, Scene::Object* object);

	////////////////////////////////////////////////////////////////////////////////
	void renderObjectOpenGL(Scene::Scene& scene, Scene::Object* simulationSettings, Scene::Object* renderSettings, Scene::Object* camera, std::string const& functionName, Scene::Object* object);

	////////////////////////////////////////////////////////////////////////////////
	void demoSetup(Scene::Scene& scene);
}

////////////////////////////////////////////////////////////////////////////////
// Component declaration
DECLARE_COMPONENT(TILED_LENS_FLARE, TiledLensFlareComponent, TiledLensFlare::TiledLensFlareComponent)
DECLARE_OBJECT(TILED_LENS_FLARE, COMPONENT_ID_TILED_LENS_FLARE, COMPONENT_ID_EDITOR_SETTINGS)