#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

// ==================================================
// CONFIGURAÇÕES DO FILTRO FIR (Passa-Baixas)
// ==================================================
#define FIR_TAPS 32

// Coeficientes pré-calculados para um Filtro Passa-Baixa de Janela (ex: Hamming)
// N = 32. A soma de todos os coeficientes é aproximadamente 1.0.
static float fir_coeffs[FIR_TAPS] = {
    0.0016f, 0.0028f, 0.0048f, 0.0080f, 0.0125f, 0.0183f, 0.0254f, 0.0336f,
    0.0427f, 0.0524f, 0.0622f, 0.0718f, 0.0805f, 0.0880f, 0.0938f, 0.0976f,
    0.0976f, 0.0938f, 0.0880f, 0.0805f, 0.0718f, 0.0622f, 0.0524f, 0.0427f,
    0.0336f, 0.0254f, 0.0183f, 0.0125f, 0.0080f, 0.0048f, 0.0028f, 0.0016f
};

// Vetor que guarda o histórico das últimas 32 amostras
static float x_history[FIR_TAPS] = {0.0f};

// Função de Convolução do Filtro FIR
float apply_fir_windowed(float new_sample) {
    float output = 0.0f;

    // 1. Deslocar todo o histórico antigo para a direita
    for (int i = FIR_TAPS - 1; i > 0; i--) {
        x_history[i] = x_history[i - 1];
    }
    
    // 2. Inserir a nova amostra no início do vetor
    x_history[0] = new_sample;

    // 3. Calcular a convolução (Multiplicação e Acumulação pesada)
    // É aqui que a CPU sem FPU vai "suar"!
    for (int i = 0; i < FIR_TAPS; i++) {
        output += x_history[i] * fir_coeffs[i];
    }

    return output;
}



/* * Regista o módulo de log para este ficheiro.
 * Parâmetro 1: Nome do módulo (aparecerá no monitor serial).
 * Parâmetro 2: Nível de log a ser compilado (LOG_LEVEL_DBG permite todos os níveis).
 */
LOG_MODULE_REGISTER(adc_log, LOG_LEVEL_DBG);


//ADC
#define ADC_RESOLUTION      12
#define ADC_GAIN            ADC_GAIN_1
#define ADC_REFERENCE       ADC_REF_INTERNAL
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME_DEFAULT
#define ADC_CHANNEL_ID      0  //Canal do ADC, veja a pinagem pino PTE 20
#define ADC_VREF_MV         3300
static int16_t sample_buffer;
static const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc0));


//adc data
static int32_t adcDataMv;
static int32_t adcFilteredDataMv;
static uint32_t adcFilteredTimeTaken;
static uint32_t timeData = 0;
static uint32_t acquisitionPeriod = 0;
static uint32_t quantAvaliableAdcData = 0;

//semaforo
K_SEM_DEFINE(sem_adc_data, 0, 1);
// mutex
K_MUTEX_DEFINE(mutex_adc_data);



// 1. Definir o tamanho da pilha (stack)
#define STACK_SIZE 1024
// 2. Definir a prioridade (números menores = maior prioridade)
#define THREAD_ADC_PRIORITY 1

void thread_adc(void *p1, void *p2, void *p3)
{
    LOG_INF("Thread ADC iniciada\n");

    //ADC
    if (!device_is_ready(adc_dev)) {
        LOG_ERR("ADC não está pronto\n");
        return;
    }

    struct adc_channel_cfg channel_cfg = {
        .gain = ADC_GAIN,
        .reference = ADC_REFERENCE,
        .acquisition_time = ADC_ACQUISITION_TIME,
        .channel_id = ADC_CHANNEL_ID,
        .differential = 0,
    };

    if (adc_channel_setup(adc_dev, &channel_cfg) != 0) {
        LOG_ERR("Erro ao configurar canal ADC\n");
        return;
    }

    struct adc_sequence sequence = {
        .channels    = BIT(ADC_CHANNEL_ID),
        .buffer      = &sample_buffer,
        .buffer_size = sizeof(sample_buffer),
        .resolution  = ADC_RESOLUTION,
    };

    uint32_t sys_clock_freq = sys_clock_hw_cycles_per_sec();

   
    while (1) {
 
        //ADC
        int err = adc_read(adc_dev, &sequence);
        if (err != 0) {
            LOG_ERR("Falha na leitura do ADC: %d\n", err);
            printk("Falha na leitura do ADC: %d\n", err);
        } else {
            int32_t mv = sample_buffer;
            adc_raw_to_millivolts(ADC_VREF_MV, ADC_GAIN, ADC_RESOLUTION, &mv);
            //LOG_INF("ADC: %d (raw), %d mV\n", sample_buffer, mv);
            uint32_t loca_timeData = k_uptime_get_32();

            //Aplica o filtro Passa-Baixas
            uint32_t start_cycles = k_cycle_get_32();
            int32_t filtered_value = apply_fir_windowed(mv);
            //int32_t filtered_value = mv; //desliga filtro
            uint32_t end_cycles = k_cycle_get_32();

            uint32_t cycles_taken = end_cycles - start_cycles;
            uint32_t time_taken_us = (cycles_taken * 1000000) / sys_clock_freq;

            
            k_mutex_lock(&mutex_adc_data, K_FOREVER);
            adcDataMv = mv;
            adcFilteredDataMv = filtered_value;
            acquisitionPeriod = loca_timeData - timeData;
            timeData = loca_timeData;
            adcFilteredTimeTaken = time_taken_us;
            quantAvaliableAdcData++;
            k_mutex_unlock(&mutex_adc_data);

            k_sem_give(&sem_adc_data);
            /*Se o contador já estiver em 1 e a thread do ADC chamar k_sem_give(&sem_adc_data), 
            o Zephyr simplesmente ignora o comando. O contador não passa para 2, a thread não trava*/
        }

        //Wait
        k_sleep(K_MSEC(5)); 
        // LOG_INF sem filtro => 5
        // LOG_INF com filtro => 5
    }

}

