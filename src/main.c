#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "pwm_z42.h"
#include "hcsr04.h"

// TODO: colocar uns comentarios sobre funcinamento

void main(void)
{
    printk("Sistema Inicializado. Lendo Sensor HC-SR04...\n");

    // Inicializa o hardware do sensor com uma unica chamada
    hcsr04_init();

    while (1)
    {
        // Verifica continuamente (sem travar a CPU) se o sensor concluiu uma medicao
        if (hcsr04_is_data_ready())
        {
            uint32_t distancia = hcsr04_get_distance_cm();
            printk("Distancia atual: %u cm\n", distancia);
        }
        
        k_msleep(10); // Pausa leve para nao sobrecarregar o loop
    }
}