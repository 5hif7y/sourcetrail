import itertools

class Calculator:
    def add(self, a, b):
        return a + b
    
    def multiply(self, a, b):
        return a * b

def main():
    calc = Calculator()
    c = itertools.cycle([calc.add(1, 2), calc.multiply(3, 4)])
    print(next(c))
    print(next(c))

if __name__ == "__main__":
    main()
