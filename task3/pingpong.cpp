#include <mpi.h>
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int data = 1;
    MPI_Status status;

    if (rank == 0) {
        data = data*2;
        MPI_Send(&data, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
        std::cout << "Процесс " << rank <<" Отправил данные " << data << "\n"; 
        MPI_Recv(&data, 1, MPI_INT, size - 1, 0, MPI_COMM_WORLD, &status);
        std::cout << "Процесс " << rank << " Получил данные " << data << " от процесса " << size - 1 << "\n";
    } else if (rank == size - 1) {
        MPI_Recv(&data, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, &status);
        std::cout << "Получены данные " << data << " от процесса " << rank - 1 << "\n";
        data = data*2;
        MPI_Send(&data, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    } else {
        MPI_Recv(&data, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, &status);
        std::cout << "Получены данные " << data << " от процесса " << rank - 1 << "\n";
        data = data*2;
        MPI_Send(&data, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}