uint16_t i_global = 0;

void thread_log(void *p1, void *p2, void *p3){

    LOG_INF("Thread log iniciada\n");

    // Variáveis locais para guardar a cópia
    uint32_t local_time;
    int32_t local_mv;
    uint32_t local_quant;
    uint32_t local_adcFilteredTimeTaken;
    uint32_t local_acquisitionPeriod;
    uint32_t local_adcFilteredDataMv;

    while (1)
    {
        k_sem_take(&sem_adc_data, K_FOREVER); //dorme ate ter informacao para enviar

        // 1. ZONA CRÍTICA: Faz a cópia e liberta o ADC imediatamente
        k_mutex_lock(&mutex_adc_data, K_FOREVER);
        local_quant = quantAvaliableAdcData;
        local_time = timeData;
        local_mv = adcDataMv;
        quantAvaliableAdcData = 0; // Reseta o contador 
        local_adcFilteredTimeTaken = adcFilteredTimeTaken;
        local_acquisitionPeriod = acquisitionPeriod;
        local_adcFilteredDataMv = adcFilteredDataMv;
        k_mutex_unlock(&mutex_adc_data);

        // 2. AVALIAÇÃO DE PERDA DE DADOS
        if(local_quant > 1){
            // Não esquecer o \n para forçar o Zephyr a enviar pela UART
            printk("Data lost! Amostras sobrescritas: %d\n", local_quant - 1);
        }

        // 3. ENVIO DA MENSAGEM
        LOG_INF("L%d#%d#%d", timeData, adcDataMv, local_adcFilteredDataMv);
        //LOG_INF("acquisitionPeriod = %d",local_acquisitionPeriod);
        //LOG_INF("fir time taken (us) = %d",local_adcFilteredTimeTaken);
        //printk("P%d#%d\n", local_time, local_mv);
        
        
        // 4. GARGALO INTENCIONAL para simular o atraso da UART
        //k_sleep(K_MSEC(10));
        /*
        aparentemente se tempo de execucao da thread log eh maior que o tempo de sleep da thread adc, entao acontece Starvation:
        a cpu fica 100% do tempo presa entre as 2 thread, assim fica sem tempo de "respirar" para que as tarefas básicas de I/O
        de fundo (como limpar buffers e enviar o serial) possam ser executadas
        */
        //esses 2 travam a cpu e nada mais funciona
        //k_busy_wait(5000); // Espera microssegundos exatos sem liberar a thread
        //delayMs(10);
        
    }
    
}

// cria as thread
K_THREAD_DEFINE(thread_adc_id, STACK_SIZE, thread_adc, NULL, NULL, NULL, THREAD_ADC_PRIORITY, 0, 0);
K_THREAD_DEFINE(thread_log_id, STACK_SIZE, thread_log, NULL, NULL, NULL, THREAD_ADC_PRIORITY + 1, 0, 0);


void main(void)
{
    LOG_INF("iniciando ver1.2 ...\n");
    
    // ==========================================
    // NORMALIZAÇÃO DO FILTRO FIR (Correção de Ganho DC)
    // ==========================================
    float soma_coeficientes = 0.0f;
    
    // 1. Calcula a soma total dos coeficientes arredondados
    for (int i = 0; i < FIR_TAPS; i++) {
        soma_coeficientes += fir_coeffs[i];
    }
    
    // 2. Divide cada coeficiente pela soma total. 
    // Isso garante matematicamente que a nova soma será exatamente 1.000000!
    for (int i = 0; i < FIR_TAPS; i++) {
        fir_coeffs[i] = fir_coeffs[i] / soma_coeficientes;
    }
    
    printk("Filtro Normalizado!\n");
    // ==========================================
    
    while (1) {
        k_sleep(K_FOREVER);
    }
}