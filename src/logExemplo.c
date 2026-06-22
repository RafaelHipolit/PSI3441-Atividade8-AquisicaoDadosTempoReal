#include <zephyr/kernel.h>
/* Inclui o cabeçalho oficial do subsistema de log */
#include <zephyr/logging/log.h>

/* * Regista o módulo de log para este ficheiro.
 * Parâmetro 1: Nome do módulo (aparecerá no monitor serial).
 * Parâmetro 2: Nível de log a ser compilado (LOG_LEVEL_DBG permite todos os níveis).
 */
LOG_MODULE_REGISTER(meu_sensor, LOG_LEVEL_DBG);

void main3(void) {
    int contador = 0;
    int temperatura_simulada = 25;

    /* LOG_INF é ideal para mensagens de estado normais do sistema */
    LOG_INF("Iniciando o sistema de telemetria via Zephyr Logging...");
    k_msleep(1000);

    while (1) {
        /* LOG_DBG é útil para monitorizar variáveis internas e fluxo (geralmente oculto em produção) */
        LOG_DBG("A iterar loop principal. Ciclo: %d", contador);

        // Simulando variação de temperatura
        if (contador % 4 == 0) {
            temperatura_simulada += 15; // Causa um pico de calor
        } else if (contador % 5 == 0) {
            temperatura_simulada = -50; // Simula falha catastrófica de leitura
        } else {
            temperatura_simulada = 25;  // Volta ao normal
        }

        // Condicionais para os diferentes níveis de log
        if (temperatura_simulada < -40) {
            /* LOG_ERR para falhas críticas (Aparece a VERMELHO no terminal) */
            LOG_ERR("Falha no sensor! Leitura irreal: %d C", temperatura_simulada);
        } 
        else if (temperatura_simulada > 35) {
            /* LOG_WRN para situações anormais, mas não fatais (Aparece a AMARELO) */
            LOG_WRN("Atenção! Temperatura alta detectada: %d C", temperatura_simulada);
        } 
        else {
            /* LOG_INF para fluxo normal da aplicação (Cor padrão do terminal) */
            LOG_INF("Temperatura estável em %d C", temperatura_simulada);
        }

        contador++;
        k_msleep(2000); // Aguarda 2 segundos para não inundar a porta serial
    }
}

//para teste
void main4(void){
    printk("Iniciando teste...");
    k_msleep(1000);
    int i = 0;
    while(1){
        //LOG_INF("i=%d\n", i);
        printk("i=%d\n", i);
        k_msleep(1);
        i++;
    } 
}