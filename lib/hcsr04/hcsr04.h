#ifndef HCSR04_H
#define HCSR04_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Inicializa os temporizadores (TPM1 e TPM2) e as interrupções 
 * necessárias para operar o sensor HC-SR04.
 */
void hcsr04_init(void);

/**
 * @brief Verifica se uma nova leitura de distância foi concluída pelo hardware.
 * @return true se há novos dados, false caso contrário. Ao retornar true, 
 * a flag interna é limpa automaticamente.
 */
bool hcsr04_is_data_ready(void);

/**
 * @brief Retorna a última distância calculada pelo Input Capture.
 * @return Distância em centímetros.
 */
uint32_t hcsr04_get_distance_cm(void);

#endif // HCSR04_H