#ifndef CXX_QUALIFIER_FLAGS_H
#define CXX_QUALIFIER_FLAGS_H

#include <cstdint>
#include <string>

class CxxQualifierFlags
{
public:
	enum class QualifierType : std::uint8_t
	{
		NONE = 0,
		
		CONST     = 1 << 0,
		CONSTEXPR = 1 << 1
	};

	CxxQualifierFlags(QualifierType qualifier = QualifierType::NONE);
	
	void addQualifier(QualifierType qualifier);

	bool empty() const;
	std::string toString() const;

private:
	QualifierType m_flags = QualifierType::NONE;
};

#endif
