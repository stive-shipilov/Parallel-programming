#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <memory>
#include <mpi.h>
#include <mpfr.h>

class MPFRSerializer {
public:
    static int toBuffer(const mpfr_t x, std::vector<unsigned char>& buffer) {
        char* strBuf;
        mpfr_exp_t exponent;
        size_t strLen;
        unsigned int strLenUint;
        int expInt;
        const int headerSize = 8;
        
        strBuf = mpfr_get_str(nullptr, &exponent, 10, 0, x, MPFR_RNDN);
        if (strBuf == nullptr) return -1;
        
        strLen = std::strlen(strBuf);
        strLenUint = static_cast<unsigned int>(strLen);
        expInt = static_cast<int>(exponent);
        
        buffer.resize(headerSize + strLen + 1);
        std::memcpy(buffer.data(), &strLenUint, 4);
        std::memcpy(buffer.data() + 4, &expInt, 4);
        std::memcpy(buffer.data() + 8, strBuf, strLen + 1);
        
        mpfr_free_str(strBuf);
        return 0;
    }
    
    static int fromBuffer(mpfr_t x, const std::vector<unsigned char>& buffer) {
        unsigned int strLen;
        int expInt;
        const int headerSize = 8;
        std::string fullStr;
        
        if (buffer.size() < headerSize + 1) return -1;
        
        std::memcpy(&strLen, buffer.data(), 4);
        std::memcpy(&expInt, buffer.data() + 4, 4);
        
        fullStr.reserve(strLen + 32);
        fullStr.push_back(static_cast<char>(buffer[headerSize]));
        fullStr.push_back('.');
        fullStr.append(reinterpret_cast<const char*>(buffer.data() + headerSize + 1), strLen - 1);
        
        if (expInt - 1 != 0) {
            fullStr += "e" + std::to_string(expInt - 1);
        }
        
        return mpfr_set_str(x, fullStr.c_str(), 10, MPFR_RNDN);
    }
};


class MPFRNumber {
private:
    mpfr_t value;
    mpfr_prec_t precision;
    
public:
    MPFRNumber(mpfr_prec_t prec = 64) : precision(prec) {
        mpfr_init2(value, precision);
        mpfr_set_ui(value, 0, MPFR_RNDN);
    }
    
    MPFRNumber(unsigned long int init_val, mpfr_prec_t prec = 64) : precision(prec) {
        mpfr_init2(value, precision);
        mpfr_set_ui(value, init_val, MPFR_RNDN);
    }
    
    ~MPFRNumber() {
        mpfr_clear(value);
    }
    
    MPFRNumber(const MPFRNumber&) = delete;
    MPFRNumber& operator=(const MPFRNumber&) = delete;
    
    MPFRNumber(MPFRNumber&& other) noexcept : precision(other.precision) {
        mpfr_init2(value, precision);
        mpfr_set(value, other.value, MPFR_RNDN);
        mpfr_set_ui(other.value, 0, MPFR_RNDN);
    }
    
    MPFRNumber& operator=(MPFRNumber&& other) noexcept {
        if (this != &other) {
            mpfr_clear(value);
            precision = other.precision;
            mpfr_init2(value, precision);
            mpfr_set(value, other.value, MPFR_RNDN);
            mpfr_set_ui(other.value, 0, MPFR_RNDN);
        }
        return *this;
    }
    
    mpfr_t& get() { return value; }
    const mpfr_t& get() const { return value; }
    
    void set(unsigned long int val) {
        mpfr_set_ui(value, val, MPFR_RNDN);
    }
    
    void set(const MPFRNumber& other) {
        mpfr_set(value, other.value, MPFR_RNDN);
    }
    
    void add(const MPFRNumber& a, const MPFRNumber& b) {
        mpfr_add(value, a.value, b.value, MPFR_RNDN);
    }
    
    void mul(const MPFRNumber& a, const MPFRNumber& b) {
        mpfr_mul(value, a.value, b.value, MPFR_RNDN);
    }
    
    void div_ui(unsigned long int divisor) {
        mpfr_div_ui(value, value, divisor, MPFR_RNDN);
    }
    
    void print(int digits) const {
        mpfr_out_str(stdout, 10, digits, value, MPFR_RNDN);
    }
    
    std::string getFirstDigits(int digits) const {
        char* str = mpfr_get_str(nullptr, nullptr, 10, digits, value, MPFR_RNDN);
        std::string result(str);
        mpfr_free_str(str);
        return result;
    }
    
    mpfr_prec_t getPrecision() const { return precision; }
};


class ExponentialCalculator {
private:
    int rank;
    int numProcesses;
    int requiredDigits;
    int numTerms;
    mpfr_prec_t precision;
    std::vector<int> starts;
    std::vector<int> ends;
    std::vector<MPFRNumber> multipliers;
    
    int findNForPrecision(int K) {
        int N = 1;
        double logFact = 0.0;
        double log10 = std::log(10);
        
        while (logFact <= K * log10) {
            N++;
            logFact += std::log(N);
        }
        return N;
    }
    
