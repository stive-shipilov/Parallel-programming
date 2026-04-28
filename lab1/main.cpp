#include <mpi.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <fstream>
#include <functional>
#include <chrono>

template<typename T>
class Matrix {
private:
    std::vector<T> data;
    size_t rows, cols;
    
public:
    Matrix(size_t rows, size_t cols) 
        : rows(rows), cols(cols), data(rows * cols, T()) {}
    
    T& operator()(size_t i, size_t j) {
        return data[i * cols + j];
    }
    
    const T& operator()(size_t i, size_t j) const {
        return data[i * cols + j];
    }
    
    size_t getRows() const { return rows; }
    size_t getCols() const { return cols; }
    
    void fill(const T& value) {
        std::fill(data.begin(), data.end(), value);
    }
};

class TransferEquation {
private:
    int x_points = 0;
    int t_points = 0;
    double a = 0.0;
    double X = 0.0;
    double T_max = 0.0;
    double nu = 0.0;
    double h = 0.0;
    double tau = 0.0;

    int rank = 0, size = 1;

    double total_comm_time = 0.0;
    int comm_count = 0;  

    std::vector<double> ghost_left;
    std::vector<double> ghost_right;

    std::function<double(double, double)> function;

public:
    TransferEquation() = default;

    void setMPI(int r, int s) {
        rank = r;
        size = s;
    }
    
    void setXNumberPoints(int points) {
        x_points = points;
        if (X > 0) h = X / (x_points - 1);
    }
    
    void setTNumberPoints(int points) {
        t_points = points;
        if (T_max > 0) tau = T_max / (t_points - 1);
        ghost_left.resize(t_points, 0.0);
        ghost_right.resize(t_points, 0.0);
    }

    void setFunction(std::function<double(double, double)> func) {
        function = func;
    }
    
    void setTCondition(double temperature) {
        T_max = temperature;
        if (t_points > 0) tau = T_max / (t_points - 1);
    }
    
    void setXCondition(double coordinate) {
        X = coordinate;
        if (x_points > 0) h = X / (x_points - 1);
    }
    
    void setA(double a_value) {
        a = a_value;
    }
    
    int getXPoints() const { return x_points; }
    int getTPoints() const { return t_points; }
    double getH() const { return h; }
    double getTau() const { return tau; }
    double getNu() { 
        nu = a * tau / h;
        return nu; 
    }

    double getAverageCommTime() const {
        if (comm_count == 0) return 0.0;
        return total_comm_time / comm_count;
    }

    bool verifyMinStep() {
        nu = a * tau / h;
        return std::abs(nu) <= 1.0;
    }

    template<typename Func>
    void computeXInitialCondition(Func func, Matrix<double>& grid) {
        for(int x = 0; x < x_points; x++) {
            double coord = x * h;
            grid(x, 0) = func(coord);
        }
    }

    template<typename Func>
    void computeTInitialCondition(Func func, Matrix<double>& grid) {
        for(int t = 0; t < t_points; t++) {
            double coord = t * tau;
            grid(0, t) = func(coord);
        }
    }
    
    void computeFirstTimeLayer(Matrix<double>& grid) {
        // Вычисление второго врменного слоя методо левого угла
        for(int x = 1; x < x_points; x++) {
            grid(x, 1) = grid(x, 0) - getNu() * (grid(x, 0) - grid(x-1, 0)) 
                    + tau * function(x*h, 0);
        }
    }
    
