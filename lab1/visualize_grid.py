import numpy as np
import matplotlib.pyplot as plt

# Загрузите ВАШИ данные из C++
data = np.loadtxt('result.dat', comments='#')
x = data[:, 0]
t = data[:, 1]
u_numerical = data[:, 2]

# Параметры
X, T = 10.0, 5.0
a = 1.0  # скорость из C++

# ПРАВИЛЬНОЕ точное решение (без источника!)
u_exact_correct = np.sin(x - a*t)

# НЕПРАВИЛЬНОЕ (то, что вы использовали)
u_exact_wrong = np.sin(x - a*t)  # с источником -sin(x-t) это НЕ решение

# Построим сравнение
plt.figure(figsize=(15, 5))

for i, time in enumerate([0, 1, 2, 3, 4, 5]):
    mask = np.abs(t - time) < 0.05
    if np.any(mask):
        x_t = x[mask]
        u_num = u_numerical[mask]
        
        # Сортируем
        idx = np.argsort(x_t)
        x_t = x_t[idx]
        u_num = u_num[idx]
        
        # Правильное точное решение
        u_exact = np.sin(x_t - a*time)
        
        plt.subplot(2, 3, i+1)
        plt.plot(x_t, u_num, 'b--', label='Численное', linewidth=2)
        
        plt.title(f't = {time}')
        plt.legend(fontsize=8)
        plt.grid(True)
        plt.ylim(-1.5, 1.5)

plt.suptitle('Численное решения', fontsize=14)
plt.tight_layout()
plt.show()