    void distributeBlocks() {
        int block_size = numTerms / numProcesses;
        int remainder = numTerms % numProcesses;
        
        starts.resize(numProcesses);
        ends.resize(numProcesses);
        
        for (int i = 0; i < numProcesses; i++) {
            if (i < remainder) {
                starts[i] = i * (block_size + 1);
                ends[i] = starts[i] + (block_size + 1);
            } else {
                starts[i] = i * block_size + remainder;
                ends[i] = starts[i] + block_size;
            }
            if (ends[i] > numTerms) ends[i] = numTerms;
        }
    }
    
    MPFRNumber computeSumInBrackets(int start, int end) const {
        MPFRNumber result(precision);
        MPFRNumber term(precision);
        term.set(1);
        result.set(0);
        
        for (int n = start; n < end; n++) {
            mpfr_add(result.get(), result.get(), term.get(), MPFR_RNDN);
            if (n < end - 1) {
                term.div_ui(n + 1);
            }
        }
        
        return result;
    }
    
    void computeMultipliers() {
        multipliers[0].set(1);
        
        MPFRNumber current(precision);
        current.set(1);
        
        for (int i = 1; i < numProcesses; i++) {
            for (int k = starts[i-1] + 1; k <= starts[i]; k++) {
                current.div_ui(k);
            }
            multipliers[i].set(current);
        }
    }
    
    void broadcastMultipliers() {
        if (rank == 0) {
            for (int i = 1; i < numProcesses; i++) {
                std::vector<unsigned char> buffer;
                MPFRSerializer::toBuffer(multipliers[i].get(), buffer);
                int buf_size = buffer.size();
                MPI_Send(&buf_size, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                MPI_Send(buffer.data(), buf_size, MPI_BYTE, i, 1, MPI_COMM_WORLD);
            }
        } else {
            int buf_size;
            MPI_Recv(&buf_size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            std::vector<unsigned char> buffer(buf_size);
            MPI_Recv(buffer.data(), buf_size, MPI_BYTE, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPFRSerializer::fromBuffer(multipliers[rank].get(), buffer);
        }
        
        MPI_Barrier(MPI_COMM_WORLD);
    }
    
    MPFRNumber collectResults(const MPFRNumber& contribution) {
        MPFRNumber totalSum(precision);
        
        if (rank == 0) {
            totalSum.set(contribution);
            
            for (int i = 1; i < numProcesses; i++) {
                int buf_size;
                MPI_Recv(&buf_size, 1, MPI_INT, i, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                std::vector<unsigned char> buffer(buf_size);
                MPI_Recv(buffer.data(), buf_size, MPI_BYTE, i, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                MPFRNumber remoteContrib(precision);
                MPFRSerializer::fromBuffer(remoteContrib.get(), buffer);
                
                mpfr_add(totalSum.get(), totalSum.get(), remoteContrib.get(), MPFR_RNDN);
            }
        } else {
            std::vector<unsigned char> buffer;
            MPFRSerializer::toBuffer(contribution.get(), buffer);
            int buf_size = buffer.size();
            MPI_Send(&buf_size, 1, MPI_INT, 0, 2, MPI_COMM_WORLD);
            MPI_Send(buffer.data(), buf_size, MPI_BYTE, 0, 3, MPI_COMM_WORLD);
        }
        
        return totalSum;
    }
    
    void printInfo() const {
        std::cout << "========================================" << std::endl;
        std::cout << "ВЫЧИСЛЕНИЕ ЭКСПОНЕНТЫ" << std::endl;
        std::cout << "Количество цифр: " << requiredDigits << std::endl;
        std::cout << "Количество процессов: " << numProcesses << std::endl;
        std::cout << "========================================" << std::endl;
    }
    
public:
    ExponentialCalculator(int argc, char** argv) {
        MPI_Init(&argc, &argv);
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &numProcesses);
        
        if (argc != 2) {
            if (rank == 0) {
                std::cerr << "Использование: " << argv[0] << " <K>" << std::endl;
            }
            MPI_Finalize();
            exit(1);
        }
        
        requiredDigits = std::atoi(argv[1]);
        numTerms = findNForPrecision(requiredDigits);
        precision = static_cast<mpfr_prec_t>(requiredDigits * 3.5 + 100);
        
        distributeBlocks();
        
        multipliers.reserve(numProcesses);
        for (int i = 0; i < numProcesses; i++) {
            multipliers.emplace_back(precision);
        }
    }
    
    ~ExponentialCalculator() {
        MPI_Finalize();
    }
    
    MPFRNumber compute() {
        if (rank == 0) {
            printInfo();
            computeMultipliers();
        }
        
        broadcastMultipliers();
        
        MPFRNumber localSum = computeSumInBrackets(starts[rank], ends[rank]);
        
        MPFRNumber contribution(precision);
        contribution.mul(multipliers[rank], localSum);
        
        MPFRNumber totalSum = collectResults(contribution);
        
        if (rank == 0) {
            std::cout << "РЕЗУЛЬТАТ: e = " << std::endl;
            
            totalSum.print(requiredDigits);
            std::cout << std::endl;
            
        }
        
        return totalSum;
    }
    
    int getRank() const { return rank; }
    int getNumProcesses() const { return numProcesses; }
};

int main(int argc, char** argv) {
    ExponentialCalculator calculator(argc, argv);
    MPFRNumber result = calculator.compute();
    return 0;
}