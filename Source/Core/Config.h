#pragma once

////////////////////////////////////////////////////////////////////////////////
//  Headers
////////////////////////////////////////////////////////////////////////////////

#include "PCH.h"
#include "Constants.h"

////////////////////////////////////////////////////////////////////////////////
/// CONFIGURATIONS
////////////////////////////////////////////////////////////////////////////////
namespace Config
{
	////////////////////////////////////////////////////////////////////////////////
	/** Helper class for converting from string captures to actual objects */
	template<typename T>
	struct AttribValueConstructor
	{
		static T construct(std::vector<std::string> const& values)
		{
			// Construct an input stream from the config value
			std::istringstream inputstream(values[0]);

			// Extract the result
			T result;
			inputstream >> result;

			// Return the result
			return result;
		}
	};

	////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	struct AttribValueConstructor<glm::tvec2<T>>
	{
		static glm::tvec2<T> construct(std::vector<std::string> const& values)
		{
			return glm::tvec2<T>(
				AttribValueConstructor<T>::construct({ values[0] }), 
				AttribValueConstructor<T>::construct({ values[1] }));
		}
	};

	////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	struct AttribValueConstructor<glm::tvec3<T>>
	{
		static glm::tvec3<T> construct(std::vector<std::string> const& values)
		{
			return glm::tvec3<T>(
				AttribValueConstructor<T>::construct({ values[0] }), 
				AttribValueConstructor<T>::construct({ values[1] }), 
				AttribValueConstructor<T>::construct({ values[2] }));
		}
	};

	////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	struct AttribValueConstructor<glm::tvec4<T>>
	{
		static glm::tvec3<T> construct(std::vector<std::string> const& values)
		{
			return glm::tvec4<T>(
				AttribValueConstructor<T>::construct({ values[0] }), 
				AttribValueConstructor<T>::construct({ values[1] }), 
				AttribValueConstructor<T>::construct({ values[2] }), 
				AttribValueConstructor<T>::construct({ values[3] }));
		}
	};

	////////////////////////////////////////////////////////////////////////////////
	/** An object used for referencing the value of a config attribute. */
	struct AttribValue
	{
		// Name of the config attrib.
		std::string m_name;

		// Reference to the attrib value map
		std::reference_wrapper<std::unordered_map<std::string, std::vector<std::vector<std::string>>>> m_values;

		// Constructor.
		AttribValue(std::string const& name);

		// Return the number of values held
		size_t count() const;

		// Test whether the value has any value assigned to it
		bool any() const;

		// Tests whether any of the attrib values matches the parameter
		template<typename T> 
		bool contains(T const& refVal) const
		{
			for (size_t i = 0; i < count(); ++i)
				if (get<T>(i) == refVal)
					return true;
			return false;
		}

		// Generic getter
		template<typename T>
		T get(size_t id = 0) const
		{
			return AttribValueConstructor<T>::construct(m_values.get()[m_name][id]);
		}

		// Generic getter
		template<typename T>
		T get_or(T defaultVal, size_t id = 0) const
		{
			return (!any()) ? defaultVal : get<T>(id);
		}

		// Generic conversion operator
		template<typename T>
		explicit operator T() const
		{
			return get<T>(0);
		}
	};

	////////////////////////////////////////////////////////////////////////////////
	/** Display a list of available command line arguments. */
	void printUsage();

	////////////////////////////////////////////////////////////////////////////////
	/** Display a list of parsed values. */
	void printValues();

	////////////////////////////////////////////////////////////////////////////////
	/** Extract all the config values. */
	void init();
}