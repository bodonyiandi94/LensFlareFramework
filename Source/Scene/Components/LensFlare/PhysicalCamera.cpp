////////////////////////////////////////////////////////////////////////////////
//  Headers
////////////////////////////////////////////////////////////////////////////////

#include "PCH.h"
#include "PhysicalCamera.h"

namespace PhysicalCamera
{
	////////////////////////////////////////////////////////////////////////////////
	void initApertureTextures(Scene::Scene& scene)
	{
		// Load the needed textures.
		Asset::loadTexture(scene, "Textures/FX/PhysicalLensFlare/apertureDist.bmp", "Textures/FX/PhysicalLensFlare/apertureDist.bmp");
		Asset::loadTexture(scene, "Textures/FX/PhysicalLensFlare/apertureFFT.bmp", "Textures/FX/PhysicalLensFlare/apertureFFT.bmp");
		Asset::loadTexture(scene, "Textures/FX/PhysicalLensFlare/apertureFFT2.bmp", "Textures/FX/PhysicalLensFlare/apertureFFT2.bmp");
		Asset::loadTexture(scene, "Textures/FX/PhysicalLensFlare/starburst.bmp", "Textures/FX/PhysicalLensFlare/starburst.bmp");

		// Generate custom textures.

		Scene::renderTexture(scene, "Textures/FX/PhysicalLensFlare/apertureDistCircle", GL_TEXTURE_2D, 512, 512, 1, GL_R32F, GL_RED, GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, "LensFlare/Aperture/circle_dist");
		Scene::renderTexture(scene, "Textures/FX/PhysicalLensFlare/apertureDistOctagon", GL_TEXTURE_2D, 512, 512, 1, GL_R32F, GL_RED, GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, "LensFlare/Aperture/octagon_dist");
	}

	////////////////////////////////////////////////////////////////////////////////
	void initApertureShaders(Scene::Scene& scene)
	{
		Asset::loadShader(scene, "LensFlare/Aperture", "circle");
		Asset::loadShader(scene, "LensFlare/Aperture", "octagon");
		Asset::loadShader(scene, "LensFlare/Aperture", "circle_dist");
		Asset::loadShader(scene, "LensFlare/Aperture", "octagon_dist");
		Asset::loadShader(scene, "LensFlare/Aperture", "octagon_smooth");
	}

