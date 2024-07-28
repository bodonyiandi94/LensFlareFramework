#include "PCH.h"
#include "Config.h"
#include "Debug.h"

namespace Config
{
	////////////////////////////////////////////////////////////////////////////////
	// Config attrib values
	struct AttributeDescriptor
	{
		// Name of the attribute
		std::string m_name;

		// Which category it belongs to
		std::string m_category;

		// What the variable does
		std::string m_description;

		// Name of the expected value
		std::string m_valueName;

		// Default value for the attribute
		std::vector<std::string> m_default;

		// Values available
		std::unordered_map<std::string, std::string> m_choices;

		// Regex pattern for the iterator
		std::string m_regex;
	};

	////////////////////////////////////////////////////////////////////////////////
	std::string configRegex(std::string const& value, int count = 1, 
		std::string const& sep = "", std::string const& open = "", std::string const& close = "")
	{
		std::stringstream ss;
		ss << "[[:space:]]+";
		ss << open;
		for (int i = 0; i < count; ++i)
		{
			if (i > 0) ss << sep;
			ss << "(" << value << ")";
		}
		ss << close;
		return ss.str();
	}

	////////////////////////////////////////////////////////////////////////////////
	std::string configRegexBool()
	{
		return configRegex("[[:digit:]]+");
	}
	std::string configRegexInt(int numComponents = 1)
	{
		return configRegex("[[:digit:]]+", numComponents, 
			"[[:space:]]*[\\.,;:xX]?[[:space:]]*", "[\\(\\[\\{]?", "[\\)\\]\\}]?");
	}
	std::string configRegexString()
	{
		return configRegex("[[:alnum:]_]+");
	}