    void computeBlock(Matrix<double>& grid, int start_x, int end_x) {
    for(int t = 1; t < t_points - 1; t++) {
        // Обмен крайними слоями
        if (rank > 0) {
            auto t1 = MPI_Wtime();  // ЗАМЕР НАЧАЛА
            MPI_Send(&grid(start_x, t), 1, MPI_DOUBLE, rank-1, 0, MPI_COMM_WORLD);
            MPI_Recv(&ghost_left[t], 1, MPI_DOUBLE, rank-1, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            auto t2 = MPI_Wtime();  // ЗАМЕР КОНЦА
            total_comm_time += (t2 - t1);
            comm_count += 2;
        }
        
        if (rank < size - 1) {
            auto t1 = MPI_Wtime();  // ЗАМЕР НАЧАЛА

            MPI_Send(&grid(end_x, t), 1, MPI_DOUBLE, rank+1, 1, MPI_COMM_WORLD);
            MPI_Recv(&ghost_right[t], 1, MPI_DOUBLE, rank+1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            auto t2 = MPI_Wtime();  // ЗАМЕР КОНЦА
            total_comm_time += (t2 - t1);
            comm_count += 2;
        }
        
        for(int x = start_x + 1; x <= end_x - 1; x++) {
            grid(x, t+1) = grid(x, t-1) - getNu() * (grid(x+1, t) - grid(x-1, t))
                         + 2 * tau * function(x*h, t*tau);
            if(rank == size - 1) {
                grid(end_x, t) = grid(end_x, t-1) 
                                - getNu() * (grid(end_x, t-1) - grid(end_x-1, t-1));
            }
        }
        
        if (rank > 0 && start_x > 0) {
            double u_right = grid(start_x + 1, t);
            double u_left = ghost_left[t];
            
            grid(start_x, t+1) = grid(start_x, t-1) - getNu() * (u_right - u_left)
                               + 2 * tau * function(start_x*h, t*tau);
        }
        
        if (rank < size - 1 && end_x < x_points - 1) {
            double u_left = grid(end_x - 1, t);
            double u_right = ghost_right[t];
            
            grid(end_x, t+1) = grid(end_x, t-1) - getNu() * (u_right - u_left)
                             + 2 * tau * function(end_x*h, t*tau);
        }

    }
}
};

void writeSolutionToFile(const Matrix<double>& grid, int x_points, int t_points,
                         double X, double T, const std::string& filename) {
    std::ofstream file(filename);
    
    file << "# Решение уравнения переноса (схема крест)\n";
    file << "# ∂u/∂t + a ∂u/∂x = 0\n";
    file << "# X = " << X << ", T = " << T << "\n";
    file << "# nx = " << x_points << ", nt = " << t_points << "\n";
    file << "# Формат: x t u(x,t)\n\n";
    
    for(int t = 0; t < t_points; t++) {
        for(int x = 0; x < x_points; x++) {
            double x_coord = x * X / (x_points - 1);
            double t_coord = t * T / (t_points - 1);
            file << x_coord << " " << t_coord << " " << grid(x, t) << "\n";
        }
        if (t < t_points - 1) file << "\n";
    }
    
    file.close();
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    double T_condition = 1.0;
    double X_condition = 1.0;
    double a = 1.0;
    
    int x_points = 1000;
    int t_points = 1000;
    
    if (argc > 1) T_condition = std::stod(argv[1]);
    if (argc > 2) X_condition = std::stod(argv[2]);
    if (argc > 3) a = std::stod(argv[3]);
    
    TransferEquation equation;
    equation.setTCondition(5.0);
    equation.setXCondition(10.0);
    equation.setA(1.0);
    
    equation.setXNumberPoints(x_points);
    equation.setTNumberPoints(t_points);
    equation.setMPI(rank, size);

    if (!equation.verifyMinStep()) {
        if (rank == 0) {
            std::cout << "ОШИБКА: Число Куранта = " << equation.getNu() 
                      << " > 1! Решение неустойчиво.\n";
            std::cout << "Увеличьте x_points или уменьшите t_points\n";
        }
        MPI_Finalize();
        return 1;
    }
    
    Matrix<double> localGrid(x_points, t_points);
    localGrid.fill(0.0);
    
    // Начальное условие: гауссов пакет
    double x0 = X_condition / 2.0;
    double sigma = X_condition / 25.0;
    
    equation.computeXInitialCondition([](double x) { 
        return sin(x);
    }, localGrid);

    equation.computeTInitialCondition([](double t) { 
        return -sin(t);
    }, localGrid);

    equation.setFunction([](double x, double t) { 
        return 0.0;
    });
    
    equation.computeFirstTimeLayer(localGrid);

    
    int cols_to_compute = x_points - 2;
    int block_size = cols_to_compute / size;
    int remainder = cols_to_compute % size;
    
    int start_x, end_x;
    
    if (rank < remainder) {
        start_x = rank * (block_size + 1);
        end_x = start_x + block_size;
    } else {
        start_x = rank * block_size + remainder;
        end_x = start_x + block_size - 1;
    }
    
    if (rank == 0) {
        std::cout << "\n=== Распределение вычислений ===\n";
    }
    
    std::cout << "Процесс " << rank << ": x = [" << start_x << ", " << end_x 
              << "], точек: " << (end_x - start_x + 1) << "\n";
    
    auto start_time = MPI_Wtime();
    
    equation.computeBlock(localGrid, start_x, end_x);
    
    auto end_time = MPI_Wtime();
    double elapsed = end_time - start_time;
    
    double local_avg = equation.getAverageCommTime();
    double global_avg = 0.0;


    MPI_Reduce(&local_avg, &global_avg, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        std::cout << "Среднее время на процесс: " << (global_avg / size) << " сек\n";
        std::cout << "Время вычислений: " << elapsed << " сек\n";
    }
    
    Matrix<double> fullGrid(x_points, t_points);
    
    if (rank == 0) {
        for(int x = 0; x < x_points; x++) {
            for(int t = 0; t < t_points; t++) {
                fullGrid(x, t) = localGrid(x, t);
            }
        }
        
        for(int p = 1; p < size; p++) {
            int p_start_x, p_end_x;
            if (p < remainder) {
                p_start_x = p * (block_size + 1);
                p_end_x = p_start_x + block_size;
            } else {
                p_start_x = p * block_size + remainder;
                p_end_x = p_start_x + block_size - 1;
            }
            
            p_start_x = std::max(p_start_x, 1);
            p_end_x = std::min(p_end_x, x_points - 2);
            
            for(int x = p_start_x; x <= p_end_x; x++) {
                MPI_Recv(&fullGrid(x, 0), t_points, MPI_DOUBLE, p, x, 
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
        }
        
        std::string filename = "solution2.dat";
        if (argc > 4) filename = argv[4];
        std::cout << "Идёт запись в файл" << "\n";
        writeSolutionToFile(fullGrid, x_points, t_points, X_condition, T_condition, filename);
        
        std::cout << "\nРезультат сохранён в " << filename << "\n";
        
    } else if (start_x <= end_x) {
        for(int x = start_x; x <= end_x; x++) {
            MPI_Send(&localGrid(x, 0), t_points, MPI_DOUBLE, 0, x, MPI_COMM_WORLD);
        }
    }
    
    MPI_Finalize();
    return 0;
}