	////////////////////////////////////////////////////////////////////////////////
	PhysicalCamera::PhysicalCameraAttributes loadCameraPatent(Scene::Scene& scene, const std::string& patentName)
	{
		PhysicalCamera::PhysicalCameraAttributes result;

		Debug::log_debug() << "Loading lens patent: " << patentName << Debug::end;

		// Generate the full file path
		std::string fullFileName = (EnginePaths::assetsFolder() / "Lenses" / patentName).string();

		tinyxml2::XMLDocument doc;
		doc.LoadFile(fullFileName.c_str());

		auto opticalSystem = doc.FirstChildElement("opticalSystem");
		result.m_name = opticalSystem->FirstChildElement("name")->GetText();
		result.m_fNumber = opticalSystem->FirstChildElement("fnumber")->ToElement()->FloatText();
		result.m_focalDistance = opticalSystem->FirstChildElement("effectiveFocalLength")->ToElement()->FloatText();
		result.m_fieldOfView = opticalSystem->FirstChildElement("fieldOfView")->ToElement()->FloatText();
		result.m_filmSize.x = opticalSystem->FirstChildElement("filmWidth")->ToElement()->FloatText();
		result.m_filmSize.y = opticalSystem->FirstChildElement("filmHeight")->ToElement()->FloatText();
		result.m_coatingRefraction = 1.38f;
		result.m_coatingWavelength = 620.0f;
		result.m_minWavelength = 475.0f;
		result.m_maxWavelength = 650.0f;
		result.m_maxNumWavelengths = 3;
		result.m_apertureTexture = "Textures/FX/PhysicalLensFlare/apertureDist.bmp";
		result.m_apertureFFT = "Textures/FX/PhysicalLensFlare/apertureFFT2.bmp";
		if (tinyxml2::XMLElement* heightMultiplierNode = opticalSystem->FirstChildElement("heightMultiplier"); heightMultiplierNode)
			result.m_heightMultiplier = heightMultiplierNode->ToElement()->FloatText();
		else
			result.m_heightMultiplier = 1.0f;

		auto elements = opticalSystem->FirstChildElement("elements");
		for (tinyxml2::XMLNode* element = elements->FirstChild(); element != nullptr; element = element->NextSibling())
		{
			PhysicalCameraAttributes::LensComponent lens;

			std::string elementType = element->FirstChildElement("type")->GetText();
			if (elementType == "lensSpherical")
			{
				lens.m_height = element->FirstChildElement("height")->ToElement()->FloatText() * result.m_heightMultiplier;
				lens.m_thickness = element->FirstChildElement("thickness")->ToElement()->FloatText();
				lens.m_radius = element->FirstChildElement("radius")->ToElement()->FloatText();
				lens.m_refraction = element->FirstChildElement("refractiveIndex")->ToElement()->FloatText();
				lens.m_abbeNumber = element->FirstChildElement("abbeNumber")->ToElement()->FloatText();
				lens.m_coatingWavelength = element->FirstChildElement("coatingLambda")->ToElement()->FloatText();
				lens.m_coatingRefraction = element->FirstChildElement("coatingIor")->ToElement()->FloatText();
			}
			else if (elementType == "apertureStop")
			{
				lens.m_height = element->FirstChildElement("height")->ToElement()->FloatText() * 0.5f;;
				lens.m_thickness = element->FirstChildElement("thickness")->ToElement()->FloatText();
				//lens.m_radius = element->FirstChildElement("radius")->ToElement()->FloatText();
				lens.m_radius = FLT_MAX;
				lens.m_refraction = element->FirstChildElement("refractiveIndex")->ToElement()->FloatText();
				lens.m_abbeNumber = element->FirstChildElement("abbeNumber")->ToElement()->FloatText();
				lens.m_coatingWavelength = element->FirstChildElement("coatingLambda")->ToElement()->FloatText();
				lens.m_coatingRefraction = element->FirstChildElement("coatingIor")->ToElement()->FloatText();
				if (tinyxml2::XMLElement* maskNode = element->FirstChildElement("mask"); maskNode)
					result.m_apertureTexture = "Textures/FX/PhysicalLensFlare/" + std::string(maskNode->GetText());
				if (tinyxml2::XMLElement* maskFtNode = element->FirstChildElement("maskFT"); maskFtNode)
					result.m_apertureFFT = "Textures/FX/PhysicalLensFlare/" + std::string(maskFtNode->GetText());
			}
			else if (elementType == "sensor")
			{
				lens.m_height = element->FirstChildElement("height")->ToElement()->FloatText();
				lens.m_thickness = element->FirstChildElement("thickness")->ToElement()->FloatText();
				lens.m_radius = element->FirstChildElement("radius")->ToElement()->FloatText();
				lens.m_refraction = element->FirstChildElement("refractiveIndex")->ToElement()->FloatText();
				lens.m_abbeNumber = element->FirstChildElement("abbeNumber")->ToElement()->FloatText();
				lens.m_coatingWavelength = element->FirstChildElement("coatingLambda")->ToElement()->FloatText();
				lens.m_coatingRefraction = element->FirstChildElement("coatingIor")->ToElement()->FloatText();
			}

			result.m_lenses.push_back(lens);
		}

		result.m_apertureTexture = "Textures/FX/PhysicalLensFlare/apertureDistOctagon";

		// Fill the front dummy element
		PhysicalCameraAttributes::LensComponent front;
		front.m_thickness = 0.0f;
		front.m_radius = 0.0f;
		front.m_refraction = 1.0f;
		front.m_abbeNumber = 89.3f;
		front.m_height = result.m_lenses[1].m_height;
		result.m_lenses.insert(result.m_lenses.begin(), front);
		PhysicalCameraAttributes::LensComponent& back = result.m_lenses.back();
		//back.m_height = result.m_filmSize.x / 2.0f;
		back.m_height = glm::max(result.m_filmSize.x, result.m_filmSize.y);
		//back.m_height = 1000.0f;

		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	bool saveCameraPatent(CameraPresets& presets, std::string const& patentName)
	{
		// Make sure it actually exists
		if (presets.find(patentName) == presets.end())
			return false;

		Debug::log_debug() << "Saving lens patent: " << patentName << Debug::end;

		// Extract the optical system
		auto const& opticalSystem = presets[patentName];

		// Output file
		std::string fileName = patentName + ".xml";
		std::string fullFileName = (EnginePaths::assetsFolder() / "Lenses" / fileName).string();
		std::ofstream xml(fullFileName);

		xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;
		xml << "<opticalSystem>" << std::endl;;

		xml << std::string(4, ' ') << "<name>" << patentName << "</name>" << std::endl;
		xml << std::string(4, ' ') << "<fnumber>" << opticalSystem.m_fNumber << "</fnumber>" << std::endl;
		xml << std::string(4, ' ') << "<effectiveFocalLength>" << opticalSystem.m_focalDistance << "</effectiveFocalLength>" << std::endl;
		xml << std::string(4, ' ') << "<fieldOfView>" << opticalSystem.m_fieldOfView << "</fieldOfView>" << std::endl;
		xml << std::string(4, ' ') << "<filmWidth>" << opticalSystem.m_filmSize.x << "</filmWidth>" << std::endl;
		xml << std::string(4, ' ') << "<filmHeight>" << opticalSystem.m_filmSize.y << "</filmHeight>" << std::endl;
		xml << std::string(4, ' ') << "<elements>" << std::endl;

		for (size_t i = 1; i < opticalSystem.m_lenses.size(); ++i)
		{
			const auto& lens = opticalSystem.m_lenses[i];

			std::string type;
			if (lens.m_radius == FLT_MAX)
			{
				type = "apertureStop";
			}
			else if (i == opticalSystem.m_lenses.size() - 1)
			{
				type = "sensor";
			}
			else
			{
				type = "lensSpherical";
			}

			xml << std::string(8, ' ') << "<element>" << std::endl;

			xml << std::string(12, ' ') << "<type>" << type << "</type>" << std::endl;
			xml << std::string(12, ' ') << "<height>" << std::to_string(lens.m_height) << "</height>" << std::endl;
			xml << std::string(12, ' ') << "<thickness>" << std::to_string(lens.m_thickness) << "</thickness>" << std::endl;
			xml << std::string(12, ' ') << "<radius>" << (lens.m_radius == FLT_MAX ? "0.0" : std::to_string(lens.m_radius)) << "</radius>" << std::endl;
			xml << std::string(12, ' ') << "<refractiveIndex>" << std::to_string(lens.m_refraction) << "</refractiveIndex>" << std::endl;
			xml << std::string(12, ' ') << "<abbeNumber>" << std::to_string(lens.m_abbeNumber) << "</abbeNumber>" << std::endl;
			xml << std::string(12, ' ') << "<coatingLambda>" << std::to_string(lens.m_coatingWavelength) << "</coatingLambda>" << std::endl;
			xml << std::string(12, ' ') << "<coatingIor>" << std::to_string(lens.m_coatingRefraction) << "</coatingIor>" << std::endl;

			xml << std::string(8, ' ') << "</element>" << std::endl;
		}

		xml << std::string(4, ' ') << "</elements>" << std::endl;
		xml << "</opticalSystem>" << std::endl;

		Debug::log_debug() << "Successfully saved lens patent: " << patentName << Debug::end;

		return true;
	}


	////////////////////////////////////////////////////////////////////////////////
	CameraPresets initCameraPatents(Scene::Scene& scene)
	{
		CameraPresets result;

		// Get the root filesystem path
		std::filesystem::path root = EnginePaths::assetsFolder() / "Lenses";

		// Make sure the path exists
		if (std::filesystem::exists(root) == false) return result;

		// Go through each lens patent
		for (auto const& lensPatent : std::filesystem::directory_iterator(root))
		{
			// Make sure its a lens patent xml
			if (lensPatent.path().extension() != ".xml")
				continue;

			// Load the camera patent
			std::string patentName = lensPatent.path().stem().string();
			result[patentName] = loadCameraPatent(scene, lensPatent.path().filename().string());
		}

		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	std::array<bool, 3> generateGui(Scene::Scene& scene, Scene::Object* guiSettings, Scene::Object* owner, CameraPresets& presets, PhysicalCameraAttributes& camera)
	{
		bool descriptionChanged = false, systemChanged = false, lensesChanged = false;

		descriptionChanged = ImGui::Combo("Objective", camera.m_name, presets);

		if (descriptionChanged) camera = presets[camera.m_name];

		systemChanged = systemChanged || descriptionChanged;
		systemChanged |= ImGui::SliderFloat("f-number", &camera.m_fNumber, 0, 64.0f);
		systemChanged |= ImGui::SliderFloat("Focal Length", &camera.m_focalDistance, 0, 1000.0f);
		systemChanged |= ImGui::SliderFloat("Field of View", &camera.m_fieldOfView, 0, 180.0f);
		const float oldHeightMultiplier = camera.m_heightMultiplier;
		if (ImGui::SliderFloat("Height Multiplier", &camera.m_heightMultiplier, 0.0f, 2.0f))
		{
			if (camera.m_heightMultiplier <= 0.0f)
				camera.m_heightMultiplier = oldHeightMultiplier;
			else
				for (int i = 0; i < camera.m_lenses.size() - 1; ++i)
					camera.m_lenses[i].m_height = camera.m_lenses[i].m_height * (camera.m_heightMultiplier / oldHeightMultiplier);

			systemChanged = true;
		}
		systemChanged |= ImGui::DragFloat2("Film Size", glm::value_ptr(camera.m_filmSize), 0, 64.0f);
		systemChanged |= ImGui::SliderFloat("Min Wavelength", &camera.m_minWavelength, 400.0f, 500.0f);
		systemChanged |= ImGui::SliderFloat("Max Wavelength", &camera.m_maxWavelength, 600.0f, 800.0f);
		systemChanged |= ImGui::SliderInt("Max Num Wavelengths", &camera.m_maxNumWavelengths, 1, 10);
		systemChanged |= ImGui::SliderFloat("Coating IoR", &camera.m_coatingRefraction, 1.0f, 2.0f);
		systemChanged |= ImGui::SliderFloat("Coating Wavelength", &camera.m_coatingWavelength, 200.0f, 1200.0f);
		systemChanged |= GuiSettings::generateTextureCombo(scene, "Aperture Texture", camera.m_apertureTexture);
		systemChanged |= GuiSettings::generateTextureCombo(scene, "Aperture Texture FFT", camera.m_apertureFFT);

		ImGui::TextDisabled("Properties");
		ImGui::Text("Entrance Pupil: %f", getCameraEffectiveApertureHeight(camera));
		ImGui::Text("Physical Pupil: %f", getCameraPhysicalApertureHeight(camera));
		ImGui::Text("Sensor Distance: %f", getSensorDistance(camera));

		if (ImGui::TreeNodeEx("Lenses"))
		{
			for (int i = 0; i < camera.m_lenses.size(); ++i)
			{
				if (ImGui::TreeNodeEx(&camera.m_lenses[i], ImGuiTreeNodeFlags_DefaultOpen, "Lens #%d", i + 1))
				{
					ImGui::PushID(i);

					auto& lens = camera.m_lenses[i];
					if (lens.m_radius != FLT_MAX) // Don't allow the aperture curvature and height to be changed
					{
						lensesChanged = ImGui::SliderFloat("Curvature", &lens.m_radius, -100.0f, 100.0f) || lensesChanged;
						lensesChanged = ImGui::SliderFloat("Height", &lens.m_height, 0.0f, 200.0f) || lensesChanged;
					}
					lensesChanged = ImGui::SliderFloat("Thickness", &lens.m_thickness, 0.0f, 200.0f) || lensesChanged;
					lensesChanged = ImGui::SliderFloat("Refraction", &lens.m_refraction, 1.0f, 2.0f) || lensesChanged;
					lensesChanged = ImGui::SliderFloat("Abbe Number", &lens.m_abbeNumber, 0.0f, 160.0f) || lensesChanged;
					lensesChanged = ImGui::SliderFloat("Coating Wavelength", &lens.m_coatingWavelength, 200.0f, 1200.0f) || lensesChanged;

					ImGui::PopID();
					ImGui::TreePop();
				}
			}
			ImGui::TreePop();
		}
		
		return { descriptionChanged, systemChanged, lensesChanged };
	}

	////////////////////////////////////////////////////////////////////////////////
	glm::mat2 translationMatrix(const float dist)
	{
		glm::mat2 result = glm::mat2(1.0f, 0.0f, 0.0f, 1.0f);

		result[1][0] = dist;

		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	glm::mat2 reflectionMatrix(const float radius)
	{
		glm::mat2 result = glm::mat2(1.0f, 0.0f, 0.0f, 1.0f);

		if (glm::abs(radius) > 0.0001f)
		{
			result[0][1] = 2.0f / radius;
		}

		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	glm::mat2 refractionMatrix(const float n1, const float n2, const float radius)
	{
		glm::mat2 result = glm::mat2(1.0f, 0.0f, 0.0f, 1.0f);

		result[1][1] = n1 / n2;
		if (glm::abs(radius) > 1e-4f)
		{
			result[0][1] = (n1 - n2) / (n2 * radius);
		}

		return result;
	}

	////////////////////////////////////////////////////////////////////////////////
	// Based on: https://wiki.luxcorerender.org/Glass_Material_IOR_and_Dispersion
	float cauchyDispersion(const float lambda, const float nd, const float v)
	{
		// Compute the coefficients
		const float B = ((nd - 1.0f) / v) * 0.52345f;
		const float A = nd - (B * 2.897f);

		// Return the result
		const float lambdaMuM = lambda * 1e-3f;
		return A + B / (lambdaMuM * lambdaMuM);
	}

	////////////////////////////////////////////////////////////////////////////////
	float cauchyDispersion(PhysicalCameraAttributes const& camera, const size_t elementId, const float lambda)
	{
		return cauchyDispersion(lambda, camera.m_lenses[elementId].m_refraction, camera.m_lenses[elementId].m_abbeNumber);
	}

	////////////////////////////////////////////////////////////////////////////////
	float cauchyDispersion(PhysicalCameraAttributes const& camera, const size_t numChannels, const size_t elementId, const size_t channelID)
	{
		return cauchyDispersion(camera.m_wavelengths[numChannels][channelID], camera.m_lenses[elementId].m_refraction, camera.m_lenses[elementId].m_abbeNumber);
	}

	////////////////////////////////////////////////////////////////////////////////
	float fresnelWithAntiReflectionCoating(const float lambda, const float coatingLambda, const float theta0, const float n0, const float n1In, const float n2)
	{
		// Compute the optimial coating refractive index
		const float n1 = glm::max(glm::sqrt(n0 * n2), n1In);

		// Compute the optimal coating thickness
		float d = coatingLambda / 4.0f / n1;

		// Apply Snell's law to get the other angles
		float theta1 = glm::asin(glm::sin(theta0) * n0 / n1);
		float theta2 = glm::asin(glm::sin(theta0) * n0 / n2);

		float cos0 = glm::cos(theta0);
		float cos1 = glm::cos(theta1);
		float cos2 = glm::cos(theta2);

		float beta = (glm::two_pi<float>()) / lambda * n1 * d * cos1;

		// Compute the fresnel terms for the first and second interfaces for 
		// both s and p polarized light
		float r12p = (n1 * cos0 - n0 * cos1) / (n1 * cos0 + n0 * cos1);
		float r12p2 = r12p * r12p;

		float r23p = (n2 * cos1 - n1 * cos2) / (n2 * cos1 + n1 * cos2);
		float r23p2 = r23p * r23p;

		float rp = (r12p2 + r23p2 + 2.0f * r12p * r23p * glm::cos(2.0f * beta)) /
			(1.0f + r12p2 * r23p2 + 2.0f * r12p * r23p * glm::cos(2.0f * beta));

		float r12s = (n0 * cos0 - n1 * cos1) / (n0 * cos0 + n1 * cos1);
		float r12s2 = r12s * r12s;

		float r23s = (n1 * cos1 - n2 * cos2) / (n1 * cos1 + n2 * cos2);
		float r23s2 = r23s * r23s;

		float rs = (r12s2 + r23s2 + 2.0f * r12s * r23s * glm::cos(2.0f * beta)) /
			(1.0f + r12s2 * r23s2 + 2.0f * r12s * r23s * glm::cos(2.0f * beta));

		return (rs + rp) * 0.5f;
	}

	////////////////////////////////////////////////////////////////////////////////
	int getApertureIndex(PhysicalCameraAttributes const& camera)
	{
		for (int i = 1; i < camera.m_lenses.size(); ++i)
			if (camera.m_lenses[i].m_radius == FLT_MAX)
				return i;
		return -1;
	}

	////////////////////////////////////////////////////////////////////////////////
	#define GHOST_MATRIX_ID(ID) ((ID) < apertureIndex ? 0 : 1)
	std::pair<glm::mat2, glm::mat2> getSystemMatrices(PhysicalCameraAttributes const& camera)
	{
		// Find the aperture index
		int apertureIndex = getApertureIndex(camera);

		// Resulting matrices
		glm::mat2 result[2];

		// New keep on refracting and translating forward until we reach the sensor plane
		for (int i = 1; i < camera.m_lenses.size() - 1; ++i)
		{
			glm::mat2 Ri = PhysicalCamera::refractionMatrix(camera.m_lenses[i - 1].m_refraction, camera.m_lenses[i].m_refraction, camera.m_lenses[i].m_radius);
			glm::mat2 Ti = PhysicalCamera::translationMatrix(camera.m_lenses[i].m_thickness);

			result[GHOST_MATRIX_ID(i)] = Ti * Ri * result[GHOST_MATRIX_ID(i)];
		}

		// Return the result
		return{ result[0], result[1] };
	}

	////////////////////////////////////////////////////////////////////////////////
	// Return the computed effective aperture height
	float getEffectiveApertureHeight(const float efl, const float fNumber)
	{
		// fNumber = efl / D
		return (efl / fNumber) / 2.0f;
	}

	////////////////////////////////////////////////////////////////////////////////
	// Return the computed effective aperture height
	float getCameraEffectiveApertureHeight(PhysicalCameraAttributes const& camera)
	{
		return getEffectiveApertureHeight(camera.m_focalDistance, camera.m_fNumber);
	}

	////////////////////////////////////////////////////////////////////////////////
	// Return the computed effective aperture height
	float getPhysicalApertureHeight(PhysicalCameraAttributes const& camera, const float efl, const float fNumber, const float lambda)
	{
		// Compute the effective aperture height
		const float effectiveApertureHeight = getEffectiveApertureHeight(efl, fNumber);

		// The transfer matrix that is accumulated
		glm::mat2 abcd = glm::mat2(1.0f, 0.0f, 0.0f, 1.0f);

		for (int i = 1; i < camera.m_lenses.size(); ++i)
		{
			// Trace a ray to compute the aperture height
			if (camera.m_lenses[i].m_radius == FLT_MAX)
				return effectiveApertureHeight * abcd[0][0];

			const glm::mat2 Ri = PhysicalCamera::refractionMatrix(
				PhysicalCamera::cauchyDispersion(camera, i - 1, lambda),
				PhysicalCamera::cauchyDispersion(camera, 1, lambda),
				camera.m_lenses[i].m_radius);
			const glm::mat2 Ti = PhysicalCamera::translationMatrix(camera.m_lenses[i].m_thickness);
			abcd = Ti * Ri * abcd;
		}

		// Return the result
		return -1.0f;
	}

	////////////////////////////////////////////////////////////////////////////////
	// Return the computed effective aperture height
	float getCameraPhysicalApertureHeight(PhysicalCameraAttributes const& camera, const float lambda)
	{
		return getPhysicalApertureHeight(camera, camera.m_focalDistance, camera.m_fNumber, lambda);
	}

	////////////////////////////////////////////////////////////////////////////////
	int getApertureId(PhysicalCameraAttributes const& camera)
	{
		for (int i = 0; i < camera.m_lenses.size(); ++i)
			if (camera.m_lenses[i].m_radius == FLT_MAX)
				return i;
		return -1;
	}

	////////////////////////////////////////////////////////////////////////////////
	// Calculate the entrance plane's distance from the sensor plane along the optical axis
	float getSensorDistance(PhysicalCameraAttributes const& camera)
	{
		float sensorDistance = 0.0f;
		for (int i = 0; i < camera.m_lenses.size(); ++i)
			sensorDistance += camera.m_lenses[i].m_thickness;
		return sensorDistance;
	}

	////////////////////////////////////////////////////////////////////////////////
	float getCoatingThickness(const float coatingLambda, const float n0, const float n1, const float n2)
	{
		return coatingLambda / 4.0f / glm::max(glm::sqrt(n0 * n2), n1);
	}

	////////////////////////////////////////////////////////////////////////////////
	float getCoatingThickness(PhysicalCameraAttributes const& camera, const size_t lensID, const float lambda)
	{
		return getCoatingThickness(
			camera.m_lenses[lensID].m_coatingWavelength,
			cauchyDispersion(camera, (lensID > 0 ? lensID - 1 : 0), lambda),
			camera.m_lenses[lensID].m_coatingRefraction,
			cauchyDispersion(camera, lensID, lambda));
	}

	////////////////////////////////////////////////////////////////////////////////
	float getCoatingThickness(PhysicalCameraAttributes const& camera, const size_t numChannels, const size_t lensID, const size_t channelID)
	{
		return getCoatingThickness(camera, lensID, camera.m_wavelengths[numChannels][channelID]);
	}

	////////////////////////////////////////////////////////////////////////////////
	std::vector<GhostIndices> enumerateGhosts(PhysicalCameraAttributes const& camera, const int reflections, const bool apertureCrossing)
	{
		// Find the aperture index
		const int apertureIndex = getApertureIndex(camera);

		// Resulting vector
		std::vector<GhostIndices> ghostIndices;

		// Enumerate the ghosts that are generated before the aperture
		for (int i = 1; i < apertureIndex; ++i)
		for (int j = i + 1; j < apertureIndex; ++j)
			ghostIndices.push_back({ i, j });

		// Enumerate the ghosts that are generated after the aperture
		for (int i = apertureIndex + 1; i < camera.m_lenses.size() - 1; ++i)
		for (int j = i + 1; j < camera.m_lenses.size() - 1; ++j)
			ghostIndices.push_back({ i, j });

		// Return the result
		return ghostIndices;
	}

	////////////////////////////////////////////////////////////////////////////////
	size_t getNumGhosts(PhysicalCameraAttributes const& camera, const int reflections, const bool apertureCrossing)
	{
		return enumerateGhosts(camera, reflections, apertureCrossing).size();
	}

	////////////////////////////////////////////////////////////////////////////////
	void computeWavelengths(PhysicalCameraAttributes& camera)
	{
		camera.m_wavelengths.resize(camera.m_maxNumWavelengths + 1);
		for (int i = 1; i <= camera.m_maxNumWavelengths; ++i)
		{
			camera.m_wavelengths[i].resize(i);
			const float lambdaStep = (camera.m_maxWavelength - camera.m_minWavelength) / std::max(i - 1, 1);
			for (size_t channelId = 0; channelId < i; ++channelId)
				camera.m_wavelengths[i][channelId] = camera.m_minWavelength + channelId * lambdaStep;
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void propagateCoatingInformation(PhysicalCameraAttributes& camera)
	{
		for (int i = 0; i < camera.m_lenses.size(); ++i)
		{
			camera.m_lenses[i].m_coatingRefraction = camera.m_coatingRefraction;
			camera.m_lenses[i].m_coatingWavelength = camera.m_coatingWavelength;
		}
	}
}