	////////////////////////////////////////////////////////////////////////////////
	/** Map of all the config value tokens */
	std::vector<AttributeDescriptor> s_descriptors =
	{
		////////////////////////////////////////////////////////////////////////////////
		// Application 
		////////////////////////////////////////////////////////////////////////////////

		AttributeDescriptor
		{
			"config", "Application",
			"Current build config.",
			"CONFIG", { CONFIG }, {},
			configRegexString()
		},
		AttributeDescriptor
		{
			"info", "Application",
			"Whether to stop after printing out the startup info.",
			"", { "Off" }, {},
			configRegexString()
		},

		// @CONSOLE_VAR(Max Threads, -threads, 16, 12, 10, 8, 6, 4, 2, 1)
		AttributeDescriptor
		{ 
			"threads", "Application", 
			"Maximum allowed number of threads.", 
			"N", { "16" }, {},
			configRegexInt()
		},

		// @CONSOLE_VAR(Background Init, -background_init, 1, 0)
		AttributeDescriptor
		{ 
			"background_init", "Application", 
			"Whether to load assets in the background or not.",
			"0|1", { "1" }, {},
			configRegexBool()
		},

		// @CONSOLE_VAR(Profiling, -profiling, 1, 0)
		AttributeDescriptor
		{ 
			"profiling", "Application", 
			"Should we enable profiling by default?",
			"0|1", { "0" }, {},
			configRegexBool()
		},

		////////////////////////////////////////////////////////////////////////////////
		// Scene
		////////////////////////////////////////////////////////////////////////////////
		
		// Active object group
		AttributeDescriptor
		{ 
			"object_group", "Scene", 
			"Which object groups to enable on startup.",
			"NAME", { "Scene_Empty", "LensFlare_TiledLensFlare" }, {},
			configRegexString()
		},

		// Active camera
		AttributeDescriptor
		{
			"camera", "Scene",
			"Which camera to enable on startup.",
			"NAME", { "SponzaCameraFree" }, {},
			configRegexString()
		},

		////////////////////////////////////////////////////////////////////////////////
		// Logging
		////////////////////////////////////////////////////////////////////////////////
	
		// @CONSOLE_VAR(Log - Memory, -log_memory, Debug, Trace, Info, Warning, Error)
		AttributeDescriptor
		{
			"log_memory", "Logging",
			"Enable memory logging for a certain level.",
			"LEVEL", { "Info", "Warning", "Error" },
			{
				{ "Debug", "Debug level" },
				{ "Trace", "Trace level" },
				{ "Info", "Info level" },
				{ "Warning", "Warning level" },
				{ "Error", "Error level" },
			},
			configRegexString()
		},

		// @CONSOLE_VAR(Log - Console, -log_console, Debug, Trace, Info, Warning, Error)
		AttributeDescriptor
		{ 
			"log_console", "Logging", 
			"Enable console output for a certain level.",
			"LEVEL", { "Info", "Warning", "Error" },
			{
				{ "Debug", "Debug level" },
				{ "Trace", "Trace level" },
				{ "Info", "Info level" },
				{ "Warning", "Warning level" },
				{ "Error", "Error level" },
			},
			configRegexString()
		},

		// @CONSOLE_VAR(Log - File, -log_file, Debug, Trace, Info, Warning, Error)
		AttributeDescriptor
		{ 
			"log_file", "Logging", 
			"Enable file output for a certain log level.", 
			"LEVEL",  { "Info", "Warning", "Error" },
			{
				{ "Debug", "Debug level" },
				{ "Trace", "Trace level" },
				{ "Info", "Info level" },
				{ "Warning", "Warning level" },
				{ "Error", "Error level" },
			},
			configRegexString()
		},

		////////////////////////////////////////////////////////////////////////////////
		// Rendering
		////////////////////////////////////////////////////////////////////////////////
		
		// @CONSOLE_VAR(Renderer, -renderer, OpenGL)
		AttributeDescriptor
		{ 
			"renderer", "Rendering", 
			"Default renderer.",
			"RENDERER", { "OpenGL" },
			{ 
				{ "OpenGL", "OpenGL renderer" } 
			},
			configRegexString()
		},

		// @CONSOLE_VAR(Resolution, -resolution, 3840x2160, 2560x1440, 1920x1080, 1366x768, 1280x720, 960x540, 640x360, 320x180, 160x90)
		AttributeDescriptor
		{ 
			"resolution", "Rendering", 
			"Default render resolution.",
			"WxH", { "1920x1080" }, {},
			configRegexInt(2)
		},

		// @CONSOLE_VAR(Max Resolution, -max_resolution, 3840x2160, 2560x1440, 1920x1080, 1366x768, 1280x720, 960x540, 640x360, 320x180, 160x90)
		AttributeDescriptor
		{ 
			"max_resolution", "Rendering", 
			"Maximum possible resolution.",
			"WxH", { "1920x1080" }, {},
			configRegexInt(2)
		},

		// @CONSOLE_VAR(MSAA, -msaa, 1, 2, 4, 8, 16)
		AttributeDescriptor
		{ 
			"msaa", "Rendering", 
			"MSAA samples.",
			"N", { "1" }, {},
			configRegexInt()
		},

		// @CONSOLE_VAR(Voxel Grid Resolution, -voxel_grid, 8, 16, 32, 64, 128, 256, 512)
		AttributeDescriptor
		{ 
			"voxel_grid", "Rendering", 
			"Voxel grid size.",
			"N", { "256" }, {},
			configRegexInt()
		},

		// @CONSOLE_VAR(Shadow Map Resolution, -shadow_map_resolution, 8192, 4096, 2048, 1024, 512)
		AttributeDescriptor
		{ 
			"shadow_map_resolution", "Rendering", 
			"Reference resolution for shadow maps.",
			"N", { "4096" }, {},
			configRegexInt()
		},

		// @CONSOLE_VAR(Num Layers, -layers, 1, 2, 3, 4)
		AttributeDescriptor
		{ 
			"layers", "Rendering", 
			"Maximum (and default) number of layers.",
			"N", { "1" }, {},
			configRegexInt()
		},

		// @CONSOLE_VAR(OpenGL - Version, -gl_version, 4.4, 4.5, 4.6)
		AttributeDescriptor
		{ 
			"gl_version", "Rendering", 
			"OpenGL version.",
			"X.Y", { "4.6" }, {},
			configRegexInt(2)
		},
		// @CONSOLE_VAR(OpenGL - Context, -gl_context, Compatibility, Core)
		AttributeDescriptor
		{ 
			"gl_context", "Rendering", 
			"OpenGL profile.",
			"PROFILE", { "Compatibility" },
			{ 
				{ "Core", "OpenGL Core profile" }, 
				{ "Compatibility", "OpenGL compatibility profile" } 
			},
			configRegexString()
		},

		// @CONSOLE_VAR(OpenGL - Trace Level, -gl_trace, 0, 1, 2, 3)
		AttributeDescriptor
		{ 
			"gl_trace", "Rendering", 
			"GLIntercept trace level.",
			"0,...,3", { "0" }, {},
			configRegexInt()
		},

		// @CONSOLE_VAR(OpenGL - Debug Level, -gl_debug, 0, 1, 2, 3, 4)
		AttributeDescriptor
		{ 
			"gl_debug", "Rendering", 
			"OpenGL debug output threshold.",
			"0,...,4", { "0" }, {},
			configRegexInt()
		},

		// @CONSOLE_VAR(OpenGL - Optional Extensions, -glsl_extensions, 0, 1)
		AttributeDescriptor
		{ 
			"glsl_extensions", "Rendering", 
			"Whether we want to rely on vendor-specific GLSL extensions or not.",
			"0|1", { "1" }, {},
			configRegexBool()
		},
	};

	////////////////////////////////////////////////////////////////////////////////
	/** Map of all the config values. */
	std::unordered_map<std::string, std::vector<std::vector<std::string>>> s_attribValues;

	////////////////////////////////////////////////////////////////////////////////
	// Constructor.
	AttribValue::AttribValue(std::string const& name) :
		m_name(name),
		m_values(s_attribValues)
	{
		if (s_attribValues.empty())
			init();
	}

	////////////////////////////////////////////////////////////////////////////////
	// Test whether the value has any value assigned to it
	bool AttribValue::any() const
	{
		return m_values.get().find(m_name) != m_values.get().end() && m_values.get()[m_name].size() > 0;
	}

