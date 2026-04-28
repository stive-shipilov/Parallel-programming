#include <mpi.h>
#include <iostream>

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);
    
    MPI_Comm parent;
    MPI_Comm_get_parent(&parent);
    
    if (parent == MPI_COMM_NULL) {
        // Родитель
        MPI_Comm children;
        MPI_Comm_spawn(argv[0], NULL, 4, MPI_INFO_NULL, 0, MPI_COMM_SELF, &children, MPI_ERRCODES_IGNORE);
        
        std::cout << "Spawned 4 children" << std::endl;
        
        MPI_Barrier(MPI_COMM_WORLD);
        MPI_Comm_free(&children);
    } 
    else {
        // Дети
        std::cout << "Hello from child process!" << std::endl;
        MPI_Comm_free(&parent);
    }
    
    MPI_Finalize();
}