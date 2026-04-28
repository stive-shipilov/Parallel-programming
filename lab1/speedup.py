import matplotlib.pyplot as plt
import numpy as np

# Данные: количество ядер и время выполнения
cores = np.array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14])
time = np.array([5.81138, 2.76012, 1.76759, 1.46322, 1.12365, 0.87125, 
                 0.920535, 0.651881, 1.10412, 1.07595, 1.00106, 0.942943, 0.932241])

# Вычисляем ускорение (T1 / Tn)
speedup = time[0] / time

# Создаём график
plt.figure(figsize=(10, 6))

# График ускорения
plt.subplot(1, 2, 1)
plt.plot(cores, speedup, 'bo-', linewidth=2, markersize=8, label='Ускорение')
plt.plot(cores, cores, 'r--', linewidth=1, alpha=0.5, label='Идеальное ускорение')
plt.xlabel('Количество ядер', fontsize=12)
plt.ylabel('Ускорение', fontsize=12)
plt.title('Закон Амдала: Ускорение', fontsize=14)
plt.grid(True, alpha=0.3)
plt.legend()

# График времени выполнения
plt.subplot(1, 2, 2)
plt.plot(cores, time, 'go-', linewidth=2, markersize=8)
plt.xlabel('Количество ядер', fontsize=12)
plt.ylabel('Время выполнения (сек)', fontsize=12)
plt.title('Время выполнения программы', fontsize=14)
plt.grid(True, alpha=0.3)

plt.tight_layout()
plt.show()

# Вывод результатов
print("=== Анализ закона Амдала ===")
print(f"Время на 1 ядре: {time[0]:.4f} сек")
print(f"Минимальное время: {min(time):.4f} сек на {cores[np.argmin(time)]} ядрах")
print(f"\nУскорение:")
for i, (c, s) in enumerate(zip(cores, speedup)):
    print(f"  {c} ядер: {s:.4f}x")