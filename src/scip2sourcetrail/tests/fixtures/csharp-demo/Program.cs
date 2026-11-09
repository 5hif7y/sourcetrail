using System;

namespace Demo;

public static class Program
{
    public static void Main()
    {
        var q = new Square(2.0);
        Console.WriteLine(Total(q.Scale(3)));
    }

    static double Total(IShape shape) => shape.Area() + Math.PI;
}
