// client_server_simple.cpp
#include <mpi.h>
#include <iostream>
#include <cstring>

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2) {
        if (rank == 0)
            std::cerr << "Нужно два процесса" << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    const int MAX_MSG = 100;
    char buffer[MAX_MSG];

    if (rank == 0) {  // Сервер
        std::cout << "Сервер стартанул" << std::endl;

        // Обслуживаем всех клиентов (все кроме сервера)
        for (int i = 1; i < size; i++) {
            MPI_Status status;
            
            // Принимаем сообщение
            MPI_Recv(buffer, MAX_MSG, MPI_CHAR, MPI_ANY_SOURCE, 0, 
                     MPI_COMM_WORLD, &status);
            
            std::cout << "Сервер получил от клиента " << status.MPI_SOURCE 
                      << ": " << buffer << std::endl;
            
            // Отправляем ответ
            char response[MAX_MSG];
            snprintf(response, MAX_MSG, "Echo: %s", buffer);
            MPI_Send(response, strlen(response) + 1, MPI_CHAR, 
                     status.MPI_SOURCE, 0, MPI_COMM_WORLD);
        }
        
        std::cout << "Сервер выключается" << std::endl;
    } 
    else {  // Клиенты
        // Формируем и отправляем сообщение
        snprintf(buffer, MAX_MSG, "Hello from client %d", rank);
        MPI_Send(buffer, strlen(buffer) + 1, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
        
        std::cout << "КЛт " << rank << " sent: " << buffer << std::endl;
        
        // Получаем ответ
        MPI_Recv(buffer, MAX_MSG, MPI_CHAR, 0, 0, MPI_COMM_WORLD, 
                 MPI_STATUS_IGNORE);
        
        std::cout << "Client " << rank << " got: " << buffer << std::endl;
    }

    MPI_Finalize();
    return 0;
}