	////////////////////////////////////////////////////////////////////////////////
	// Return the number of values held
	size_t AttribValue::count() const
	{
		return (!any()) ? 0 : m_values.get()[m_name].size();
	}

	////////////////////////////////////////////////////////////////////////////////
	void printUsage()
	{
		Debug::log_info() << "Available command line arguments: " << Debug::end;
		
		// Determine the max length of the attribute name
		size_t maxNameLength = 0;
		for (auto const& attribute : s_descriptors)
		{
			size_t nameLength = 4 + 1 + attribute.m_name.length() + 2 + 1 + attribute.m_valueName.length();
			maxNameLength = std::max(nameLength, maxNameLength);
		}

		// Log the attributes
		std::string prevCategory;
		for (auto const& attribute: s_descriptors)
		{
			// Print the category header
			if (attribute.m_category != prevCategory)
			{
				if (prevCategory.empty() == false) Debug::log_info() << " " << Debug::end;
				prevCategory = attribute.m_category;
				Debug::log_info() << "OPTIONS - " << attribute.m_category << Debug::end;
				Debug::log_info() << std::string(strlen("OPTIONS") + 3 + attribute.m_category.length(), '-') << Debug::end;
				Debug::log_info() << " " << Debug::end;
			}

			// Length of this variable's name
			size_t nameLength = 4 + 1 + attribute.m_name.length() + (attribute.m_valueName.empty() ? 0 : 1) + attribute.m_valueName.length();

			Debug::log_info() 
				<< std::string(4, ' ') << '-' << attribute.m_name // name
				<< (attribute.m_valueName.empty() ? "" : "=") << attribute.m_valueName // value
				<< std::string(maxNameLength - nameLength + 1, ' ') << attribute.m_description // description
				<< (attribute.m_choices.empty() ? "" : " One of:")
				<< Debug::end;

			// Log the default value
			if (attribute.m_choices.empty())
			{
				if (attribute.m_default.size() > 1)
					Debug::log_info() << std::string(10, ' ') << "default: " << attribute.m_default << Debug::end;
				else
					Debug::log_info() << std::string(10, ' ') << "default: " << attribute.m_default[0] << Debug::end;
			}

			// Log the list of options
			else
			{
				for (auto const& option : attribute.m_choices)
				{
					// Extract the name and the description
					auto const& [name, description] = option;

					// Length of the option name
					size_t nameLength = 10 + name.length();

					// Check if the value is on by default or not
					bool isDefault = false;
					for (auto const& defaultVal : attribute.m_default)
						isDefault |= (name == defaultVal);

					// Log the option
					Debug::log_info()
						<< std::string(9, ' ') << (isDefault ? '*' : ' ') << name
						<< std::string(maxNameLength - nameLength + 3, ' ') << description
						<< Debug::end;
				}
			}
		}
		Debug::log_info() << " " << Debug::end;
	}

	////////////////////////////////////////////////////////////////////////////////
	void printValues()
	{
		for (auto const& attrib : s_attribValues)
		{
			Debug::log_info() << "  - " << attrib.first << ": " << attrib.second << Debug::end;
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void matchConfigValue(AttributeDescriptor const& token, std::string const& target)
	{
		std::string pattern = "-" + token.m_name + token.m_regex;
		std::regex matcher = std::regex(pattern);
		std::string::const_iterator searchStart(target.cbegin());
		std::smatch capture;

		// Extract all matches
		while (std::regex_search(searchStart, target.cend(), capture, matcher))
		{
			std::vector<std::string> matches(std::vector<std::string>(capture.begin() + 1, capture.end()));
			s_attribValues[token.m_name].push_back(matches);
			searchStart += capture.position() + capture.length();
			Debug::log_debug() << "attrib '" << token.m_name << "' matched, value(s): '" << matches << "'" << Debug::end;
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	/** Extract all the config values. */
	void init()
	{
		Debug::log_debug() << std::string(128, '=') << Debug::end;
		Debug::log_debug() << "Processing command line arguments..." << Debug::end;

		// Construct the full command line argument list
		std::string fullArgList = GetCommandLineA();

		Debug::log_debug() << "Full command line command: '" << fullArgList << "'" << Debug::end;

		// Extract the remainder of the config values
		for (auto const& token : s_descriptors)
		{
			std::regex pattern = std::regex("-" + token.m_name + token.m_regex);
			std::string::const_iterator searchStart(fullArgList.cbegin());
			std::smatch capture;

			// Extract all matches
			matchConfigValue(token, fullArgList);

			// Assign default values
			if (s_attribValues[token.m_name].empty())
			{
				Debug::log_debug() << "attrib '" << token.m_name << "' used default value" << Debug::end;

				for (auto const& defaultVal : token.m_default)
				{
					std::string defaultDefinition = "-" + token.m_name + " " + defaultVal;
					matchConfigValue(token, defaultDefinition);
				}
			}
		}
		Debug::log_info() << std::string(128, '=') << Debug::end;
	}
}