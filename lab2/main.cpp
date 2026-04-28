#include <iostream>
#include <vector>
#include <cmath>
#include <pthread.h>
#include <chrono>
#include <iomanip>
#include <unistd.h>

using namespace std;
using namespace chrono;

double func(double x) {
    return cos(1.0/x);
}

struct Task {
    double a;
    double b;
    double step;
    
    Task(double _a, double _b, double _step) 
        : a(_a), b(_b), step(_step) {}
};

struct ThreadData {
    int id;
    vector<Task>* tasks;
    pthread_mutex_t* mutex;
    double* total_result;
    double tolerance;
    bool* finished;
    int* active_threads;
    double a_global;
    double b_global;
};

double simpson(double a, double b, double step) {
    int n = max(2, (int)((b - a) / step));
    if (n % 2 == 1) n++;
    
    double h = (b - a) / n;
    double sum = func(a) + func(b);
    
    for (int i = 1; i < n; i++) {
        double x = a + i * h;
        sum += (i % 2 == 0) ? 2 * func(x) : 4 * func(x);
    }
    
    return sum * h / 3.0;
}

double integrate_with_error(double a, double b, double step, double &error) {
    double I_rough = simpson(a, b, step);
    double I_fine = simpson(a, b, step / 2.0);
    
    error = fabs(I_fine - I_rough) / 15.0;
    return I_fine;
}

void* worker_thread(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    while (true) {
        // Захватываем мьютекс
        pthread_mutex_lock(data->mutex);
        
        // Проверяем флаг завершения
        if (*(data->finished)) {
            pthread_mutex_unlock(data->mutex);
            break;
        }
        
        // Берем задачу из очереди
        if (data->tasks->empty()) {
            // Если очередь пуста, проверяем - не закончили ли мы
            if (*(data->active_threads) == 0) {
                *(data->finished) = true;
                pthread_mutex_unlock(data->mutex);
                break;
            }
            pthread_mutex_unlock(data->mutex);
            // Небольшая задержка перед следующей попыткой
            struct timespec ts = {0, 1000000}; // 1 мс
            nanosleep(&ts, nullptr);
            continue;
        }
        
        // Забираем задачу
        Task task = data->tasks->back();
        data->tasks->pop_back();
        
        // Увеличиваем счетчик активных потоков, показывая что мы работаем
        (*(data->active_threads))++;
        pthread_mutex_unlock(data->mutex);
        
        // Вычисляем интеграл на этом участке
        double error;
        double result = integrate_with_error(task.a, task.b, task.step, error);
        
        // Проверяем точность
        double required_error = data->tolerance * (task.b - task.a) / (data->b_global - data->a_global);
        
        if (error < required_error || task.step < 1e-12) {
            // Точность достигнута или шаг слишком мал
            pthread_mutex_lock(data->mutex);
            *(data->total_result) += result;
            (*(data->active_threads))--;
            pthread_mutex_unlock(data->mutex);
        } else {
            // Разбиваем задачу на две
            double mid = (task.a + task.b) / 2.0;
            double new_step = task.step / 2.0;
            
            Task left(task.a, mid, new_step);
            Task right(mid, task.b, new_step);
            
            pthread_mutex_lock(data->mutex);
            data->tasks->push_back(left);
            data->tasks->push_back(right);
            (*(data->active_threads))--;
            pthread_mutex_unlock(data->mutex);
        }
    }
    
    return nullptr;
}

double parallel_integrate(double a, double b, int num_threads, double tolerance) {
    vector<Task> tasks;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    double total_result = 0.0;
    bool finished = false;
    int active_threads = 0;
    
    int initial_parts = max(num_threads * 2, 8);
    double initial_step = (b - a) / initial_parts;
    
    for (int i = 0; i < initial_parts; i++) {
        double left = a + i * initial_step;
        double right = left + initial_step;
        tasks.push_back(Task(left, right, initial_step / 10.0));
    }
    
    vector<pthread_t> threads(num_threads);
    vector<ThreadData> thread_data(num_threads);
    
    auto start_time = high_resolution_clock::now();
    
    for (int i = 0; i < num_threads; i++) {
        thread_data[i] = {i, &tasks, &mutex, &total_result, 
                         tolerance, &finished, &active_threads, a, b};
        pthread_create(&threads[i], nullptr, worker_thread, &thread_data[i]);
    }
    
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], nullptr);
    }
    
    auto end_time = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end_time - start_time);
    
    cout << "Время выполнения: " << duration.count() / 1000.0 << " мс" << endl;
    
    pthread_mutex_destroy(&mutex);
    
    return total_result;
}

double sequential_integrate(double a, double b, double tolerance) {
    vector<Task> tasks;
    double total_result = 0.0;
    
    int initial_parts = 8;
    double initial_step = (b - a) / initial_parts;
    
    for (int i = 0; i < initial_parts; i++) {
        double left = a + i * initial_step;
        double right = left + initial_step;
        tasks.push_back(Task(left, right, initial_step / 10.0));
    }
    
    while (!tasks.empty()) {
        Task task = tasks.back();
        tasks.pop_back();
        
        double error;
        double result = integrate_with_error(task.a, task.b, task.step, error);
        
        double required_error = tolerance * (task.b - task.a) / (b - a);
        
        if (error < required_error || task.step < 1e-12) {
            total_result += result;
        } else {
            double mid = (task.a + task.b) / 2.0;
            double new_step = task.step / 2.0;
            tasks.push_back(Task(task.a, mid, new_step));
            tasks.push_back(Task(mid, task.b, new_step));
        }
    }
    
    return total_result;
}


int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Использование: " << argv[0] << " <число_потоков> <допустимая_погрешность>" << endl;
        cerr << "Пример: " << argv[0] << " 4 1e-6" << endl;
        return 1;
    }
    
    int num_threads = atoi(argv[1]);
    double tolerance = atof(argv[2]);
    
    double a = 1/1000.0;
    double b = 1.0;
    
    cout << fixed << setprecision(10);
    cout << "=== Численное интегрирование ===" << endl;
    cout << "Интервал: [" << a << ", " << b << "]" << endl;
    cout << "Число потоков: " << num_threads << endl;
    cout << "Допустимая погрешность: " << tolerance << endl;
    cout << endl;
    
    cout << "Последовательное интегрирование:" << endl;
    auto start_seq = high_resolution_clock::now();
    double seq_result = sequential_integrate(a, b, tolerance);
    auto end_seq = high_resolution_clock::now();
    double seq_time = duration_cast<microseconds>(end_seq - start_seq).count() / 1000.0;
    cout << "Результат: " << seq_result << endl;
    cout << "Время: " << seq_time << " мс" << endl;
    cout << endl;
    
    cout << "Параллельное интегрирование:" << endl;
    auto start_par = high_resolution_clock::now();
    double par_result = parallel_integrate(a, b, num_threads, tolerance);
    auto end_par = high_resolution_clock::now();
    double par_time = duration_cast<microseconds>(end_par - start_par).count() / 1000.0;
    cout << "Результат: " << par_result << endl;
    cout << "Время: " << par_time << " мс" << endl;
    cout << endl;
    
    // Оценка производительности
    double speedup = seq_time / par_time;
    double efficiency = speedup / num_threads * 100.0;
    
    cout << "=== Оценка производительности ===" << endl;
    cout << "Ускорение: " << speedup << "x" << endl;
    cout << "Эффективность: " << efficiency << "%" << endl;
    
    return 0;
}