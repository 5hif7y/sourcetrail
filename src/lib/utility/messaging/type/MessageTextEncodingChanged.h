#ifndef MESSAGE_TEXT_ENCODING_CHANGED_H
#define MESSAGE_TEXT_ENCODING_CHANGED_H

#include "Message.h"

#include <boost/preprocessor/stringize.hpp>

class MessageTextEncodingChanged : public Message<MessageTextEncodingChanged>
{
public:
	MessageTextEncodingChanged(const std::string &encoding)
		: textEncoding(encoding)
	{
	}

	static const std::string getStaticType()
	{
		return BOOST_PP_STRINGIZE(MessageTextEncodingChanged);
	}
	
	const std::string textEncoding;
};

#endif
