package com.sourcetrail;

// these values need to be the same as DefinitionKind in C++ code
public enum DefinitionKind {
	NONE(0),
	IMPLICIT(1),
	EXPLICIT(2);

	private final int m_value;

	private DefinitionKind(int value)
	{
		this.m_value = value;
	}

	public int getValue()
	{
		return m_value;
	}
}
