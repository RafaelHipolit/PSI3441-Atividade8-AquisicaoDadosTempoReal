#include "hcsr04.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "pwm_z42.h"

// ================= MACROS de mascaras DE REGISTRADORES do TPM =================
#define TPM_INPUT_CAPTURE_BOTH (TPM_CnSC_ELSA_MASK | TPM_CnSC_ELSB_MASK) // config TPM para gerar irq quando detectar borda de descida E borda de subida
#define TPM_CHANNEL_INTERRUPT  (TPM_CnSC_CHIE_MASK) // habilita irq
#define TPM_PWM_HIGH_TRUE      (TPM_CnSC_MSB_MASK | TPM_CnSC_ELSB_MASK) //saida do pwm com dutycycle alto

// ================= CONFIGURAÇÕES DO ECHO INPUT (INPUT CAPTURE) ===
#define TPM_IRQ_LINE       TPM1_IRQn
#define TPM_IRQ_PRIORITY   1
#define TPM_ECHO           TPM1
#define CANAL_ECHO         0
#define PINO_ECHO_PORT     GPIOE
#define PINO_ECHO_NUM      20

// ================= CONFIGURAÇÕES DO TRIGGER OUTPUT (PWM) ==========
#define TPM_TRIGGER        TPM2
#define CANAL_TRIGGER      0
#define PINO_TRIG_PORT     GPIOB // Mude para a porta do seu pino TPM2_CH0
#define PINO_TRIG_NUM      2    // Mude para o número do seu pino TPM2_CH0

// ================= CONFIGURAÇÕES DE TEMPO ==================
// Clock 8 MHz, Prescaler 16 => 1 Tick = 2 microssegundos (us)
#define MODULO_100MS       50000 // 50.000 * 2us = 100ms
#define PULSO_20US         10    // 10 * 2us = 20us

// ================= VARIÁVEIS INTERNAS (Encapsuladas) =======================
// coloca as variaveis como volatile para evitar otimizacao.
// sem o volatile, o compilador pode acabar vedo que no codigo c elas nao sao alterardas diretemente no main(), 
// achar que sao inuteis(pois ele nao tem conhecimento de irq) e eliminalar na compilacao
static volatile uint16_t t1 = 0;
static volatile uint16_t t2 = 0;
static volatile uint16_t largura_ticks = 0;
static volatile bool nova_captura = false;
static volatile bool first_irq = true;

// ================= TRATADOR DE INTERRUPÇÃO (ISR) ===========
void tpm1_isr(void *arg)
{
    // Limpa a flag de interrupção forçando '1' no bit CHF do canal 0
    TPM1->CONTROLS[CANAL_ECHO].CnSC |= TPM_CnSC_CHF_MASK;

    // Variavel estática para lembrar se estamos na subida ou descida
    // Quando declara uma variavel local como static, ela fica restrita aquela função (nenhuma outra função a consegue ver ou modificar),
    // mas ela não morre quando a funcao termina. Ela guarda o valor para a próxima vez que a função for chamada. 
    // Eh perfeita para criar "máquinas de estados" dentro de ISRs.
    // boas práticas de arquitetura de software (Encapsulamento)
    static uint8_t estado_borda = 0; 
    uint16_t valor_atual = TPM1->CONTROLS[CANAL_ECHO].CnV;

    if (first_irq)
    {
        first_irq = false;
    }
    else
    {
        if (estado_borda == 0) 
        {
            // Borda de Subida detetada (início do pulso)
            t1 = valor_atual;
            estado_borda = 1;
        } 
        else 
        {
            // Borda de Descida detetada (fim do pulso)
            t2 = valor_atual;

            largura_ticks = t2 - t1;

            if (largura_ticks < 20000){
                // nao houve Inversão de Fase
                nova_captura = true; // Avisa a main que há um dado novo
                estado_borda = 0;    // Reinicia para aguardar o próximo pulso
            }else{
                printk("possivel Inversao de Fase -> tick = %u \n", largura_ticks);
                t1 = valor_atual;
                // mantem estado_borda = 1;
            }
            
        }
    }
}


// ================= FUNÇÕES DA API =====================

void hcsr04_init(void){
    // 1. Configura a Interrupção do Input Capture no Zephyr
    IRQ_CONNECT(TPM_IRQ_LINE, TPM_IRQ_PRIORITY, tpm1_isr, NULL, 0);
    irq_enable(TPM_IRQ_LINE);

    // 2. Inicializa o TPM1 (Echo) - Contagem contínua até 65535
    pwm_tpm_Init(TPM_ECHO, TPM_OSCERCLK, 65535, TPM_CLK, PS_16, EDGE_PWM);

    /*
    OBS.: Macro PS_16 eh um prescaler de 16
    O Prescaler é um divisor de frequência por hardware.
    Se o Prescaler for 1, o contador dá 1 passo para cada pulso de clock.
    Se o Prescaler for 16, o clock precisa dar 16 pulsos para que o contador avance 1 passo (1 tick).
    */
    
    // Configura TPM1_CH0 para capturar ambas as bordas com interrupção
    pwm_tpm_Ch_Init(TPM_ECHO, CANAL_ECHO, TPM_INPUT_CAPTURE_BOTH | TPM_CHANNEL_INTERRUPT, PINO_ECHO_PORT, PINO_ECHO_NUM);

    // 3. Inicializa o TPM2 (Trigger) - Período de 100ms
    pwm_tpm_Init(TPM_TRIGGER, TPM_OSCERCLK, MODULO_100MS, TPM_CLK, PS_16, EDGE_PWM);
    
    // Configura TPM2_CH0 para gerar PWM
    pwm_tpm_Ch_Init(TPM_TRIGGER, CANAL_TRIGGER, TPM_PWM_HIGH_TRUE, PINO_TRIG_PORT, PINO_TRIG_NUM);
    
    // Define a largura do pulso de Trigger (10 ticks = 20 us)
    TPM_TRIGGER->CONTROLS[CANAL_TRIGGER].CnV = PULSO_20US;
}

bool hcsr04_is_data_ready(void)
{
    if (nova_captura) {
        nova_captura = false; 
        return true;
    }
    return false;
}

uint32_t hcsr04_get_distance_cm(void)
{
    // Aplica a fórmula física da distância em cm
    // Como 1 tick = 2us, a distância é: (ticks * 2) / 58 = ticks / 29
    return largura_ticks / 29;
}