#ifndef MESSAGE_SAVE_AS_IMAGE_H
#define MESSAGE_SAVE_AS_IMAGE_H

#include "Message.h"

#include <string>

class MessageSaveAsImage: public Message<MessageSaveAsImage>
{
public:
	MessageSaveAsImage(std::string path) : path(std::move(path)) {}

	static const std::string getStaticType()
	{
		return "MessageSaveAsImage";
	}

	std::string path;
};

#endif /* MESSAGE_SAVE_AS_IMAGE_H */
