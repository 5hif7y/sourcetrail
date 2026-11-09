namespace Demo;

public interface IShape
{
    double Area();
}

public class Square : IShape
{
    public double Side { get; }

    public Square(double side)
    {
        Side = side;
    }

    public double Area()
    {
        var s = Side;
        return s * s;
    }

    public Square Scale(double factor) => new Square(Side * factor);

    public Square Scale(int factor) => Scale((double)factor);
}
