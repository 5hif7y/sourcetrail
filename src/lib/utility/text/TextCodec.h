#ifndef TEXT_CODEC_H
#define TEXT_CODEC_H

#include <string>
#include <vector>

class TextCodec
{
public:
	static std::vector<std::string> availableCodecs();

	TextCodec(const std::string& name);

	std::string decode(const std::string& encodedString);
	std::string encode(const std::string& decodedString);

	std::string getName() const;

private:
	std::string m_name;
};

#endif // TEXT_CODEC_H
