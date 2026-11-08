#include "CxxQualifierFlags.h"

#include "utilityEnum.h"

CxxQualifierFlags::CxxQualifierFlags(QualifierType qualifier)
{
	addQualifier(qualifier);
}

void CxxQualifierFlags::addQualifier(QualifierType qualifier)
{
	m_flags = m_flags | qualifier;
}

bool CxxQualifierFlags::empty() const
{
	return m_flags == QualifierType::NONE;
}

std::string CxxQualifierFlags::toString() const
{
	if ((m_flags & QualifierType::CONSTEXPR) == QualifierType::CONSTEXPR)
		return "constexpr";
	
	if ((m_flags & QualifierType::CONST) == QualifierType::CONST)
		return "const";
	
	return "";
}
