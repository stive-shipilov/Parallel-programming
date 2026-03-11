#include <mpi.h>
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <cstring> 

const std::vector<std::string> FUNC_TYPES = {"MPI_Send", "MPI_Ssend", "MPI_Rsend", "MPI_Bsend"};

template<typename SendFunc>
double send_time_measuring(SendFunc send_func, void* data, int count, MPI_Datatype datatype, 
        int dest, int tag, MPI_Comm comm, int rank) {
    auto start = std::chrono::high_resolution_clock::now();
    
    send_func(data, count, datatype, dest, tag, comm);
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::micro> elapsed = end - start;
    return elapsed.count();
}

void test_message_time(int rank, int size, const std::string& send_type) {
    std::vector<int> test_sizes = {1, 10, 100, 1000, 10000, 50000, 100000};
    
    for (int msg_size : test_sizes) {
        std::vector<char> send_buffer(msg_size, '1');
        std::vector<char> recv_buffer(msg_size);

        MPI_Status status;
        double elapsed_time = 0.0;

        if (rank == 0) {
            std::cout << "Тестирование размера " << msg_size << "\n";

            if (send_type == "MPI_Send" || send_type == "MPI_Ssend") {
                elapsed_time = send_time_measuring(MPI_Send, send_buffer.data(), msg_size, 
                                                MPI_CHAR, 1, 0, MPI_COMM_WORLD, rank);
            }
            else if (send_type == "MPI_Bsend") {
                int buffer_size = msg_size + MPI_BSEND_OVERHEAD;
                std::vector<char> bsnd_buffer(buffer_size);
                MPI_Buffer_attach(bsnd_buffer.data(), buffer_size);
                elapsed_time = send_time_measuring(MPI_Bsend, send_buffer.data(), msg_size, 
                                                MPI_CHAR, 1, 0, MPI_COMM_WORLD, rank);
                MPI_Buffer_detach(bsnd_buffer.data(), &buffer_size);
            }
            else if (send_type == "MPI_Rsend") {
                // проверяем что receive готов
                int ready_signal = 1;
                int ack;
                MPI_Send(&ready_signal, 1, MPI_INT, 1, 1, MPI_COMM_WORLD);
                MPI_Recv(&ack, 1, MPI_INT, 1, 2, MPI_COMM_WORLD, &status);
                if (ack != 1) {
                    throw "Ошибка обмена процессами в MPI_Rsend";
                }
                elapsed_time = send_time_measuring(MPI_Rsend, send_buffer.data(), msg_size, 
                                                MPI_CHAR, 1, 0, MPI_COMM_WORLD, rank);
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            MPI_Recv(recv_buffer.data(), msg_size, MPI_CHAR, 1, 0, MPI_COMM_WORLD, &status);
            
            std::cout << "Время выполнения функции " << send_type << ": " << elapsed_time 
                      << " микросекунд" << std::endl;
        }
        else if (rank == 1) {
            if (send_type == "MPI_Rsend") {
                int ready_signal;
                MPI_Recv(&ready_signal, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
                
                int ack = 1;
                MPI_Send(&ack, 1, MPI_INT, 0, 2, MPI_COMM_WORLD);
            }
            
            MPI_Recv(recv_buffer.data(), msg_size, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);
            MPI_Send(recv_buffer.data(), msg_size, MPI_CHAR, 0, 0, MPI_COMM_WORLD);
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Передайти один из типов отправки: MPI_Send, MPI_Ssend, MPI_Bsend, MPI_Rsend" << std::endl;
        return 1;
    }
    std::string function_type = argv[1];

    if (std::find(FUNC_TYPES.begin(), FUNC_TYPES.end(), function_type) == FUNC_TYPES.end()) {
        std::cerr << "Тип функции " << function_type << " не поддерживается";
        std::cerr << "Доступны только: ";
        for (const auto& type : FUNC_TYPES) {
            std::cerr << type << " ";
        }
        std::cerr << std::endl;
    }

    MPI_Init(&argc, &argv);
    
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    if (size < 2) {
        if (rank == 0) {
            std::cerr << "Ошибка: нужно минимум 2 процесса!" << std::endl;
        }
        MPI_Finalize();
        return 1;
    }

    test_message_time(rank, size, function_type);
    
    MPI_Finalize();
    return 0;
}
