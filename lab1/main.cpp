#include <mpi.h>
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>

template<typename T>
class Matrix {
private:
    std::vector<std::vector<T>> data = ;
    size_t rows, cols;
    
public:
    Matrix(size_t rows, size_t cols) 
        : rows(rows), cols(cols), data(rows * cols) {
        std::vector<std::vector<T>> data(rows, std::vector<int>(cols, 0));
    }
    
    T& operator()(size_t i, size_t j) {
        return data[i * cols + j];
    }
    
    const T& operator()(size_t i, size_t j) const {
        return data[i * cols + j];
    }
    
    size_t getRows() const { return rows; }
    size_t getCols() const { return cols; }

    void setElement(int row, int column, double value) {
        data[row][column] = value;
    }
};

class TransferEquation {
    private:
        int x_points = 0;
        int t_points = 0;
        double a = 0.0;
        double X = 0.0;
        double T = 0.0;
        double nu = 0.0;

    public:
        TransferEquation() = default;
        
        explicit TransferEquation(double a) : a(a) {}
        
        void setXNumberPoints(int points) {
            x_points = points;
        }
        
        void setTNumberPoints(int points) {
            t_points = points;
        }
        
        void setTCondition(double temperature) {
            T = temperature;
        }
        
        void setXCondition(double coordinate) {
            X = coordinate;
        }
        
        int getXPoints() const { return x_points; }
        int getTPoints() const { return t_points; }
        double getA() const { return a; }
        double getXCondition() const { return X; }
        double getTCondition() const { return T; }

        bool verifyMinStep() {
            double tau = T/double(t_points);
            double h = X/double(x_points);
            nu = a*tau/h;
            if (nu <= 1) {
                return true;
            }
            return false;
        }

        template<typename Func>
        void computeTBorders(Func func, Matrix<dobule>& grid) {
            for(int t = 0; t < t_points; t++) {
                grid[0, t] = func(t);
            }
        }

        template<typename Func>
        void computeXBorders(Func func, Matrix<dobule>& grid) {
            for(int x = 0; x < x_points; x++) {
                grid[x, 0] = func(x);
            }
        }

        void computeBlock(Matrix<dobule>& grid, int start, int end) {
            for(int computedRow = 3; computedRow < t_points; computedRow++) {
                int currentColumnId = start + 1;
                while(currentColumnId <= (end - 1)) {
                    grid[computedRow][currentColumnId] = grid[computedRow][currentColumnId - 1] - nu * (grid[computedRow + 1][currentColumnId] - grid[computedRow - 1][currentColumnId]);
                    currentColumnId++;
                }
            }
        }

        void computeXLine(Matrix<dobule>& grid, int columnId) {
            for(int computedRow = 3; computedRow < t_points; computedRow++) {
                    grid[computedRow][columnId] = grid[computedRow][columnId - 1] - nu * (grid[computedRow + 1][columnId] - grid[computedRow - 1][columnId]);
                    currentColumnId++;
            }
        }

};


int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    MPI_Status status;

    if (argc != 4) {
        std::cerr << "Не все аргумент переданы";
    }

    int x_points = 10000;
    int t_points = 10000;
    
    TransferEquation equation = TransferEquation();
    equation.setTCondition(std::stod(argv[1]));
    equation.setXCondition(std::stod(argv[2]));
    equation.setXCondition(std::stod(argv[3]));

    equation.setXNumberPoints(x_points);
    equation.setTNumberPoints(t_points);

    int block_size = equation.getTPoints() / size;
    int remainder = equation.getTPoints() % size;

    Matrix<double> gridSolution = Matrix(equation.getXPoints, equation.getTPoints);

    equation.computeTBorders([](double t) { return t*t; }, gridSolution);
    equation.computeXBorders([](double x) { return x*x; }, gridSolution);

    int start, end;
    double local_sum;

    if (rank < remainder) {
        start = rank * (block_size + 1);
        end = start + block_size;
        local_sum = equation.computeBlock(gridSolution, start, end);
    } else {
        start = rank * block_size + remainder;
        end = start + block_size - 1;
        local_sum = equation.computeBlock(gridSolution, start, end);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    double global_sum = 0.0;
    MPI_Reduce(&local_sum, &global_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank != size - 1) {
        if (rank < remainder) {
            start = rank * (block_size + 1);
            end = start + block_size;
            local_sum = equation.computeXLine(gridSolution, start);
        } else {
            start = rank * block_size + remainder;
            end = start + block_size - 1;
            local_sum = equation.computeXLine(gridSolution, start);
        }
    }

    MPI_Finalize();
    return 0;
}
