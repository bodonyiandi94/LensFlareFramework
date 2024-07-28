#pragma once

////////////////////////////////////////////////////////////////////////////////
//  Headers
////////////////////////////////////////////////////////////////////////////////

#include "PCH.h"

////////////////////////////////////////////////////////////////////////////////
/// RELEVANT ENGINE PATHS
////////////////////////////////////////////////////////////////////////////////
namespace EnginePaths
{
	////////////////////////////////////////////////////////////////////////////////
	std::filesystem::path systemFontsFolder();

	////////////////////////////////////////////////////////////////////////////////
    std::filesystem::path rootFolder();

	////////////////////////////////////////////////////////////////////////////////
	std::filesystem::path executableFolder();

	////////////////////////////////////////////////////////////////////////////////
    std::filesystem::path assetsFolder();

	////////////////////////////////////////////////////////////////////////////////
	std::filesystem::path generatedFilesFolder();

	////////////////////////////////////////////////////////////////////////////////
	std::filesystem::path configFilesFolder();

	////////////////////////////////////////////////////////////////////////////////
	std::filesystem::path logFilesFolder();

	////////////////////////////////////////////////////////////////////////////////
	std::filesystem::path openGlShadersFolder();

	////////////////////////////////////////////////////////////////////////////////
	std::filesystem::path generatedOpenGlShadersFolder();

	////////////////////////////////////////////////////////////////////////////////
	bool makeDirectoryStructure(std::filesystem::path path, bool isFile = false);

	////////////////////////////////////////////////////////////////////////////////
	/** Unique file name generator. */
	std::string generateUniqueFilename(std::string prefix = "", std::string extension = "");

	////////////////////////////////////////////////////////////////////////////////
	/** Unique file path generator. */
	std::string generateUniqueFilepath(std::string prefix = "", std::string extension = "");
}