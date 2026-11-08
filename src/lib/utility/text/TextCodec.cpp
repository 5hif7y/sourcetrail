#include "TextCodec.h"

#include <array>
#include <algorithm>

#include <boost/locale/encoding.hpp>

using namespace std;

static const array<string, 9> AVAILABLE_ENCODINGS = {
	"UTF-8",
	"UTF-16",
	"UTF-16LE",
	"UTF-16BE",
	"UTF-32",
	"UTF-32LE",
	"UTF-32BE",
	"Latin1",
	"ISO-8859-1"
};

static string makeValidEncoding(const string& name)
{
	for (const auto& encoding : AVAILABLE_ENCODINGS)
	{
		if (encoding == name)
			return encoding;
	}
	// Default to UTF-8 if encoding is not found
	return "UTF-8";
}

std::vector<std::string> TextCodec::availableCodecs()
{
	return {AVAILABLE_ENCODINGS.begin(), AVAILABLE_ENCODINGS.end()};
}

TextCodec::TextCodec(const string& name)
	: m_name(makeValidEncoding(name))
{
}

string TextCodec::decode(const string& encodedString)
{
	if (m_name == "UTF-8")
		return encodedString;  // Already UTF-8, no conversion needed

	try
	{
		return boost::locale::conv::between(encodedString, "UTF-8", m_name);
	}
	catch (const boost::locale::conv::conversion_error&)
	{
		return encodedString;  // Return as-is on conversion failure
	}
}

string TextCodec::encode(const string& decodedString)
{
	if (m_name == "UTF-8")
		return decodedString;  // Already UTF-8, no conversion needed

	try
	{
		return boost::locale::conv::between(decodedString, m_name, "UTF-8");
	}
	catch (const boost::locale::conv::conversion_error&)
	{
		return decodedString;  // Return as-is on conversion failure
	}
}

string TextCodec::getName() const
{
	return m_name;
}
