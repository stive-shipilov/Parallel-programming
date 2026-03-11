#!/bin/bash

SOURCE_FILE="$1"
PROCESS_AMOUNT="$2"
N_VALUE="$3"

mkdir -p "./UTILS" "./ERRORS" "./OUTPUTS"

mpic++ "./$SOURCE_FILE" -o "./UTILS/compiled_code"

cat <<EOF > "./UTILS/mpi_script.sh"
#!/bin/bash
#SBATCH --job-name=${SOURCE_FILE%%.*}
#SBATCH --output=./OUTPUTS/${SOURCE_FILE%%.*}-%j.out
#SBATCH --error=./ERRORS/${SOURCE_FILE%%.*}-%j.err
#SBATCH --time=00:05:00
#SBATCH --ntasks=$PROCESS_AMOUNT

mpirun -np $PROCESS_AMOUNT ./UTILS/compiled_code $N_VALUE
EOF

sbatch ./UTILS/mpi_script.sh