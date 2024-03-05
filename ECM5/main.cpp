#include <iostream>
#include <string>
#include <cmath>
#include <fstream>
#include <omp.h>
#include <chrono>

// Сортировка подсчётом
int* sortR (int maxVal, const int* data, size_t size) {
    int* answer = new int[maxVal];
    for (int i = 0; i < maxVal; i++) {
        answer[i] = 0;
    }
#pragma omp parallel for default(none) schedule(static) shared(size, answer, data)
    for (size_t i = 0; i < size; i++) {
        answer[data[i]]++;
    }
    return answer;
}

int main(int argc, char* argv[]){
        std::string inFileName; // Имя входного файла
        std::string outFileName; // имя выходного файла
        int countThreads; // количество тредов
        double coefficient; // игнорируемая доля
        std::ifstream fin;
        std::string magicNumber;
        int width;
        int height;
        int maxVal;
        std::string maxValStr;

        if (argc != 5) {
            std::cout << "You must enter your input in format: <countThreads> "
                         "<input file name> <output file name> <coefficient>" << std::endl;
            std::cout << "input file format must be PPM or PGM" << std::endl;
            std::cout << "coefficient must be in range[0.0; 0.5)" << std::endl;
            std::cout << "countThreads must be an integer >= 0" << std::endl;
            return 1;
        }

        try {
            countThreads = std::stoi(argv[1]);
            if (countThreads < 0) {
                std::cout << "incorrect number of treads (< 0)" << std::endl;
                return 1;
            }
        } catch (const std::invalid_argument &e) {
            std::cout << "you write incorrect number of treads" << std::endl;
            return 1;
        }

        if (countThreads != 0) {
            omp_set_num_threads(countThreads);
        }

        inFileName = argv[2];
        outFileName = argv[3];

        try {
            coefficient = std::stod(argv[4]);
            if (coefficient < 0.0 || coefficient >= 0.5) {
                std::cout << "you wrote incorrect coefficient (must be 0 <= coefficient < 0.5)" << std::endl;
                return 1;
            }
        } catch (const std::invalid_argument &e) {
            std::cout << "" << std::endl;
            return 1;
        }

        std::string str;
        fin.open(inFileName, std::ios::in | std::ios::binary);
        if (!fin.is_open()) {
            std::cout << "This input file doesn't exist";
            return 1;
        }
        getline(fin, magicNumber);
        getline(fin, str);
        getline(fin, maxValStr);
        try {
            maxVal = stoi(maxValStr);
        } catch (const std::invalid_argument &e) {
            std::cout << "you write incorrect max value" << std::endl;
            return 1;
        }

        if (maxVal != 255) {
            std::cout << "Incorrect file (max value != 255)" << std::endl;
            return 1;
        }

//    запарсили width и height из строки
        int *str2 = new int[2];
        std::string str1 = str + " ";
        int beg = 0;
        int end = static_cast<int>(str1.find(' '));
        for (int i = 0; i < 2; i++) {
            try {
                str2[i] = stoi(str.substr(beg, end - beg));
            } catch (const std::invalid_argument &e) {
                std::cout << "you write incorrect width or height" << std::endl;
                return 1;
            }
            beg = end + 1;
            end = static_cast<int>(str1.find(' ', i + 1));
        }

        width = str2[0];
        height = str2[1];
        size_t size = height * width; // количество пикселей
        int needIgn = static_cast<int>(static_cast<double>(size) * coefficient); // количество пикселей,
        // которых нужно проигнорировать с каждой стороны

        if (magicNumber == "P5") {

            // pgm

            int *data = new int[size];
            int downVal = 0; // первое значение, которые мы не должны игнорировать
            int upVal = maxVal; // последнее значение, которые мы не должны игнорировать

//        запись данных о пикселях в память
            for (size_t i = 0; i < size; i++) {
                data[i] = static_cast<int>(fin.get());
            }
            fin.close();

            auto start = std::chrono::high_resolution_clock::now();

            // сортируем значение пикселей
            int *values = sortR(maxVal + 1, data, size);
            int i;
            int minCol;
            int maxCol;

#pragma omp parallel sections default(none) private(i) shared(needIgn, values, downVal, upVal, minCol, maxCol)
            {
// находим минимальное значение пикселей, после учёта коэффициента игнорирования
#pragma omp section
                {
                    i = needIgn;
                    while (true) {
                        i -= values[downVal];
                        if (i < 0) {
                            break;
                        } else {
                            downVal++;
                        }
                    }
                    minCol = downVal;
                }

// находим максимальное значение пикселей, после учёта коэффициента игнорирования
#pragma omp section
                {
                    i = needIgn;
                    while (true) {
                        i -= values[upVal];
                        if (i < 0) {
                            break;
                        } else {
                            upVal--;
                        }
                    }
                    maxCol = upVal;
                }
            }
            if (maxCol != minCol) { // если картинка не одноцветная
// изменяем контрастность изображения, так,
// чтобы равномерно все растянуть (также учитываем, что у нас арифметика с насыщением)
#pragma omp parallel for default(none) schedule(static) shared(data, maxVal, minCol, maxCol, size)
                for (size_t j = 0; j < size; j++) {
                    data[j] = std::max(std::min(((data[j] - minCol) * maxVal) / (maxCol - minCol), maxVal), 0);
                }
            }

            auto finish = std::chrono::high_resolution_clock::now();
            auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(finish - start);

//        выводим данные в выходной файл
            std::ofstream out;
            out.open(outFileName, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!out.is_open()) {
                std::cout << "This output file can't be read or found";
                return 1;
            }
            out << magicNumber << std::endl << width << " " << height << std::endl << maxVal << std::endl;
            for (size_t j = 0; j < size; j++) {
                out << static_cast<char>((std::byte) data[j]);
            }
            out.close();

            printf("Time (%i thread(s)) : %g ms\n", countThreads, static_cast<double>(microseconds.count()) / 1000);

            delete[] data;
            delete[] values;


        } else if (magicNumber == "P6") {

//        ppm

//        все данные считываем также как и в pgm (только 3 цвета, вместо 1)
            int *dataR = new int[size];
            int *dataG = new int[size];
            int *dataB = new int[size];
            int downValR = 0;
            int upValR = maxVal;
            int downValG = 0;
            int upValG = maxVal;
            int downValB = 0;
            int upValB = maxVal;
            for (int i = 0; i < size; i++) {
                dataR[i] = static_cast<int>(fin.get());
                dataG[i] = static_cast<int>(fin.get());
                dataB[i] = static_cast<int>(fin.get());
            }
            fin.close();

            auto start = std::chrono::high_resolution_clock::now();

            int i;
            // сортируем значение пикселей
            int *valuesR = sortR(maxVal + 1, dataR, size);
            int *valuesG = sortR(maxVal + 1, dataG, size);
            int *valuesB = sortR(maxVal + 1, dataB, size);

// находим минимальные и максимальные значения пикселей, после учёта коэффициента игнорирования
#pragma omp parallel sections default(none) private(i) shared(needIgn, valuesR, valuesG, valuesB, dataR, dataG, dataB, size, downValR, downValG, downValB, upValR, upValG, upValB, maxVal)
            {
#pragma omp section
                {
                    i = needIgn;
                    while (true) {
                        i -= valuesR[downValR];
                        if (i < 0) {
                            break;
                        } else {
                            downValR++;
                        }
                    }
                }
#pragma omp section
                {
                    i = needIgn;
                    while (true) {
                        i -= valuesG[downValG];
                        if (i < 0) {
                            break;
                        } else {
                            downValG++;
                        }
                    }
                }
#pragma omp section
                {
                    i = needIgn;
                    while (true) {
                        i -= valuesB[downValB];
                        if (i < 0) {
                            break;
                        } else {
                            downValB++;
                        }
                    }
                }
#pragma omp section
                {
                    i = needIgn;
                    while (true) {
                        i -= valuesR[upValR];
                        if (i < 0) {
                            break;
                        } else {
                            upValR--;
                        }
                    }
                }
#pragma omp section
                {
                    i = needIgn;
                    while (true) {
                        i -= valuesG[upValG];
                        if (i < 0) {
                            break;
                        } else {
                            upValG--;
                        }
                    }
                }
#pragma omp section
                {
                    i = needIgn;
                    while (true) {
                        i -= valuesB[upValB];
                        if (i < 0) {
                            break;
                        } else {
                            upValB--;
                        }
                    }
                }
            }

            // находим минимальное и максимальное значение из всех цветов
            int minCol = std::min(downValR, std::min(downValG, downValB));
            int maxCol = std::max(upValR, std::max(upValG, upValB));

            if (minCol != maxCol) { // если картинка не одноцветная

// изменяем контрастность изображения, так,
// чтобы равномерно все растянуть (также учитываем, что у нас арифметика с насыщением)
#pragma omp parallel for default(none) schedule(static) shared(dataR, dataG, dataB, maxVal, minCol, maxCol, size)
                for (size_t j = 0; j < size; j++) {
                    dataR[j] = std::max(std::min(((dataR[j] - minCol) * maxVal) / (maxCol - minCol), maxVal), 0);
                    dataG[j] = std::max(std::min(((dataG[j] - minCol) * maxVal) / (maxCol - minCol), maxVal), 0);
                    dataB[j] = std::max(std::min(((dataB[j] - minCol) * maxVal) / (maxCol - minCol), maxVal), 0);
                }
            }

            auto finish = std::chrono::high_resolution_clock::now();
            auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(finish - start);

// выводим данные в выходной файл
            std::ofstream out;
            out.open(outFileName, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!out.is_open()) {
                std::cout << "This output file can't be read or found";
                return 1;
            }
            out << magicNumber << std::endl << width << " " << height << std::endl << maxVal << std::endl;
            for (size_t j = 0; j < size; j++) {
                out << static_cast<char>((std::byte) dataR[j])
                    << static_cast<char>((std::byte) dataG[j])
                    << static_cast<char>((std::byte) dataB[j]);
            }
            out.close();

            auto ee = static_cast<double>(microseconds.count());
            printf("Time (%i thread(s)) : %g ms\n", countThreads, ee / 1000);

            delete[] dataR;
            delete[] dataG;
            delete[] dataB;

            delete[] valuesR;
            delete[] valuesG;
            delete[] valuesB;
        } else {
            std::cout << "Wrong object (not PGM or PPM)";
        }
}