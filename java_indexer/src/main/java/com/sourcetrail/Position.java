package com.sourcetrail;

public record Position(int line, int column) 
{
	Position()
	{
		this(0, 0);
	}
}
