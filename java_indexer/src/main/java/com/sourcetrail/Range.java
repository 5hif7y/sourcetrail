package com.sourcetrail;

public class Range
{
	public Position begin;
	public Position end;

	public Range(int beginLine, int beginColumn, int endLine, int endColumn)
	{
		this.begin = new Position(beginLine, beginColumn);
		this.end = new Position(endLine, endColumn);
	}

	public Range(Position begin, Position end)
	{
		this.begin = new Position(begin.line(), begin.column());
		this.end = new Position(end.line(), end.column());
	}
}
